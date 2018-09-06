add_executable(run_tests test.cpp duktape_tests.cpp)
target_link_libraries(run_tests ModDukTape Catch2::Catch2 )

include(CTest)
include(Catch)
catch_discover_tests(run_tests)

enable_testing()
#add_test(BASIC_TESTS run_tests)
