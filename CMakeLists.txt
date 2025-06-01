cmake_minimum_required(VERSION 3.16)
project(amortizationCalcQt)

set(CMAKE_CXX_STANDARD 17)

find_package(Qt6 COMPONENTS Widgets Charts REQUIRED)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC_PATH_PREFIX OFF) # <-- Add this line

get_target_property(Qt6Charts_INCLUDE_DIRS Qt6::Charts INTERFACE_INCLUDE_DIRECTORIES)
include_directories(${Qt6Charts_INCLUDE_DIRS})

add_executable(amortizationCalcQt
    src/amortizationCalcQt.cpp
    src/amortizationCalcQt.h
)

target_link_libraries(amortizationCalcQt
    Qt6::Widgets
    Qt6::Charts
)
