#include "MessageQueue.h"

#include <benchmark/benchmark.h>

#include <thread>
#include <vector>

// ─── Single-threaded throughput ──────────────────────────────────────

static void BM_MessageQueue_SendReceive(benchmark::State &state) {
    const auto capacity = static_cast<std::size_t>(state.range(0));
    MessageQueue<int> q(capacity);

    for (auto _ : state) {
        q.send(42);
        benchmark::DoNotOptimize(q.receive());
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MessageQueue_SendReceive)
    ->Arg(1)->Arg(16)->Arg(256)
    ->Unit(benchmark::kMicrosecond);

// ─── Multi-producer, single-consumer throughput ─────────────────────

static void BM_MessageQueue_MPSC(benchmark::State &state) {
    const int num_producers = static_cast<int>(state.range(0));
    constexpr int kMessagesPerProducer = 1000;

    for (auto _ : state) {
        MessageQueue<int> q(static_cast<std::size_t>(
            num_producers * kMessagesPerProducer));

        // Launch producers
        std::vector<std::jthread> producers;
        producers.reserve(static_cast<std::size_t>(num_producers));
        for (int p = 0; p < num_producers; ++p) {
            producers.emplace_back([&q] {
                for (int i = 0; i < kMessagesPerProducer; ++i) {
                    q.send(i);
                }
            });
        }

        // Consumer: drain all messages
        int total = num_producers * kMessagesPerProducer;
        for (int i = 0; i < total; ++i) {
            benchmark::DoNotOptimize(q.receive());
        }

        producers.clear();
    }

    state.SetItemsProcessed(
        state.iterations() * num_producers * kMessagesPerProducer);
}
BENCHMARK(BM_MessageQueue_MPSC)
    ->Arg(1)->Arg(2)->Arg(4)->Arg(8)->Arg(16)
    ->Unit(benchmark::kMillisecond);

// ─── Contended send (measures lock contention) ──────────────────────

static void BM_MessageQueue_ContendedSend(benchmark::State &state) {
    const int num_threads = static_cast<int>(state.range(0));
    constexpr int kOpsPerThread = 10000;

    for (auto _ : state) {
        MessageQueue<int> q(static_cast<std::size_t>(
            num_threads * kOpsPerThread));

        std::vector<std::jthread> threads;
        threads.reserve(static_cast<std::size_t>(num_threads));
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&q] {
                for (int i = 0; i < kOpsPerThread; ++i) {
                    q.send(i);
                }
            });
        }
        threads.clear();
    }

    state.SetItemsProcessed(
        state.iterations() * num_threads * kOpsPerThread);
}
BENCHMARK(BM_MessageQueue_ContendedSend)
    ->Arg(1)->Arg(2)->Arg(4)->Arg(8)
    ->Unit(benchmark::kMillisecond);
