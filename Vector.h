#include <algorithm>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <utility>

template <typename T>
class Vector {
    T* data_;
    size_t size_;
    size_t capacity_;

    void relocate(T* new_data, size_t new_capacity) {
        static_assert(std::is_copy_constructible_v<T> ||
                      std::is_nothrow_move_constructible_v<T> ||
                      std::is_move_constructible_v<T>);
        if (std::is_nothrow_move_constructible_v<T>) {
            for (size_t i = 0; i < size_; ++i) {
                new(&new_data[i]) T(std::move(data_[i]));
            }
            if (capacity_ != size_) {
                operator delete(&data_[size_], (capacity_ - size_) * sizeof(T), std::align_val_t(alignof(T)));
            }
        } else if (std::is_copy_constructible_v<T>) {
            try {
                for (size_t i = 0; i < size_; ++i) {
                    new(&new_data[i]) T(data_[i]);
                }
            } catch (...) {
                operator delete(new_data, new_capacity * sizeof(T), std::align_val_t(alignof(T)));
                throw;
            }
            operator delete(data_, capacity_ * sizeof(T), std::align_val_t(alignof(T)));
        } else {
            try {
                for (size_t i = 0; i < size_; ++i) {
                    new(&new_data[i]) T(std::move(data_[i]));
                }
            } catch (...) {
                throw;
            }
            if (capacity_ != size_) {
                operator delete(&data_[size_], (capacity_ - size_) * sizeof(T), std::align_val_t(alignof(T)));
            }
        }
        data_ = new_data;
        capacity_ = new_capacity;
    }

//    void swap_T(T* &lhs, T* &rhs) {
//        T* tmp = std::move(lhs);
//        lhs = std::move(rhs);
//        rhs = std::move(tmp);
//    }

    size_t new_possible_capacity() {
        if (capacity_ < SIZE_MAX / 2) {
            return std::max(static_cast<size_t>(1), std::min(max_size(), 2 * capacity_));
        }
        return max_size();
    }

public:
    constexpr Vector() noexcept : data_(nullptr), size_(0), capacity_(0) {}

    Vector(size_t count, const T& value) : data_(nullptr), size_(0), capacity_(0) {
        if (count > 0) {
            if (count > max_size())
                throw std::length_error("maximum length has been exceeded");

            static_assert(std::is_copy_constructible_v<T>);

            T* new_data;
            try {
                new_data = reinterpret_cast<T*>(operator new(sizeof(T) * count, std::align_val_t(alignof(T))));
            } catch (...){
                throw;
            };

            try {
                for (size_t i = 0; i < count; ++i) {
                    new (&new_data[i]) T(value);
                }
            } catch(...) {
                operator delete(&new_data, sizeof(T) * count, std::align_val_t(alignof(T)));
                throw;
            }

            data_ = new_data;
            capacity_ = count;
            size_ = count;
        }
    };

    constexpr explicit Vector(size_t n) : data_(nullptr), size_(0), capacity_(0) {
        if (n > 0) {
            if (n > max_size())
                throw std::length_error("maximum length has been exceeded");

            static_assert(std::is_default_constructible_v<T>);

            T* new_data;
            try {
                new_data = reinterpret_cast<T*>(operator new(sizeof(T) * n, std::align_val_t(alignof(T))));
            } catch (...) {
                throw;
            }

            try{
                for (size_t i = 0; i < n; ++i) {
                    new (&new_data[i]) T();
                }
            } catch(...) {
                operator delete(&new_data, sizeof(T) * n, std::align_val_t(alignof(T)));
                throw;
            }

            data_ = new_data;
            size_ = n;
            capacity_ = n;
        }
    };

    template<class InputIt>
    constexpr Vector(InputIt first, InputIt last) : data_(nullptr), size_(0), capacity_(0) {
        size_t new_size = static_cast<size_t>(last - first);
        if (new_size > max_size())
            throw std::length_error("maximum length has been exceeded");

        static_assert(std::is_copy_constructible_v<T>);

        T* new_data;
        try {
            new_data = reinterpret_cast<T*>(operator new(sizeof(T) * new_size, std::align_val_t(alignof(T))));
        } catch (...){
            throw;
        }

        try {
            for (size_t i = 0; first != last; ++i, ++first) {
                new (&new_data[i]) T(*first);
            }
        } catch (...) {
            operator delete(&new_data, sizeof(T) * new_size, std::align_val_t(alignof(T)));
            throw;
        }

        data_ = new_data;
        size_ = new_size;
        capacity_ = size_;
    }

    constexpr Vector(const Vector& other) : data_(nullptr), size_(0), capacity_(0) {
        if (other.size_ > 0) {
            if (other.size_ > max_size())
                throw std::length_error("maximum length has been exceeded");

            static_assert(std::is_copy_constructible_v<T>);

            T* new_data;
            try {
                new_data = reinterpret_cast<T*>(operator new(sizeof(T) * other.size_, std::align_val_t(alignof(T))));
            } catch (...){
                throw;
            }

            try {
                for (size_t i = 0; i < other.size_; ++i) {
                    new (&new_data[i]) T(other[i]);
                }
            } catch (...) {
                operator delete(&new_data, sizeof(T) * other.size_, std::align_val_t(alignof(T)));
                throw;
            }

            data_ = new_data;
            capacity_ = other.size_;
            size_ = other.size_;
        }
    }

    constexpr Vector(Vector&& other) noexcept : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    constexpr Vector(std::initializer_list<T> init) : data_(nullptr), size_(0), capacity_(0) {
        size_t new_size = static_cast<size_t>(init.size());
        if (new_size > max_size())
            throw std::length_error("maximum length has been exceeded");

        static_assert(std::is_nothrow_move_constructible_v<T> ||
                      std::is_copy_constructible_v<T> ||
                      std::is_move_constructible_v<T>);

        T* new_data;
        try {
            new_data = reinterpret_cast<T*>(operator new(sizeof(T) * new_size, std::align_val_t(alignof(T))));
        } catch (...){
            throw;
        }

        size_t i = 0;
        try {
            if (std::is_nothrow_move_constructible_v<T>) {
                for (const auto& current : init) {
                    new (&new_data[i++]) T(std::move(current));
                }
            } else if (std::is_copy_constructible_v<T>) {
                for (const auto& current : init) {
                    new (&new_data[i++]) T(current);
                }
            } else {
                for (const auto& current : init) {
                    new (&new_data[i++]) T(std::move(current));
                }
            }
        } catch(...) {
            operator delete(&new_data, sizeof(T) * new_size, std::align_val_t(alignof(T)));
            throw;
        }
        data_ = new_data;
        size_ = new_size;
        capacity_ = size_;
    }

    ~Vector() {
        operator delete(data_, capacity_ * sizeof(T), std::align_val_t(alignof(T)));
        data_ = nullptr;
        size_ = 0;
        capacity_ = 0;
    }

    constexpr Vector& operator=(const Vector& other)  {
        static_assert(std::is_copy_constructible_v<T>);

        T* new_data;
        try {
            new_data = reinterpret_cast<T*>(operator new(sizeof(T) * other.capacity_, std::align_val_t(alignof(T))));
        } catch (...){
            throw;
        }
        try {
            for (size_t i = 0; i < other.size_; ++i) {
                new (&new_data[i]) T(other[i]);
            }
        } catch (...){
            operator delete(&new_data, sizeof(T) * other.capacity_, std::align_val_t(alignof(T)));
            throw;
        }

        data_ = new_data;
        size_ = other.size_;
        capacity_ = other.capacity_;

        return *this;
    }

    Vector& operator=(Vector&& other) noexcept {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
        return *this;
    }

    constexpr Vector& operator=(std::initializer_list<T> ilist) {
        size_t new_size = static_cast<size_t>(ilist.size_);
        if (new_size > max_size()) {
            throw std::length_error("maximum length has been exceeded");
        }

        sstatic_assert(std::is_nothrow_move_constructible_v<T> ||
                       std::is_copy_constructible_v<T> ||
                       std::is_move_constructible_v<T>);

        T* new_data;
        try {
            new_data = reinterpret_cast<T*>(operator new(sizeof(T) * new_size, std::align_val_t(alignof(T))));
        } catch (...){
            throw;
        }

        size_t i = 0;
        try {
            if (std::is_nothrow_move_constructible_v<T>) {
                for (const auto& current : ilist) {
                    new (&new_data[i++]) T(std::move(current));
                }
            } else if (std::is_copy_constructible_v<T>) {
                for (const auto& current : ilist) {
                    new (&new_data[i++]) T(current);
                }
            } else {
                for (const auto& current : ilist) {
                    new (&new_data[i++]) T(std::move(current));
                }
            }
        } catch(...) {
            operator delete(&new_data, sizeof(T) * new_size, std::align_val_t(alignof(T)));
            throw;
        }

        data_ = new_data;
        size_ = new_size;
        capacity_ = new_size;

        return *this;
    }

    // access

    constexpr T& operator[](size_t position) {
        return data_[position];
    }
    constexpr const T& operator[](size_t position) const {
        return data_[position];
    }

    constexpr void assign(size_t count, const T& value) {
        Vector<T> new_vector;
        try {
            new_vector = Vector<T>(count, value);
        } catch (...) {
            throw;
        }
        *this = new_vector;
    }
    template<class InputIt>
    constexpr void assign(InputIt first, InputIt last) {
        Vector<T> new_vector;
        try {
            new_vector = Vector<T>(first, last);
        } catch (...) {
            throw;
        }
        *this = new_vector;
    }
    constexpr void assign(std::initializer_list<T> ilist) {
        Vector<T> new_vector;
        try {
            new_vector = Vector<T>(ilist);
        } catch (...) {
            throw;
        }
        *this = new_vector;
    }

    constexpr T& at(size_t position) {
        if (position >= size_) {
            throw std::out_of_range("");
        }
        return data_[position];
    }
    constexpr const T& at(size_t position) const {
        if (position >= size_) {
            throw std::out_of_range("");
        }
        return data_[position];
    }

    constexpr T& front() {
        return data_[0];
    }
    constexpr const T& front() const {
        return data_[0];
    }

    constexpr T& back() {
        return data_[size_ - 1];
    }
    constexpr const T& back() const {
        return data_[size_ - 1];
    }

    constexpr T* data() {
        return data_;
    }
    constexpr const T* data() const {
        return data_;
    }

    // Iterators

    class Iterator {
    private:
        T* ptr_;
    public:
        Iterator(T* ptr) : ptr_(ptr) {}

        T& operator*() {
            return *ptr_;
        }
        T* operator->() {
            return ptr_;
        }

        Iterator& operator++() {
            ++ptr_;
            return *this;
        }
        Iterator& operator--() {
            --ptr_;
            return *this;
        }
        Iterator operator++(int) {
            Iterator tmp{ptr_};
            ++ptr_;
            return tmp;
        }
        Iterator operator--(int) {
            Iterator tmp{ptr_};
            --ptr_;
            return tmp;
        }

        bool operator==(const Iterator& other) const {
            return ptr_ == other.ptr_;
        }
        bool operator!=(const Iterator& other) const {
            return ptr_ != other.ptr_;
        }

        bool operator<(const Iterator& other) {
            return other - *this > 0;
        }
        bool operator>(const Iterator& other) {
            return !(*this < other || *this == other);
        }
        bool operator<=(const Iterator& other) {
            return !(*this > other);
        }
        bool operator>=(const Iterator& other) {
            return !(*this < other);
        }

        Iterator& operator+=(long long n) {
            ptr_ += n;
            return *this;
        }
        Iterator& operator-=(long long n) {
            std::ptrdiff_t m = -n;
            return this += m;
        }
        Iterator operator+(long long n) const {
            Iterator tmp{ptr_};
            return tmp += n;
        }
        Iterator operator-(long long n) const {
            Iterator tmp{ptr_};
            return tmp -= n;
        }

        std::ptrdiff_t operator-(const Iterator& other) const {
            return ptr_ - other.ptr_;
        }

        T* operator[](std::ptrdiff_t n) {
            return ptr_ + n;
        }
    };

    Iterator begin() {
        return Iterator(data_);
    }
    Iterator end() {
        return Iterator(data_ + size_);
    }

    const Iterator begin() const noexcept {
        return Iterator(data_);
    }
    const Iterator end() const noexcept {
        return Iterator(data_ + size_);
    }

    // capacity

    [[nodiscard]] constexpr bool empty() const noexcept {
        return size_ == 0;
    }
    [[nodiscard]] constexpr size_t size() const noexcept {
        return size_;
    }
    [[nodiscard]] constexpr size_t max_size() const noexcept {
        return std::numeric_limits<size_t>::max() / sizeof(T);
    }
    constexpr void reserve(size_t new_cap) {
        if (new_cap <= capacity_) {
            return;
        }
        if (new_cap > max_size()) {
            throw std::length_error("maximum length has been exceeded");
        }
        T* new_data;
        try {
            new_data = reinterpret_cast<T*>(operator new(sizeof(T) * new_cap, std::align_val_t(alignof(T))));
        } catch (...) {
            throw;
        }

        relocate(new_data, new_cap);
    }
    [[nodiscard]] constexpr size_t capacity() const noexcept {
        return capacity_;
    }
    constexpr void shrink_to_fit() {
        if (capacity_ > size_) {
            operator delete(&data_[size_], (capacity_ - size_) * sizeof(T), std::align_val_t(alignof(T)));
        }
    }

    // modifiers

    constexpr void clear() noexcept {
        operator delete(&data_, size_ * sizeof(T), std::align_val_t(alignof(T)));
    }

    constexpr Iterator insert(const Iterator pos, const T& value) {
        static_assert(std::is_copy_assignable_v<T> && std::is_copy_constructible_v<T>);
        size_t index = pos - begin();

        if (size_ == capacity_) {
            size_t new_capacity = new_possible_capacity();

            T* new_data;
            try {
                new_data = reinterpret_cast<T*>(operator new(sizeof(T) * new_capacity, std::align_val_t(alignof(T))));
            } catch (...) {
                throw;
            }

            relocate(new_data, new_capacity);
        }

        if (index == size_) {
            push_back(value);
        } else {
            new (&data_[size_]) T(data_[size_ - 1]);
            for (size_t i = size_ - 1; i > index; --i) {
                data_[i] = data_[i - 1];
            }
            data_[index] = value;
        }
        ++size_;
        return begin() + index;
    }

    constexpr Iterator insert(const Iterator pos, T&& value) {
        static_assert(std::is_move_assignable_v<T> && std::is_move_constructible_v<T>);

        size_t index = pos - begin();

        if (size_ == capacity_) {
            size_t new_capacity = new_possible_capacity();

            T* new_data;
            try {
                new_data = reinterpret_cast<T*>(operator new(sizeof(T) * new_capacity, std::align_val_t(alignof(T))));
            } catch (...) {
                throw;
            }

            relocate(new_data, new_capacity);
        }

        if (index == size_) {
            push_back(std::move(value));
        } else {
            for (size_t i = size_; i > index; --i) {
                new (&data_[i]) T(std::move(data_[i - 1]));
            }

            new (&data_[index]) T(std::move(value));
        }
        ++size_;
        return begin() + index;
    }

    constexpr Iterator insert(const Iterator pos, size_t count, const T& value) {
        if (count == 0) {
             return pos;
        }

        Iterator res = pos;
        for (size_t i = 0; i < pos; ++i) {
            res = insert(res, value);
        }

        return res;
    }

    template< class... Args >
    constexpr Iterator emplace(const Iterator pos, Args&&... args) {
        size_t index = pos - begin();
        if (index == size_) {
            Iterator res = &(emplace_back(std::forward<Args>(args)...));
            return res;
        } else {
            T value(std::forward<Args>(args)...);
            return insert(pos, std::move(value));
        }

    }

    constexpr Iterator erase(const Iterator pos) {
        static_assert(std::is_move_assignable_v<T>);

        size_t index = pos - begin();
        if (index == size_ - 1) {
            pop_back();
            return end();
        }

        try {
            for (size_t i = index; i < size_ - 1; ++i) {
                data_[i] = std::move(data_[i + 1]);
            }
        } catch(...) {
            throw;
        }

        --size_;
        return begin() + index;
    }

    constexpr void push_back(const T& value) {
        static_assert(std::is_copy_constructible_v<T>);
        if (size_ == capacity_) {
            size_t new_capacity = new_possible_capacity();

            T* new_data;
            try {
                new_data = reinterpret_cast<T*>(operator new(sizeof(T) * new_capacity, std::align_val_t(alignof(T))));
            } catch (...) {
                throw;
            }

            relocate(new_data, new_capacity);
        }

        try {
            new (&data_[size_]) T(value);
        } catch (...) {
            delete (&data_[size_]);
            throw;
        }
        ++size_;
    }
    constexpr void push_back(T&& value) {
        static_assert(std::is_move_constructible_v<T>);
        if (size_ == capacity_) {
            T* new_data;
            size_t new_capacity = new_possible_capacity();
            try {
                new_data = reinterpret_cast<T*>(operator new(sizeof(T) * new_capacity, std::align_val_t(alignof(T))));
            } catch (...) {
                throw;
            }

            relocate(new_data, new_capacity);
        }

        try {
            new (&data_[size_]) T(std::move(value));
        } catch (...) {
            delete (&data_[size_]);
            throw;
        }
        ++size_;
    }

    template<class... Args>
    constexpr T& emplace_back(Args&&... args) {
        if (size_ == capacity_) {
            T* new_data;
            size_t new_capacity = new_possible_capacity();
            try {
                new_data = reinterpret_cast<T*>(operator new(sizeof(T) * new_capacity, std::align_val_t(alignof(T))));
            } catch (...) {
                throw;
            }

            relocate(new_data, new_capacity);
        }

        try {
            new (&data_[size_]) T(std::forward<Args>(args)...);
        } catch (...) {
            delete (&data_[size_]);
            throw;
        }
        return data_[size_++];
    }

    constexpr void pop_back() {
        data_[--size_].~T();
    }

    constexpr void resize(size_t count) {
        if (size_ > count) {
            for(size_t i = count; i < size_; ++i) {
                data_[i].~T();
            }
        } else if (count <= capacity_) {
            static_assert(std::is_default_constructible_v<T>);
            try {
                for (size_t i = size_; i < count; ++i) {
                    new (&data_[i]) T();
                }
            } catch (...) {
                throw;
            }

        } else if (capacity_ < count){
            static_assert(std::is_default_constructible_v<T>);

            Vector new_vector;
            try {
                new_vector = Vector(count);
            } catch(...) {
                throw;
            }
            relocate(new_vector.data(), count);
        }
        size_ = count;
    }
    constexpr void resize(size_t count, const T& value) {
        if (count < size_) {
            for(size_t i = count; i < size_; ++i) {
                data_[i].~T();
            }
            size_ = count;
        } else if (count <= capacity_) {
            static_assert(std::is_copy_constructible_v<T>);
            try {
                for (size_t i = size_; i < count; ++i) {
                    new (&data_[i]) T(value);
                }
            } catch (...) {
                throw;
            }
        } else if (capacity_ < count){
            static_assert(std::is_copy_constructible_v<T>);

            T* new_data;
            try {
                new_data = reinterpret_cast<T*>(operator new(sizeof(T) * count, std::align_val_t(alignof(T))));
            } catch(...) {
                throw;
            }

            try {
                for (size_t i = 0; i < size_; ++i) {
                    new (&new_data[i]) T(data_[i]);
                }
                for (size_t i = size_; i < count; ++i) {
                    new (&new_data[i]) T(value);
                }
            } catch (...){
                operator delete(&new_data, sizeof(T) * count, std::align_val_t(alignof(T)));
                throw;
            }

            data_ = new_data;
            capacity_ = count;
        }
        size_ = count;
    }

    constexpr void swap(Vector& other) noexcept {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
    }
};

// non-member function

template<class T>
constexpr bool operator==(const Vector<T>& lhs, const Vector<T>& rhs) {
    static_assert(std::equality_comparable<T>);

    if (lhs.size() != rhs.size())
        return false;

    for (size_t i = 0; i < lhs.size(); ++i) {
        if (lhs[i] != rhs[i])
            return false;
    }

    return true;
}

// template<class T>
// constexpr std::strong_ordering operator<=>(Vector<T> lhs, Vector<T>& rhs) {
//     static_assert(std::equality_comparable<T>);
//     return std::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
// }