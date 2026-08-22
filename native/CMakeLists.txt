cmake_minimum_required(VERSION 3.21)
project(JiangxinVectorStudio VERSION 1.8.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Svg SvgWidgets PrintSupport)

qt_add_executable(JiangxinVectorStudio WIN32
    src/main.cpp
    src/MainWindow.hpp
    src/MainWindow.cpp
    src/CanvasView.hpp
    src/CanvasView.cpp
    src/PathNodeHandle.hpp
    src/PathNodeHandle.cpp
    src/RulerWidget.hpp
    src/RulerWidget.cpp
    src/DocumentIO.hpp
    src/DocumentIO.cpp
)

if(WIN32)
    target_sources(JiangxinVectorStudio PRIVATE windows/app.rc)
endif()

target_link_libraries(JiangxinVectorStudio PRIVATE
    Qt6::Widgets
    Qt6::Svg
    Qt6::SvgWidgets
    Qt6::PrintSupport
)

target_compile_definitions(JiangxinVectorStudio PRIVATE
    JX_APP_VERSION="${PROJECT_VERSION}"
    QT_NO_KEYWORDS
)

if(MSVC)
    target_compile_options(JiangxinVectorStudio PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(JiangxinVectorStudio PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS JiangxinVectorStudio RUNTIME DESTINATION bin)
