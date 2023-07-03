#pragma once

#define BUFFER_INFO_INDEX 0
#define BUFFER_ADDR_INDEX 1
#define BUFFER_SIZE_INDEX 2
struct SendBufferInfo
{
	uint8_t* buffer = nullptr;
	size_t writableIndex = 0;
	size_t readableSize = 0;

	void Reset()
	{
		writableIndex = 0;
		readableSize = 0;
	}
	bool IsNotUse()
	{
		return (readableSize == 0);
	}
};
using ReservedSendData_t = our::list<std::tuple<SendBufferInfo*, uint8_t*, size_t>>;
class SendBuffer
{
public:
	SendBuffer();
	~SendBuffer();

	Result Write(const PacketSize_t& _size, const uint8_t* _serializedData, ReservedSendData_t& _output);
	void UpdateUsedBufferInfo(SendBufferInfo* _bufferInfo, const size_t& _usedSize);

private:

	SendBufferInfo* GetFreeBuffer();

private:
	SendBufferInfo* m_lastUsingBufferInfo = nullptr;
	our::unordered_set<SendBufferInfo*> m_usingBuffers;
	our::queue<SendBufferInfo*> m_freeBufferQueue;
	uint64_t m_allocatedSize = 0;
};

