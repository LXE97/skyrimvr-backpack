cmake_minimum_required(VERSION 3.21)

project(BackpackVR VERSION 1.0.1 LANGUAGES CXX)

# Destination to copy .dll
if(DEFINED ENV{SKYRIM_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_FOLDER}/Data")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_FOLDER}/Data/SKSE/Plugins")
endif()

file(GLOB_RECURSE source_files src/*.cpp external/*.cpp)

find_package(CommonLibSSE CONFIG REQUIRED)
add_commonlibsse_plugin(${PROJECT_NAME} SOURCES ${source_files} main.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_precompile_headers(${PROJECT_NAME} PRIVATE PCH.h)
target_include_directories(${PROJECT_NAME} PRIVATE include external)

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
    message(STATUS "SKSE plugin output folder: ${DLL_FOLDER}")

    add_custom_command(
        TARGET "${PROJECT_NAME}"
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${OUTPUT_FOLDER}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${OUTPUT_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
        VERBATIM
    )
endif()
