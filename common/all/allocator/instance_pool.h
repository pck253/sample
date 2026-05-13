#pragma once
class MemPoolInstance
{
public:
    MemPoolInstance() = default;
    virtual ~MemPoolInstance() = default;

    // =========================
    // NEW
    // =========================

    // scalar new (C++98~)
    void* operator new(std::size_t _size)
    {
        return g_memoryPool->allocate(_size);
    }

    // aligned new (C++17)
    void* operator new(std::size_t _size, std::align_val_t _align)
    {
        if (static_cast<std::size_t>(_align) > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
        {
            return _aligned_malloc(_size, static_cast<std::size_t>(_align));
        }

        return g_memoryPool->allocate(_size);
    }

    // =========================
    // DELETE
    // =========================

    // scalar delete : not use because need size.
    //void operator delete(void* _ptr) noexcept
    //{
    //    g_memoryPool->deallocate(static_cast<uint8_t*>(_ptr), 0);
    //}

    // sized delete (C++14)
    void operator delete(void* _ptr, std::size_t _size) noexcept
    {
        g_memoryPool->deallocate(static_cast<uint8_t*>(_ptr), _size);
    }

    // aligned delete (C++17)
    void operator delete(void* _ptr, std::align_val_t _align) noexcept
    {
        if (static_cast<std::size_t>(_align) > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
        {
            _aligned_free(_ptr);
            return;
        }

        g_memoryPool->deallocate(static_cast<uint8_t*>(_ptr), 0);
    }

    // sized + aligned delete (C++17)
    void operator delete(void* _ptr, std::size_t _size, std::align_val_t _align) noexcept
    {
        if (static_cast<std::size_t>(_align) > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
        {
            _aligned_free(_ptr);
            return;
        }

        g_memoryPool->deallocate(static_cast<uint8_t*>(_ptr), _size);
    }
};