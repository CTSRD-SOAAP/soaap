# Build libxo as an external project.
#
# This module defines the following variables:
#     LIBXO_INCLUDE_DIRS  - include dirs for libxo
#     LIBXO_LIBRARIES     - libs needed to use libxo
#     LIBXO_PREFIX        - prefix (e.g., /usr/local) of all libxo files
#     LIBXO_VERSION       - version of libxo fetched from GitHub

if (CMAKE_SYSTEM_NAME MATCHES ".*BSD")
  set(GMAKE "gmake")
else()
  set(GMAKE "make")
endif()

set(LIBXO_VERSION ${LIBXO_NEED_VERSION})

message(STATUS "Building external libxo ${LIBXO_VERSION}")
include(ExternalProject)
ExternalProject_Add(libxo
  GIT_REPOSITORY https://github.com/Juniper/libxo.git
  GIT_TAG ${LIBXO_VERSION}
  BUILD_IN_SOURCE 1
  CONFIGURE_COMMAND rm -rf configure m4
                    COMMAND sh bin/setup.sh
                    COMMAND cd build
                    COMMAND ../configure --prefix=<INSTALL_DIR> --libdir=<INSTALL_DIR>/lib
  BUILD_COMMAND cd <SOURCE_DIR>/build COMMAND ${GMAKE}
  INSTALL_COMMAND cd <SOURCE_DIR>/build COMMAND ${GMAKE} install
  PATCH_COMMAND patch < ${CMAKE_SOURCE_DIR}/patches/libxo.patch
  STEP_TARGETS build install
)

ExternalProject_Get_Property(libxo install_dir)
set(LIBXO_PREFIX ${install_dir})

set(LIBXO_INCLUDE_DIRS "${LIBXO_PREFIX}/include")
set(LIBXO_LIBRARIES "${LIBXO_PREFIX}/lib/libxo.so")
