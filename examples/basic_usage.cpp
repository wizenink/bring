// Basic producer-consumer example with Bring ring buffer
#include <bring/ring_buffer.hpp>
#include <iostream>
#include <thread>

int main() {
  // Create ring buffer with capacity 64 (must be power of 2)
  bring::RingBuffer<int, 64> buffer;

  // Producer thread - sends 100 integers
  std::thread producer([&buffer]() {
    std::cout << "Producer: Starting...\n";
    for (int i = 0; i < 100; ++i) {
      while (!buffer.try_push(i)) {
        // Buffer is full, wait
        std::this_thread::yield();
      }
      if (i % 10 == 0) {
        std::cout << "Producer: Pushed " << i << "\n";
      }
    }
    std::cout << "Producer: Done!\n";
  });

  // Consumer thread - receives 100 integers
  std::thread consumer([&buffer]() {
    std::cout << "Consumer: Starting...\n";
    for (int i = 0; i < 100; ++i) {
      auto value = buffer.try_pop();
      while (!value.has_value()) {
        // Buffer is empty, wait
        std::this_thread::yield();
        value = buffer.try_pop();
      }

      if (value.value() != i) {
        std::cerr << "ERROR: Expected " << i << " but got " << value.value() << "\n";
        return;
      }

      if (i % 10 == 0) {
        std::cout << "Consumer: Received " << value.value() << "\n";
      }
    }
    std::cout << "Consumer: Done!\n";
  });

  producer.join();
  consumer.join();

  std::cout << "\nAll data transferred successfully!\n";
  return 0;
}
