project(godotDriver)

if(${target} STREQUAL "x11")
    add_subdirectory(alsa)
    add_subdirectory(alsamidi)
endif()

if(${target} STREQUAL "osx")
    add_subdirectory(coreaudio)
    add_subdirectory(coremidi)
endif()

if(${target} STREQUAL "iphone")
    add_subdirectory(coreaudio)
endif()

if(${target} STREQUAL "server")
    add_subdirectory(dummy)
else()
    add_subdirectory(gles2)
    add_subdirectory(gles3)
endif()