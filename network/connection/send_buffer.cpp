#include "pch.h"

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

Result SendBuffer::Write(const PacketSize_t& _size, const uint8_t* _serializedData, ReservedSendData_t& _output)
{
	if (_size > (MAX_PACKET_SIZE - PACKET_SIZE_BYTE))
	{
		return EError::OverMaxPacketSize;
	}
	PacketSize_t includeHeaderSize = _size + PACKET_SIZE_BYTE;

	our::vector<ReservedSendData_t::value_type*> sendDatas;
	sendDatas.reserve(2);

	{
		// calc encryption size

		SendBufferInfo* bufferInfo01 = nullptr;
		SendBufferInfo* bufferInfo02 = nullptr;

		size_t freeSize = MAX_PACKET_SIZE - m_lastUsingBufferInfo->writableIndex;
		size_t remainSize = (includeHeaderSize <= freeSize) ? 0 : includeHeaderSize - freeSize;

		bufferInfo01 = (freeSize != 0) ? m_lastUsingBufferInfo : nullptr;
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

		if (bufferInfo01)
		{
			auto& ret = _output.emplace_back(std::make_tuple(bufferInfo01, bufferInfo01->buffer + bufferInfo01->writableIndex, (includeHeaderSize - remainSize)));
			sendDatas.emplace_back(&ret);
			bufferInfo01->writableIndex += (includeHeaderSize - remainSize);
			bufferInfo01->readableSize += (includeHeaderSize - remainSize);
		}

		if (bufferInfo02)
		{
			auto& ret = _output.emplace_back(std::make_tuple(bufferInfo02, bufferInfo02->buffer + bufferInfo02->writableIndex, remainSize));
			sendDatas.emplace_back(&ret);
			bufferInfo02->writableIndex += remainSize;
			bufferInfo02->readableSize += remainSize;
		}
	}

	// calc encryption and copy
	bool headerWrite = true;
	size_t headerRemain = PACKET_SIZE_BYTE;
	size_t dataRemain = _size;

	ReservedSendData_t remainBuffers;
	for (auto& sendData : sendDatas)
	{
		uint8_t* buffer = std::get<BUFFER_ADDR_INDEX>(*sendData);
		size_t size = std::get<BUFFER_SIZE_INDEX>(*sendData);

		if (headerWrite)
		{
			const uint8_t* header = ((const uint8_t*)&_size) + (PACKET_SIZE_BYTE - headerRemain);
			if (size >= headerRemain)
			{
				memcpy_s(buffer, size, header, headerRemain);
				buffer += headerRemain;
				size -= headerRemain;

				headerRemain = 0;
				headerWrite = false;
			}
			else
			{
				memcpy_s(buffer, size, header, size);
				headerRemain -= size;
				continue;
			}
		}

		const uint8_t* data = _serializedData + (_size - dataRemain);
		if (size >= dataRemain)
		{
			memcpy_s(buffer, size, data, dataRemain);
			dataRemain = 0;
			break;
		}
		else
		{
			memcpy_s(buffer, size, data, size);
			dataRemain -= size;
		}
	}

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