# Native build for the Android app: SDL2 (shared) + gl4es (static) + OMT engine + viewer -> libmain.so
# SDL/ and gl4es/ are fetched by fetch-deps.sh (pinned versions in deps.lock).
cmake_minimum_required(VERSION 3.16)
project(negatron C CXX)

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/SDL/CMakeLists.txt OR NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/gl4es/CMakeLists.txt OR NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unshield/lib/libunshield.h)
  message(FATAL_ERROR "SDL, gl4es or unshield sources are missing - run ./fetch-deps.sh from the repo root first")
endif()

# --- SDL2 (shared libSDL2.so, loaded by SDLActivity)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory(SDL)

# --- gl4es: OpenGL 1.x -> OpenGL ES 2.0, linked statically, initialised by the engine
set(STATICLIB ON CACHE BOOL "" FORCE)
set(NOX11 ON CACHE BOOL "" FORCE)
set(NOEGL ON CACHE BOOL "" FORCE)
set(NO_LOADER ON CACHE BOOL "" FORCE)
set(NO_INIT_CONSTRUCTOR ON CACHE BOOL "" FORCE)
set(USE_ANDROID_LOG ON CACHE BOOL "" FORCE)
add_subdirectory(gl4es)
set(GL4ES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/gl4es CACHE PATH "" FORCE)

# --- OMT engine + SDL platform layer + viewer (engine/CMakeLists.txt builds libmain.so on Android)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../engine engine)
