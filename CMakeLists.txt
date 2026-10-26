cmake_minimum_required(VERSION 3.16)

project(MarbleSolitaire VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

# The game rules depend only on the standard library, so they live in their
# own library. The app links it, and so do the unit tests, which keeps Qt out
# of the test build.
add_library(game_rules STATIC
  src/marble_game.cpp
  src/marble_game.h
  src/tic_tac_toe_game.cc
  src/tic_tac_toe_game.h
)
target_include_directories(game_rules PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(marble_solitaire
  src/main.cc
  src/main_window.cc
  src/main_window.h
  src/marble_board_widget.cpp
  src/marble_board_widget.h
  src/tic_tac_toe_widget.cc
  src/tic_tac_toe_widget.h
  assets/assets.qrc
)
target_link_libraries(marble_solitaire PRIVATE
  game_rules
  Qt${QT_VERSION_MAJOR}::Widgets
)
set_target_properties(marble_solitaire PROPERTIES
  WIN32_EXECUTABLE ON
  MACOSX_BUNDLE ON
)

if(MSVC)
  set(MARBLE_WARNING_FLAGS /W4)
else()
  set(MARBLE_WARNING_FLAGS -Wall -Wextra)
endif()
target_compile_options(game_rules PRIVATE ${MARBLE_WARNING_FLAGS})
target_compile_options(marble_solitaire PRIVATE ${MARBLE_WARNING_FLAGS})

# include(CTest) defines the BUILD_TESTING option (ON by default) and calls
# enable_testing().
include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()