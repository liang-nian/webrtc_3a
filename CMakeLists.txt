cmake_minimum_required(VERSION 3.16)
project(webrtc_3a LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(qmf STATIC qmf/splitting_filter.c)
target_include_directories(qmf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/qmf)

add_executable(qmf_chirp sim/qmf/qmf_chirp.cc)
target_link_libraries(qmf_chirp PRIVATE qmf)
