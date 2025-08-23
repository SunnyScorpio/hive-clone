add_executable(hive_desktop
  src/main.cpp
  src/ui_app.cpp
)

# Include local headers (ui_app.hpp) and any helper headers in src
target_include_directories(hive_desktop PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link against the engine and SFML libs
target_link_libraries(hive_desktop PRIVATE hive_engine sfml-graphics sfml-window sfml-system)

# Put the executable in build/bin
set_target_properties(hive_desktop PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Copy assets next to the binary after build
add_custom_command(TARGET hive_desktop POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets
          ${CMAKE_BINARY_DIR}/bin/assets)

# Copy SFML runtime DLLs next to the executable (Windows)
add_custom_command(TARGET hive_desktop POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:sfml-graphics> $<TARGET_FILE_DIR:hive_desktop>
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:sfml-window>   $<TARGET_FILE_DIR:hive_desktop>
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:sfml-system>   $<TARGET_FILE_DIR:hive_desktop>
)