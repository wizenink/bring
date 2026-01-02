#pragma once
#include <array>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>

namespace bring {

template <typename T>
concept RingElement = std::move_constructible<T> && std::destructible<T>;
template <RingElement T, size_t Capacity> class RingBuffer {
  static_assert((Capacity & (Capacity - 1)) == 0,
                "Capacity must be power of two");
  static_assert(Capacity > 1, "Capacity must be greater than 1");

private:
  struct Slot {
    alignas(T) std::array<std::byte, sizeof(T)> _data;
  };

  // Prevent false sharing. head and tail should be on different cache lines. 64
  // is a safe option for all modern architectures
  static constexpr size_t align_size{64};
  alignas(align_size) std::atomic<size_t> _head{0};
  alignas(align_size) std::atomic<size_t> _tail{0};

  // NOLINTNEXTLINE(modernize-avoid-c-arrays,cppcoreguidelines-avoid-c-arrays)
  std::unique_ptr<Slot[]> _storage;

  T *get_ptr(size_t idx) noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<T *>(&_storage[idx]._data);
  }

public:
  // NOLINTNEXTLINE(modernize-avoid-c-arrays,cppcoreguidelines-avoid-c-arrays)
  RingBuffer() : _storage(std::make_unique<Slot[]>(Capacity)) {};

  ~RingBuffer() {
    while (try_consume([]([[maybe_unused]] T && /* discard */) {})) {
    }
  }
  RingBuffer(const RingBuffer &) = delete;
  RingBuffer &operator=(const RingBuffer &) = delete;

  RingBuffer(RingBuffer &&other) noexcept
      : _head(other._head.load(std::memory_order_relaxed)),
        _tail(other._tail.load(std::memory_order_relaxed)),
        _storage(std::move(other._storage)) {
    other._head.store(0, std::memory_order_relaxed);
    other._tail.store(0, std::memory_order_relaxed);
  }

  RingBuffer &operator=(RingBuffer &&other) noexcept {
    if (this != &other) {
      // Clean up existing elements
      while (try_pop()) { /* Destroy all */
      }

      _head.store(other._head.load(std::memory_order_relaxed),
                  std::memory_order_relaxed);
      _tail.store(other._tail.load(std::memory_order_relaxed),
                  std::memory_order_relaxed);
      _storage = std::move(other._storage);

      other._head.store(0, std::memory_order_relaxed);
      other._tail.store(0, std::memory_order_relaxed);
    }
    return *this;
  }

  [[nodiscard]] bool is_full() noexcept {
    const size_t current_head = _head.load(std::memory_order_relaxed);
    const size_t next_head = (current_head + 1) & (Capacity - 1);
    // If next_head hits tail, buffer is full
    return next_head == _tail.load(std::memory_order_acquire);
  }

  [[nodiscard]] bool is_empty() noexcept {
    const size_t current_tail = _tail.load(std::memory_order_relaxed);
    // If tail equals head, buffer is empty
    return current_tail == _head.load(std::memory_order_acquire);
  }

  template <typename U>
    requires std::convertible_to<U, T>
  bool try_push(U &&item) {
    const size_t current_head = _head.load(std::memory_order_relaxed);
    const size_t next_head = (current_head + 1) & (Capacity - 1);
    // If next_head hits tail, buffer is full
    if (next_head == _tail.load(std::memory_order_acquire)) {
      return false;
    }

    new (get_ptr(current_head)) T(std::forward<U>(item));

    _head.store(next_head, std::memory_order_release);
    return true;
  }

  bool try_pop_ip(T &out) {
    const size_t current_tail = _tail.load(std::memory_order_relaxed);
    if (current_tail == _head.load(std::memory_order_acquire)) {
      return false;
    }

    T *element_ptr = get_ptr(current_tail);
    out = std::move(*element_ptr);
    element_ptr->~T();
    _tail.store((current_tail + 1) & (Capacity - 1), std::memory_order_release);
    return true;
  }

  [[nodiscard]] std::optional<T> try_pop() {
    const size_t current_tail = _tail.load(std::memory_order_relaxed);
    if (current_tail == _head.load(std::memory_order_acquire)) {
      return std::nullopt;
    }

    T *element_ptr = get_ptr(current_tail);
    std::optional<T> result(std::move(*element_ptr));
    element_ptr->~T();
    _tail.store((current_tail + 1) & (Capacity - 1), std::memory_order_release);
    return result;
  }
  template <typename Func> bool try_consume(Func &&processor) {
    const size_t current_tail = _tail.load(std::memory_order_relaxed);
    if (current_tail == _head.load(std::memory_order_acquire)) {
      return false;
    }

    T *element_ptr = get_ptr(current_tail);
    std::forward<Func>(processor)(std::move(*element_ptr));
    element_ptr->~T();
    _tail.store((current_tail + 1) & (Capacity - 1), std::memory_order_release);
    return true;
  }

  template <typename... Args>
    requires std::constructible_from<T, Args...>
  bool emplace(Args &&...args) {
    const size_t current_head = _head.load(std::memory_order_relaxed);
    const size_t next_head = (current_head + 1) & (Capacity - 1);
    // If next_head hits tail, buffer is full
    if (next_head == _tail.load(std::memory_order_acquire)) {
      return false;
    }
    new (get_ptr(current_head)) T(std::forward<Args>(args)...);
    _head.store(next_head, std::memory_order_release);
    return true;
  }
};
} // namespace bring
