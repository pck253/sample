#include "pch.h"

static_assert(NETWORK_MODULE == 1);

ReceiveBuffer::ReceiveBuffer()
	: m_buffer(MAX_PACKET_SIZE * 2, 0), m_freeSize(MAX_PACKET_SIZE * 2)
{
}

void ReceiveBuffer::Read(std::vector<uint8_t>& _output)
{
	auto readableSize = m_buffer.size() - m_freeSize;
	if (readableSize <= PACKET_SIZE_BYTE)
	{
		return;
	}

	// read && check raw data size
	std::array<uint8_t, PACKET_SIZE_BYTE> readRawDataSize;
	size_t tempReadableIndex = m_readableIndex;
	for (size_t i = 0; i < PACKET_SIZE_BYTE; ++i)
	{
		readRawDataSize[i] = m_buffer[tempReadableIndex];
		++tempReadableIndex;
		if (tempReadableIndex == m_buffer.size())
		{
			tempReadableIndex = 0;
		}
	}
	readableSize -= PACKET_SIZE_BYTE;

	PacketSize_t rawDataSize = *((PacketSize_t*)(readRawDataSize.data()));
	if (rawDataSize == 0 || readableSize < rawDataSize)
	{
		return;
	}
	// read && check raw data size

	// read raw data
	_output.resize(rawDataSize);
	if ((tempReadableIndex + rawDataSize) <= m_buffer.size())
	{
		memcpy_s(&_output[0], rawDataSize, &m_buffer[tempReadableIndex], rawDataSize);
		tempReadableIndex += rawDataSize;

		if (tempReadableIndex == m_buffer.size())
		{
			tempReadableIndex = 0;
		}
	}
	else
	{
		auto secondReadSize = (tempReadableIndex + rawDataSize) - m_buffer.size();
		auto firstReadSize = (rawDataSize - secondReadSize);
		memcpy_s(&_output[0], firstReadSize, &m_buffer[tempReadableIndex], firstReadSize);

		memcpy_s(&_output[firstReadSize], secondReadSize, &m_buffer[0], secondReadSize);
		tempReadableIndex = secondReadSize;
	}
	// read raw data

	m_readableIndex = tempReadableIndex;

	m_freeSize += (PACKET_SIZE_BYTE + rawDataSize);
}

ReservedReceiveBuffer_t ReceiveBuffer::GetWritableBuffer()
{
	if (m_freeSize == 0)
	{
		return ReservedReceiveBuffer_t();
	}

	ReservedReceiveBuffer_t buffers;
	// must be m_writableIndex < m_buffer.size(). because that m_writableIndex is updated in UpdateWritableBufferInfo
	if (m_readableIndex <= m_writableIndex)
	{
		buffers.emplace_back(std::make_pair(&(m_buffer[m_writableIndex]), m_buffer.size() - m_writableIndex));
		if (m_readableIndex != 0)
		{
			buffers.emplace_back(std::make_pair(&(m_buffer[0]), m_readableIndex));
		}
	}
	else
	{
		buffers.emplace_back(std::make_pair(&(m_buffer[m_writableIndex]), m_readableIndex - m_writableIndex));
	}

	return std::move(buffers);
}

void ReceiveBuffer::UpdateWritableBufferInfo(const size_t& _usedSize)
{
	if (_usedSize == 0)
	{
		return;
	}

	m_writableIndex += _usedSize;
	if (m_writableIndex >= m_buffer.size())
	{
		m_writableIndex -= m_buffer.size();
	}

	m_freeSize -= _usedSize;
}