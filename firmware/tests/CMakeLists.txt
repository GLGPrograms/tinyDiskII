cmake_minimum_required(VERSION 3.24)

project(tinyDiskII-tests)

add_executable(tinyDiskII-tests
  test_runner.cpp

  test_fat_filename.cpp
  
  # Testing sdcard_fat.cpp
  ../src/sdcard_fat.cpp
  test_fat.cpp
  
  test_gui.cpp
)

# SdCard image file must be copied in build directory
file(COPY SAMPLEDISK.bin DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME tinyDiskII-tests
        COMMAND tinyDiskII-tests)
