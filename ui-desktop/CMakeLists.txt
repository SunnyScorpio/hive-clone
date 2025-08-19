add_executable(hive_desktop
  src/main.cpp
)

target_link_libraries(hive_desktop PRIVATE hive_engine sfml-graphics sfml-window sfml-system)

# Output binary into bin folder
set_target_properties(hive_desktop PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
