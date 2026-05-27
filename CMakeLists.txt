cmake_minimum_required(VERSION 3.16)
project(SteamGifCutter)


# 设置图标（Windows）
set(ICON_PATH ${CMAKE_CURRENT_SOURCE_DIR}/app.ico)
if(EXISTS ${ICON_PATH})
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
    
    # 创建资源文件
    set(ICON_RC ${CMAKE_CURRENT_BINARY_DIR}/app_icon.rc)
    file(WRITE ${ICON_RC} "IDI_ICON1 ICON DISCARDABLE \"${ICON_PATH}\"\n")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--subsystem,windows")
endif()

# 查找 Qt 模块
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# 导出编译数据库（clangd 需要）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 自动处理 moc/uic/rcc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# 添加可执行文件
qt_add_executable(SteamGifCutter
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    gifprocessor.cpp
    gifprocessor.h
    resources.qrc
    ${ICON_RC}          # 添加图标资源文件
)

# 链接 Qt 库
target_link_libraries(SteamGifCutter
    PRIVATE
    Qt6::Widgets
    Qt6::Core
)

# 设置 C++ 标准
set_target_properties(SteamGifCutter PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)