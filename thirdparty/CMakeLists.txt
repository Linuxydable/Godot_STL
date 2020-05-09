
add_definitions(-w)

add_subdirectory(misc)
add_subdirectory(zlib)
add_subdirectory(minizip)
add_subdirectory(zstd)
add_subdirectory(mbedtls)

remove_definitions(-w)