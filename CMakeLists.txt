cmake_minimum_required(VERSION 3.20)
project(Bring VERSION 0.1.0 LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


include(cmake/CPM.cmake)

CPMAddPackage("gh:catchorg/Catch2@3.4.0")




find_program(CLANG_TIDY_EXE NAMES "clang-tidy")

if(CLANG_TIDY_EXE)
  message(STATUS "Clang-tidy found: ${CLANG_TIDY_EXE}")
  set(CMAKE_CXX_CLANG_TIDY
        ${CLANG_TIDY_EXE}
        "-config-file=${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy"
        "--extra-arg=-Wno-unknown-warning-option"
    )
else()
  message(WARNING "Clang-tidy not found. Static analysis disabled.")
endif()


if(BUILD_TESTING) # Standard CMake flag
  add_executable(unit_tests tests/test_main.cpp)
  target_link_libraries(unit_tests PRIVATE bring::bring Catch2::Catch2WithMain)

  # This enables Clang-Tidy for the test executable too
  set_target_properties(unit_tests PROPERTIES CXX_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY}")

  add_executable(benchmark tests/benchmark.cpp)
  target_link_libraries(benchmark PRIVATE bring::bring)
  set_target_properties(benchmark PROPERTIES CXX_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY}")

  add_executable(mt_tests tests/test_multithreaded.cpp)
  target_link_libraries(mt_tests PRIVATE bring::bring Catch2::Catch2WithMain)
  set_target_properties(mt_tests PROPERTIES CXX_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY}")
  find_package(Threads REQUIRED)
  target_link_libraries(mt_tests PRIVATE Threads::Threads)

  # Only build fuzzer on Linux with Clang (not macOS - fuzzer runtime not available)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND UNIX AND NOT APPLE AND NOT CMAKE_CXX_FLAGS MATCHES "-fsanitize")
    message(STATUS "Linux + Clang detected without external sanitizers: Enabling Fuzzing target")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/fuzzer_test.cpp")
      add_executable(fuzzer_test tests/fuzzer_test.cpp)
      target_compile_options(fuzzer_test PRIVATE -fsanitize=fuzzer,address,undefined)
      target_link_options(fuzzer_test PRIVATE -fsanitize=fuzzer,address,undefined)
      target_link_libraries(fuzzer_test PRIVATE bring::bring)
    endif()
  endif()
endif()

if(BUILD_BENCHMARKS)
  CPMAddPackage(
    NAME benchmark
    GITHUB_REPOSITORY google/benchmark
    GIT_TAG v1.8.3
    OPTIONS
      "BENCHMARK_ENABLE_TESTING OFF"
      "BENCHMARK_ENABLE_GTEST_TESTS OFF"
      "BENCHMARK_ENABLE_INSTALL OFF"
  )

  add_executable(benchmarks benchmarks/bench_main.cpp)
  target_link_libraries(benchmarks PRIVATE
        bring::bring
        benchmark::benchmark
    )

  # Optimization is mandatory for benchmarks
  if(MSVC)
    target_compile_options(benchmarks PRIVATE /O2)
  else()
    target_compile_options(benchmarks PRIVATE -O3)
  endif()
endif()


add_library(bring INTERFACE)
add_library(bring::bring ALIAS bring)

target_include_directories(bring INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(MSVC)
  target_compile_options(bring INTERFACE /W4 /WX /wd4324)
else()
  target_compile_options(bring INTERFACE -Wall -Wextra
  -Wpedantic -Wshadow -Werror)
endif()

# Installation
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS bring
    EXPORT BringTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT BringTargets
    FILE BringTargets.cmake
    NAMESPACE bring::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Bring
)

# Create package version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/BringConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Create package config file
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/BringConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/BringConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Bring
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/BringConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/BringConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Bring
)

# Install LICENSE and README
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
    "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

