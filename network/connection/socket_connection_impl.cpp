#include "pch.h"

static_assert(NETWORK_MODULE == 1);

#ifdef _DEBUG
#define CHECK_ONSEND_DUPLICATE_CALL
#endif

SocketConnectionImpl::SocketConnectionImpl(asio::ip::tcp::socket* _socket, const ConnectionId_t _connectionId, asio::io_context* _ioContext,
    const bool& _isPublic, const AcceptorIndex& _acceptorIndex, ConnectionManager& _connectionManager, const ReceivedHandler_t _receivedHandler)
    : Connection(_connectionId, _isPublic),
    m_ioContext(_ioContext), m_socket(_socket), m_strand(asio::make_strand(*_ioContext)),
    m_acceptorIndex(_acceptorIndex), m_connectionManager(_connectionManager), m_receivedHandler(_receivedHandler)
{
}

SocketConnectionImpl::~SocketConnectionImpl()
{
    SafeDelete(m_socket);
    m_ioContext = nullptr;

    PacketInfo* packet = nullptr;
    while (m_packetBunch.try_pop(packet))
    {
        SafeDelete(packet);
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

    m_packetBunch.push(new PacketInfo(_serializedData, _size, _deallocator));
    ++m_packetBunchCount;
    const auto old = m_packetBunchBytes.fetch_add((_size + PACKET_SIZE_BYTE));

    if (0 == old)
    {
#ifdef CHECK_ONSEND_DUPLICATE_CALL
        ++m_onSendCallCount;
#endif
        OnSend(Get());
    }

    return EError::Success;
}

void SocketConnectionImpl::OnSend(ConnectionShared_t&& _self)
{
#ifdef CHECK_ONSEND_DUPLICATE_CALL
    --m_onSendCallCount;
    if (m_onSendCallCount > 0)
    {
        LogError("OnSend call duplication.");
    }
#endif

    if (m_isClosed)
    {
        return;
    }

    uint32_t count = m_packetBunchCount.exchange(0);

    PacketInfo* packet = nullptr;
    for (uint32_t i = 0; i < count; ++i)
    {
        if (m_packetBunch.try_pop(packet))
        {
            const auto result = m_sendBuffer.Write(packet->size, packet->serializedData, m_reservedSendDatas);
            if (!result)
            {
                SafeDelete(packet);
                LogWarning("not exist send buffer.");
                Close(result);
                return;
            }
            SafeDelete(packet);
        }
        else
        {
            LogWarning("fail pop packetBunch");
            break;
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

    asio::post(m_strand,
        [self = std::move(_self), this, buffers = std::move(buffers)]() mutable
        {
            if (m_isClosed)
            {
                return;
            }

            m_socket->async_send(
                buffers,
                [this, self = std::move(self)]
                (const asio::error_code& error, size_t bytesTransferred) mutable
                {
                    OnSent(error, bytesTransferred, std::move(self));
                });
        });
}

void SocketConnectionImpl::OnSent(const asio::error_code& _error, const size_t _bytesTransferred, ConnectionShared_t&& _self)
{
    if (m_isClosed)
    {
        return;
    }

    if (!_error)
    {
        auto ramainTransferredSize = _bytesTransferred;
        for (ReservedSendData_t::iterator itr = m_reservedSendDatas.begin(); itr != m_reservedSendDatas.end();)
        {
            size_t& size = std::get<BUFFER_SIZE_INDEX>(*itr);
            size_t backupSize = size;

            size = (size <= ramainTransferredSize) ? 0 : size - ramainTransferredSize;
            size_t thisBuffReleaseSize = (0 == size) ? backupSize : ramainTransferredSize;

            SendBufferInfo* info = std::get<BUFFER_INFO_INDEX>(*itr);
            m_sendBuffer.UpdateUsedBufferInfo(info, thisBuffReleaseSize);

            ramainTransferredSize -= thisBuffReleaseSize;
            if (0 == size)
            {
                itr = m_reservedSendDatas.erase(itr);
            }
            else
            {
                auto& buffer = std::get<BUFFER_ADDR_INDEX>(*itr);
                buffer += thisBuffReleaseSize;
                ++itr;
            }
            if (0 == ramainTransferredSize)
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
            OnSend(std::move(_self));
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

    // OnClosed also runs in this job, so it stays asynchronous to the caller :
    //   - m_connectionManager is call connection instance's member function : flow is top-down => ok
    //   - connection instance is call connection m_connectionManager 's member function : flow is down-top => possible dead lock
    asio::post(m_strand, [self = Get(), this, _reason]()
        {
            asio::error_code ec;
            m_socket->shutdown(asio::ip::tcp::socket::shutdown_both, ec); // no more can't request async job
            m_socket->close(ec);    // disconnect && destroy socket

            m_connectionManager.OnClosed(_reason, GetConnectionId(), m_isPublic);
        });

    return EError::Success;
}

void SocketConnectionImpl::Receive(ConnectionShared_t&& _self)
{
    if (m_isClosed)
    {
        m_receivedHandler = {};
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

    asio::post(m_strand,
        [self = std::move(_self), this, buffers = std::move(buffers)]() mutable
        {
            if (m_isClosed)
            {
                m_receivedHandler = {};
                return;
            }

            m_socket->async_receive(
                buffers,
                0,
                [this, self = std::move(self)]
                (const asio::error_code& error, size_t bytesTransferred) mutable
                {
                    OnReceived(error, bytesTransferred, std::move(self));
                });
        });
}

void SocketConnectionImpl::OnReceived(const asio::error_code& _error, const size_t _bytesTransferred, ConnectionShared_t&& _self)
{
    if (m_isClosed)
    {
        m_receivedHandler = {};
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

        Receive(std::move(_self));
    }
    else
    {
        LogWarning("OnReceived failed to receive.");
        Close(Result(EError::FailedReceive, _error.message()));
    }
}