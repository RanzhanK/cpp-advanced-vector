#pragma once

#include <cassert>
#include <memory>
#include <algorithm>
#include <cstdlib>
#include <new>
#include <utility>
#include <iterator>

template<typename T>
class RawMemory {
public:
    RawMemory() noexcept = default;

    explicit RawMemory(size_t capacity) : buffer_(Allocate(capacity)), capacity_(capacity) {}

    RawMemory& operator=(const RawMemory &rhs) = delete;

    ~RawMemory() { Deallocate(buffer_); }

    RawMemory(const RawMemory &) = delete;

    RawMemory(RawMemory &&other) noexcept: buffer_(std::exchange(other.buffer_, nullptr)),
                                           capacity_(std::exchange(other.capacity_, 0)) {}

    RawMemory& operator=(RawMemory &&rhs) noexcept;

    T *operator+(size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T *operator+(size_t offset) const noexcept { return const_cast<RawMemory &>(*this) + offset; }

    const T &operator[](size_t index) const noexcept { return const_cast<RawMemory &>(*this)[index]; }

    T &operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory &other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T *GetAddress() const noexcept { return buffer_; }

    T *GetAddress() noexcept { return buffer_; }

    size_t Capacity() const { return capacity_; }

private:
    T *buffer_ = nullptr;
    size_t capacity_ = 0;

    static T *Allocate(size_t n) { return n != 0 ? static_cast<T *>(operator new(n * sizeof(T))) : nullptr; }

    static void Deallocate(T *buf) noexcept { operator delete(buf); }
};

template <typename T>
RawMemory<T>& RawMemory<T>::operator=(RawMemory &&rhs) noexcept {
    Init(std::move(rhs));
    return *this;
}
template<typename T>
class Vector {
public:

    using iterator = T *;
    using const_iterator = const T *;

    Vector() = default;

    explicit Vector(size_t size) : data_(size), size_(size) {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector &other) : data_(other.size_), size_(other.size_) {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector &&other) noexcept: data_(std::move(other.data_)), size_(std::exchange(other.size_, 0)) {}

    ~Vector() { std::destroy_n(data_.GetAddress(), size_); }

    iterator begin() noexcept { return data_.GetAddress(); }

    iterator end() noexcept { return size_ + data_.GetAddress(); }

    const_iterator cbegin() const noexcept { return data_.GetAddress(); }

    const_iterator cend() const noexcept { return size_ + data_.GetAddress(); }

    const_iterator begin() const noexcept { return cbegin(); }

    const_iterator end() const noexcept { return cend(); }

    size_t Size() const noexcept { return size_; }

    size_t Capacity() const noexcept { return data_.Capacity(); }

    void Swap(Vector &other) noexcept { data_.Swap(other.data_), std::swap(size_, other.size_); }

    void Reserve(size_t new_capacity) {

        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);

        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {

        if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);

        } else {

            if (new_size > data_.Capacity()) {
                Reserve(new_size);
                std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
            }
        }

        size_ = new_size;
    }

    template<typename... Args>
    T &EmplaceBack(Args &&... args);

    template<typename... Args>
    iterator Emplace(const_iterator pos, Args &&... args);

    iterator Insert(const_iterator pos, const T &item) { return Emplace(pos, item); }

    iterator Insert(const_iterator pos, T &&item) { return Emplace(pos, std::move(item)); }

    iterator Erase(const_iterator pos) {

        assert(size_ > 0);
        auto position = static_cast<size_t>(pos - begin());
        std::move(begin() + position + 1, end(), begin() + position);
        data_[size_ - 1].~T();
        --size_;
        return begin() + position;
    }

    T& PushBack(const T &value);
    T& PushBack(T &&value);

    void PopBack() {
        assert(size_);
        std::destroy_at(data_.GetAddress() + size_ - 1);
        --size_;
    }

    Vector &operator=(const Vector &other) {

        if (this != &other) {

            if (other.size_ <= data_.Capacity()) {

                if (size_ <= other.size_) {

                    std::copy(other.data_.GetAddress(),
                              other.data_.GetAddress() + size_,
                              data_.GetAddress());

                    std::uninitialized_copy_n(other.data_.GetAddress() + size_,
                                              other.size_ - size_,
                                              data_.GetAddress() + size_);
                } else {

                    std::copy(other.data_.GetAddress(),
                              other.data_.GetAddress() + other.size_,
                              data_.GetAddress());

                    std::destroy_n(data_.GetAddress() + other.size_,
                                   size_ - other.size_);
                }

                size_ = other.size_;

            } else {
                Vector other_copy(other);
                Swap(other_copy);
            }
        }

        return *this;
    }

    Vector &operator=(Vector &&other) noexcept {
        Swap(other);
        return *this;
    }

    const T &operator[](size_t index) const noexcept { return const_cast<Vector &>(*this)[index]; }

    T &operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
    static void SafeMove(T *from, size_t size, T *to);

    template <typename... Args>
    iterator EmplaceWithReallocate(const_iterator pos, Args&&... args);

    template <typename... Args>
    iterator EmplaceWithoutReallocate(const_iterator pos, Args&&... args);
};


template<typename T>
T& Vector<T>::PushBack(const T &value) {
    return EmplaceBack(value);
}

template<typename T>
T& Vector<T>::PushBack(T &&value) {
    return EmplaceBack(std::move(value));
}

template<typename T>
template<typename... Args>
T& Vector<T>::EmplaceBack(Args&&... args) {
    if (size_ == Capacity()) {
        RawMemory<T> new_data{size_ == 0 ? 1 : size_ * 2};
        new (new_data + size_) T(std::forward<Args>(args)...);
        SafeMove(data_.GetAddress(), size_, new_data.GetAddress());
        data_.Swap(new_data);
    } else {
        new (data_ + size_) T(std::forward<Args>(args)...);
    }
    ++size_;
    return data_[size_ - 1];
}

template<typename T>
template<typename... Args>
typename Vector<T>::iterator Vector<T>::Emplace(const_iterator pos, Args&&... args) {
    if (pos == end()) {
        return &EmplaceBack(std::forward<Args>(args)...);
    }
    if (size_ == Capacity()) {
        return EmplaceWithReallocate(pos, std::forward<Args>(args)...);
    } else {
        return EmplaceWithoutReallocate(pos, std::forward<Args>(args)...);
    }

}

template<typename T>
void Vector<T>::SafeMove(T *from, size_t size, T *to) {
    if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
        std::uninitialized_move_n(from, size, to);
    } else {
        std::uninitialized_copy_n(from, size, to);
    }
    std::destroy_n(from, size);
}

template<typename T>
template <typename... Args>
typename Vector<T>::iterator
Vector<T>::EmplaceWithReallocate(const_iterator pos, Args&&... args) {
    size_t index = static_cast<size_t>(pos - begin());
    RawMemory<T> new_data{size_ == 0 ? 1 : size_ * 2};
    new (new_data + index) T(std::forward<Args>(args)...);
    try {
        SafeMove(data_.GetAddress(), index, new_data.GetAddress());
    }  catch (...) {
        new_data[index].~T();
        throw;
    }

    try {
        SafeMove(data_+index, size_ - index, new_data + (index+1));
    }  catch (...) {
        std::destroy_n(new_data.GetAddress(), index + 1);
        throw;
    }
    data_.Swap(new_data);

    ++size_;
    return begin() + index;
}

template<typename T>
template <typename... Args>
typename Vector<T>::iterator
Vector<T>::EmplaceWithoutReallocate(const_iterator pos, Args&&... args) {
    size_t index = static_cast<size_t>(pos - begin());
    T temp(std::forward<Args>(args)...);
    new (end()) T(std::move(data_[size_ - 1]));
    std::move_backward(begin() + index, end() - 1, end());
    data_[index] = std::move(temp);

    ++size_;
    return begin() + index;
}