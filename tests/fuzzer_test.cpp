// NOLINTBEGIN
#include <bring/ring_buffer.hpp>
#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // We'll use a small capacity to increase the chance of wrap-around/collision
  static bring::RingBuffer<uint32_t, 16> buffer;

  for (size_t i = 0; i < size; ++i) {
    uint8_t command = data[i] % 4;

    switch (command) {
    case 0: // try_push
      buffer.try_push(static_cast<uint32_t>(i));
      break;
    case 1: { // try_pop_ip
      uint32_t out;
      buffer.try_pop_ip(out);
      break;
    }
    case 2: { // try_pop
      [[maybe_unused]]
      auto out = buffer.try_pop();
      break;
    }
    case 3: // emplace
      buffer.emplace(static_cast<uint32_t>(i));
      break;
    }
  }
  return 0;
}

// NOLINTEND
