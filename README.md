# Bring - Lock-Free SPSC Ring Buffer

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)

A high-performance, lock-free **Single Producer Single Consumer (SPSC)** ring buffer implementation in modern C++23. Designed for maximum throughput and minimal latency in concurrent scenarios.

## Features

- **Lock-Free**: Uses atomics with acquire-release semantics for thread safety
- **Cache-Optimized**: False sharing prevention with 64-byte alignment
- **Zero-Copy**: Move semantics and in-place construction support
- **Type-Safe**: C++23 concepts for compile-time validation
- **Header-Only**: Easy integration, just include and use

## Quick Start

### Requirements

- C++23 compatible compiler (GCC 12+, Clang 16+, MSVC 2022+)
- CMake 3.20+

### Installation

#### Header-Only

Simply copy `include/bring/ring_buffer.hpp` to your project:

```cpp
#include <bring/ring_buffer.hpp>

bring::RingBuffer<int, 1024> buffer;
```

#### CMake Integration

```cmake
include(FetchContent)
FetchContent_Declare(
  bring
  GIT_REPOSITORY https://github.com/wizenink/bring.git
  GIT_TAG master
)
FetchContent_MakeAvailable(bring)

target_link_libraries(your_target PRIVATE bring::bring)
```

### Basic Usage

```cpp
#include <bring/ring_buffer.hpp>
#include <thread>

// Create a ring buffer (capacity must be power of 2)
bring::RingBuffer<int, 64> buffer;

// Producer thread
std::thread producer([&buffer]() {
    for (int i = 0; i < 1000; ++i) {
        while (!buffer.try_push(i)) {
            std::this_thread::yield(); // Buffer full, retry
        }
    }
});

// Consumer thread
std::thread consumer([&buffer]() {
    for (int i = 0; i < 1000; ++i) {
        auto value = buffer.try_pop();
        while (!value.has_value()) {
            std::this_thread::yield(); // Buffer empty, retry
            value = buffer.try_pop();
        }
        // Process value.value()
    }
});

producer.join();
consumer.join();
```

## API Reference

### Construction

```cpp
bring::RingBuffer<T, Capacity> buffer;
```

- `T`: Element type (must be move-constructible and destructible)
- `Capacity`: Buffer size (must be power of 2, > 1)

### Core Operations

#### `try_push(item) -> bool`

Try to push an item into the buffer. Returns `true` on success, `false` if buffer is full.

```cpp
if (buffer.try_push(42)) {
    // Success
}
```

#### `try_pop() -> std::optional<T>`

Try to pop an item from the buffer. Returns `std::optional` with value on success, `std::nullopt` if empty.

```cpp
auto result = buffer.try_pop();
if (result.has_value()) {
    int value = result.value();
}
```

#### `try_pop_ip(T& out) -> bool`

In-place pop for better performance (avoids `std::optional` overhead).

```cpp
int value;
if (buffer.try_pop_ip(value)) {
    // value contains popped element
}
```

#### `try_consume(Func&& processor) -> bool`

Process element with callback without extracting it.

```cpp
buffer.try_consume([](int&& value) {
    // Process value
});
```

#### `emplace(Args&&... args) -> bool`

Construct element in-place. Returns `true` on success.

```cpp
buffer.emplace(arg1, arg2, arg3);
```

### Query Operations

#### `is_empty() -> bool`

Check if buffer is empty.

#### `is_full() -> bool`

Check if buffer is full.

## Design Decisions

### Why Power-of-2 Capacity?

Enables fast modulo operations using bitwise AND: `(index + 1) & (Capacity - 1)`

### Memory Ordering

- **Acquire-Release**: Ensures visibility of data between producer/consumer
- **Relaxed**: Used for same-thread operations where ordering not critical

### Cache Line Alignment

Head and tail pointers are aligned to 64-byte boundaries to prevent false sharing between producer and consumer threads.

### One-Slot Reservation

The buffer reserves one slot to distinguish between full and empty states (when `head == tail`).

## Building & Testing

```bash
# Configure
cmake -B build -DBUILD_TESTING=ON

# Build
cmake --build build

# Run tests
./build/unit_tests        # Single-threaded tests
./build/mt_tests          # Multi-threaded stress tests
./build/benchmark         # Performance benchmark
```

### Run with Valgrind

```bash
valgrind --tool=cachegrind ./build/benchmark
cg_annotate --auto=yes cachegrind.out
```

## Thread Safety

**SPSC Only**: This ring buffer is designed for exactly one producer thread and one consumer thread. Using it with multiple producers or consumers will result in race conditions.

For MPSC/MPMC scenarios, use a different data structure or add external synchronization.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
