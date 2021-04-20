cmake_minimum_required(VERSION 3.4.1)
project(pthreads C CXX)

include_directories(.)

add_definitions(-D__PTW32_RC_MSC
                -D__PTW32_STATIC_LIB
                -D__PTW32_BUILD
                -D__PTW32_ARCHx86
                -DHAVE_CONFIG_H)

set(PTHREADS_HEADER
    config.h
    context.h
    implement.h
    need_errno.h
    pthread.h
    sched.h
    semaphore.h
)

set(PTHREADS_SRC
    pthread.c
)

add_library(pthreads ${PTHREADS_SRC} ${PTHREADS_HEADER})
