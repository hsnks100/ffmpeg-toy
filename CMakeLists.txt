# 요구 CMake 최소 버전
CMAKE_MINIMUM_REQUIRED ( VERSION 2.8 )
 
# 프로젝트 이름 및 버전
PROJECT ( "toy" )
SET ( PROJECT_VERSION_MAJOR 0 )
SET ( PROJECT_VERSION_MINOR 1 )
 
# 빌드 형상(Configuration) 및 주절주절 Makefile 생성 여부
SET ( CMAKE_BUILD_TYPE Release )
SET ( CMAKE_VERBOSE_MAKEFILE true )
 
# 빌드 대상 바이너리 파일명 및 소스 파일 목록
SET ( OUTPUT_ELF
        ../${CMAKE_PROJECT_NAME}-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.out
        )
SET ( SRC_FILES 
    sample_encoding.c
    #FileWriter.cpp
    #FFFileWriter.cpp
    #main.cpp
    #demux.c
    #remux.c
    #avio_reading.c
    #metadata.c
    #muxing.c
    #qsvdec.c
    #remuxing.c
    #gitmain.c
    #resampling_audio.c
    #scaling_video.c
    #transcode_aac.c
    #transcoding.c
    #http_multiclient.c
    #hw_decode.c
    #filter_audio.c
    #filtering_audio.c
    #filtering_video.c
    #decode_audio.c
    #decode_video.c
    #demuxing_decoding.c
    #encode_audio.c
    #encode_video.c
    #extract_mvs.c
    #avio_dir_cmd.c
        )



# 공통 컴파일러
#SET ( CMAKE_C_COMPILER "arm-hisiv400-linux-gcc" )
#SET ( CMAKE_CXX_COMPILER "arm-hisiv400-linux-g++" ) 
#SET(CMAKE_STRIP "arm-hisiv400-linux-strip")
 



# 공통 컴파일 옵션, 링크 옵션
ADD_DEFINITIONS ( -DRELEASE -DC_FLAGS )
ADD_COMPILE_OPTIONS ( -g -Wall -fno-aggressive-loop-optimizations -Wall -fPIE -pie)

SET ( CMAKE_EXE_LINKER_FLAGS " ")

 
# 공통 링크 라이브러리 (-l)
LINK_LIBRARIES( -Wl,-Bstatic rt avdevice avformat avfilter avcodec swresample swscale avutil -Wl,-Bdynamic pthread dl z png bz2 lzma m) 
 #libjpeg libpng libprotobuf zlib  
#-lavdevice -lavformat -lavfilter -lavcodec -lswresample -lswscale -lavutil
 #-lbz2 -lm -lz -lpthread -lpostproc
#/home/ksoo/hisdk/mpp/lib/libmpi.a /home/ksoo/hisdk/mpp/lib/libive.a /home/ksoo/hisdk/mpp/lib/libmd.a /home/ksoo/hisdk/mpp/lib/libmpi.a /home/ksoo/hisdk/mpp/lib/libive.a /home/ksoo/hisdk/mpp/lib/libVoiceEngine.a /home/ksoo/hisdk/mpp/lib/libupvqe.a /home/ksoo/hisdk/mpp/lib/libdnvqe.a /home/ksoo/hisdk/mpp/lib/libisp.a /home/ksoo/hisdk/mpp/lib/libsns_ov9732.a /home/ksoo/hisdk/mpp/lib/lib_cmoscfg.a /home/ksoo/hisdk/mpp/lib/lib_iniparser.a /home/ksoo/hisdk/mpp/lib/lib_hiae.a /home/ksoo/hisdk/mpp/lib/lib_hiawb.a /home/ksoo/hisdk/mpp/lib/lib_hiaf.a /home/ksoo/hisdk/mpp/lib/lib_hidefog.a

# 공통 링크 라이브러리 디렉토리 (-L)
LINK_DIRECTORIES ( ./ffmpeg/lib  /usr/lib/x86_64-linux-gnu )
#TARGET_LINK_LIBRARIES(${OUTPUT_ELF} Bdynamic pthread )

# 공통 헤더 파일 Include 디렉토리 (-I)
INCLUDE_DIRECTORIES (ffmpeg/include )
 
# "Debug" 형상 한정 컴파일 옵션, 링크 옵션
SET ( CMAKE_C_FLAGS_DEBUG "-DDEBUG -DC_FLAGS" )
SET ( CMAKE_EXE_LINKER_FLAGS_DEBUG "-DDEBUG -DLINKER_FLAGS" )

SET ( CMAKE_CXX_FLAGS "-std=c++11" )
 
# "Release" 형상 한정 컴파일 옵션, 링크 옵션
SET ( CMAKE_C_FLAGS_RELEASE "" )
SET ( CMAKE_EXE_LINKER_FLAGS_RELEASE "-DRELEASE -DLINKER_FLAGS " )
 
# 출력 디렉토리
SET ( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BUILD_TYPE} )
SET ( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BUILD_TYPE}/lib )
SET ( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BUILD_TYPE}/lib )
 
# 빌드 대상 바이너리 추가
ADD_EXECUTABLE( ${OUTPUT_ELF} ${SRC_FILES} )


ADD_CUSTOM_COMMAND (
    TARGET ${OUTPUT_ELF}
    POST_BUILD

    COMMAND cp Release/${OUTPUT_ELF} ~/nfs/


    )
