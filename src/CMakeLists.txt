FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)
FILE(GLOB_RECURSE binary_files ${CMAKE_SOURCE_DIR}/binary/*)

idf_component_register(SRCS ${app_sources} EMBED_FILES ${binary_files})