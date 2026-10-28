# Concurrent Traffic Simulation

![Simulation in action](assets/Recording_2026-03-03_151433.gif)

A multithreaded city traffic simulator written in C++20. Vehicles drive around a map, queue at intersections, and wait for traffic lights — all running concurrently across independent threads.

Built as a deep dive into C++ concurrency primitives: `std::jthread`, `std::stop_token`, promises/futures, atomic operations, and a hand-rolled thread-safe message queue.

---

## What it does

Vehicles spawn on streets and drive toward intersections. When they get close, they enter a FIFO queue and block until the intersection grants them entry — one at a time. If the traffic light is red, they wait for green before crossing. Once through, they pick a random outgoing street and keep going.

The whole thing renders live using OpenCV, with each vehicle getting its own color (derived from its ID) and intersections showing red/green based on their current light phase.

Two maps are included: Paris (star topology, all roads lead to a central plaza) and NYC (ring topology with a diagonal shortcut).

---

## Dependencies

- C++20 compiler with `std::jthread` support (GCC 10+, Clang 12+ with libstdc++ 10+ or libc++ 18+, MSVC 2022)
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

Both are downloaded at configure time with `FetchContent`, so they need git and network access rather than a local install.

---

## Project structure

```
src/
  TrafficObject      base class, owns jthreads, atomic position
  TrafficLight       toggles red/green every 4-6 s, exposes waitForGreen()
  MessageQueue       templated thread-safe queue used by TrafficLight
  Intersection       manages WaitingVehicles queue + processes entries
  Street             connects two intersections, length = pixel distance between them
  Vehicle            drives, queues, crosses, picks next street
  Graphics           OpenCV render loop
  main               scene setup + simulation launch
```

---

## Concurrency design

Each vehicle runs on its own `jthread`. Each intersection runs a queue processor thread. Each traffic light runs a cycle thread. No shared mutable state except where explicitly protected.

**Vehicle to Intersection handoff** uses a `std::promise`/`std::future` pair. The vehicle thread calls `addVehicleToQueue` directly and blocks on `ftr.wait()`. The intersection's processor thread fulfills the promise when it's the vehicle's turn, unblocking it, so the waiting vehicle doesn't poll. The processor itself checks the queue every millisecond, and so does the traffic light's cycle thread.

**Traffic light phase propagation** goes through `MessageQueue<TrafficLightPhase>`. The atomic current phase is the source of truth; queue messages only wake the waiter. `waitForGreen()` re-checks the phase after every message, so a stale message (red or green) can't release a vehicle at the wrong time.

**Stop propagation**: every blocking wait takes the thread's `std::stop_token`. `MessageQueue::receive(stop_token)` uses `std::condition_variable_any`, and a vehicle waiting in an intersection queue is removed by a `std::stop_callback`, which breaks its promise and wakes it. Each derived class (`Vehicle`, `Intersection`, `TrafficLight`) stops and joins its threads at the start of its own destructor, before the members those threads use are destroyed.

**Ownership**: streets own their intersections (`shared_ptr`), while intersections only keep `weak_ptr`s to their streets, so the graph has no reference cycle and is freed when the scene goes away. Vehicles never hold a `shared_ptr` to themselves on their own thread, so a vehicle is always destroyed, and joined, from outside its thread.

One subtle thing: `is_blocked_` on `Intersection` is an `std::atomic<bool>` rather than a mutex-protected flag. The queue processor sets it to `true` just before granting entry, and the admitted vehicle sets it back to `false` via `vehicleHasLeft()`. Only one vehicle holds the intersection at a time, so the two writers never race.

---

## Switching scenes

In `src/main.cpp`, swap `createParisScene` for `createNYCScene`; each scene sets its own background image. Vehicle count is the first argument. Each vehicle starts on its own street, so a count above the number of streets (8 for Paris, 7 for NYC) is capped with a warning.

---

## Known limitations

- The Paris scene only has 8 streets, each connecting an outer intersection to the center. A vehicle at the center picks one of the other 7 streets at random, but every outer intersection is a dead end, so the vehicle turns around there and comes straight back to the center.
- No collision detection or minimum following distance between vehicles.
- `base_speed_` is in pixels/second relative to the original image resolution (3000+ px wide), so it looks fast on the resized display. Tweak it in `src/Vehicle.h` if needed.
- Stopping a vehicle is only meant for shutdown: one stopped after being granted entry leaves its intersection blocked.
