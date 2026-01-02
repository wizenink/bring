// NOLINTBEGIN
#include <benchmark/benchmark.h>
#include <bring/ring_buffer.hpp>
#include <cstring>
#include <mutex>
#include <queue>
#include <thread>

// ============================================================================
// Single-threaded benchmarks (baseline comparisons)
// ============================================================================

static void BM_LockedQueue(benchmark::State &state) {
  std::queue<int> queue;
  std::mutex mtx;

  for (auto _ : state) {
    {
      std::lock_guard<std::mutex> lock(mtx);
      queue.push(42);
    }
    {
      std::lock_guard<std::mutex> lock(mtx);
      int out = queue.front();
      queue.pop();
      benchmark::DoNotOptimize(out);
    }
  }
}
BENCHMARK(BM_LockedQueue);

static void BM_RingBuffer_Int(benchmark::State &state) {
  bring::RingBuffer<int, 1024> buffer;
  for (auto _ : state) {
    buffer.try_push(42);
    int out = 0;
    buffer.try_pop_ip(out);
    benchmark::DoNotOptimize(out);
  }
}
BENCHMARK(BM_RingBuffer_Int);

// Test with different buffer sizes
template <size_t Size>
static void BM_RingBuffer_Sizes(benchmark::State &state) {
  bring::RingBuffer<int, Size> buffer;
  for (auto _ : state) {
    buffer.try_push(42);
    int out = 0;
    buffer.try_pop_ip(out);
    benchmark::DoNotOptimize(out);
  }
}
BENCHMARK(BM_RingBuffer_Sizes<64>);
BENCHMARK(BM_RingBuffer_Sizes<256>);
BENCHMARK(BM_RingBuffer_Sizes<1024>);
BENCHMARK(BM_RingBuffer_Sizes<4096>);

// Test with large data structures
struct LargeStruct {
  uint64_t data[16]; // 128 bytes
  LargeStruct() { std::memset(data, 0, sizeof(data)); }
};

static void BM_RingBuffer_LargeStruct(benchmark::State &state) {
  bring::RingBuffer<LargeStruct, 512> buffer;
  for (auto _ : state) {
    buffer.emplace();
    LargeStruct out;
    buffer.try_pop_ip(out);
    benchmark::DoNotOptimize(out);
  }
}
BENCHMARK(BM_RingBuffer_LargeStruct);

// Test try_consume pattern
static void BM_RingBuffer_TryConsume(benchmark::State &state) {
  bring::RingBuffer<int, 1024> buffer;
  for (auto _ : state) {
    buffer.try_push(42);
    buffer.try_consume([](int &&value) { benchmark::DoNotOptimize(value); });
  }
}
BENCHMARK(BM_RingBuffer_TryConsume);

// Test emplace vs push
static void BM_RingBuffer_Emplace(benchmark::State &state) {
  bring::RingBuffer<int, 1024> buffer;
  for (auto _ : state) {
    buffer.emplace(42);
    int out = 0;
    buffer.try_pop_ip(out);
    benchmark::DoNotOptimize(out);
  }
}
BENCHMARK(BM_RingBuffer_Emplace);

static void BM_SPSC_Throughput(benchmark::State &state) {
  static bring::RingBuffer<int, 65536> buffer;
  static std::atomic<bool> done{false};

  if (state.thread_index() == 0) {
    // Producer thread
    done.store(false, std::memory_order_relaxed);
    for (auto _ : state) {
      while (!buffer.try_push(42))
        ;
    }
    done.store(true, std::memory_order_release);
  } else {
    // Consumer thread - drain until producer signals done AND buffer empty
    for (auto _ : state) {
      int out;
      while (!buffer.try_pop_ip(out)) {
        if (done.load(std::memory_order_acquire) && buffer.is_empty()) {
          return;
        }
      }
      benchmark::DoNotOptimize(out);
    }
    // Drain remaining items
    while (!done.load(std::memory_order_acquire) || !buffer.is_empty()) {
      int out;
      if (buffer.try_pop_ip(out)) {
        benchmark::DoNotOptimize(out);
      }
    }
  }
}
BENCHMARK(BM_SPSC_Throughput)->Threads(2)->UseRealTime();

// ============================================================================
// Multi-threaded benchmarks with different buffer sizes
// ============================================================================

template <size_t Size> static void BM_SPSC_BufferSize(benchmark::State &state) {
  static bring::RingBuffer<int, Size> buffer;
  static std::atomic<bool> done{false};

  if (state.thread_index() == 0) {
    done.store(false, std::memory_order_relaxed);
    for (auto _ : state) {
      while (!buffer.try_push(42))
        ;
    }
    done.store(true, std::memory_order_release);
  } else {
    for (auto _ : state) {
      int out;
      while (!buffer.try_pop_ip(out)) {
        if (done.load(std::memory_order_acquire) && buffer.is_empty()) {
          return;
        }
      }
      benchmark::DoNotOptimize(out);
    }
    while (!done.load(std::memory_order_acquire) || !buffer.is_empty()) {
      int out;
      if (buffer.try_pop_ip(out)) {
        benchmark::DoNotOptimize(out);
      }
    }
  }
}

BENCHMARK(BM_SPSC_BufferSize<256>)->Threads(2)->UseRealTime();
BENCHMARK(BM_SPSC_BufferSize<1024>)->Threads(2)->UseRealTime();
BENCHMARK(BM_SPSC_BufferSize<16384>)->Threads(2)->UseRealTime();

// ============================================================================
// SPSC with large structs
// ============================================================================

static void BM_SPSC_LargeStruct(benchmark::State &state) {
  static bring::RingBuffer<LargeStruct, 4096> buffer;
  static std::atomic<bool> done{false};

  if (state.thread_index() == 0) {
    done.store(false, std::memory_order_relaxed);
    for (auto _ : state) {
      while (!buffer.emplace())
        ;
    }
    done.store(true, std::memory_order_release);
  } else {
    for (auto _ : state) {
      LargeStruct out;
      while (!buffer.try_pop_ip(out)) {
        if (done.load(std::memory_order_acquire) && buffer.is_empty()) {
          return;
        }
      }
      benchmark::DoNotOptimize(out);
    }
    while (!done.load(std::memory_order_acquire) || !buffer.is_empty()) {
      LargeStruct out;
      if (buffer.try_pop_ip(out)) {
        benchmark::DoNotOptimize(out);
      }
    }
  }
}
BENCHMARK(BM_SPSC_LargeStruct)->Threads(2)->UseRealTime();

// ============================================================================
// SPSC with try_consume
// ============================================================================

static void BM_SPSC_TryConsume(benchmark::State &state) {
  static bring::RingBuffer<int, 16384> buffer;
  static std::atomic<bool> done{false};

  if (state.thread_index() == 0) {
    done.store(false, std::memory_order_relaxed);
    for (auto _ : state) {
      while (!buffer.try_push(42))
        ;
    }
    done.store(true, std::memory_order_release);
  } else {
    for (auto _ : state) {
      while (!buffer.try_consume(
          [](int &&value) { benchmark::DoNotOptimize(value); })) {
        if (done.load(std::memory_order_acquire) && buffer.is_empty()) {
          return;
        }
      }
    }
    while (!done.load(std::memory_order_acquire) || !buffer.is_empty()) {
      buffer.try_consume([](int &&value) { benchmark::DoNotOptimize(value); });
    }
  }
}
BENCHMARK(BM_SPSC_TryConsume)->Threads(2)->UseRealTime();

// ============================================================================
// Contention test - smaller buffer to force more contention
// ============================================================================

static void BM_SPSC_HighContention(benchmark::State &state) {
  static bring::RingBuffer<int, 64> buffer;
  static std::atomic<bool> done{false};

  if (state.thread_index() == 0) {
    done.store(false, std::memory_order_relaxed);
    for (auto _ : state) {
      while (!buffer.try_push(42))
        ;
    }
    done.store(true, std::memory_order_release);
  } else {
    for (auto _ : state) {
      int out;
      while (!buffer.try_pop_ip(out)) {
        if (done.load(std::memory_order_acquire) && buffer.is_empty()) {
          return;
        }
      }
      benchmark::DoNotOptimize(out);
    }
    while (!done.load(std::memory_order_acquire) || !buffer.is_empty()) {
      int out;
      if (buffer.try_pop_ip(out)) {
        benchmark::DoNotOptimize(out);
      }
    }
  }
}
BENCHMARK(BM_SPSC_HighContention)->Threads(2)->UseRealTime();

BENCHMARK_MAIN();

// NOLINTEND
