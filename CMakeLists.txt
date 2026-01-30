cmake_minimum_required(VERSION 3.16)
project(ChaCha20AEAD VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(demo_exe demo/rfc_vector.cpp)

target_include_directories(demo_exe PRIVATE 
    ${PROJECT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(demo_exe PRIVATE /arch:AVX2)
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT result OUTPUT error)
if(result)
    set_target_properties(demo_exe PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(STATUS "IPO/LTO nao suportado: ${error}")
endif()
