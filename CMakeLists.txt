cmake_minimum_required(VERSION 3.18)
project(oryon LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

file(GLOB ORYON_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp ${CMAKE_SOURCE_DIR}/src/gen/*.cpp)
add_library(oryon SHARED ${ORYON_SOURCES})
target_include_directories(oryon PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(oryon PRIVATE ORYON_VERSION="0.1.0")
target_compile_options(oryon PRIVATE
  -O2 -nostdinc++ -fvisibility=hidden -fvisibility-inlines-hidden
  -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-math-errno
  -ffunction-sections -fdata-sections
  -Wall -Wextra -Wno-unused-parameter)
# No C++ runtime (no STL, no libc++_shared.so): only libc/libm/libdl (+liblog on Android).
target_link_options(oryon PRIVATE
  -nostdlib++ -Wl,--gc-sections -Wl,-Bsymbolic -Wl,--exclude-libs,ALL
  -Wl,--no-undefined -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now)
if(ANDROID)
  target_link_libraries(oryon PRIVATE dl log m)
  target_link_options(oryon PRIVATE -Wl,-z,max-page-size=16384)
else()
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)
  target_link_libraries(oryon PRIVATE dl m Threads::Threads)
endif()
