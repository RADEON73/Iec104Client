cmake_minimum_required(VERSION 3.16)
project(Iec104Client VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON) #Упрощенное включение
set(CMAKE_CXX_EXTENSIONS OFF) #Более строгий стандарт
set(QT_USE_STATIC_LIBS ON) # Настройка статической линковки Qt

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS
    Core
    Widgets
    Network
)

if(MSVC)
    # Для статической линковки CRT
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
endif()

set(SOURCES
    main.cpp
    MainWindow.cpp
    SettingsDialog.cpp
    AppSettings.cpp
	TLogMsg.cpp
    QIec104.cpp
    iec104/iec104_class.cpp
)

set(HEADERS
    MainWindow.h
    SettingsDialog.h
    AppSettings.h
    TLogMsg.h
    QIec104.h
    iec104/iec104_class.h
    iec104/iec104_types.h
    iec104/iec104.h
)

set(FORMS
    MainWindow.ui
    SettingsDialog.ui
)

set(OTHER
    cpp.hint
)

qt5_add_resources(APP_RESOURCES Resources.qrc)

add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${FORMS}
	${APP_RESOURCES}
	${OTHER}
)

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt5::Widgets
        Qt5::Network
)