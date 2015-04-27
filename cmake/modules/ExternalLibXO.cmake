if (CMAKE_SYSTEM_NAME MATCHES "*BSD*")
  set(GMAKE "gmake")
else()
  set(GMAKE "make")
endif()

message(STATUS "Building external libxo ${SYSTEM_XO_VERSION}")
include(ExternalProject)
ExternalProject_Add(libxo
  GIT_REPOSITORY https://github.com/Juniper/libxo.git
  GIT_TAG 0.1.6
  BUILD_IN_SOURCE 1
  CONFIGURE_COMMAND rm -rf configure m4
                    COMMAND sh bin/setup.sh
                    COMMAND cd build
                    COMMAND ../configure --prefix=<INSTALL_DIR>
  BUILD_COMMAND cd <SOURCE_DIR>/build COMMAND ${GMAKE}
  INSTALL_COMMAND cd <SOURCE_DIR>/build COMMAND ${GMAKE} install
  STEP_TARGETS build install
)

ExternalProject_Get_Property(libxo install_dir)
set(XO_PREFIX ${install_dir})
