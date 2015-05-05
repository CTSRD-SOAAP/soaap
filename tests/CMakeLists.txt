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

# Export test results to XUnit XML format.
set(LIT_EXTRA --xunit-xml-output=test-results.xml)

#
# Have we set platform-specific CFLAGS for tests?
#
if (DEFINED TEST_C_FLAGS)
	set(LIT_EXTRA ${LIT_EXTRA} --param=extra_cflags=${TEST_C_FLAGS})
endif ()

add_custom_command(OUTPUT lit.site.cfg
        COMMAND clang -v 2>&1
		| grep Target
		| awk '{print $2}'
	        | sh -c 'read T\; echo config.target_triple=\\"$$T\\"'
	        > lit.site.cfg
	COMMENT "Checking host's LLVM triple..."
)

add_custom_target(soaap-test
	COMMAND
		PATH=${LLVM_BINARY_DIR}/bin:$ENV{PATH} ${LIT}
		${LIT_OPTIONS} ${CMAKE_CURRENT_SOURCE_DIR}
		--param=build_dir=${CMAKE_BINARY_DIR}
		--param=source_dir=${CMAKE_SOURCE_DIR}
		--param=output_dir=${CMAKE_CURRENT_BINARY_DIR}
		${LIT_EXTRA}
	DEPENDS lit.site.cfg
	COMMENT "Running test suite")

add_dependencies(soaap-test SOAAP)
