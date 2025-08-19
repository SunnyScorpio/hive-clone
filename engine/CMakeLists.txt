add_library(hive_engine STATIC
  src/engine.cpp
)

target_include_directories(hive_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Public compile options (strict warnings)
if(MSVC)
  target_compile_options(hive_engine PUBLIC /W4 /permissive-)
else()
  target_compile_options(hive_engine PUBLIC -Wall -Wextra -Wpedantic)
endif()