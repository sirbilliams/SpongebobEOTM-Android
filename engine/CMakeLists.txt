cmake_minimum_required(VERSION 3.16)
project(omt_port CXX C)
set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_EXTENSIONS ON)

# --------------------------------------------------------------------------- sources
file(GLOB_RECURSE OMT_SRC OMTClasses/*.cpp)
list(FILTER OMT_SRC EXCLUDE REGEX "OMTClasses/OS/(MSWindows|MacOS)/")
file(GLOB SDL_SRC sdl/*.cpp)
file(GLOB OMT1_SRC omt1/*.cpp)          # OMT.dll's (2002) classes, rebuilt on the SDL layer
list(FILTER SDL_SRC EXCLUDE REGEX "OMediaSDLMain\\.cpp$")      # compiled into each app, see below
set(OMT_MAIN ${CMAKE_CURRENT_SOURCE_DIR}/sdl/OMediaSDLMain.cpp)
file(GLOB_RECURSE OMT_HDR_DIRS LIST_DIRECTORIES true OMTClasses/*)
set(OMT_INC "")
foreach(d ${OMT_HDR_DIRS})
  if(IS_DIRECTORY ${d} AND NOT d MATCHES "OS/(MSWindows|MacOS)")
    list(APPEND OMT_INC ${d})
  endif()
endforeach()

# --------------------------------------------------------------------------- options
option(OMT_GL4ES "Render through gl4es on an OpenGL ES 2 context (always on for Android)" OFF)
set(GL4ES_DIR "" CACHE PATH "gl4es source dir (for include/)")
set(GL4ES_LIB "" CACHE FILEPATH "gl4es static lib, if not built as the 'GL' target")
if(ANDROID)
  set(OMT_GL4ES ON)
endif()

# --------------------------------------------------------------------------- SDL2
# An existing target (Android: SDL added as a subproject first) or an installed package.
if(NOT TARGET SDL2::SDL2 AND NOT TARGET SDL2 AND NOT TARGET SDL2::SDL2-static)
  find_package(SDL2 REQUIRED)
endif()
if(TARGET SDL2::SDL2)
  set(OMT_SDL SDL2::SDL2)
elseif(TARGET SDL2)
  set(OMT_SDL SDL2)
elseif(TARGET SDL2::SDL2-static)
  set(OMT_SDL SDL2::SDL2-static)
else()
  set(OMT_SDL ${SDL2_LIBRARIES})
endif()

# --------------------------------------------------------------------------- GL
find_package(ZLIB REQUIRED)
if(OMT_GL4ES)
  if(TARGET GL)
    set(OMT_GL_LIBS GL dl m)              # gl4es added as a subproject
  else()
    set(OMT_GL_LIBS ${GL4ES_LIB} dl m)
  endif()
  set(OMT_GL_INC ${GL4ES_DIR}/include)
else()
  find_package(OpenGL REQUIRED)
  set(OMT_GL_LIBS OpenGL::GL)
  set(OMT_GL_INC "")
endif()

# --------------------------------------------------------------------------- engine
add_library(omt STATIC ${OMT_SRC} ${SDL_SRC} ${OMT1_SRC})
target_include_directories(omt PUBLIC ${OMT_INC} sdl omt1 ${SDL2_INCLUDE_DIRS} ${OMT_GL_INC})
target_include_directories(omt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../game/bink)     # bink_mixer.h
target_compile_options(omt PRIVATE -fpermissive -Wno-deprecated -Wno-write-strings -Werror=int-to-pointer-cast -fno-strict-aliasing)
target_link_libraries(omt PUBLIC ${OMT_SDL} ${OMT_GL_LIBS} ZLIB::ZLIB)
if(OMT_GL4ES)
  target_compile_definitions(omt PUBLIC OMT_USE_GL4ES)
endif()
if(ANDROID)
  target_link_libraries(omt PUBLIC android log)
endif()


# --------------------------------------------------------------------------- OMT 1.x engine layer
# engine/omt1: the OMT.dll (OMT 1.x, Aug 2002) classes sb2.exe uses, rebuilt from the DLL, on the
# shared OMT core (streams, database, math, messages) and an SDL/GL back end (sdl/OMediaSDLGfx.cpp).
# Its own include path: the OMT 2.5 world/graphics headers of the same names are not visible.
file(GLOB_RECURSE OMT1_SRC omt1/*.cpp)
set(OMT1_SHARED_DIRS Core Utility Math Messages Compression Stream/Core Stream/Class Stream/Formatted Stream/Database)
set(OMT1_SHARED_SRC "")
set(OMT1_INC "")
file(GLOB OMT1_DIRS LIST_DIRECTORIES true omt1/*)
foreach(d ${OMT1_DIRS})
  if(IS_DIRECTORY ${d})
    list(APPEND OMT1_INC ${d})
  endif()
endforeach()
foreach(d ${OMT1_SHARED_DIRS})
  file(GLOB s OMTClasses/${d}/*.cpp)
  list(APPEND OMT1_SHARED_SRC ${s})
  list(APPEND OMT1_INC ${CMAKE_CURRENT_SOURCE_DIR}/OMTClasses/${d})
endforeach()
add_library(omt1 STATIC ${OMT1_SRC} ${OMT1_SHARED_SRC} sdl/OMediaSDLCore.cpp sdl/OMediaSDLGfx.cpp)
target_include_directories(omt1 PUBLIC ${OMT1_INC} sdl ${SDL2_INCLUDE_DIRS} ${OMT_GL_INC})
target_compile_definitions(omt1 PUBLIC OMT1_ENGINE OMD_SDL_NO_GL_WRAP GL_GLEXT_PROTOTYPES)
target_compile_options(omt1 PRIVATE -Wno-deprecated -Wno-write-strings -Wno-multichar -Werror=int-to-pointer-cast -fno-strict-aliasing)
target_link_libraries(omt1 PUBLIC ${OMT_SDL} ${OMT_GL_LIBS} ZLIB::ZLIB)
if(OMT_GL4ES)
  target_compile_definitions(omt1 PUBLIC OMT_USE_GL4ES)
endif()

# --------------------------------------------------------------------------- game importer
# Installs the game files from the game's CD image (.iso): ISO 9660 + InstallShield cabinet
# (unshield, MIT, fetched by fetch-deps.sh). Android: libimporter.so for the setup screen.
set(UNSHIELD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../android/app/jni/unshield" CACHE PATH "unshield source dir")
if(EXISTS ${UNSHIELD_DIR}/lib/libunshield.h)
  file(GLOB US_SRC ${UNSHIELD_DIR}/lib/*.c)
  add_library(sbimport STATIC importer/SbImport.c ${US_SRC}
              ${UNSHIELD_DIR}/lib/convert_utf/ConvertUTF.c ${UNSHIELD_DIR}/lib/md5/md5c.c)
  target_include_directories(sbimport PUBLIC importer PRIVATE importer/config ${UNSHIELD_DIR}/lib ${UNSHIELD_DIR})
  target_compile_options(sbimport PRIVATE -w)
  set_target_properties(sbimport PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 99)
  target_link_libraries(sbimport PUBLIC ZLIB::ZLIB)
  if(ANDROID)
    add_library(importer SHARED importer/SbImportJNI.c)
    target_link_libraries(importer sbimport log)
  else()
    add_executable(sb_import importer/sb_import_main.c)
    target_link_libraries(sb_import sbimport)
  endif()
endif()

# --------------------------------------------------------------------------- Granny 1.2b replacement
# game/granny: the 26 granny.dll functions the game imports, reading the game's .grn files directly
# (docs/granny-replacement.md).  Its float arithmetic reproduces the DLL's bit for bit, which needs IEEE
# float/double operations exactly as written: no fused multiply-add, no x87 excess precision.
add_library(granny_compat STATIC ${CMAKE_CURRENT_SOURCE_DIR}/../game/granny/granny_compat.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../game/granny/granny_grn.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../game/granny/granny_bink.cpp)
target_include_directories(granny_compat PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../game/granny)

# game/bink: binkw32.dll replacement on a minimal FFmpeg (tools/ffmpeg/build-ffmpeg.sh), plus the bink_play tool
include(${CMAKE_CURRENT_SOURCE_DIR}/../game/bink/CMakeLists.txt)
target_compile_options(granny_compat PRIVATE -ffp-contract=off -fno-fast-math)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i.86|x86)$")
  target_compile_options(granny_compat PRIVATE -msse2 -mfpmath=sse)
endif()

# --------------------------------------------------------------------------- apps
# The reconstructed game (../game/src). MSVC's char is signed and the original code relies on it,
# so -fsigned-char (arm64's default char is unsigned).
file(GLOB_RECURSE SB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../game/src/*.cpp)
set(SB_FLAGS -fsigned-char -Wno-multichar -Wno-write-strings -Werror=narrowing -Wfloat-conversion -fno-strict-aliasing)

if(ANDROID)
  # SDLActivity loads libspongebob.so (the game) and calls SDL_main (OMediaSDLMain.cpp's main).
  if(SB_SRC)
    add_library(spongebob SHARED ${SB_SRC} ${OMT_MAIN})
    target_link_libraries(spongebob omt granny_compat)
    target_compile_options(spongebob PRIVATE ${SB_FLAGS})
  endif()
else()
  add_executable(omt_viewer viewer/OMTViewer.cpp ${OMT_MAIN})
  target_link_libraries(omt_viewer omt)
  target_compile_options(omt_viewer PRIVATE -w -fpermissive)

  add_executable(omt1_view tests/omt1_view.cpp ${OMT_MAIN})
  target_link_libraries(omt1_view omt1)
  target_compile_options(omt1_view PRIVATE -Wno-multichar)

  add_executable(omt_datatest tests/omt_datatest.cpp ${OMT_MAIN})
  target_link_libraries(omt_datatest omt)
  target_compile_options(omt_datatest PRIVATE -w -fpermissive)

  # OMT.dll's sound manager and input driver on SDL (omt1/): sound to the disk audio driver, mouse and touch to a listener
  add_executable(omt1_soundinput_test tests/omt1_soundinput_test.cpp ${OMT_MAIN})
  target_link_libraries(omt1_soundinput_test omt)
  target_compile_options(omt1_soundinput_test PRIVATE -w -fpermissive)

  # Bink movie audio through the sound manager's mixer
  if(TARGET bink)
    add_executable(omt1_bink_mixer_test tests/omt1_bink_mixer_test.cpp ${OMT_MAIN})
    target_link_libraries(omt1_bink_mixer_test omt bink)
    target_compile_options(omt1_bink_mixer_test PRIVATE -w -fpermissive)
    set_target_properties(omt1_bink_mixer_test PROPERTIES BUILD_RPATH "${FFMPEG_PREFIX}/lib")
  endif()

  # The reconstructed game, desktop build
  if(SB_SRC)
    add_executable(spongebob ${SB_SRC} ${OMT_MAIN})
    target_link_libraries(spongebob omt granny_compat)
    target_compile_options(spongebob PRIVATE ${SB_FLAGS})
  endif()

  # Script runner for comparing granny_compat with the real granny.dll (tools/granny/verify.sh)
  add_executable(grntest_compat ${CMAKE_CURRENT_SOURCE_DIR}/../tools/granny/grntest.c)
  target_compile_definitions(grntest_compat PRIVATE GRNTEST_COMPAT)
  target_link_libraries(grntest_compat granny_compat m)

  # OMT's own samples, built unmodified (after tools/portify.py) against the SDL layer
  foreach(s Cube Teapot)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/samples/${s}.cpp)
      add_executable(sample_${s} samples/${s}.cpp ${OMT_MAIN})
      target_link_libraries(sample_${s} omt)
      target_compile_options(sample_${s} PRIVATE -w -fpermissive)
    endif()
  endforeach()
endif()
