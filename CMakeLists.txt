cmake_minimum_required(VERSION 3.16)
project(IEC104Client VERSION 1.0.0 LANGUAGES CXX)

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
    appwindow.cpp
    mainwindow.cpp
	logmsg.cpp
    iec104_class.cpp
    qiec104.cpp
)

set(HEADERS
    appwindow.h
    mainwindow.h
	logmsg.h
    iec104_class.h
    qiec104.h
)

set(FORMS
    mainwindow.ui
)

set(OTHER_FILES
)

add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${FORMS}
    ${OTHER_FILES}
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

target_include_directories(IEC104Client SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/qtester104
)