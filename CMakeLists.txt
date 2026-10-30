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

add_library(band3 STATIC band3/three_band_filter_bank.cc)
target_include_directories(band3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/band3)
target_compile_features(band3 PUBLIC cxx_std_20)

add_executable(band3_chirp sim/band3/band3_chirp.cc)
target_link_libraries(band3_chirp PRIVATE band3)
target_compile_features(band3_chirp PRIVATE cxx_std_20)
