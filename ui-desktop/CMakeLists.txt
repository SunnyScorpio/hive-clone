add_executable(hive_desktop
  src/main.cpp
  src/ui_app.cpp
)

target_include_directories(hive_desktop PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(hive_desktop PRIVATE hive_engine sfml-graphics sfml-window sfml-system)

# Warnings for the UI target as well
if(MSVC)
  target_compile_options(hive_desktop PRIVATE /W4 /permissive- $<$<BOOL:${HIVE_WARN_AS_ERRORS}>:/WX>)
  target_compile_definitions(hive_desktop PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(hive_desktop PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter $<$<BOOL:${HIVE_WARN_AS_ERRORS}>:-Werror>)
endif()

set_target_properties(hive_desktop PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_custom_command(TARGET hive_desktop POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets
          ${CMAKE_BINARY_DIR}/bin/assets)

add_custom_command(TARGET hive_desktop POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:sfml-graphics> $<TARGET_FILE_DIR:hive_desktop>
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:sfml-window>   $<TARGET_FILE_DIR:hive_desktop>
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:sfml-system>   $<TARGET_FILE_DIR:hive_desktop>
)