cmake_minimum_required(VERSION 3.21)

project(VR-backpack VERSION 0.0.1 LANGUAGES CXX)

# Destination to copy .dll
if(DEFINED ENV{SKYRIM_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_FOLDER}/Data")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_FOLDER}/Data/SKSE/Plugins")
endif()

file(GLOB_RECURSE source_files src/*.cpp include/*.cpp)

find_package(CommonLibSSE CONFIG REQUIRED)
add_commonlibsse_plugin(${PROJECT_NAME} SOURCES ${source_files} main.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_precompile_headers(${PROJECT_NAME} PRIVATE PCH.h)
target_include_directories(${PROJECT_NAME} PRIVATE include src)


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
