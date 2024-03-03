macro(CopyDLL target_name)
    if(WIN32)
        # add_custom_command(
        #     TARGET ${target_name} POST_BUILD
        #     COMMAND ${CMAKE_COMMAND} -E copy ${SDL_ROOT}/bin/SDL2.dll $<TARGET_FILE_DIR:${target_name}>
        # )
        # If you have selected SDL2 component when installed Vulkan SDK, the command as follows will work
        add_custom_command(
            TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${SDL2_BIN_DIR}/SDL2.dll $<TARGET_FILE_DIR:${target_name}>
            )
    endif()
endmacro(CopyDLL)

macro(CopyShader target_name)
    add_custom_command(
        TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/spvs $<TARGET_FILE_DIR:${target_name}>/spvs)
endmacro(CopyShader)

macro(CopyResource target_name)
    add_custom_command(
        TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/resources $<TARGET_FILE_DIR:${target_name}>/resources)
endmacro(CopyResource)