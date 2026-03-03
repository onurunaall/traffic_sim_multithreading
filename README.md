# Concurrent Traffic Simulation

![Simulation in action](assets/Recording_2026-03-03_151433.gif)

A multithreaded city traffic simulator written in C++20. Vehicles drive around a map, queue at intersections, and wait for traffic lights — all running concurrently across independent threads.

Built as a deep dive into C++ concurrency primitives: `std::jthread`, `std::stop_token`, promises/futures, atomic operations, and a hand-rolled thread-safe message queue.

---

## What it does

Vehicles spawn on streets and drive toward intersections. When they get close, they enter a FIFO queue and block until the intersection grants them entry — one at a time. If the traffic light is red, they wait for green before crossing. Once through, they pick a random outgoing street and keep going.

The whole thing renders live using OpenCV, with each vehicle getting a unique color and intersections showing red/green based on their current light phase.

Two maps are included: Paris (star topology, all roads lead to a central plaza) and NYC (ring topology with a diagonal shortcut).

---

## Dependencies

- C++20 compiler (GCC 10+, Clang 12+, MSVC 2022)
- CMake 3.16+
- OpenCV 4.1+
- pthreads (Linux/macOS) — handled automatically on Windows via a stub

---

## Building

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

The background images need to be in a `data/` folder at the project root. The Paris scene looks for `data/paris.jpg`.

To disable the GUI (e.g. for headless testing):

```bash
cmake .. -DBUILD_GUI=OFF
```

Optional extras:

```bash
cmake .. -DBUILD_TESTS=ON        # GTest unit tests
cmake .. -DBUILD_BENCHMARKS=ON   # Google Benchmark
```

---

## Project structure

```
src/
  TrafficObject      base class, owns jthreads, atomic position
  TrafficLight       toggles red/green every 4-6 s, exposes waitForGreen()
  MessageQueue       templated thread-safe queue used by TrafficLight
  Intersection       manages WaitingVehicles queue + processes entries
  Street             connects two intersections, stores length
  Vehicle            drives, queues, crosses, picks next street
  Graphics           OpenCV render loop
  main               scene setup + simulation launch
```

---

## Concurrency design

Each vehicle runs on its own `jthread`. Each intersection runs a queue processor thread. Each traffic light runs a cycle thread. No shared mutable state except where explicitly protected.

**Vehicle to Intersection handoff** uses a `std::promise`/`std::future` pair. The vehicle thread blocks on `ftr.wait()` inside `addVehicleToQueue`. The intersection's processor thread fulfills the promise when it's the vehicle's turn, unblocking it. This avoids polling.

**Traffic light phase propagation** goes through `MessageQueue<TrafficLightPhase>`. `waitForGreen()` drains the queue until it sees a green phase — it won't unblock on a stale red.

**Stop propagation**: `jthread` destructor calls `request_stop()` + `join()` automatically. The `TrafficObject` destructor explicitly requests stop on all owned threads before clearing the vector, so shutdown order is deterministic.

One subtle thing: `is_blocked_` on `Intersection` is an `std::atomic<bool>` rather than a mutex-protected flag. The queue processor checks and sets it without locking, which is fine because it's the only writer and the check-set doesn't need to be atomic with anything else.

---

## Switching scenes

In `main.cpp`, swap `createParisScene` for `createNYCScene` (and point `background_image` at `data/nyc.jpg`). Vehicle count is the first argument — keep it at or below the number of streets or you'll get an out-of-bounds access on the street vector during setup.

---

## Known limitations

- The Paris scene only has 8 streets connecting outer intersections to the center. Vehicles that reach the center pick from available outgoing streets — but since all streets point inward, they effectively reverse direction on the same street every cycle.
- No collision detection or minimum following distance between vehicles.
- `base_speed_` is in pixels/second relative to the original image resolution (3000+ px wide), so it looks fast on the resized display. Tweak it in `Vehicle.h` if needed.
