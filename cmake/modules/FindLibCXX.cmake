# - Find libc++
# Find libc++ if installed
#
# This module defines the following variables:
#     LIBCXX_FOUND         - whether or not we found libc++
#     LIBCXX_INCLUDE_DIRS  - include dirs for libc++ and (no) dependencies
#

if (LIBCXX_BUILD_DIR)
  set(LIBCXX_INCLUDE_DIR "${LIBCXX_BUILD_DIR}/include")
  set(LIBCXX_INCLUDE_DIRS
    "${LIBCXX_INCLUDE_DIR}"
    "${LIBCXX_SOURCE_DIR}/include")

  find_library(LIBCXX_LIBRARY c++
	PATHS "${LIBCXX_BUILD_DIR}/lib"
	NO_DEFAULT_PATH)

else()

  find_path(LIBCXX_PREFIX c++/v1/string REQUIRED
    DOC "container for versioned libc++ include directories"
    PATHS
      ${CMAKE_SYSTEM_PREFIX_PATH}
      /Library/Developer/CommandLineTools/usr/include
fi

  )

  if (LIBCXX_PREFIX)
    set(LIBCXX_INCLUDE_DIR "${LIBCXX_PREFIX}/c++/v1"
      CACHE STRING "Include directory for libc++ headers")

    set(LIBCXX_INCLUDE_DIRS "${LIBCXX_INCLUDE_DIR}" "${LIBCXX_PREFIX}")

    find_library(LIBCXX_LIBRARY c++)

  endif()

endif ()

set(LIBCXX_LIBRARIES "${LIBCXX_LIBRARY}")

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBCXX DEFAULT_MSG
  LIBCXX_LIBRARY
  LIBCXX_INCLUDE_DIRS
)

find_path(LIBCXX_CXXABI cxxabi.h
  DOC "C++ ABI declarations"
  PATHS ${LIBCXX_INCLUDE_DIR} ${CMAKE_SYSTEM_PREFIX_PATH}
)
if(NOT LIBCXX_CXXABI)
  # work around systems that have libc++ but only the libcstdc++ <cxxabi.h> header
  message(WARNING "libc++ found but required header cxxabi.h doesn't exist! libc++ does not seem to be installed correctly!")
  set(LIBCXX_FOUND FALSE)
endif()

mark_as_advanced (LIBCXX_INCLUDE_DIRS)
