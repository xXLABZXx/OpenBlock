# OpenBlox prototype build.
# Embeds the genuine Luau VM (MIT, third_party/luau) and builds the
# OpenBlox host: DataModel, services, events, task scheduler, sandbox.
cmake_minimum_required(VERSION 3.16)
project(OpenBlox VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build only what the host needs from Luau: the VM and the compiler.
# (No CLI tools, no test suite, no analyzer.)
set(LUAU_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LUAU_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUAU_BUILD_WEB OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/luau)

add_executable(openblox
    src/main.cpp
    src/rbx.cpp
    src/scheduler.cpp
    src/bindings.cpp
)

target_include_directories(openblox PRIVATE
    src
    third_party/luau/VM/include
    third_party/luau/Compiler/include
)

target_link_libraries(openblox PRIVATE Luau.VM Luau.Compiler)

if (MSVC)
    target_compile_options(openblox PRIVATE /W4)
else()
    target_compile_options(openblox PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# Convenience: `cmake --build build && ./build/openblox`
