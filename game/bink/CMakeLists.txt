# Bink compat library (binkw32.dll replacement on FFmpeg) and the bink_play tool.
# FFmpeg is built minimal by tools/ffmpeg/build-ffmpeg.sh; FFMPEG_PREFIX = its install dir.
set(FFMPEG_PREFIX "" CACHE PATH "install prefix of an already built minimal FFmpeg; empty: build the fetched one")
set(BINK_DIR ${CMAKE_CURRENT_LIST_DIR})
set(FFMPEG_SRC ${CMAKE_CURRENT_LIST_DIR}/../../android/app/jni/ffmpeg)    # fetched by fetch-deps.sh
if(NOT FFMPEG_PREFIX AND EXISTS ${FFMPEG_SRC}/configure)
  # Build the pinned FFmpeg here (desktop, or the NDK's clang for Android arm64)
  include(ExternalProject)
  set(FFMPEG_PREFIX ${CMAKE_BINARY_DIR}/ffmpeg-install)
  set(FF_ARGS "")
  if(ANDROID)
    set(FF_ARGS android ${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/${CMAKE_ANDROID_NDK_TOOLCHAIN_HOST_TAG}/bin ${CMAKE_SYSTEM_VERSION})
  endif()
  ExternalProject_Add(ffmpeg_ep
    SOURCE_DIR ${FFMPEG_SRC}
    DOWNLOAD_COMMAND "" UPDATE_COMMAND "" CONFIGURE_COMMAND "" INSTALL_COMMAND ""
    BUILD_COMMAND bash ${CMAKE_CURRENT_LIST_DIR}/../../tools/ffmpeg/build-ffmpeg.sh ${FFMPEG_SRC}
                  ${CMAKE_BINARY_DIR}/ffmpeg-build ${FFMPEG_PREFIX} ${FF_ARGS}
    BUILD_BYPRODUCTS ${FFMPEG_PREFIX}/lib/libavformat.so ${FFMPEG_PREFIX}/lib/libavcodec.so ${FFMPEG_PREFIX}/lib/libavutil.so)
  file(MAKE_DIRECTORY ${FFMPEG_PREFIX}/include)
  set(BINK_NEED_EP ON)
elseif(NOT FFMPEG_PREFIX)
  message(STATUS "bink: no FFmpeg (run fetch-deps.sh or set FFMPEG_PREFIX), not building the Bink compat library")
  return()
endif()
add_library(avformat SHARED IMPORTED)
add_library(avcodec SHARED IMPORTED)
add_library(avutil SHARED IMPORTED)
foreach(l avformat avcodec avutil)
  set_target_properties(${l} PROPERTIES IMPORTED_LOCATION ${FFMPEG_PREFIX}/lib/lib${l}.so
                        INTERFACE_INCLUDE_DIRECTORIES ${FFMPEG_PREFIX}/include)
endforeach()
set_target_properties(avformat PROPERTIES INTERFACE_LINK_LIBRARIES "avcodec;avutil")
set_target_properties(avcodec PROPERTIES INTERFACE_LINK_LIBRARIES "avutil")

add_library(bink STATIC ${BINK_DIR}/bink.cpp)
target_include_directories(bink PUBLIC ${BINK_DIR} ${SDL2_INCLUDE_DIRS})
if(BINK_NEED_EP)
  add_dependencies(bink ffmpeg_ep)
endif()
target_link_libraries(bink PUBLIC avformat avcodec avutil ${OMT_SDL})
if(NOT ANDROID)
  add_executable(bink_play ${BINK_DIR}/bink_play.cpp)
  target_link_libraries(bink_play bink)
  set_target_properties(bink_play PROPERTIES BUILD_RPATH "${FFMPEG_PREFIX}/lib")
endif()
