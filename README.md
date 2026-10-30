# Concurrent Traffic Simulation

![Simulation in action](assets/Recording_2026-03-03_151433.gif)

A small traffic simulator in C++20 where every car, intersection and traffic light runs on its own thread. Cars drive along streets, line up at intersections, wait for a green light, cross, and pick a new street at random. OpenCV draws it all on top of a city map.

I wrote this mainly to get comfortable with the C++ concurrency toolbox: `std::jthread`, `std::stop_token`, promises and futures, atomics, condition variables. Getting cars to move was the easy part. Most of my time went into making the program shut down cleanly without hanging or touching freed memory, so a lot of what's below is about that.

## How it works

There are three kinds of threads:

- **Vehicles.** One thread per car. It advances the car along its street, and when the car reaches the end it asks the intersection to let it in.
- **Intersections.** One thread per intersection that looks at the waiting queue every millisecond and lets the first car in, but only if nobody else is currently inside.
- **Traffic lights.** One thread per light. It flips between red and green every 4 to 6 seconds (random each cycle).

### Getting into an intersection

When a car arrives, it creates a `std::promise<void>`, puts it in the intersection's queue, and blocks on the matching future. The intersection thread fulfills the front promise when it's that car's turn. So the car doesn't spin or poll, it just sleeps until it's woken up.

After being let in, the car still has to wait for green. The light pushes every phase change into a small `MessageQueue` (a mutex + condition variable wrapper I wrote), and the car waits on that. The light's current phase is also stored in an atomic, and that's what the car actually trusts. The queue is just the wake-up signal. That way a leftover "green" message sitting in the queue can't let a car through on red.

`is_blocked_` on the intersection is a plain `std::atomic<bool>`. The intersection thread sets it right before letting a car in, and that car clears it in `vehicleHasLeft()`. Only one car is inside at a time, so there are never two writers at once.

### Shutting down

This was the tricky part. When you close the window, `main` returns and everything gets destroyed while threads may still be blocked somewhere. A few things make that safe:

- Every blocking wait takes the thread's `std::stop_token`. `MessageQueue::receive` uses `std::condition_variable_any`, which can be woken by a stop request.
- A car that's stuck in an intersection queue registers a `std::stop_callback`. On stop, the callback removes the car's entry from the queue, which destroys its promise. A destroyed promise makes the future ready (with a `broken_promise` error), so the car wakes up and exits.
- Each class stops and joins its threads at the very start of its own destructor. Otherwise members like the waiting queue would be destroyed first while the thread is still using them.
- In `main`, vehicles are destroyed before intersections and streets, so no car outlives the road it's on.
- Streets own their intersections with `shared_ptr`, while intersections only hold `weak_ptr`s back to their streets. Without that there would be a reference cycle and nothing would ever get freed.

## Building and running

You'll need:

- A C++20 compiler with `std::jthread` (GCC 10+, Clang 12+ with libstdc++ 10+ or libc++ 18+, or MSVC 2022)
- CMake 3.16 or newer
- OpenCV 4.1 or newer

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
./traffic_simulation
```

On Windows with MSVC the executable ends up in `build/Release/`, and CMake copies the OpenCV DLL next to it. OpenCV's Windows build also expects a `pthread.lib`, so CMake creates an empty stand-in for it.

The map images live in `data/`. The path is baked in at compile time, so you can run the executable from anywhere. Close the window to quit.

To pick a map, change `createParisScene` to `createNYCScene` in `src/main.cpp`. Paris is a star (every road goes to the center), NYC is a ring with one diagonal. The number passed in is how many cars to spawn. Each car starts on its own street, so you can't have more cars than streets (8 in Paris, 7 in NYC). If you ask for more, it warns you and caps the count.

## Tests and benchmarks

Both are off by default. CMake downloads GoogleTest and Google Benchmark with `FetchContent`, so the first configure needs network access.

```bash
cmake .. -DBUILD_TESTS=ON -DBUILD_BENCHMARKS=ON
cmake --build . --parallel
ctest --output-on-failure
./traffic_benchmarks
```

(With MSVC, use `ctest -C Release` and look in `Release/` for the binaries.)

Besides the basic stuff (FIFO ordering, phase toggling, street lengths), the tests cover the cases that actually bit me: a stop request waking a blocked `receive`, a stop request waking a car stuck in a queue, and destroying a vehicle or traffic light mid-simulation without hanging. The benchmarks measure message queue throughput under contention, how the simulation scales with the number of cars and intersections, and how fast intersections can let cars in.

If you only want the core library and tests without OpenCV, add `-DBUILD_GUI=OFF`.

## Code layout

```
src/
  TrafficObject   base class: id, atomic position, owns the threads
  Vehicle         driving loop, queueing, picking the next street
  Intersection    waiting queue + the thread that lets cars in
  TrafficLight    red/green cycle, waitForGreen()
  MessageQueue    bounded thread-safe queue (header only)
  Street          connects two intersections, length = pixel distance
  Graphics        OpenCV drawing loop
  main            builds the scenes and starts everything
tests/            GoogleTest
benchmarks/       Google Benchmark
data/             map images
```

## Known issues

- In the Paris map every outer intersection is a dead end, so cars just bounce between the center and the edges. It works, it's just not very interesting traffic.
- Cars don't see each other. There's no collision check or following distance, so two cars on the same street can drive right through each other.
- Speeds are in pixels per second of the original image (3000+ px wide). The window shows a scaled-down version, so cars look fast. `base_speed_` in `src/Vehicle.h` is the knob.
- A car that gets stopped right after being let into an intersection leaves it marked as blocked. That only happens during shutdown, so I left it.
