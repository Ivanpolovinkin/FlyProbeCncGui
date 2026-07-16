cmake_minimum_required(VERSION 3.19)
project(FlyProbeCncGui LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)
find_package(Qt6 REQUIRED COMPONENTS Widgets SerialPort)
find_package(Qt6 REQUIRED COMPONENTS Multimedia MultimediaWidgets)


qt_standard_project_setup()

qt_add_executable(FlyProbeCncGui
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    jsonparser.h
    jsonparser.cpp
    Point.h
    style.qss
    scan.h scan.cpp
    pinrectitem.h
    pinrectitem.cpp
    probe_view.h
    probe_view.cpp
    project_parser.h
    project_parser.cpp
    project_data.h
    scan_data.h
    scan_controller.h scan_controller.cpp
)

target_include_directories(FlyProbeCncGui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(FlyProbeCncGui
    PRIVATE
        Qt::Core
        Qt::Widgets
)

target_link_libraries(FlyProbeCncGui PRIVATE Qt6::Widgets Qt6::SerialPort)
target_link_libraries(FlyProbeCncGui PRIVATE Qt6::Multimedia Qt6::MultimediaWidgets)

include(GNUInstallDirs)

install(TARGETS FlyProbeCncGui
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET FlyProbeCncGui
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
