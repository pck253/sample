#include "pch.h"

static_assert(NETWORK_MODULE == 1);

SendBuffer::SendBuffer()
{
	m_lastUsingBufferInfo = GetFreeBuffer();
	m_usingBuffers.emplace(m_lastUsingBufferInfo);
}

SendBuffer::~SendBuffer()
{
	Log("allocated sendbuffer size : {}", m_allocatedSize);
	m_lastUsingBufferInfo = nullptr;
	while (!m_freeBufferQueue.empty())
	{
		auto bufferInfo = m_freeBufferQueue.front();
		
		g_memoryPool->deallocate(bufferInfo->buffer, MAX_PACKET_SIZE);
		bufferInfo->buffer = nullptr;
		g_memoryPool->deallocate((uint8_t*)bufferInfo, sizeof(SendBufferInfo));

		m_freeBufferQueue.pop();
	}

	for (auto bufferInfo : m_usingBuffers)
	{
		g_memoryPool->deallocate(bufferInfo->buffer, MAX_PACKET_SIZE);
		bufferInfo->buffer = nullptr;
		g_memoryPool->deallocate((uint8_t*)bufferInfo, sizeof(SendBufferInfo));
	}
}

SendBufferInfo* SendBuffer::GetFreeBuffer()
{
	if (m_freeBufferQueue.empty())
	{
		m_allocatedSize += MAX_PACKET_SIZE;
		
		auto memory = g_memoryPool->allocate(sizeof(SendBufferInfo));
		return new (memory)SendBufferInfo{ g_memoryPool->allocate(MAX_PACKET_SIZE), 0, 0};
	}

	auto buffer = m_freeBufferQueue.front();
	m_freeBufferQueue.pop();

	return buffer;
}

Result SendBuffer::Write(const PacketSize_t& _packetSize, const uint8_t* _serializedData, ReservedSendData_t& _output)
{
	if (_packetSize > (MAX_PACKET_SIZE - PACKET_SIZE_BYTE))
	{
		return EError::OverMaxPacketSize;
	}
	PacketSize_t includeHeaderSize = _packetSize + PACKET_SIZE_BYTE;

	/////////////////////////////////////////////////////////////////////////////////////////////
	// allocate buffers
	//	: using max 2 buffers because buffer total size is MAX_PACKET_SIZE.
	size_t freeSize = MAX_PACKET_SIZE - m_lastUsingBufferInfo->writableIndex;
	size_t remainSize = (includeHeaderSize <= freeSize) ? 0 : includeHeaderSize - freeSize;

	SendBufferInfo* bufferInfo01 = (freeSize != 0) ? m_lastUsingBufferInfo : nullptr;
	SendBufferInfo* bufferInfo02 = nullptr;
	if (remainSize > 0)
	{
		bufferInfo02 = GetFreeBuffer();
		if (bufferInfo02 == nullptr)
		{
			return EError::NotExistSendBuffer;
		}
		m_lastUsingBufferInfo = bufferInfo02;
		m_usingBuffers.emplace(bufferInfo02);
	}
	// allocate buffers
	/////////////////////////////////////////////////////////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////////////////////////////
	// copy to buffers
	{

		bool headerWrite = true;
		size_t headerRemain = PACKET_SIZE_BYTE;
		size_t dataRemain = _packetSize;

		auto copyFunc = [&headerWrite, &headerRemain, &dataRemain, &_packetSize, &_serializedData](uint8_t* _buffer, size_t _bufferSize)
			{
				if (headerWrite)
				{
					const uint8_t* header = ((const uint8_t*)&_packetSize) + (PACKET_SIZE_BYTE - headerRemain);
					if (_bufferSize >= headerRemain)
					{
						memcpy_s(_buffer, _bufferSize, header, headerRemain);
						_buffer += headerRemain;
						_bufferSize -= headerRemain;

						headerRemain = 0;
						headerWrite = false;
					}
					else
					{
						memcpy_s(_buffer, _bufferSize, header, _bufferSize);
						headerRemain -= _bufferSize;
					}
				}

				if (!headerWrite && _bufferSize > 0)
				{
					const uint8_t* data = _serializedData + (_packetSize - dataRemain);
					if (_bufferSize >= dataRemain)
					{
						memcpy_s(_buffer, _bufferSize, data, dataRemain);
						dataRemain = 0;
					}
					else
					{
						memcpy_s(_buffer, _bufferSize, data, _bufferSize);
						dataRemain -= _bufferSize;
					}
				}
			};

		if (bufferInfo01)
		{
			uint8_t* buffer = bufferInfo01->buffer + bufferInfo01->writableIndex;
			size_t bufferSize = includeHeaderSize - remainSize;

			if (_output.empty() ||
				std::get<BUFFER_INFO_INDEX>(_output.back()) != bufferInfo01)
			{
				_output.emplace_back(std::make_tuple(bufferInfo01, buffer, bufferSize));
			}
			else
			{
				// does not create new ReservedSendData because current buffer is same a last ReservedSendData's buffer.
				// only update send data size.
				std::get<BUFFER_SIZE_INDEX>(_output.back()) += bufferSize;
			}

			bufferInfo01->writableIndex += bufferSize;
			bufferInfo01->readableSize += bufferSize;

			copyFunc(buffer, bufferSize);
		}

		if (bufferInfo02)
		{
			uint8_t* buffer = bufferInfo02->buffer + bufferInfo02->writableIndex;
			size_t bufferSize = remainSize;

			_output.emplace_back(std::make_tuple(bufferInfo02, buffer, bufferSize));
			bufferInfo02->writableIndex += bufferSize;
			bufferInfo02->readableSize += bufferSize;

			copyFunc(buffer, bufferSize);
		}
	}
	// copy to buffers
	/////////////////////////////////////////////////////////////////////////////////////////////

	return EError::Success;
}

void SendBuffer::UpdateUsedBufferInfo(SendBufferInfo* _bufferInfo, const size_t& _usedSize)
{
	_bufferInfo->readableSize -= _usedSize;
	if (_bufferInfo->IsNotUse())
	{
		_bufferInfo->Reset();
		if (m_lastUsingBufferInfo != _bufferInfo)
		{
			m_freeBufferQueue.push(_bufferInfo);
			m_usingBuffers.erase(_bufferInfo);
		}
	}
}