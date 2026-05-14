#include "pch.h"

static_assert(NETWORK_MODULE == 1);

ImnConnectionImpl::ImnConnectionImpl(const ConnectionId_t _connectionId, ConnectionManager& _connectionManager, ThreadPool& _threadPool,
    const ReceivedHandler_t _receivedHandler, const ClosedHandler_t _closedHandler)
    : Connection(_connectionId, false),
    m_connectionManager(_connectionManager),
    m_receivedHandler(_receivedHandler),
    m_closedHandler(_closedHandler)
{
    m_jobQueue = SerializedJobQueue::Create(_threadPool);
    m_receivedJobQueue = SerializedJobQueue::Create(_threadPool);
}

ImnConnectionImpl::~ImnConnectionImpl()
{
    Log("ImnConnectionImpl is deleted. connection id={}", m_connectionId);
}

Result ImnConnectionImpl::Send(const PacketSize_t& _size, const uint8_t* _serializedData, const PacketDeallocatorShared_t& _deallocator)
{
    if (m_isClosed)
    {
        return EError::ClosedSocket;
    }

    std::vector<uint8_t> rawData(_size);
    memcpy_s(&rawData[0], _size, _serializedData, _size);

    m_jobQueue->PushJob([self = Get(), sendRawData = std::move(rawData)](SerializedJobQueue& _jobQueue) mutable
        {
            auto size = sendRawData.size();
            ImnConnectionImpl* conn = static_cast<ImnConnectionImpl*>(self.get());

            Result ret;
            auto targetConnection = conn->GetTargetConnection();
            if (targetConnection)
            {
                ImnConnectionImpl* tConn = static_cast<ImnConnectionImpl*>(targetConnection.get());
                ret = tConn->Receive(std::move(sendRawData));
            }

            conn->OnSent(ret, size);
        });

    return EError::Success;
}

void ImnConnectionImpl::OnSent(const Result& _error, const size_t& _bytesTransferred)
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

    m_receivedJobQueue->PushJob([_reason, this](SerializedJobQueue& _jobQueue)
        {
            m_receivedHandler = nullptr;
        });
    m_receivedJobQueue->Shutdown(EShutdownMode::CurrentJob, "imn received job queue shutdown.");

    m_jobQueue->PushJob([_reason, this](SerializedJobQueue& _jobQueue)
        {
            if (m_targetConnection)
            {
                m_targetConnection->Close(_reason);
                m_targetConnection.reset();
            }
        });
    m_jobQueue->Shutdown(EShutdownMode::CurrentJob, "imn job queue shutdown.");

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

    return EError::Success;
}

Result ImnConnectionImpl::Receive(std::vector<uint8_t>&& _rawData)
{
    if (m_receivedJobQueue->IsShutdown())
    {
        return EError::ClosedSocket;
    }

    m_receivedJobQueue->PushJob([self = Get(), receivedRawData = std::move(_rawData)](SerializedJobQueue& _jobQueue) mutable
        {
            ImnConnectionImpl* conn = static_cast<ImnConnectionImpl*>(self.get());
            conn->OnReceived(std::move(receivedRawData));
        });

    return EError::Success;
}

void ImnConnectionImpl::OnReceived(std::vector<uint8_t>&& _rawData)
{
    if (m_isClosed)
    {
        m_receivedHandler = nullptr;
        return;
    }

    if (m_receivedHandler)
    {
        m_receivedHandler(std::move(_rawData), Get());
    }
}