# - Find libxo
# This module defines the following variables:
#     LIBXO_FOUND         - whether or not we found libxo
#     LIBXO_INCLUDE_DIRS  - include dirs for libxo
#     LIBXO_LIBRARIES     - libs needed to use libxo
#     LIBXO_PREFIX        - prefix (e.g., /usr/local) of all libxo files
#     LIBXO_VERSION       - version of libxo as reported by `xo` binary

find_path(LIBXO_PREFIX include/libxo/xo.h REQUIRED)
find_library(LIBXO_LIBRARY xo REQUIRED)

set(LIBXO_INCLUDE_DIR "${LIBXO_PREFIX}/include"
  CACHE STRING "Include directory for libxo header")
set(LIBXO_INCLUDE_DIRS "${LIBXO_INCLUDE_DIR}")
set(LIBXO_LIBRARIES "${LIBXO_LIBRARY}")

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBXO DEFAULT_MSG
  LIBXO_LIBRARY LIBXO_INCLUDE_DIR
)

execute_process(
  COMMAND ${LIBXO_PREFIX}/bin/xo --version
  OUTPUT_VARIABLE XO_OUT
  ERROR_VARIABLE XO_OUT
)

string(REGEX REPLACE
  ".*libxo version ([0-9.]+).*" "\\1" LIBXO_VERSION
  "${XO_OUT}"
)

mark_as_advanced (LIBXO_INCLUDE_DIR LIBXO_LIBRARY)
