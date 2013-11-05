#
# By default, we don't show details of test failures, but allow this
# to be overridden.
#
set(SHOW_TEST_STATS false CACHE BOOL "Show statistics after running tests")

if (SHOW_TEST_STATS)
  message(STATUS "Will show statistics after each test run")
  set(LIT_OPTIONS "-sv")
else ()
  message(STATUS "Will run successful tests quietly")
  set(LIT_OPTIONS "-qv")
endif ()


#
# If we have David's llvm-lit patch, we can export XUnit-style XML to be
# read by continuous integration tools like Jenkins.
#
set(EXPORT_XUNIT_XML false CACHE BOOL
    "Export test results as XUnit XML (requires David's lit patches")

if (EXPORT_XUNIT_XML)
  message(STATUS "Will export test results XUnit XML")
  set(LIT_EXTRA --junit-xml-output=test-results.xml)
else ()
  message(STATUS "Will not export test results XUnit XML")
endif ()


#
# Have we set platform-specific CFLAGS for tests?
#
if (DEFINED TEST_C_FLAGS)
	set(LIT_EXTRA ${LIT_EXTRA} --param=extra_cflags=${TEST_C_FLAGS})
endif ()

add_custom_target(test
	COMMAND
		PATH=${LLVM_BINARY_DIR}/bin:$ENV{PATH} ${LIT}
		${LIT_OPTIONS} ${CMAKE_CURRENT_SOURCE_DIR}
		--param=build_dir=${CMAKE_BINARY_DIR}
		--param=source_dir=${CMAKE_SOURCE_DIR}
		--param=output_dir=${CMAKE_CURRENT_BINARY_DIR}
		${LIT_EXTRA}
	COMMENT "Running test suite")

add_dependencies(test libsoaap)
