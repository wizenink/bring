#include <bring/ring_buffer.hpp>
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers,bugprone-chained-comparison,readability-identifier-length,bugprone-unchecked-optional-access,bugprone-use-after-move,readability-function-cognitive-complexity,readability-identifier-naming)

TEST_CASE("RingBuffer basic construction", "[ring_buffer]") {
  bring::RingBuffer<int, 8> buffer;

  SECTION("Newly created buffer is empty") {
    auto result = buffer.try_pop();
    REQUIRE_FALSE(result.has_value());
  }
}

TEST_CASE("RingBuffer is_empty and is_full", "[ring_buffer]") {
  bring::RingBuffer<int, 4> buffer;

  SECTION("New buffer is empty") {
    REQUIRE(buffer.is_empty());
    REQUIRE_FALSE(buffer.is_full());
  }

  SECTION("Buffer not empty after push") {
    buffer.try_push(1);
    REQUIRE_FALSE(buffer.is_empty());
    REQUIRE_FALSE(buffer.is_full());
  }

  SECTION("Buffer is full at capacity") {
    // Capacity is 4, but we can only store 3 elements (one slot reserved)
    REQUIRE(buffer.try_push(1));
    REQUIRE(buffer.try_push(2));
    REQUIRE(buffer.try_push(3));
    REQUIRE(buffer.is_full());
    REQUIRE_FALSE(buffer.is_empty());
    REQUIRE_FALSE(buffer.try_push(4)); // Confirm it's actually full
  }

  SECTION("Buffer becomes empty after popping all") {
    buffer.try_push(1);
    buffer.try_push(2);
    REQUIRE_FALSE(buffer.is_empty());

    static_cast<void>(buffer.try_pop());
    REQUIRE_FALSE(buffer.is_empty());

    static_cast<void>(buffer.try_pop());
    REQUIRE(buffer.is_empty());
  }

  SECTION("Alternating empty and full states") {
    // Fill
    REQUIRE(buffer.try_push(1));
    REQUIRE(buffer.try_push(2));
    REQUIRE(buffer.try_push(3));
    REQUIRE(buffer.is_full());

    // Pop one
    static_cast<void>(buffer.try_pop());
    REQUIRE_FALSE(buffer.is_full());
    REQUIRE_FALSE(buffer.is_empty());

    // Fill again
    REQUIRE(buffer.try_push(4));
    REQUIRE(buffer.is_full());

    // Drain completely
    static_cast<void>(buffer.try_pop());
    static_cast<void>(buffer.try_pop());
    static_cast<void>(buffer.try_pop());
    REQUIRE(buffer.is_empty());
    REQUIRE_FALSE(buffer.is_full());
  }
}

TEST_CASE("RingBuffer try_push and try_pop", "[ring_buffer]") {
  bring::RingBuffer<int, 4> buffer;

  SECTION("Push and pop single element") {
    REQUIRE(buffer.try_push(42));
    auto result = buffer.try_pop();
    REQUIRE(result.has_value());
    const int value = result.value();
    REQUIRE(value == 42);
  }

  SECTION("Pop from empty buffer returns nullopt") {
    auto result = buffer.try_pop();
    REQUIRE_FALSE(result.has_value());
  }

  SECTION("Push and pop multiple elements in order") {
    REQUIRE(buffer.try_push(1));
    REQUIRE(buffer.try_push(2));
    REQUIRE(buffer.try_push(3));

    auto r1 = buffer.try_pop();
    REQUIRE(r1.value() == 1);
    auto r2 = buffer.try_pop();
    REQUIRE(r2.value() == 2);
    auto r3 = buffer.try_pop();
    REQUIRE(r3.value() == 3);
    REQUIRE_FALSE(buffer.try_pop().has_value());
  }
}

TEST_CASE("RingBuffer capacity handling", "[ring_buffer]") {
  bring::RingBuffer<int, 4> buffer;

  SECTION("Fill buffer to capacity") {
    // Capacity is 4, but we can only store 3 elements (one slot reserved)
    REQUIRE(buffer.try_push(1));
    REQUIRE(buffer.try_push(2));
    REQUIRE(buffer.try_push(3));
    REQUIRE_FALSE(buffer.try_push(4)); // Buffer full
  }

  SECTION("Can push after popping from full buffer") {
    REQUIRE(buffer.try_push(1));
    REQUIRE(buffer.try_push(2));
    REQUIRE(buffer.try_push(3));
    REQUIRE_FALSE(buffer.try_push(4)); // Full

    static_cast<void>(buffer.try_pop()); // Remove one element
    REQUIRE(buffer.try_push(4)); // Now we can push
  }
}

TEST_CASE("RingBuffer wrapping around", "[ring_buffer]") {
  bring::RingBuffer<int, 4> buffer;

  SECTION("Elements wrap around correctly") {
    // Fill the buffer
    REQUIRE(buffer.try_push(1));
    REQUIRE(buffer.try_push(2));
    REQUIRE(buffer.try_push(3));

    // Pop some elements
    auto r1 = buffer.try_pop();
    REQUIRE(r1.value() == 1);
    auto r2 = buffer.try_pop();
    REQUIRE(r2.value() == 2);

    // Push more elements (should wrap around)
    REQUIRE(buffer.try_push(4));
    REQUIRE(buffer.try_push(5));

    // Verify order
    auto r3 = buffer.try_pop();
    REQUIRE(r3.value() == 3);
    auto r4 = buffer.try_pop();
    REQUIRE(r4.value() == 4);
    auto r5 = buffer.try_pop();
    REQUIRE(r5.value() == 5);
    REQUIRE_FALSE(buffer.try_pop().has_value());
  }
}

TEST_CASE("RingBuffer try_pop_ip", "[ring_buffer]") {
  bring::RingBuffer<int, 8> buffer;

  SECTION("Pop into reference parameter") {
    buffer.try_push(42);
    int value = 0;
    REQUIRE(buffer.try_pop_ip(value));
    REQUIRE(value == 42);
  }

  SECTION("Returns false when empty") {
    int value = 0;
    REQUIRE_FALSE(buffer.try_pop_ip(value));
  }

  SECTION("Multiple pops with references") {
    buffer.try_push(10);
    buffer.try_push(20);
    buffer.try_push(30);

    int a = 0;
    int b = 0;
    int c = 0;
    REQUIRE(buffer.try_pop_ip(a));
    REQUIRE(buffer.try_pop_ip(b));
    REQUIRE(buffer.try_pop_ip(c));

    REQUIRE(a == 10);
    REQUIRE(b == 20);
    REQUIRE(c == 30);
  }
}

TEST_CASE("RingBuffer try_consume", "[ring_buffer]") {
  bring::RingBuffer<int, 8> buffer;

  SECTION("Consume with processor function") {
    buffer.try_push(42);

    int consumed_value = 0;
    bool consumed = buffer.try_consume([&consumed_value](int value) {
      consumed_value = value;
    });

    REQUIRE(consumed);
    REQUIRE(consumed_value == 42);
    REQUIRE_FALSE(buffer.try_pop().has_value()); // Buffer should be empty
  }

  SECTION("Returns false when empty") {
    bool consumed = buffer.try_consume([](int /*value*/) {
      // Should not be called
    });
    REQUIRE_FALSE(consumed);
  }

  SECTION("Consume multiple elements") {
    buffer.try_push(1);
    buffer.try_push(2);
    buffer.try_push(3);

    std::vector<int> consumed_values;
    while (buffer.try_consume([&consumed_values](int value) {
      consumed_values.push_back(value);
    })) {}

    REQUIRE(consumed_values.size() == 3);
    REQUIRE(consumed_values[0] == 1);
    REQUIRE(consumed_values[1] == 2);
    REQUIRE(consumed_values[2] == 3);
  }
}

TEST_CASE("RingBuffer emplace", "[ring_buffer]") {
  bring::RingBuffer<std::string, 8> buffer;

  SECTION("Emplace constructs in-place") {
    REQUIRE(buffer.emplace("Hello"));
    auto result = buffer.try_pop();
    REQUIRE(result.has_value());
    REQUIRE(result.value() == "Hello");
  }

  SECTION("Emplace with multiple arguments") {
    REQUIRE(buffer.emplace(5, 'a')); // std::string(5, 'a') -> "aaaaa"
    auto result = buffer.try_pop();
    REQUIRE(result.has_value());
    REQUIRE(result.value() == "aaaaa");
  }

  SECTION("Emplace fails when buffer is full") {
    REQUIRE(buffer.emplace("1"));
    REQUIRE(buffer.emplace("2"));
    REQUIRE(buffer.emplace("3"));
    REQUIRE(buffer.emplace("4"));
    REQUIRE(buffer.emplace("5"));
    REQUIRE(buffer.emplace("6"));
    REQUIRE(buffer.emplace("7"));
    REQUIRE_FALSE(buffer.emplace("8")); // Full
  }
}

TEST_CASE("RingBuffer with move-only types", "[ring_buffer]") {
  struct MoveOnly {
    int value;
    explicit MoveOnly(int v) : value(v) {}
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&&) noexcept = default;
    MoveOnly& operator=(MoveOnly&&) noexcept = default;
    ~MoveOnly() = default;
  };

  bring::RingBuffer<MoveOnly, 8> buffer;

  SECTION("Can push and pop move-only types") {
    REQUIRE(buffer.try_push(MoveOnly(42)));
    auto result = buffer.try_pop();
    REQUIRE(result.has_value());
    const int val = result.value().value;
    REQUIRE(val == 42);
  }

  SECTION("Can emplace move-only types") {
    REQUIRE(buffer.emplace(99));
    auto result = buffer.try_pop();
    REQUIRE(result.has_value());
    const int val = result.value().value;
    REQUIRE(val == 99);
  }
}

TEST_CASE("RingBuffer with complex types", "[ring_buffer]") {
  bring::RingBuffer<std::string, 16> buffer;

  SECTION("Handles strings correctly") {
    std::string test_string = "Hello, World!";
    REQUIRE(buffer.try_push(test_string));

    auto result = buffer.try_pop();
    REQUIRE(result.has_value());
    REQUIRE(result.value() == "Hello, World!");
  }

  SECTION("Handles string moves") {
    std::string test_string = "Move me";
    REQUIRE(buffer.try_push(std::move(test_string)));

    auto result = buffer.try_pop();
    REQUIRE(result.has_value());
    REQUIRE(result.value() == "Move me");
  }
}

TEST_CASE("RingBuffer destructor cleanup", "[ring_buffer]") {
  static int destructor_count = 0;

  struct CountDestructor {
    CountDestructor() = default;
    ~CountDestructor() { destructor_count++; }
    CountDestructor(const CountDestructor&) = default;
    CountDestructor(CountDestructor&&) noexcept = default;
    CountDestructor& operator=(const CountDestructor&) = default;
    CountDestructor& operator=(CountDestructor&&) noexcept = default;
  };

  SECTION("Destructor cleans up remaining elements") {
    destructor_count = 0;
    {
      bring::RingBuffer<CountDestructor, 8> buffer;
      buffer.emplace();
      buffer.emplace();
      buffer.emplace();
      // Buffer goes out of scope, should destroy all 3 elements
    }
    REQUIRE(destructor_count == 3);
  }
}

TEST_CASE("RingBuffer stress test", "[ring_buffer]") {
  bring::RingBuffer<int, 64> buffer;

  SECTION("Push and pop many elements") {
    for (int i = 0; i < 1000; ++i) {
      buffer.try_push(i);
      auto result = buffer.try_pop();
      REQUIRE(result.has_value());
      const int val = result.value();
      REQUIRE(val == i);
    }
  }

  SECTION("Interleaved push and pop operations") {
    std::vector<int> expected_values;

    for (int i = 0; i < 10; ++i) {
      buffer.try_push(i);
      expected_values.push_back(i);
    }

    for (int i = 0; i < 5; ++i) {
      auto result = buffer.try_pop();
      REQUIRE(result.has_value());
      const int val = result.value();
      REQUIRE(val == expected_values[static_cast<size_t>(i)]);
    }

    for (int i = 10; i < 20; ++i) {
      buffer.try_push(i);
    }

    for (int i = 5; i < 10; ++i) {
      auto result = buffer.try_pop();
      REQUIRE(result.has_value());
      const int val = result.value();
      REQUIRE(val == expected_values[static_cast<size_t>(i)]);
    }

    for (int i = 10; i < 20; ++i) {
      auto result = buffer.try_pop();
      REQUIRE(result.has_value());
      const int val = result.value();
      REQUIRE(val == i);
    }
  }
}

TEST_CASE("RingBuffer move semantics", "[ring_buffer]") {
  bring::RingBuffer<int, 8> buffer1;

  SECTION("Move constructor") {
    buffer1.try_push(1);
    buffer1.try_push(2);

    bring::RingBuffer<int, 8> buffer2(std::move(buffer1));

    auto r1 = buffer2.try_pop();
    REQUIRE(r1.value() == 1);
    auto r2 = buffer2.try_pop();
    REQUIRE(r2.value() == 2);
  }

  SECTION("Move assignment") {
    buffer1.try_push(3);
    buffer1.try_push(4);

    bring::RingBuffer<int, 8> buffer2;
    buffer2 = std::move(buffer1);

    auto r1 = buffer2.try_pop();
    REQUIRE(r1.value() == 3);
    auto r2 = buffer2.try_pop();
    REQUIRE(r2.value() == 4);
  }
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers,bugprone-chained-comparison,readability-identifier-length,bugprone-unchecked-optional-access,bugprone-use-after-move,readability-function-cognitive-complexity,readability-identifier-naming)
