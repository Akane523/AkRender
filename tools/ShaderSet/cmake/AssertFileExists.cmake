# Assert that FILE exists.  Used by ShaderSet CTest integration checks.
if(NOT DEFINED FILE)
    message(FATAL_ERROR "AssertFileExists.cmake: FILE not set")
endif()

if(NOT EXISTS "${FILE}")
    message(FATAL_ERROR "Expected file does not exist: ${FILE}")
endif()
