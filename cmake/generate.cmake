macro(generateIcon os gen)
find_file(fileIcon 
    NAMES "run_icon.png"
    PATHS "${CMAKE_CURRENT_SOURCE_DIR}" 
    NO_DEFAULT_PATH
)


set(iconFile)
if(fileIcon)
    list(APPEND iconFile ${fileIcon})
endif()



list(APPEND ${gen} "${CMAKE_CURRENT_SOURCE_DIR}/logo.gen.h" ${iconFile})
add_custom_command(OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/logo.gen.h" ${iconFile}
     COMMAND ${Python_EXECUTABLE} "${CMAKE_SOURCE_DIR}/script/generate_icon.py" "${os}"
     DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/logo.png"
     WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/script"
)
endmacro()


# TODO : temporary
macro(generateBindings gen)

add_custom_command(OUTPUT "${CMAKE_SOURCE_DIR}/core/method_bind.gen.inc" "${CMAKE_SOURCE_DIR}/core/method_bind_ext.gen.inc" "${CMAKE_SOURCE_DIR}/core/method_bind_free_func.gen.inc"
  COMMAND ${Python_EXECUTABLE} "${CMAKE_SOURCE_DIR}/script/generate_binders.py"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/script"
	)
list(APPEND ${gen} "${CMAKE_SOURCE_DIR}/core/method_bind.gen.inc" "${CMAKE_SOURCE_DIR}/core/method_bind_ext.gen.inc" "${CMAKE_SOURCE_DIR}/core/method_bind_free_func.gen.inc")

endmacro()

