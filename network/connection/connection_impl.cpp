#include "pch.h"

#ifdef _DEBUG
#define CHECK_ONSEND_DUPLICATE_CALL
#endif

ConnectionImpl::ConnectionImpl(ip::tcp::socket* _socket, io_context* _ioContext, const bool& _isPublic,
    const AcceptorIndex& _acceptorIndex, ConnectionManager* _connectionManager)
    : m_socket(_socket), m_ioContext(_ioContext), m_strand(*_ioContext), m_acceptorIndex(_acceptorIndex), m_connectionManager(*_connectionManager)
{
    m_isPublic = _isPublic;
}

ConnectionImpl::~ConnectionImpl()
{
    SAFE_DELETE(m_socket);
    m_ioContext = nullptr;


    Packet* packet = nullptr;
    while (m_packetBunch.try_pop(packet))
    {
        SAFE_DELETE(packet);
    }

    if (m_connectionId == INVALID_CONNECTION_ID)
    {
        Log("ConnectionImpl is deleted. waiting accept or connecting fail");
    }
    else
    {
        Log("ConnectionImpl is deleted. connection id={}", m_connectionId);
    }
}

Result ConnectionImpl::Send(const PacketSize_t& _size, const uint8_t* _serializedData, PacketDeallocatorShared_t& _deallocator)
{
    if (m_isClosed)
    {
        return EError::ClosedSocket;
    }

    m_packetBunch.push(new Packet(_serializedData, _size, _deallocator));
    ++m_packetBunchCount;
    auto old = m_packetBunchBytes.fetch_add((_size + PACKET_SIZE_BYTE));

    if (old == 0)
    {
#ifdef CHECK_ONSEND_DUPLICATE_CALL
        ++m_onSendCallCount;
#endif
        boost::asio::post(bind_executor(m_strand, boost::bind(&ConnectionImpl::OnSend, this, Get())));
    }

    return EError::Success;
}

void ConnectionImpl::OnSend(ConnectionShared_t _self)
{
    if (m_isClosed)
    {
        return;
    }

#ifdef CHECK_ONSEND_DUPLICATE_CALL
    --m_onSendCallCount;
    if (m_onSendCallCount > 0)
    {
        LogError("OnSend call duplication.");
    }
#endif

    uint32_t count = m_packetBunchCount.exchange(0);

    Packet* packet = nullptr;
    for (uint32_t i = 0; i < count; ++i)
    {
        if (m_packetBunch.try_pop(packet))
        {
            auto result = m_sendBuffer.Write(packet->size, packet->serializedData, m_reservedSendDatas);
            if (result != EError::Success)
            {
                SAFE_DELETE(packet);
                LogWarning("not exist send buffer.");
                Close(result);
                return;
            }
            SAFE_DELETE(packet);
        }
        else
        {
            LogWarning("fail pop packetBunch");
        }
    }

    if (m_reservedSendDatas.empty())
    {
        LogError("empty reserved send data.");
        return;
    }

    our::vector<const_buffer> buffers;
    buffers.reserve(m_reservedSendDatas.size());
    for (auto& sendData : m_reservedSendDatas)
    {
        uint8_t* buffer = std::get<BUFFER_ADDR_INDEX>(sendData);
        size_t size = std::get<BUFFER_SIZE_INDEX>(sendData);

        buffers.push_back(boost::asio::buffer(buffer, size));
    }

    m_socket->async_send(buffers,
        bind_executor(m_strand, boost::bind(&ConnectionImpl::OnSent, this, placeholders::error, placeholders::bytes_transferred, _self)));
}

void ConnectionImpl::OnSent(const error_code& _error, const size_t& _bytesTransferred, ConnectionShared_t _self)
{
    if (m_isClosed)
    {
        return;
    }

    if (m_sentHandler)
    {
        m_sentHandler(!_error ? _bytesTransferred : 0);
    }

    if (!_error)
    {
        auto old = m_packetBunchBytes.fetch_sub(_bytesTransferred);
        {
            auto raminTransferredSize = _bytesTransferred;
            for (ReservedSendData_t::iterator itr = m_reservedSendDatas.begin(); itr != m_reservedSendDatas.end();)
            {
                size_t& size = std::get<BUFFER_SIZE_INDEX>(*itr);
                size_t backupSize = size;

                size = (size <= raminTransferredSize) ? 0 : size - raminTransferredSize;
                size_t thisBuffReleaseSize = (size == 0) ? backupSize : raminTransferredSize;

                SendBufferInfo* info = std::get<BUFFER_INFO_INDEX>(*itr);
                m_sendBuffer.UpdateUsedBufferInfo(info, thisBuffReleaseSize);

                raminTransferredSize -= thisBuffReleaseSize;
                if (size == 0)
                {
                    itr = m_reservedSendDatas.erase(itr);
                }
                else
                {
                    auto& buffer = std::get<BUFFER_ADDR_INDEX>(*itr);
                    buffer += thisBuffReleaseSize;
                    ++itr;
                }
                if (raminTransferredSize == 0)
                {
                    break;
                }
            }
        }

        if (old > _bytesTransferred)
        {
#ifdef CHECK_ONSEND_DUPLICATE_CALL
            ++m_onSendCallCount;
#endif
            OnSend(_self);
        }
    }
    else
    {
        LogWarning("failed to send.");
        Close(Result(EError::FailedSend, _error.message()));
    }
}

Result ConnectionImpl::Close(const Result& _reason)
{
    bool expect = false;
    if (!m_isClosed.compare_exchange_strong(expect, true))  // check already closed with set closed.
    {
        return EError::ClosedSocket;
    }

    error_code ec;
    m_socket->shutdown(ip::tcp::socket::shutdown_both, ec); // no more can't request async job
    m_socket->close(ec);

    // async call OnClosed because avoid lock in ConnectionManager::OnClosed
    auto executor = m_socket->get_executor();
    executor.execute(boost::bind(&ConnectionImpl::OnClosed, this, _reason, Get()));

    // because SentHandler use OnSend, OnSent
    boost::asio::post(bind_executor(m_strand, boost::bind(&ConnectionImpl::RelaseSentHandler, this, Get())));

    return EError::Success;
}

void ConnectionImpl::OnClosed(const Result& _reason, ConnectionShared_t _self)
{
    if (m_closedHandler)
    {
        m_closedHandler(_reason, GetConnectionId(), m_isPublic);
        SetClosedHandler(ClosedHandler_t());
    }

    m_connectionManager.OnClosed(GetConnectionId(), m_isPublic);
}

void ConnectionImpl::Receive()
{
    if (m_isClosed)
    {
        SetReceivedHandler(ReceivedHandler_t());
        return;
    }

    ReservedReceiveBuffer_t receiveBuffers = m_receiveBuffer.GetWritableBuffer();
    if (receiveBuffers.empty())
    {
        SetReceivedHandler(ReceivedHandler_t());

        LogError("Receive failed to receive.");
        Close(EError::LackReceiveBuffer);
        return;
    }

    our::vector<mutable_buffer> buffers;
    buffers.reserve(receiveBuffers.size());
    for (auto& buffer : receiveBuffers)
    {
        buffers.push_back(boost::asio::buffer(buffer.first, buffer.second));
    }
    m_socket->async_receive(buffers, 0,
        boost::bind(&ConnectionImpl::OnReceived, this, placeholders::error, placeholders::bytes_transferred, Get()));
}

void ConnectionImpl::OnReceived(const error_code& _error, const size_t& _bytesTransferred, ConnectionShared_t _self)
{
    if (m_isClosed)
    {
        SetReceivedHandler(ReceivedHandler_t());
        return;
    }

    if (!_error)
    {
        m_receiveBuffer.UpdateWritableBufferInfo(_bytesTransferred);

        while (true)
        {
            our::vector<uint8_t> rawData;
            m_receiveBuffer.Read(rawData);

            if (rawData.empty())
            {
                break;
            }

            if (m_receivedHandler)
            {
                m_receivedHandler(std::move(rawData), _self);
            }
        }

        Receive();
    }
    else
    {
        SetReceivedHandler(ReceivedHandler_t());

        LogWarning("OnReceived failed to receive.");
        Close(Result(EError::FailedReceive, _error.message()));
    }
}