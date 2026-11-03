cmake_minimum_required(VERSION 3.20)

project(erased_id VERSION 0.1.0 LANGUAGES CXX)

add_library(erased_id INTERFACE
    include/erased_id/erased_id.hpp
    include/erased_id/id.hpp
    include/erased_id/id_generator.hpp
    include/erased_id/id_registry.hpp
    include/erased_id/integer_traits.hpp
)
add_library(erased_id::erased_id ALIAS erased_id)

target_include_directories(erased_id INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(erased_id INTERFACE cxx_std_20)

option(ERASED_ID_BUILD_TESTS "Build the erased_id unit tests" ON)
if(ERASED_ID_BUILD_TESTS)
    enable_testing()
    add_executable(erased_id_tests
        tests/test_id.cpp
    )
    target_link_libraries(erased_id_tests PRIVATE erased_id::erased_id)
    add_test(NAME erased_id_tests COMMAND erased_id_tests)
endif()
