#include "pch.h"

static_assert(NETWORK_MODULE == 1);

#ifdef _DEBUG
#define CHECK_ONSEND_DUPLICATE_CALL
#endif

SocketConnectionImpl::SocketConnectionImpl(asio::ip::tcp::socket* _socket, const ConnectionId_t _connectionId, asio::io_context* _ioContext,
    const bool& _isPublic, const AcceptorIndex& _acceptorIndex, ConnectionManager& _connectionManager, const ReceivedHandler_t _receivedHandler, const ClosedHandler_t _closedHandler)
    : Connection(_connectionId, _isPublic),
    m_socket(_socket), m_ioContext(_ioContext), m_acceptorIndex(_acceptorIndex), m_connectionManager(_connectionManager), m_receivedHandler(_receivedHandler), m_closedHandler(_closedHandler)
{
}

SocketConnectionImpl::~SocketConnectionImpl()
{
    SAFE_DELETE(m_socket);
    m_ioContext = nullptr;

    Packet* packet = nullptr;
    while (m_packetBunch.try_pop(packet))
    {
        SAFE_DELETE(packet);
    }

    Log("SocketConnectionImpl is deleted. connection id={}", m_connectionId);
}

Result SocketConnectionImpl::Send(const PacketSize_t& _size, const uint8_t* _serializedData, const PacketDeallocatorShared_t& _deallocator)
{
    if (m_isClosed)
    {
        return EError::ClosedSocket;
    }

    if (not _serializedData || 0 == _size)
    {
        return EError::EmptyPacket;
    }

    m_packetBunch.push(new Packet(_serializedData, _size, _deallocator));
    ++m_packetBunchCount;
    auto old = m_packetBunchBytes.fetch_add((_size + PACKET_SIZE_BYTE));

    if (old == 0)
    {
#ifdef CHECK_ONSEND_DUPLICATE_CALL
        ++m_onSendCallCount;
#endif
        asio::post(asio::bind_executor(*m_ioContext, std::bind(&SocketConnectionImpl::OnSend, this, Get())));
    }

    return EError::Success;
}

void SocketConnectionImpl::OnSend(ConnectionShared_t _self)
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
            if (!result)
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

    std::vector<asio::const_buffer> buffers;
    buffers.reserve(m_reservedSendDatas.size());
    for (auto& sendData : m_reservedSendDatas)
    {
        uint8_t* buffer = std::get<BUFFER_ADDR_INDEX>(sendData);
        size_t size = std::get<BUFFER_SIZE_INDEX>(sendData);

        buffers.push_back(asio::buffer(buffer, size));
    }

    m_socket->async_send(buffers,
        asio::bind_executor(*m_ioContext, std::bind(&SocketConnectionImpl::OnSent, this, std::placeholders::_1, std::placeholders::_2, _self)));
}

void SocketConnectionImpl::OnSent(const asio::error_code& _error, const size_t& _bytesTransferred, ConnectionShared_t _self)
{
    if (m_isClosed)
    {
        return;
    }

    if (!_error)
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

        auto old = m_packetBunchBytes.fetch_sub(_bytesTransferred);
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

Result SocketConnectionImpl::Close(const Result& _reason)
{
    bool expect = false;
    if (!m_isClosed.compare_exchange_strong(expect, true))  // check already closed with set closed.
    {
        return EError::ClosedSocket;
    }

    asio::error_code ec;
    m_socket->shutdown(asio::ip::tcp::socket::shutdown_both, ec); // no more can't request async job
    m_socket->close(ec);    // disconnect && destroy socket

    if (m_closedHandler)
    {
        (*m_closedHandler)(_reason, GetConnectionId(), m_isPublic);
        m_closedHandler = nullptr;
    }

    // Asynchronously call reason
    //   - m_connectionManager is call connection instance's member function : flow is top-down => ok
    //   - connection instance is call connection m_connectionManager 's member function : flow is down-top => possible dead lock
    asio::post(asio::bind_executor(*m_ioContext, std::bind(&ConnectionManager::OnClosed, &m_connectionManager, GetConnectionId(), m_isPublic)));

    return EError::Success;
}

void SocketConnectionImpl::Receive()
{
    bool resetHandler = true;
    FinalJob _([&resetHandler, this]()
        {
            if (resetHandler)
            {
                m_receivedHandler = nullptr;
            }
        });

    if (m_isClosed)
    {
        return;
    }

    ReservedReceiveBuffer_t receiveBuffers = m_receiveBuffer.GetWritableBuffer();
    if (receiveBuffers.empty())
    {
        LogError("Receive failed to receive.");
        Close(EError::LackReceiveBuffer);
        return;
    }

    std::vector<asio::mutable_buffer> buffers;
    buffers.reserve(receiveBuffers.size());
    for (auto& buffer : receiveBuffers)
    {
        buffers.push_back(asio::buffer(buffer.first, buffer.second));
    }
    m_socket->async_receive(buffers, 0,
        std::bind(&SocketConnectionImpl::OnReceived, this, std::placeholders::_1, std::placeholders::_2, Get()));

    resetHandler = false;
}

void SocketConnectionImpl::OnReceived(const asio::error_code& _error, const size_t& _bytesTransferred, ConnectionShared_t _self)
{
    bool resetHandler = true;
    FinalJob _([&resetHandler, this]()
        {
            if (resetHandler)
            {
                m_receivedHandler = nullptr;
            }
        });

    if (m_isClosed)
    {
        return;
    }

    if (!_error)
    {
        m_receiveBuffer.UpdateWritableBufferInfo(_bytesTransferred);

        while (true)
        {
            std::vector<uint8_t> rawData;
            m_receiveBuffer.Read(rawData);

            if (rawData.empty())
            {
                break;
            }

            if (m_receivedHandler)
            {
                (*m_receivedHandler)(std::move(rawData), _self);
            }
        }

        Receive();
        resetHandler = false;
    }
    else
    {
        LogWarning("OnReceived failed to receive.");
        Close(Result(EError::FailedReceive, _error.message()));
    }
}