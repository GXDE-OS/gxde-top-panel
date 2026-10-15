# Check the application's own imports, not its third-party libraries.
execute_process(
    COMMAND "${READELF}" --dyn-syms --wide "${PANEL_BINARY}"
    RESULT_VARIABLE status
    OUTPUT_VARIABLE symbols
    ERROR_VARIABLE error)
if (NOT status EQUAL 0)
    message(FATAL_ERROR "Cannot inspect panel imports: ${error}")
endif()

# Native interfaces are public, but Qt gives them no ABI guarantee either.
if (symbols MATCHES "Qt_[0-9][^ \n]*_PRIVATE_API|QNativeInterface")
    message(FATAL_ERROR "The panel imports an unstable Qt API: ${CMAKE_MATCH_0}")
endif()
