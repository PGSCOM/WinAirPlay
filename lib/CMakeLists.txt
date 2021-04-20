cmake_minimum_required(VERSION 3.4.1)
include_directories( playfair llhttp plist/include aac_decoder h264_decoder)
include_directories(../renderers
          ../renderers/fdk-aac
          ../renderers/fdk-aac/include/libAACdec
          ../renderers/fdk-aac/include/libAACenc
          ../renderers/fdk-aac/include/libArithCoding
          ../renderers/fdk-aac/include/libDRCdec
          ../renderers/fdk-aac/include/libFDK
          ../renderers/fdk-aac/include/libMpegTPDec
          ../renderers/fdk-aac/include/libMpegTPEnc
          ../renderers/fdk-aac/include/libPCMutils
          ../renderers/fdk-aac/include/libSACdec
          ../renderers/fdk-aac/include/libSACenc
          ../renderers/fdk-aac/include/libSBRdec
          ../renderers/fdk-aac/include/libSBRenc
          ../renderers/fdk-aac/libSYS/include
          ../renderers/fdk-aac/win32
          )

aux_source_directory(. play_src)
set(DIR_SRCS ${play_src})

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

add_library( airplay
        STATIC
        ${DIR_SRCS}
        )

target_link_libraries(airplay
      Threads::Threads
      OpenSSL::SSL 
      OpenSSL::Crypto
      playfair
      llhttp
      plist 
      ws2_32
      aac_decoder
      fdkaac_decoder
      h264_decoder
      pthreads)

#if( UNIX AND NOT APPLE )
#  find_package(OpenSSL 1.1.1 REQUIRED)
#  target_compile_definitions(airplay PUBLIC OPENSSL_API_COMPAT=0x10101000L)
#  target_link_libraries( airplay OpenSSL::Crypto )
#  target_link_libraries( airplay dns_sd )
#else()
#  include_directories( /usr/local/opt/openssl@1.1/include/ )
#  target_link_libraries( airplay /usr/local/opt/openssl@1.1/lib/libcrypto.a )
#endif()
        
