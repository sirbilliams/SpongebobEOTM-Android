# port: writes OUT (neg_version.h) with the source tree's git commit (short hash), for the pause
# screen's credit line (game/src/port/BillsUi.cpp). Run at every
# build by the neg_version target; the file is rewritten only when the text changes.
set(commit "")
find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(COMMAND ${GIT_EXECUTABLE} -C ${SRC} rev-parse --short HEAD
                  OUTPUT_VARIABLE commit OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
if(NOT commit)
  set(commit "unknown")
endif()
set(text "#define NEG_GIT_COMMIT \"${commit}\"\n")
if(EXISTS ${OUT})
  file(READ ${OUT} old)
endif()
if(NOT "${old}" STREQUAL "${text}")
  file(WRITE ${OUT} "${text}")
endif()
