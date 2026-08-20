cmake_minimum_required(VERSION 3.16)

project(Iec104Client VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON) #Упрощенное включение
set(CMAKE_CXX_EXTENSIONS OFF) #Более строгий стандарт

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)


option(USE_STATIC_RUNTIME
    "Build with static runtime (/MT) instead of dynamic (/MD)"
    ON
)

if(WIN32)
    option(USE_STATIC_RUNTIME
        "Build with static runtime (/MT) instead of dynamic (/MD)"
        ON
    )

    if(USE_STATIC_RUNTIME)
        set(QT_ROOT "$ENV{QT_ROOT_STATIC}")
        set(QT_ROOT_NAME "QT_ROOT_STATIC")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        message(STATUS "Build with static runtime: ON")
    else()
        set(QT_ROOT "$ENV{QT_ROOT_DYNAMIC}")
        set(QT_ROOT_NAME "QT_ROOT_DYNAMIC")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        message(STATUS "Build with static runtime: OFF")
    endif()
#else()
#    set(QT_ROOT "$ENV{QT_ROOT_DYNAMIC}")
#    set(QT_ROOT_NAME "QT_ROOT_DYNAMIC")
#    message(STATUS "Linux build: using dynamic Qt")
#endif()

	if(QT_ROOT)
		message(STATUS "${QT_ROOT_NAME} is set: ${QT_ROOT}")
		list(PREPEND CMAKE_PREFIX_PATH
			"${QT_ROOT}"
		)
	else()
		message(STATUS
			"${QT_ROOT_NAME} is not set, using default Qt search"
		)
	endif()
endif()

find_package(Qt5 REQUIRED COMPONENTS
    Core
    Widgets
    Network
)

set(SOURCES
    main.cpp
    MainWindow.cpp
    SettingsDialog.cpp
    AppSettings.cpp
    Iec104TcpSocket.cpp
    LogController.cpp
    SocketManager.cpp
    model/TableCell.cpp
    model/TableRow.cpp
    model/TableModel.cpp
    model/TableProxyModel.cpp
    iec104/iec104_class.cpp
    iec104/iec104_log.cpp
)

set(HEADERS
    MainWindow.h
    SettingsDialog.h
    AppSettings.h
    Iec104TcpSocket.h
    LogController.h
    SocketManager.h
    model/TableCell.h
    model/TableRow.h
    model/TableModel.h
    model/TableProxyModel.h
    iec104/iec104_class.h
    iec104/iec104_types.h
    iec104/iec104.h
    iec104/iec104_log.h
)

set(FORMS
    MainWindow.ui
    SettingsDialog.ui
)

set(OTHER
    Iec104Client.rc
    Iec104Client.ico
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

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt5::Widgets
        Qt5::Network
)

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
        LINK_FLAGS "/SUBSYSTEM:WINDOWS"
    )
	if(MSVC)
		target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8")
		set_target_properties(${PROJECT_NAME} PROPERTIES
			RC_OPTIONS "/l 0x0419"
		)
	endif()
	set_target_properties(${PROJECT_NAME} PROPERTIES
		VS_PLATFORM_TOOLSET "v142"
		VS_WINDOWS_TARGET_PLATFORM_VERSION "10.0"
	)
endif()