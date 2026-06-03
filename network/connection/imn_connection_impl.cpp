#include "pch.h"

static_assert(NETWORK_MODULE == 1);

ImnJobQueue::ImnJobQueue(ThreadPool& _threadPool, const ConnectionShared_t& _conn)
    : Super_t(_threadPool), m_connection(_conn)
{
};

//-----------------------------------------------------------------------------

ImnConnectionImpl::ImnConnectionImpl(const ConnectionId_t _connectionId, ConnectionManager& _connectionManager,
    const ReceivedHandler_t _receivedHandler, const ClosedHandler_t _closedHandler)
    : Connection(_connectionId, false),
    m_connectionManager(_connectionManager),
    m_receivedHandler(_receivedHandler),
    m_closedHandler(_closedHandler)
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
    m_receivedJobQueue = ImnJobQueue::Create<ImnJobQueue>(_threadPool, self);
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
            auto& targetConnection = GetTargetConnection();
            if (targetConnection)
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

    m_receivedJobQueue->Shutdown("imn received job queue shutdown.");
    m_receivedJobQueue.reset();

    m_jobQueue->PushJob([_reason, this](SerializedJobQueue& _jobQueue)
        {
            auto& targetConnection = GetTargetConnection();
            if (targetConnection)
            {
                targetConnection->Close(_reason);
                targetConnection.reset();
            }
        });
    m_jobQueue->Shutdown("imn job queue shutdown.");

    if (m_closedHandler)
    {
        m_closedHandler(_reason, GetConnectionId(), m_isPublic);
        m_closedHandler = nullptr;
    }

    // Asynchronously call reason
    //   - m_connectionManager is call connection instance's member function : flow is top-down => ok
    //   - connection instance is call connection m_connectionManager 's member function : flow is down-top => possible dead lock
    m_jobQueue->GetThreadPool().PushJob([connectionManager = &m_connectionManager, conId = GetConnectionId(), isPublic = m_isPublic]()
        {
            connectionManager->OnClosed(conId, isPublic);
        });
    m_jobQueue.reset();

    return EError::Success;
}

Result ImnConnectionImpl::Receive(std::vector<uint8_t>&& _rawData)
{
    if (m_isClosed)
    {
        return EError::ClosedSocket;
    }

    m_receivedJobQueue->PushJob([this, receivedRawData = std::move(_rawData)](ImnJobQueue& _jobQueue) mutable
        {
            OnReceived(std::move(receivedRawData), _jobQueue.GetConnection());
        });

    return EError::Success;
}

void ImnConnectionImpl::OnReceived(std::vector<uint8_t>&& _rawData, const ConnectionShared_t& _self)
{
    if (m_isClosed)
    {
        m_receivedHandler = nullptr;
        return;
    }

    if (m_receivedHandler)
    {
        m_receivedHandler(std::move(_rawData), _self);
    }
}