# Use an installed GoogleTest if there is one; otherwise download it at
# configure time.
find_package(GTest QUIET)
if(NOT GTest_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
  # On Windows, use the same runtime library as the rest of the project.
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

add_executable(game_tests
  tic_tac_toe_game_test.cc
  marble_game_test.cc
)
target_link_libraries(game_tests PRIVATE
  game_rules
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(game_tests)