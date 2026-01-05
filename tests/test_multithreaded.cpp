#include <bring/ring_buffer.hpp>
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers,bugprone-unchecked-optional-access,bugprone-chained-comparison,readability-identifier-length,readability-function-cognitive-complexity,readability-identifier-naming)

TEST_CASE("RingBuffer SPSC basic multi-threaded", "[ring_buffer][threading]") {
  constexpr size_t NUM_ITEMS = 100000;
  constexpr size_t CAPACITY = 64;

  bring::RingBuffer<uint64_t, CAPACITY> buffer;
  std::atomic<bool> producer_done{false};
  std::atomic<uint64_t> items_consumed{0};

  // Producer thread
  std::thread producer([&buffer, &producer_done]() {
    for (uint64_t i = 0; i < NUM_ITEMS; ++i) {
      while (!buffer.try_push(i)) {
        std::this_thread::yield();
      }
    }
    producer_done.store(true, std::memory_order_release);
  });

  // Consumer thread
  std::thread consumer([&buffer, &items_consumed]() {
    uint64_t expected = 0;
    while (expected < NUM_ITEMS) {
      auto result = buffer.try_pop();
      if (result.has_value()) {
        REQUIRE(result.value() == expected);
        expected++;
        items_consumed.fetch_add(1, std::memory_order_relaxed);
      } else {
        std::this_thread::yield();
      }
    }
  });

  producer.join();
  consumer.join();

  REQUIRE(items_consumed.load() == NUM_ITEMS);
  REQUIRE(producer_done.load());
}

TEST_CASE("RingBuffer SPSC stress test - small buffer", "[ring_buffer][threading][stress]") {
  constexpr size_t NUM_ITEMS = 1000000;
  constexpr size_t CAPACITY = 8; // Very small buffer to maximize contention

  bring::RingBuffer<uint64_t, CAPACITY> buffer;
  std::atomic<bool> producer_done{false};
  std::atomic<uint64_t> push_attempts{0};
  std::atomic<uint64_t> push_failures{0};
  std::atomic<uint64_t> pop_attempts{0};
  std::atomic<uint64_t> pop_failures{0};

  std::thread producer([&]() {
    for (uint64_t i = 0; i < NUM_ITEMS; ++i) {
      push_attempts.fetch_add(1, std::memory_order_relaxed);
      while (!buffer.try_push(i)) {
        push_failures.fetch_add(1, std::memory_order_relaxed);
        std::this_thread::yield();
        push_attempts.fetch_add(1, std::memory_order_relaxed);
      }
    }
    producer_done.store(true, std::memory_order_release);
  });

  std::thread consumer([&]() {
    uint64_t expected = 0;
    while (expected < NUM_ITEMS) {
      pop_attempts.fetch_add(1, std::memory_order_relaxed);
      auto result = buffer.try_pop();
      if (result.has_value()) {
        REQUIRE(result.value() == expected);
        expected++;
      } else {
        pop_failures.fetch_add(1, std::memory_order_relaxed);
        std::this_thread::yield();
      }
    }
  });

  producer.join();
  consumer.join();

  REQUIRE(producer_done.load());

  // Report contention statistics
  const uint64_t total_pushes = push_attempts.load();
  const uint64_t failed_pushes = push_failures.load();
  const uint64_t total_pops = pop_attempts.load();
  const uint64_t failed_pops = pop_failures.load();

  INFO("Push attempts: " << total_pushes << ", failures: " << failed_pushes
       << " (" << (100.0 * failed_pushes / total_pushes) << "%)");
  INFO("Pop attempts: " << total_pops << ", failures: " << failed_pops
       << " (" << (100.0 * failed_pops / total_pops) << "%)");
}

TEST_CASE("RingBuffer SPSC stress test - large buffer", "[ring_buffer][threading][stress]") {
  constexpr size_t NUM_ITEMS = 1000000;
  constexpr size_t CAPACITY = 2048; // Large buffer to minimize contention

  bring::RingBuffer<uint64_t, CAPACITY> buffer;
  std::atomic<bool> producer_done{false};

  std::thread producer([&]() {
    for (uint64_t i = 0; i < NUM_ITEMS; ++i) {
      while (!buffer.try_push(i)) {
        std::this_thread::yield();
      }
    }
    producer_done.store(true, std::memory_order_release);
  });

  std::thread consumer([&]() {
    uint64_t expected = 0;
    while (expected < NUM_ITEMS) {
      auto result = buffer.try_pop();
      if (result.has_value()) {
        REQUIRE(result.value() == expected);
        expected++;
      } else {
        std::this_thread::yield();
      }
    }
  });

  producer.join();
  consumer.join();

  REQUIRE(producer_done.load());
}

TEST_CASE("RingBuffer SPSC with complex data", "[ring_buffer][threading]") {
  struct ComplexData {
    uint64_t id;
    uint64_t checksum;
    std::array<uint8_t, 32> payload{};

    explicit ComplexData(uint64_t val) : id(val), checksum(val * 2654435761ULL) {
      for (size_t i = 0; i < payload.size(); ++i) {
        payload.at(i) = static_cast<uint8_t>((val + i) & 0xFF);
      }
    }

    [[nodiscard]] bool verify() const {
      if (checksum != id * 2654435761ULL) {
        return false;
      }
      for (size_t i = 0; i < payload.size(); ++i) {
        if (payload.at(i) != static_cast<uint8_t>((id + i) & 0xFF)) {
          return false;
        }
      }
      return true;
    }
  };

  constexpr size_t NUM_ITEMS = 100000;
  constexpr size_t CAPACITY = 128;

  bring::RingBuffer<ComplexData, CAPACITY> buffer;
  std::atomic<bool> producer_done{false};
  std::atomic<size_t> corruption_count{0};

  std::thread producer([&]() {
    for (uint64_t i = 0; i < NUM_ITEMS; ++i) {
      while (!buffer.emplace(i)) {
        std::this_thread::yield();
      }
    }
    producer_done.store(true, std::memory_order_release);
  });

  std::thread consumer([&]() {
    uint64_t expected = 0;
    while (expected < NUM_ITEMS) {
      auto result = buffer.try_pop();
      if (result.has_value()) {
        const auto& data = result.value();
        REQUIRE(data.id == expected);
        if (!data.verify()) {
          corruption_count.fetch_add(1, std::memory_order_relaxed);
        }
        expected++;
      } else {
        std::this_thread::yield();
      }
    }
  });

  producer.join();
  consumer.join();

  REQUIRE(producer_done.load());
  REQUIRE(corruption_count.load() == 0);
}

TEST_CASE("RingBuffer SPSC burst pattern", "[ring_buffer][threading]") {
  constexpr size_t NUM_BURSTS = 1000;
  constexpr size_t BURST_SIZE = 100;
  constexpr size_t CAPACITY = 64;

  bring::RingBuffer<uint64_t, CAPACITY> buffer;
  std::atomic<bool> producer_done{false};
  std::atomic<size_t> total_consumed{0};

  // Producer sends in bursts with pauses
  std::thread producer([&]() {
    for (size_t burst = 0; burst < NUM_BURSTS; ++burst) {
      for (size_t i = 0; i < BURST_SIZE; ++i) {
        const uint64_t value = (burst * BURST_SIZE) + i;
        while (!buffer.try_push(value)) {
          std::this_thread::yield();
        }
      }
      // Small pause between bursts
      std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
    producer_done.store(true, std::memory_order_release);
  });

  // Consumer processes continuously
  std::thread consumer([&]() {
    uint64_t expected = 0;
    const uint64_t total_expected = NUM_BURSTS * BURST_SIZE;

    while (expected < total_expected) {
      auto result = buffer.try_pop();
      if (result.has_value()) {
        REQUIRE(result.value() == expected);
        expected++;
        total_consumed.fetch_add(1, std::memory_order_relaxed);
      } else {
        std::this_thread::yield();
      }
    }
  });

  producer.join();
  consumer.join();

  REQUIRE(producer_done.load());
  REQUIRE(total_consumed.load() == NUM_BURSTS * BURST_SIZE);
}

TEST_CASE("RingBuffer SPSC try_consume pattern", "[ring_buffer][threading]") {
  constexpr size_t NUM_ITEMS = 100000;
  constexpr size_t CAPACITY = 128;

  bring::RingBuffer<uint64_t, CAPACITY> buffer;
  std::atomic<bool> producer_done{false};
  std::atomic<uint64_t> sum{0};

  std::thread producer([&]() {
    for (uint64_t i = 0; i < NUM_ITEMS; ++i) {
      while (!buffer.try_push(i)) {
        std::this_thread::yield();
      }
    }
    producer_done.store(true, std::memory_order_release);
  });

  std::thread consumer([&]() {
    uint64_t expected = 0;
    uint64_t local_sum = 0;

    while (expected < NUM_ITEMS) {
      if (buffer.try_consume([&](uint64_t value) {
        REQUIRE(value == expected);
        local_sum += value;
        expected++;
      })) {
        // Successfully consumed
      } else {
        std::this_thread::yield();
      }
    }

    sum.store(local_sum, std::memory_order_release);
  });

  producer.join();
  consumer.join();

  REQUIRE(producer_done.load());

  // Verify sum: sum of 0 to N-1 = N*(N-1)/2
  const uint64_t expected_sum = NUM_ITEMS * (NUM_ITEMS - 1) / 2;
  REQUIRE(sum.load() == expected_sum);
}

TEST_CASE("RingBuffer SPSC wraparound stress", "[ring_buffer][threading]") {
  // Test that wrapping around the buffer many times works correctly
  constexpr size_t NUM_ITEMS = 10000000; // 10 million items
  constexpr size_t CAPACITY = 32; // Small buffer forces many wraparounds

  bring::RingBuffer<uint32_t, CAPACITY> buffer;
  std::atomic<bool> producer_done{false};
  std::atomic<size_t> items_consumed{0};

  std::thread producer([&]() {
    for (uint32_t i = 0; i < NUM_ITEMS; ++i) {
      while (!buffer.try_push(i)) {
        // Spin
      }
    }
    producer_done.store(true, std::memory_order_release);
  });

  std::thread consumer([&]() {
    uint32_t expected = 0;
    while (expected < NUM_ITEMS) {
      auto result = buffer.try_pop();
      if (result.has_value()) {
        REQUIRE(result.value() == expected);
        expected++;
        items_consumed.fetch_add(1, std::memory_order_relaxed);
      }
    }
  });

  producer.join();
  consumer.join();

  REQUIRE(items_consumed.load() == NUM_ITEMS);
  REQUIRE(producer_done.load());

  // With 10M items and capacity 32, we wrapped around ~312,500 times
  INFO("Successfully wrapped around buffer approximately "
       << (NUM_ITEMS / CAPACITY) << " times");
}

TEST_CASE("RingBuffer SPSC memory ordering test", "[ring_buffer][threading]") {
  // This test verifies that memory ordering is correct by using
  // a pattern where each item depends on the previous one
  constexpr size_t NUM_ITEMS = 100000;
  constexpr size_t CAPACITY = 64;

  struct DependentData {
    uint64_t value;
    uint64_t previous_hash;

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    DependentData(uint64_t v, uint64_t prev_hash)
      : value(v), previous_hash(prev_hash) {}
  };

  bring::RingBuffer<DependentData, CAPACITY> buffer;
  std::atomic<bool> producer_done{false};

  std::thread producer([&]() {
    uint64_t prev_hash = 0;
    for (uint64_t i = 0; i < NUM_ITEMS; ++i) {
      while (!buffer.emplace(i, prev_hash)) {
        std::this_thread::yield();
      }
      prev_hash = i * 2654435761ULL; // Simple hash
    }
    producer_done.store(true, std::memory_order_release);
  });

  std::thread consumer([&]() {
    uint64_t expected = 0;
    uint64_t expected_prev_hash = 0;

    while (expected < NUM_ITEMS) {
      auto result = buffer.try_pop();
      if (result.has_value()) {
        const auto& data = result.value();
        REQUIRE(data.value == expected);
        REQUIRE(data.previous_hash == expected_prev_hash);

        expected++;
        expected_prev_hash = (expected - 1) * 2654435761ULL;
      } else {
        std::this_thread::yield();
      }
    }
  });

  producer.join();
  consumer.join();

  REQUIRE(producer_done.load());
}

TEST_CASE("RingBuffer is_empty and is_full thread-safe", "[ring_buffer][threading]") {
  constexpr size_t NUM_ITERATIONS = 10000;
  constexpr size_t CAPACITY = 16;

  bring::RingBuffer<int, CAPACITY> buffer;
  std::atomic<bool> test_done{false};
  std::atomic<size_t> empty_observations{0};
  std::atomic<size_t> full_observations{0};
  std::atomic<size_t> neither_observations{0};

  // Producer thread
  std::thread producer([&]() {
    for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
      while (!buffer.try_push(static_cast<int>(i))) {
        std::this_thread::yield();
      }
    }
    test_done.store(true, std::memory_order_release);
  });

  // Observer thread - checks is_empty and is_full
  std::thread observer([&]() {
    while (!test_done.load(std::memory_order_acquire)) {
      // Use get_state() to atomically check both conditions from same snapshot
      const auto state = buffer.get_state();
      const bool empty = state.empty;
      const bool full = state.full;

      // Buffer should never be both empty and full
      REQUIRE_FALSE((empty && full));

      if (empty) {
        empty_observations.fetch_add(1, std::memory_order_relaxed);
      } else if (full) {
        full_observations.fetch_add(1, std::memory_order_relaxed);
      } else {
        neither_observations.fetch_add(1, std::memory_order_relaxed);
      }

      std::this_thread::yield();
    }
  });

  // Consumer thread
  std::thread consumer([&]() {
    size_t consumed = 0;
    while (consumed < NUM_ITERATIONS) {
      auto result = buffer.try_pop();
      if (result.has_value()) {
        consumed++;
      } else {
        std::this_thread::yield();
      }
    }
  });

  producer.join();
  consumer.join();
  observer.join();

  INFO("Empty observations: " << empty_observations.load());
  INFO("Full observations: " << full_observations.load());
  INFO("Neither observations: " << neither_observations.load());

  // Should have observed various states
  REQUIRE(empty_observations.load() + full_observations.load() + neither_observations.load() > 0);
}

TEST_CASE("RingBuffer is_empty consistency", "[ring_buffer][threading]") {
  constexpr size_t NUM_ITEMS = 100000;
  constexpr size_t CAPACITY = 64;

  bring::RingBuffer<int, CAPACITY> buffer;
  std::atomic<bool> producer_done{false};

  // Producer thread
  std::thread producer([&]() {
    for (size_t i = 0; i < NUM_ITEMS; ++i) {
      while (!buffer.try_push(static_cast<int>(i))) {
        std::this_thread::yield();
      }
    }
    producer_done.store(true, std::memory_order_release);
  });

  // Consumer thread that uses is_empty to decide when to try popping
  std::thread consumer([&]() {
    size_t consumed = 0;
    while (consumed < NUM_ITEMS) {
      if (!buffer.is_empty()) {
        auto result = buffer.try_pop();
        if (result.has_value()) {
          consumed++;
        }
      } else {
        std::this_thread::yield();
      }
    }
  });

  producer.join();
  consumer.join();

  REQUIRE(producer_done.load());
  REQUIRE(buffer.is_empty());
}

TEST_CASE("RingBuffer is_full consistency", "[ring_buffer][threading]") {
  constexpr size_t NUM_ITEMS = 100000;
  constexpr size_t CAPACITY = 32;

  bring::RingBuffer<int, CAPACITY> buffer;
  std::atomic<bool> consumer_done{false};
  std::atomic<size_t> full_spin_count{0};

  // Consumer thread (slower, will cause backpressure)
  std::thread consumer([&]() {
    for (size_t i = 0; i < NUM_ITEMS; ++i) {
      auto result = buffer.try_pop();
      while (!result.has_value()) {
        std::this_thread::yield();
        result = buffer.try_pop();
      }
      const int expected = static_cast<int>(i);
      REQUIRE(result.value() == expected);
    }
    consumer_done.store(true, std::memory_order_release);
  });

  // Producer thread that uses is_full to apply backpressure
  std::thread producer([&]() {
    for (size_t i = 0; i < NUM_ITEMS; ++i) {
      while (buffer.is_full()) {
        full_spin_count.fetch_add(1, std::memory_order_relaxed);
        std::this_thread::yield();
      }
      while (!buffer.try_push(static_cast<int>(i))) {
        std::this_thread::yield();
      }
    }
  });

  producer.join();
  consumer.join();

  REQUIRE(consumer_done.load());
  INFO("Times producer observed full buffer: " << full_spin_count.load());
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers,bugprone-unchecked-optional-access,bugprone-chained-comparison,readability-identifier-length,readability-function-cognitive-complexity,readability-identifier-naming)
