cmake_minimum_required(VERSION 3.4.1)
project(fdkaac_decoder C CXX)
include_directories(../../renderers
					../../renderers/fdk-aac
					../../renderers/fdk-aac/include/libAACdec
					../../renderers/fdk-aac/include/libAACenc
					../../renderers/fdk-aac/include/libArithCoding
					../../renderers/fdk-aac/include/libDRCdec
					../../renderers/fdk-aac/include/libFDK
					../../renderers/fdk-aac/include/libMpegTPDec
					../../renderers/fdk-aac/include/libMpegTPEnc
					../../renderers/fdk-aac/include/libPCMutils
					../../renderers/fdk-aac/include/libSACdec
					../../renderers/fdk-aac/include/libSACenc
					../../renderers/fdk-aac/include/libSBRdec
					../../renderers/fdk-aac/include/libSBRenc
					../../renderers/fdk-aac/libSYS/include
					../../renderers/fdk-aac/win32
					)

set(FDKAAC_HEADER
    ../../renderers/fdkaac_decoder.h
)

set(FDKAAC_SRC
    ../../renderers/fdkaac_decoder.cpp
)

add_library(fdkaac_decoder ${FDKAAC_SRC} ${FDKAAC_HEADER})
