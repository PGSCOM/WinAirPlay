cmake_minimum_required(VERSION 3.4.1)
project(h264_decoder C CXX)

set(MFH264_HEADER
    aligned_malloc.h
    common_types.h
    constructor_magic.h
    encoded_image.h
    h264_decoder_mf.h
    h264_decoder_mf_impl.h
    i420_buffer.h
    i420_buffer_pool.h
    ref_count.h
    ref_counted_object.h
    ref_counter.h
    scoped_refptr.h
    video_content_type.h
    video_error_codes.h
    video_frame_buffer.h
    video_frame_type.h
    video_rotation.h
    video_timing.h
)

set(MFH264_SRC
    aligned_malloc.cc
    encoded_image.cc
    h264_decoder_mf_impl.cc
    i420_buffer.cc
    i420_buffer_pool.cc
    video_content_type.cc
    video_frame_buffer.cc
)

add_library(h264_decoder ${MFH264_SRC} ${MFH264_HEADER})
