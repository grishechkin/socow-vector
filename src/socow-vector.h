#pragma once

#include "element.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <memory>
#include <utility>

template <typename T, std::size_t SMALL_SIZE>
class socow_vector {
private:
  struct dynamic_buffer {
    std::size_t _refs_count;
    std::size_t _capacity;
    T _data[0];

    dynamic_buffer() : _refs_count(0), _capacity(0) {}

    static dynamic_buffer* create_ptr(std::size_t capacity) {
      void* _ptr = operator new(sizeof(dynamic_buffer) + sizeof(T) * capacity);
      auto* new_ptr = new (_ptr) dynamic_buffer();
      new_ptr->_capacity = capacity;
      new_ptr->_refs_count = 1;
      return new_ptr;
    }
  };

public:
  using value_type = T;

  using reference = T&;
  using const_reference = const T&;

  using pointer = T*;
  using const_pointer = const T*;

  using iterator = pointer;
  using const_iterator = const_pointer;

private:
  std::size_t _size;
  bool _is_small;

  union {
    std::array<T, SMALL_SIZE> _small_array;
    dynamic_buffer* _ptr;
  };

public:
  socow_vector() : _size(0), _is_small(true) {}

  socow_vector(const T* data, std::size_t size, std::size_t capacity) : _size(size), _is_small(false) {
    _ptr = dynamic_buffer::create_ptr(capacity);
    try {
      std::uninitialized_copy_n(data, size, _ptr->_data);
    } catch (...) {
      operator delete(_ptr);
      throw;
    }
  }

  socow_vector(const socow_vector<T, SMALL_SIZE>& other) {
    if (&other == this) {
      return;
    }
    _size = 0;
    _is_small = true;
    *this = other;
  }

  socow_vector& operator=(const socow_vector<T, SMALL_SIZE>& other) {
    if (&other == this) {
      return *this;
    }
    if (is_small() && other.is_small() && other.size() >= size()) {
      std::uninitialized_copy(other.begin() + size(), other.end(), end());

      try {
        socow_vector arr;
        std::uninitialized_copy_n(other.begin(), size(), arr.begin());
        arr._size = size();
        for (std::size_t i = 0; i < size(); i++) {
          std::swap(operator[](i), arr[i]);
        }
      } catch (...) {
        delete_vector(data(), size(), other.size());
        throw;
      }
    } else if (is_small() && other.is_small()) {
      socow_vector tmp = other;
      tmp.swap(*this);
    } else if (!is_small() && other.is_small()) {
      auto this_ptr = _ptr;
      copy_to_small(other.data(), other.size());
      release_ptr(this_ptr, size());
    } else {
      this->~socow_vector();
      _ptr = other._ptr;
      add_ptr();
    }
    _size = other.size();
    _is_small = other.is_small();
    return *this;
  }

  void swap_arrays(socow_vector& other) {
    std::uninitialized_copy(other.begin() + size(), other.end(), end());

    for (std::size_t i = 0; i < size(); i++) {
      try {
        std::swap(operator[](i), other[i]);
      } catch (...) {
        delete_vector(data(), size(), other.size());
        throw;
      }
    }
    for (std::size_t i = size(); i < other.size(); i++) {
      other[i].~T();
    }
  }

  void swap(socow_vector& other) {
    if (&other == this) {
      return;
    }

    if (is_small() && other.is_small()) {
      if (size() < other.size()) {
        swap_arrays(other);
      } else {
        other.swap_arrays(*this);
      }
    } else if (!is_small() && !other.is_small()) {
      std::swap(_ptr, other._ptr);
    } else if (is_small() && !other.is_small()) {
      dynamic_buffer* other_ptr = other._ptr;
      other.copy_to_small(_small_array.data(), size());
      other._is_small = false;
      delete_vector();
      _ptr = other_ptr;
    } else {
      other.swap(*this);
      std::swap(_size, other._size);
      std::swap(_is_small, other._is_small);
    }

    std::swap(_size, other._size);
    std::swap(_is_small, other._is_small);
  }

  ~socow_vector() noexcept {
    if (!is_small()) {
      release_ptr();
    } else {
      delete_vector();
    }
  }

  void delete_vector(T* data, std::size_t first, std::size_t last) noexcept {
    for (std::size_t i = first; i < last; i++) {
      data[i].~T();
    }
  }

  void delete_vector(T* data, std::size_t size) noexcept {
    delete_vector(data, 0, size);
  }

  void delete_vector() noexcept {
    delete_vector(data(), size());
  }

private:
  void set_capacity(std::size_t capacity_) {
    *this = socow_vector(std::as_const(*this).data(), size(), capacity_);
  }

  socow_vector(const socow_vector& other, std::size_t capacity_) : _size(other.size()), _is_small(false) {
    auto new_ptr = dynamic_buffer::create_ptr(capacity_);
    std::uninitialized_copy_n(other.data(), size(), new_ptr->_data);
    _ptr = new_ptr;
  }

  void release_ptr(dynamic_buffer* _ptr, std::size_t size) {
    assert(_ptr->_refs_count > 0);
    _ptr->_refs_count--;
    if (_ptr->_refs_count == 0) {
      delete_vector(_ptr->_data, size);
      operator delete(_ptr);
    }
  }

  void release_ptr() {
    release_ptr(_ptr, size());
  }

  void add_ptr() {
    _ptr->_refs_count++;
  }

  void pop_back(std::size_t k) {
    for (std::size_t i = 0; i < k; ++i) {
      pop_back();
    }
  }

  void unshare() {
    if (is_linked()) {
      set_capacity(capacity());
    }
  }

  void copy_to_small(const T* src, std::size_t size) {
    assert(size <= SMALL_SIZE);

    if (is_small()) {
      return;
    }

    dynamic_buffer* cur_ptr = _ptr;
    try {
      std::uninitialized_copy_n(src, size, _small_array.data());
    } catch (...) {
      _ptr = cur_ptr;
      throw;
    }
    _is_small = true;
  }

  void to_small() {
    auto ptr = _ptr;
    copy_to_small(_ptr->_data, size());
    release_ptr(ptr, size());
  }

public:
  T& operator[](std::size_t ind) noexcept {
    return data()[ind];
  }

  const T& operator[](std::size_t ind) const noexcept {
    return data()[ind];
  }

  bool is_linked() const noexcept {
    return !is_small() && refs_count() > 1;
  }

  std::size_t size() const noexcept {
    return _size;
  }

  std::size_t refs_count() const {
    assert(!is_small());
    return _ptr->_refs_count;
  }

  std::size_t capacity() const noexcept {
    if (is_small()) {
      return SMALL_SIZE;
    }
    return _ptr->_capacity;
  }

  bool is_small() const noexcept {
    return _is_small;
  }

  const T* data() const noexcept {
    if (is_small()) {
      return _small_array.data();
    }
    return _ptr->_data;
  }

  T* data() {
    if (is_small()) {
      return _small_array.data();
    }
    unshare();
    return _ptr->_data;
  }

  void pop_back() {
    if (is_linked()) {
      erase(std::as_const(*this).end() - 1);
    } else {
      data()[--_size].~T();
    }
  }

  void push_back(const T& val) {
    if (size() == capacity() || is_linked()) {
      insert(std::as_const(*this).end(), val);
    } else {
      new (data() + size()) T(val);
      ++_size;
    }
  }

  void clear() noexcept {
    if (is_small() || refs_count() == 1) {
      delete_vector();
    } else {
      release_ptr();
      _is_small = true;
    }
    _size = 0;
  }

  void reserve(std::size_t n) {
    if (n < size() || (is_small() && n <= SMALL_SIZE)) {
      return;
    }

    if (!is_small() && n <= SMALL_SIZE) {
      to_small();
    } else if (n > capacity() || refs_count() > 1) {
      set_capacity(n);
    }
  }

  void shrink_to_fit() {
    if (is_small()) {
      return;
    }
    if (size() <= SMALL_SIZE) {
      to_small();
    } else if (refs_count() > 1 || size() != capacity()) {
      set_capacity(size());
    }
  }

  bool empty() const noexcept {
    return size() == 0;
  }

public:
  reference front() {
    return data()[0];
  }

  const_reference front() const noexcept {
    return data()[0];
  }

  reference back() {
    return data()[size() - 1];
  }

  const_reference back() const noexcept {
    return data()[size() - 1];
  }

  iterator begin() {
    return data();
  }

  const_iterator begin() const noexcept {
    return data();
  }

  iterator end() {
    return data() + size();
  }

  const_iterator end() const noexcept {
    return data() + size();
  }

  iterator insert(const_iterator pos, const T& value) {
    std::size_t position = pos - std::as_const(*this).begin();
    if (size() == capacity() || is_linked()) {
      auto src = std::as_const(*this).data();
      socow_vector new_vec(src, position, (size() == capacity()) ? capacity() * 2 : capacity());
      new_vec.push_back(value);
      for (std::size_t i = position; i < size(); i++) {
        new_vec.push_back(std::as_const(*this)[i]);
      }
      if (is_small()) {
        clear();
      }
      swap(new_vec);
      return begin() + position;
    }

    push_back(value);
    iterator it = begin() + position;
    for (iterator i = end() - 1; i != it; --i) {
      std::iter_swap(i, i - 1);
    }
    return it;
  }

  iterator erase(const_iterator first, const_iterator last) {
    std::size_t first_pos = first - std::as_const(*this).begin();
    std::size_t last_pos = last - std::as_const(*this).begin();
    if (first == last) {
      return begin() + first_pos;
    }
    if (is_linked()) {
      socow_vector new_vec(std::as_const(*this).data(), first_pos, capacity());
      std::uninitialized_copy_n(std::as_const(*this).data() + last_pos, size() - last_pos, new_vec.data() + first_pos);
      new_vec._size += size() - last_pos;
      swap(new_vec);
      return begin() + first_pos;
    }

    auto it_l = begin() + first_pos;
    auto it_r = begin() + last_pos;
    while (it_r != end()) {
      std::iter_swap(it_r++, it_l++);
    }
    pop_back(last_pos - first_pos);
    return begin() + first_pos;
  }

  iterator erase(const_iterator pos) {
    return erase(pos, pos + 1);
  }
};
