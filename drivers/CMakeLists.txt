project(godotDriver)

if(${target} STREQUAL "x11")
    add_subdirectory(alsa)
    add_subdirectory(alsamidi)
endif()