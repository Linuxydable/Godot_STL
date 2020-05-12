project(godotModules)

file(GLOB directories "*")

set(module_header)
set(module_register)
set(module_unregister)
foreach(directorie ${directories})
    IF(IS_DIRECTORY ${directorie})
        get_filename_component(moduleName ${directorie} NAME)
        find_file(module_${moduleName}_CONFIG NAME "config.cmake" PATHS ".")
        find_file(module_${moduleName}_CMAKE NAME "CMakeLists.txt" PATHS ".")
        set(module_${moduleName}_ENABLED OFF)
        if(module_${moduleName}_CONFIG AND module_${moduleName}_CMAKE)
            include(module_${moduleName}_CONFIG)
        endif()

        if(module_${moduleName}_ENABLED)
            option("module_${moduleName}" "enable ${moduleName}" ON)
            if(module_${moduleName})
                add_subdirectory(${directorie})
                find_file(module_${moduleName}_register_types NAME "register_types.h" PATHS ".")
                if(module_${moduleName}_register_types-NOTFOUND)
                    message(FATAL "${moduleName} have no register_types.h")
                endif()
                list(APPEND module_header "#include \"${module_${moduleName}_register_types}\n\"")
                list(APPEND module_register "register_${moduleName}_types();\n")
                list(APPEND module_unregister "unregister_${moduleName}_types();\n")
            endif()
        endif()
        unset(module_${moduleName}_CONFIG CACHE)
        unset(module_${moduleName}_CMAKE CACHE)
        unset(module_${moduleName}_ENABLED CACHE)
    endif()
endforeach()


file(GENERATE OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/register_module_types.gen.cpp"
    CONTENT "// modules.cpp - THIS FILE IS GENERATED, DO NOT EDIT!!!!!!!
    #include \"register_module_types.h\"

    ${module_header}

    void register_module_types() {
        ${module_header}
    }

    void unregister_module_types() {
        ${module_unregister}
    }
    "
)

add_library(${PROJECT_NAME} "register_module_types.h" "register_module_types.gen.cpp")
