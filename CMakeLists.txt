cmake_minimum_required(VERSION 3.21)

project(BackpackVR VERSION 1.0.0 LANGUAGES CXX)

# Destination to copy .dll
if(DEFINED ENV{SKYRIM_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_FOLDER}")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_FOLDER}/${PROJECT_NAME}/SKSE/Plugins")
endif()

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_SE OFF CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE OFF CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR ON CACHE BOOL "" FORCE)
set(SKSE_SUPPORT_XBYAK ON CACHE BOOL "" FORCE)

add_subdirectory(lib/commonlibsse-ng)

file(GLOB_RECURSE source_files src/*.cpp external/*.cpp)

include(${CMAKE_CURRENT_SOURCE_DIR}/lib/commonlibsse-ng/cmake/CommonLibSSE.cmake)

add_commonlibsse_plugin(${PROJECT_NAME}
	SOURCES
		${source_files}
		main.cpp
)

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		CommonLibSSE::CommonLibSSE
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_precompile_headers(${PROJECT_NAME} PRIVATE PCH.h)
target_include_directories(${PROJECT_NAME} PRIVATE include external include/utils)

target_compile_options(
    ${PROJECT_NAME}
    PRIVATE
    /sdl # Enable Additional Security Checks
    /utf-8 # Set Source and Executable character sets to UTF-8
    /Zi # Debug Information Format

    /permissive- # Standards conformance
    /Zc:preprocessor # Enable preprocessor conformance mode

    "$<$<CONFIG:DEBUG>:>"
    "$<$<CONFIG:RELEASE>:/Zc:inline;/JMC-;/Ob3>"
)

target_link_options(
    ${PROJECT_NAME}
    PRIVATE
    "$<$<CONFIG:DEBUG>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>"
    "$<$<CONFIG:RELEASE>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>"
)

if(DEFINED OUTPUT_FOLDER)
    # Copy the SKSE plugin .dll files into the SKSE/Plugins/ folder
    message(STATUS "SKSE plugin output folder: ${OUTPUT_FOLDER}")

    add_custom_command(
        TARGET "${PROJECT_NAME}"
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${OUTPUT_FOLDER}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${OUTPUT_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
        VERBATIM
    )
endif()
