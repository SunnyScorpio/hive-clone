add_executable(hive_tests test_engine.cpp)

target_link_libraries(hive_tests PRIVATE hive_engine GTest::gtest_main)

set_target_properties(hive_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(GoogleTest)
gtest_discover_tests(hive_tests)