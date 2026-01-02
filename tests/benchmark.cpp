#include <bring/ring_buffer.hpp>
#include <iostream>

constexpr size_t ITERATIONS = 10000;
constexpr size_t CAPACITY = 1024;

int main() {
  bring::RingBuffer<int, CAPACITY> buffer;

  // Benchmark: Fill and drain the buffer repeatedly
  for (size_t iter = 0; iter < ITERATIONS; ++iter) {
    // Fill buffer
    for (size_t i = 0; i < CAPACITY - 1; ++i) {
      buffer.try_push(static_cast<int>(i));
    }

    // Drain buffer
    for (size_t i = 0; i < CAPACITY - 1; ++i) {
      static_cast<void>(buffer.try_pop());
    }
  }

  std::cout << "Completed " << ITERATIONS << " iterations\n";
  return 0;
}
