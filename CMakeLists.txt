cmake_minimum_required(VERSION 3.16)
project(traffic_simulation VERSION 1.0.0 LANGUAGES CXX)

option(BUILD_GUI "Build the OpenCV visualization executable"  ON)
option(BUILD_TESTS "Build unit tests (requires GTest)"          OFF)
option(BUILD_BENCHMARKS "Build benchmarks (requires Google Benchmark)" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)


# MSVC: generate a dummy pthread.lib to satisfy opencv's dependency
if(MSVC)
    file(WRITE ${CMAKE_BINARY_DIR}/dummy_pthread.cpp "")
    add_library(pthread STATIC ${CMAKE_BINARY_DIR}/dummy_pthread.cpp)
    link_directories(${CMAKE_BINARY_DIR}/$<CONFIG>)
endif()


add_library(project_warnings INTERFACE)
target_compile_options(project_warnings INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4
        /w14265
        /w14263
    >
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
        -Wall
        -Wextra
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
    >
)


add_library(traffic_core STATIC
    src/TrafficObject.cpp
    src/TrafficLight.cpp
    src/Street.cpp
    src/Intersection.cpp
    src/Vehicle.cpp
)

target_compile_features(traffic_core PUBLIC cxx_std_20)
target_include_directories(traffic_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(traffic_core
    PUBLIC  Threads::Threads
    PRIVATE project_warnings
)


if(BUILD_GUI)
    find_package(OpenCV 4.1 REQUIRED)
    message(STATUS "OpenCV version : ${OpenCV_VERSION}")
    message(STATUS "OpenCV_DIR : ${OpenCV_DIR}")

    add_executable(traffic_simulation
        src/main.cpp
        src/Graphics.cpp
    )

    target_compile_features(traffic_simulation PRIVATE cxx_std_20)
    target_compile_definitions(traffic_simulation PRIVATE
        PROJECT_DATA_DIR="${CMAKE_SOURCE_DIR}/data"
    )
    target_include_directories(traffic_simulation PRIVATE
        ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(traffic_simulation PRIVATE
        traffic_core
        ${OpenCV_LIBRARIES}
        Threads::Threads
        project_warnings
    )

    # MSVC: copy the OpenCV world DLL next to the executable after build
    if(MSVC)
        add_custom_command(TARGET traffic_simulation POST_BUILD
            COMMAND powershell -ExecutionPolicy Bypass -NoProfile -Command
                "Get-ChildItem -Path '${OpenCV_DIR}' -Filter 'opencv_world*.dll' -Recurse \
                 | Copy-Item -Destination '$<TARGET_FILE_DIR:traffic_simulation>' -Force"
            COMMENT "Copying OpenCV DLL to output directory..."
        )
    endif()
endif()


if(BUILD_TESTS)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(traffic_tests
        tests/test_message_queue.cpp
        tests/test_traffic_light.cpp
        tests/test_waiting_vehicles.cpp
        tests/test_intersection.cpp
        tests/test_vehicle.cpp
    )

    target_compile_features(traffic_tests PRIVATE cxx_std_20)
    target_link_libraries(traffic_tests PRIVATE
        traffic_core
        GTest::gtest_main
        project_warnings
    )

    include(GoogleTest)
    gtest_discover_tests(traffic_tests)
endif()


if(BUILD_BENCHMARKS)
    include(FetchContent)
    FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googlebenchmark)

    add_executable(traffic_benchmarks
        benchmarks/bench_message_queue.cpp
        benchmarks/bench_simulation_scaling.cpp
    )

    target_compile_features(traffic_benchmarks PRIVATE cxx_std_20)
    target_link_libraries(traffic_benchmarks PRIVATE
        traffic_core
        benchmark::benchmark
        benchmark::benchmark_main
        project_warnings
    )

endif()
