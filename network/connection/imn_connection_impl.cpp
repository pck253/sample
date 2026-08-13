#include "pch.h"

static_assert(NETWORK_MODULE == 1);

ImnJobQueue::ImnJobQueue(ThreadPool& _threadPool, const ConnectionShared_t& _conn)
    : Super_t(_threadPool), m_connection(_conn)
{
};

//-----------------------------------------------------------------------------

ImnConnectionImpl::ImnConnectionImpl(const ConnectionId_t _connectionId, ConnectionManager& _connectionManager, const ReceivedHandler_t _receivedHandler)
    : Connection(_connectionId, false),
    m_connectionManager(_connectionManager),
    m_receivedHandler(_receivedHandler)
{
}

ImnConnectionImpl::~ImnConnectionImpl()
{
    Log("ImnConnectionImpl is deleted. connection id={}", m_connectionId);
}

void ImnConnectionImpl::InitJobQueue(ThreadPool& _threadPool)
{
    const auto self = Get();
    m_jobQueue = ImnJobQueue::Create<ImnJobQueue>(_threadPool, self);
}

Result ImnConnectionImpl::Send(const PacketSize_t& _size, const uint8_t* _serializedData, const PacketDeallocatorShared_t& _deallocator)
{
    if (m_isClosed)
    {
        return EError::ClosedSocket;
    }

    std::vector<uint8_t> rawData(_size);
    memcpy_s(&rawData[0], _size, _serializedData, _size);

    m_jobQueue->PushJob([this, sendRawData = std::move(rawData)](SerializedJobQueue& _jobQueue) mutable
        {
            const auto size = sendRawData.size();

            Result ret;
            if (auto const& targetConnection = GetTargetConnection())
            {
                ImnConnectionImpl* tConn = static_cast<ImnConnectionImpl*>(targetConnection.get());
                ret = tConn->Receive(std::move(sendRawData));
            }

            OnSent(ret, size);
        });

    return EError::Success;
}

void ImnConnectionImpl::OnSent(const Result& _error, const size_t _bytesTransferred)
{
    if (m_isClosed)
    {
        return;
    }
}

Result ImnConnectionImpl::Close(const Result& _reason)
{
    bool expect = false;
    if (!m_isClosed.compare_exchange_strong(expect, true))  // check already closed with set closed.
    {
        return EError::ClosedSocket;
    }

    m_jobQueue->PushJob([_reason, this](SerializedJobQueue& _jobQueue)
        {
            m_receivedHandler = {};

            auto& targetConnection = GetTargetConnection();
            if (targetConnection)
            {
                targetConnection->Close(_reason);
                targetConnection.reset();
            }

            auto& imnJobQueue = _jobQueue.As<ImnJobQueue>();
            imnJobQueue.ReleaseConnection();

            // Asynchronously call reason
            //   - m_connectionManager is call connection instance's member function : flow is top-down => ok
            //   - connection instance is call connection m_connectionManager 's member function : flow is down-top => possible dead lock
            m_connectionManager.OnClosed(_reason, GetConnectionId(), m_isPublic);
        });
    m_jobQueue->StopPush("imn job queue shutdown.");

    return EError::Success;
}

Result ImnConnectionImpl::Receive(std::vector<uint8_t>&& _rawData)
{
    if (m_isClosed)
    {
        return EError::ClosedSocket;
    }

    m_jobQueue->PushJob([this, recvRawData = std::move(_rawData)](SerializedJobQueue& _jobQueue) mutable
        {
            if (m_receivedHandler)
            {
                const auto& imnJobQueue = _jobQueue.As<ImnJobQueue>();
                m_receivedHandler(std::move(recvRawData), imnJobQueue.GetConnection());
            }
        });

    return EError::Success;
}