#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept
    {
        buffer_ = std::move(other.buffer_);
        capacity_ = std::move(other.capacity_);

        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }
    RawMemory& operator=(RawMemory&& rhs) noexcept
    {
        if (this != &rhs)
        {
            buffer_ = std::move(rhs.buffer_);
            capacity_ = std::move(rhs.capacity_);

            rhs.buffer_ = nullptr;
            rhs.capacity_ = 0;
        }
        return *this;
    }

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    Vector() = default;

    Vector(size_t size) : data_(size), size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size_);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    ~Vector()
    {
        std::destroy_n(data_.GetAddress(), size_);
    }

    Vector(Vector&& other) noexcept
    {
        Swap(other);
    }

    Vector& operator=(const Vector& rhs)
    {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector copy(rhs);
                Swap(copy);
            }
            else {
                if (rhs.size_ < size_)
                {
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + rhs.size_, data_.GetAddress());
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                }
                else
                {
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + size_, data_.GetAddress());
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
                }
                size_ = rhs.size_;
                /* Скопировать элементы из rhs, создав при необходимости новые
                   или удалив существующие */
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept
    {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

    void Resize(size_t new_size)
    {
        if (new_size < size_)
        {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
            size_ = new_size;
        }
        else if (new_size == size_)
        {
            return;
        }
        else
        {
            if (new_size <= data_.Capacity())
            {
                std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
                size_ = new_size;
            }
            else
            {
                Reserve(new_size);
                std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
                size_ = new_size;
            }
        }

    }

    template <typename Type>
    void PushBack(Type&& value)
    {

        this->EmplaceBack(std::forward<Type>(value));
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args)
    {
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data + size_) T(std::forward<Args>(args)...);

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
            {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else
            {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
            ++size_;
            return this->Back();
        }
        else
        {
            new (data_ + size_) T(std::forward<Args>(args)...);
            ++size_;
            return this->Back();
        }
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args)
    {
        assert(pos >= begin() && pos <= end());
        size_t shift = iterator(pos) - begin();

        if (size_ == shift) {
            EmplaceBack(std::forward<Args>(args)...);
            return std::prev(end());
        }
        else if (size_ == data_.Capacity())
        {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            auto new_elem_it = new (new_data + shift) T(std::forward<Args>(args)...);

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
            {
                try
                {
                    std::uninitialized_move_n(begin(), shift, new_data.GetAddress());
                }
                catch (...)
                {
                    std::destroy_at(new_elem_it);
                }
                try
                {
                    std::uninitialized_move_n(begin() + shift, size_ - shift, new_data.GetAddress() + shift + 1);
                }
                catch (...)
                {
                    std::destroy(begin(), begin() + shift + 1);
                }
            }
            else
            {
                try
                {
                    std::uninitialized_copy_n(begin(), shift, new_data.GetAddress());
                }
                catch (...)
                {
                    std::destroy_at(new_elem_it);
                }
                try
                {
                    std::uninitialized_copy_n(begin() + shift, size_ - shift, new_data.GetAddress() + shift + 1);
                }
                catch (...)
                {
                    std::destroy(begin(), begin() + shift + 1);
                }
            }

            std::destroy(begin(), end());
            data_.Swap(new_data);
            ++size_;
            return new_elem_it;
        }
        else
        {
            auto new_value = T(std::forward<Args>(args)...);
            new (data_ + size_) T(std::move(*(std::prev(end()))));
            try {
                std::move_backward(iterator(pos), std::prev(end()), end());
            }
            catch (...) {
                std::destroy_n(end(), 1);
                throw;
            }
            *(iterator(pos)) = std::move(new_value);
            ++size_;
            return iterator(pos);
        }
    }
    iterator Erase(const_iterator pos) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/
    {
        assert(pos >= begin() && pos < end());
        size_t shift = iterator(pos) - begin();
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
        {
            std::move(iterator(pos) + 1, end(), iterator(pos));
        }
        else
        {
            std::copy(iterator(pos) + 1, end(), iterator(pos));
        }
        std::destroy_at(std::prev(end()));
        --size_;
        return begin() + shift;
    }


    iterator Insert(const_iterator pos, const T& value)
    {
        return this->Emplace(pos, value);
    }
    iterator Insert(const_iterator pos, T&& value)
    {
        return this->Emplace(pos, std::move(value));
    }

    T& Back()
    {
        return data_[size_ - 1];
    }

    void PopBack()
    {
        assert(size_ > 0);
        std::destroy_at(data_.GetAddress() + size_ - 1);
        --size_;
    }

    void Reserve(size_t new_capacity)
    {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);

        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void Swap(Vector& other) noexcept
    {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    iterator begin() noexcept
    {
        return data_.GetAddress();
    }
    iterator end() noexcept
    {
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept
    {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept
    {
        return data_.GetAddress() + size_;
    }
    const_iterator cbegin() const noexcept
    {
        return begin();
    }
    const_iterator cend() const noexcept
    {
        return end();
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;

    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }
};