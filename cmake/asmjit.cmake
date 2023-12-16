include(FetchContent)

set(LIB_NAME "asmjit")
message(STATUS "Setting up ${LIB_NAME}")

FetchContent_Declare(
    ${LIB_NAME}
    GIT_REPOSITORY https://github.com/asmjit/asmjit.git
    GIT_TAG        3577608cab0bc509f856ebf6e41b2f9d9f71acc4
	GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(${LIB_NAME})