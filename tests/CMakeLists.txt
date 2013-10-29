add_custom_target(test
	COMMAND
		${LIT} ${LIT_OPTIONS} ${CMAKE_CURRENT_SOURCE_DIR}
		--param=build_dir=${CMAKE_BINARY_DIR}
		--param=source_dir=${CMAKE_SOURCE_DIR}
		--param=output_dir=${CMAKE_BINARY_DIR}
		${LIT_EXTRA}
	COMMENT "Running test suite")

add_dependencies(test libsoaap)
