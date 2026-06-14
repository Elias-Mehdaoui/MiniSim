cmake_minimum_required(VERSION 3.31.0)
project(MiniSim LANGUAGES C CXX)

option(ENABLE_ASAN "Enable Address Sanitizer (ASan)" OFF)
option(ENABLE_UBSAN "Enable Undefined Behavior Sanitizer (UBSan)" OFF)
option(ENABLE_MSAN "Enable Memory Sanitizer (MSan)" OFF)

add_library(arena_allocator SHARED)
target_sources(arena_allocator PUBLIC src/memory/arena_allocator.c)
target_include_directories(arena_allocator PUBLIC
        include/memory/
)
set_target_properties(arena_allocator PROPERTIES
        C_STANDARD 23
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
)

set(COMPILER_WARNINGS
        -Wall -Wextra -Werror -Wshadow -Winline
        -Wmissing-declarations -Wundef -Wnull-dereference
        -Wuninitialized -Wpadded -Wvla -pedantic
)


target_compile_options(arena_allocator PRIVATE
        ${COMPILER_WARNINGS}
        -Wstrict-prototypes
        -Wmissing-prototypes
        -Wold-style-definition
        -fno-plt
        -fstack-protector-strong
)

add_library(minisim)
target_sources(minisim PUBLIC src/engine/MiniSim.cpp)
target_include_directories(minisim PUBLIC
        include/engine/
)
set_target_properties(minisim PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
)
target_compile_options(minisim PUBLIC
        ${COMPILER_WARNINGS}
        -fno-plt
        -fstack-protector-strong
)

target_link_libraries(minisim PUBLIC arena_allocator)


set(MAIN_SRC
        src/main.cpp
)
add_executable(main ${MAIN_SRC})
set_target_properties(main PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
)

target_link_libraries(main PUBLIC
        arena_allocator
        minisim
)

target_compile_options(main PUBLIC
        ${COMPILER_WARNINGS}
        -fno-plt
        -fstack-protector-strong
        $<$<CONFIG:Release>:-O3 -march=native -mtune=native -fno-math-errno -ftree-vectorize>
        $<$<CONFIG:Debug>:-O1 -g3 -fno-omit-frame-pointer>
)

target_link_options(main PUBLIC
        ${COMPILER_WARNINGS}
        -fno-plt
        -fstack-protector-strong
)

if (ENABLE_ASAN OR ENABLE_UBSAN OR ENABLE_MSAN)
    if (ENABLE_MSAN AND ENABLE_ASAN)
        message(FATAL_ERROR "AddressSanitizer (ASan) et MemorySanitizer (MSan) ne peuvent pas être activés en même temps.")
    endif ()

    set(SANITIZER_FLAGS "")

    if (ENABLE_ASAN)
        list(APPEND SANITIZER_FLAGS "-fsanitize=address")
    endif ()

    if (ENABLE_UBSAN)
        list(APPEND SANITIZER_FLAGS "-fsanitize=undefined")
    endif ()

    if (ENABLE_MSAN)
        list(APPEND SANITIZER_FLAGS "-fsanitize=memory")
    endif ()

    list(APPEND SANITIZER_FLAGS "-fno-omit-frame-pointer" "-g")

    target_compile_options(arena_allocator PRIVATE ${SANITIZER_FLAGS})
    target_link_options(arena_allocator PRIVATE ${SANITIZER_FLAGS})
    target_compile_options(minisim PRIVATE ${SANITIZER_FLAGS})
    target_link_options(minisim PRIVATE ${SANITIZER_FLAGS})
    target_compile_options(main PRIVATE ${SANITIZER_FLAGS})
    target_link_options(main PRIVATE ${SANITIZER_FLAGS})

    enable_testing()

    include(FetchContent)

    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.17.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    set(TEST_SRC
            test/test_minisim.cpp
    )

    add_executable(minisim_test ${TEST_SRC})
    set_target_properties(minisim_test PROPERTIES
            CXX_STANDARD 23
            CXX_STANDARD_REQUIRED ON
    )

    target_link_libraries(minisim_test PUBLIC
            arena_allocator
            minisim
            GTest::gtest_main
    )

    target_compile_options(minisim_test PRIVATE ${SANITIZER_FLAGS})
    target_link_options(minisim_test PRIVATE ${SANITIZER_FLAGS})

    include(GoogleTest)
    gtest_discover_tests(minisim_test)
endif ()

