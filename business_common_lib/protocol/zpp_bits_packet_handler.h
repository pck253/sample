#pragma once

template<typename ST>
class ZppBitsPacketHandleCallerBase
{
public:
	ZppBitsPacketHandleCallerBase() {};
	virtual ~ZppBitsPacketHandleCallerBase() {};
	virtual bool CallHandler(ST& _session, zpp::bits::in<std::vector<uint8_t>>& _in) = 0;
};

template<typename ST, typename PB>
class ZppBitsPacketHandleCaller : public ZppBitsPacketHandleCallerBase<ST>
{
public:
	ZppBitsPacketHandleCaller(std::function<bool(const ST&)>&& _checker)
		: m_checker(std::move(_checker))
	{}
	virtual ~ZppBitsPacketHandleCaller() { m_checker = nullptr; }

	virtual bool CallHandler(ST& _session, zpp::bits::in<std::vector<uint8_t>>& _in) final
	{
		if (!m_checker(_session))
		{
			return false;
		}

		PB packet;
		auto result = _in(packet);

		return ::Handler(_session, std::move(packet));
	}

private:
	std::function<bool(const ST&)> m_checker;
};

namespace ZppBits
{
	struct SerializedInfo
	{
		const uint8_t* serializedBuffer{};
		PacketSize_t serializedSize{};
		PacketDeallocatorShared_t deallocator;

		SerializedInfo() {}
		SerializedInfo(const uint8_t* _serializedBuffer, const PacketSize_t& _serializedSize, const PacketDeallocatorShared_t& _deallocator)
			: serializedBuffer(_serializedBuffer), serializedSize(_serializedSize), deallocator(_deallocator) {}
		SerializedInfo(const SerializedInfo& _other)
		{
			serializedBuffer = _other.serializedBuffer;
			serializedSize = _other.serializedSize;
			deallocator = _other.deallocator;
		}
		SerializedInfo(SerializedInfo&& _other) noexcept
			: serializedBuffer(_other.serializedBuffer), serializedSize(_other.serializedSize), deallocator(std::move(_other.deallocator))
		{
			_other.serializedBuffer = {};
			_other.serializedSize = {};
		}

		SerializedInfo& operator=(const SerializedInfo& _other)
		{
			serializedBuffer = _other.serializedBuffer;
			serializedSize = _other.serializedSize;
			deallocator = _other.deallocator;
		}
		SerializedInfo& operator=(SerializedInfo&& _other) noexcept
		{
			serializedBuffer = _other.serializedBuffer;
			serializedSize = _other.serializedSize;
			deallocator = std::move(_other.deallocator);

			_other.serializedBuffer = {};
			_other.serializedSize = {};

			return *this;
		}
	};

	template<typename PB>
	static SerializedInfo Serialize(const PB& _packet)
	{
		static SerializedInfo EMPTY;

		our::vector<uint8_t> buffer;
		const auto result = zpp::bits::out(buffer)(_packet);

		if (zpp::bits::failure(result)) {
			LogError("failed packet serialization : {}", _packet.rawProtocol);
			return std::move(EMPTY);
		}

		if (buffer.size() > MAX_PACKET_SIZE - PACKET_SIZE_BYTE)
		{
			LogError("packet is too big : {}", _packet.rawProtocol);
			return std::move(EMPTY);
		}

		const auto* bufferPtr{ buffer.data() };
		const auto serializeSize{ static_cast<PacketSize_t>(buffer.size()) };
		auto deallocator = PacketDeallocator::Create([originBuffer = std::move(buffer)]() {});

		return SerializedInfo(bufferPtr, serializeSize, deallocator);
	}
	
	template<typename PB>
	static bool Serialize(const PB& _packet, std::vector<uint8_t>& _target)
	{
		const auto result = zpp::bits::out(_target)(_packet);

		if (zpp::bits::failure(result))
		{
			LogError("failed packet serialization : {}", _packet.rawProtocol);
			return false;
		}

		if (_target.size() > MAX_PACKET_SIZE - PACKET_SIZE_BYTE)
		{
			LogError("packet is too big : {}", _packet.rawProtocol);
			return false;
		}

		return true;
	}
};