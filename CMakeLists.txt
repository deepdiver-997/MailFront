cmake_minimum_required(VERSION 3.16)

project(mail_front VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Concurrent Sql)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(/Users/zhuhongrui/Desktop/code/c++/OuterLib/vmime/src)
link_directories(/Users/zhuhongrui/Desktop/code/c++/OuterLib/vmime/build/lib)

set(PROJECT_SOURCES
        main.cpp

        # UI
        app/mailapp.h
        app/mailapp.cpp
        app/mailapp.ui
        app/composewindow.h
        app/composewindow.cpp
        app/settingsdialog.h
        app/settingsdialog.cpp

        # Widgets
        widgets/foldertree.h
        widgets/foldertree.cpp
        widgets/mailpreview.h
        widgets/mailpreview.cpp

        # Models
        models/maillistmodel.h
        models/maillistmodel.cpp

        # Network
        network/smtpclient.h
        network/smtpclient.cpp
        network/imapclient.h
        network/imapclient.cpp
        network/VSMimeNetWorkAdapter.h
        network/VSMimeNetWorkAdapter.cpp

        # Services
        services/mailstore.h
        services/mailstore.cpp
        services/accountmanager.h
        services/accountmanager.cpp
        services/cryptohelper.h
        services/cryptohelper.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(mail_front
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(mail_front SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(mail_front
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(mail_front PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Concurrent Qt${QT_VERSION_MAJOR}::Sql vmime)

if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.mail_front)
endif()
set_target_properties(mail_front PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS mail_front
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(mail_front)
endif()
