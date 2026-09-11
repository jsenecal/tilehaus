cmake_minimum_required(VERSION 3.16)
project(tilehaus_firmware_tests CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/native)
enable_testing()

add_executable(deck_document_test deck_document_test.cpp)
target_compile_definitions(deck_document_test PRIVATE
  DECK_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/deck_basic.bin"
  DECK_V5_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/deck_v5_pages.bin"
  DECK_V6_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/deck_v6_grid.bin")
add_test(NAME deck_document_test COMMAND deck_document_test)

foreach(t grid_layout sensor_format slider_map toggle_state)
  add_executable(${t}_test ${t}_test.cpp)
  add_test(NAME ${t}_test COMMAND ${t}_test)
endforeach()
