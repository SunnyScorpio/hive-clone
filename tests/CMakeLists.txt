add_executable(hive_tests test_engine.cpp test_rules.cpp)

target_link_libraries(hive_tests PRIVATE hive_engine GTest::gtest_main)

# Apply warnings to tests to catch issues in headers
if(MSVC)
  target_compile_options(hive_tests PRIVATE /W4 /permissive- $<$<BOOL:${HIVE_WARN_AS_ERRORS}>:/WX>)
  target_compile_definitions(hive_tests PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(hive_tests PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter $<$<BOOL:${HIVE_WARN_AS_ERRORS}>:-Werror>)
endif()

# Put tests in build/bin for convenience
set_target_properties(hive_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(GoogleTest)
gtest_discover_tests(hive_tests)