cmake_minimum_required(VERSION 3.4.1)
project(aac_decoder C CXX)

set(MFAAC_HEADER
    aac_decoder.h
)

set(MFAAC_SRC
    aac_decoder.cpp
)

add_library(aac_decoder ${MFAAC_SRC} ${MFAAC_HEADER})
