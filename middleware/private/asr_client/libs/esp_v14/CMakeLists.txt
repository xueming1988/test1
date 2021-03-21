cmake_minimum_required(VERSION 2.8)

SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
SET(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")

project(VAD)

add_definitions( -DAEC_OPTZ_FFT_128 -DSTFT_AEC_OPTZ_FFT_256 -DFRAMEWORK_FW -DSDB_CVX_LKP_TBL)

add_executable(${PROJECT_NAME}
                 "main.cpp"
                ./VAD_Features/VAD_class.cpp
                ./VAD_Features/flagbuffer.cpp
                ./VAD_Features/minpeakclass.cpp
                ./VAD_Features/xcorr_class.cpp
                ./common/Radix4fft.c
                ./common/fixed_pt.cpp
                 )
include_directories("./VAD_Features"
                    "./common")
