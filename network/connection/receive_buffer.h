#pragma once

static_assert(NETWORK_MODULE == 1);

using ReservedReceiveBuffer_t = std::list<std::pair<uint8_t*, size_t>>;
class ReceiveBuffer
{
public:
	ReceiveBuffer();
	~ReceiveBuffer() = default;

	void Read(std::vector<uint8_t>& _output);
	ReservedReceiveBuffer_t GetWritableBuffer();
	void UpdateWritableBufferInfo(const size_t& _usedSize);

private:
	std::vector<uint8_t> m_buffer;	// use ring buffer
	size_t m_writableIndex = 0;
	size_t m_readableIndex = 0;
	size_t m_freeSize = 0;
};

