#pragma once

////////////////////////////////////////////////
// struct vector example
// --------------------------------------------
// std::vector<Global::Vec3> path;
// for (auto& pos : usedPath)
// {
// 	path.emplace_back(Global::Vec3(pos.x, pos.y, pos.z));
// }
// auto pktPath = builder.CreateVectorOfStructs(path);
////////////////////////////////////////////////

template<typename ST, typename PBB>
class FbPacketHandleCallerBase
{
public:
	FbPacketHandleCallerBase() {};
	virtual ~FbPacketHandleCallerBase() {};
	virtual bool CallHandler(ST& _session, const PBB* _packetBodyBase, our::vector<uint8_t>&& _rawData) = 0;
};

template<typename ST, typename PBB, typename PB>
class FbPacketHandleCaller : public FbPacketHandleCallerBase<ST, PBB>
{
public:
	FbPacketHandleCaller(std::function<bool(ST&)>&& _checker)
		: m_checker(std::move(_checker))
	{}
	virtual ~FbPacketHandleCaller() { m_checker = nullptr; }

	virtual bool CallHandler(ST& _session, const PBB* _packetBodyBase, our::vector<uint8_t>&& _rawData) final
	{
		if (!m_checker(_session))
		{
			return false;
		}
		auto* body = static_cast<const PB*>(_packetBodyBase->body());
		return Handler(_session, body, std::move(_rawData));
	}

private:
	std::function<bool(ST&)> m_checker;
};

class OurFlatBufferAllocator : public flatbuffers::Allocator {
public:
	uint8_t* allocate(size_t _size) FLATBUFFERS_OVERRIDE {
		return g_memoryPool->allocate(_size);
	}

	void deallocate(uint8_t* p, size_t _size) FLATBUFFERS_OVERRIDE { g_memoryPool->deallocate(p, _size); }

	static void dealloc(void* p, size_t _size) { g_memoryPool->deallocate((uint8_t*)p, _size); }
};

static OurFlatBufferAllocator ourFlatBufferAllocator;

#define FLAT_BUFFER_BUILDER(name)	flatbuffers::FlatBufferBuilder name(1024, &ourFlatBufferAllocator, false, ALIGNMENT)

namespace Fb
{
	struct SerializedInfo
	{
		uint8_t* serializedBuffer = nullptr;
		flatbuffers::uoffset_t serializedSize = 0;
		PacketDeallocatorShared_t deallocator;

		SerializedInfo() {}
		SerializedInfo(uint8_t* _serializedBuffer, const flatbuffers::uoffset_t& _serializedSize, PacketDeallocatorShared_t& _deallocator)
			: serializedBuffer(_serializedBuffer), serializedSize(_serializedSize), deallocator(_deallocator) {}
		SerializedInfo(const SerializedInfo& _other) = delete;
		SerializedInfo(SerializedInfo&& _other)
			: serializedBuffer(_other.serializedBuffer), serializedSize(_other.serializedSize), deallocator(std::move(_other.deallocator))
		{
			_other.serializedBuffer = nullptr;
			_other.serializedSize = 0;
		}

		SerializedInfo& operator=(const SerializedInfo& _other) = delete;
		SerializedInfo& operator=(SerializedInfo&& _other)
		{
			serializedBuffer = _other.serializedBuffer;
			serializedSize = _other.serializedSize;
			deallocator = std::move(_other.deallocator);

			_other.serializedBuffer = nullptr;
			_other.serializedSize = 0;

			return *this;
		}
	};

	static SerializedInfo MakeSerializedInfo(flatbuffers::FlatBufferBuilder& _builder)
	{
		auto serializedBuffer = _builder.GetBufferPointer();
		auto serializedSize = _builder.GetSize();
		size_t allocatedSize = 0;
		size_t offset = 0;
		auto originBuffer = _builder.ReleaseRaw(allocatedSize, offset);

		assert(serializedBuffer == originBuffer + offset);
		assert(serializedSize == allocatedSize - offset);

		auto deallocator = PacketDeallocator::Create([originBuffer, allocatedSize]() mutable
			{
				//delete originBuffer;
				g_memoryPool ? g_memoryPool->deallocate(originBuffer, allocatedSize) : delete originBuffer;
			});

		return SerializedInfo(serializedBuffer, serializedSize, deallocator);
	}
};