#pragma once

namespace our
{
	// Using MemoryPoolStlAllocator may not be compatible with 3rd party lib.
	// Use only when absolutely necessary.

	template <class T>
	class MemoryPoolStlAllocator final {
	public:
		using value_type = T;

		constexpr MemoryPoolStlAllocator() noexcept {}

		constexpr MemoryPoolStlAllocator(const MemoryPoolStlAllocator&) noexcept = default;

		template <class _Other>
		constexpr MemoryPoolStlAllocator(const MemoryPoolStlAllocator<_Other>&) noexcept {}

		~MemoryPoolStlAllocator() = default;

		MemoryPoolStlAllocator& operator=(const MemoryPoolStlAllocator&) = default;

		T* allocate(const size_t _count)
		{
			size_t size = _count * sizeof(T);
			return (T*)g_memoryPool->allocate(size);
		}

		void deallocate(T* const _ptr, const size_t _count)
		{
			size_t size = _count * sizeof(T);
			g_memoryPool->deallocate((uint8_t* const)_ptr, size);
		}
	};

	template <class _Ty>
	using StlAllocator_t = MemoryPoolStlAllocator<_Ty>;

	template <class _Kty, class _Ty, class _Pr = std::less<_Kty>, class _Alloc = StlAllocator_t<std::pair<const _Kty, _Ty>>>
	using map = std::map<_Kty, _Ty, _Pr, _Alloc>;

	template <class _Kty, class _Ty, class _Pr = std::less<_Kty>, class _Alloc = StlAllocator_t<std::pair<const _Kty, _Ty>>>
	using multimap = std::multimap<_Kty, _Ty, _Pr, _Alloc>;

	template <class _Kty, class _Ty, class _Hasher = std::hash<_Kty>, class _Keyeq = std::equal_to<_Kty>, class _Alloc = StlAllocator_t<std::pair<const _Kty, _Ty>>>
	using unordered_map = std::unordered_map<_Kty, _Ty, _Hasher, _Keyeq, _Alloc>;

	template <class _Kty, class _Ty, class _Hasher = std::hash<_Kty>, class _Keyeq = std::equal_to<_Kty>, class _Alloc = StlAllocator_t<std::pair<const _Kty, _Ty>>>
	using unordered_multimap = std::unordered_multimap<_Kty, _Ty, _Hasher, _Keyeq, _Alloc>;

	template <class _Kty, class _Pr = std::less<_Kty>, class _Alloc = StlAllocator_t<_Kty>>
	using set = std::set<_Kty, _Pr, _Alloc>;

	template <class _Kty, class _Hasher = std::hash<_Kty>, class _Keyeq = std::equal_to<_Kty>, class _Alloc = StlAllocator_t<_Kty>>
	using unordered_set = std::unordered_set<_Kty, _Hasher, _Keyeq, _Alloc>;

	template <class _Ty, class _Alloc = StlAllocator_t<_Ty>>
	using vector = std::vector<_Ty, _Alloc>;

	template <class _Ty, class _Alloc = StlAllocator_t<_Ty>>
	using list = std::list<_Ty, _Alloc>;

	template <class _Ty, class _Alloc = StlAllocator_t<_Ty>>
	using deque = std::deque<_Ty, _Alloc>;

	template <class _Ty, class _Container = deque<_Ty>>
	using queue = std::queue<_Ty, _Container>;
}