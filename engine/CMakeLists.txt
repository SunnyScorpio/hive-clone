add_library(hive_engine STATIC
  src/engine.cpp
)

target_include_directories(hive_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Strict warnings on MSVC; define NOMINMAX to avoid <windows.h> macro issues
if(MSVC)
  target_compile_options(hive_engine PUBLIC /W4 /permissive-)
  target_compile_definitions(hive_engine PUBLIC NOMINMAX)
else()
  target_compile_options(hive_engine PUBLIC -Wall -Wextra -Wpedantic)
endif()