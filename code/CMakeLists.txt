cmake_minimum_required(VERSION 3.19)
project(chattcp1 LANGUAGES CXX)

# TÌM VÀ KIỂM TRA CÁC THƯ VIỆN TRONG QT6
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Network
    Sql
)

# THIẾT LẬP CẤU HÌNH CHO QT
qt_standard_project_setup()
set(CMAKE_AUTORCC ON)

# KHAI BÁO SERVER
qt_add_executable(chattcp1
    WIN32 MACOSX_BUNDLE
    Server/main.cpp
    Server/mainwindow.cpp
    Server/mainwindow.h
    Server/mainwindow.ui
    Server/chatserver.h
    Server/chatserver.cpp
)

# LIÊN KẾT CÁC THƯ VIỆN CẦN THIẾT CHO SERVER
target_link_libraries(chattcp1 PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
)

# TỰ ĐỘNG LIÊN KẾT VÀ THU THẬP FILE CHO FOLDER CLIENT
file(GLOB CLIENT_SOURCES CONFIGURE_DEPENDS
    Client/*.cpp
    Client/*.h
)

# KHAI BÁO CLIENT
qt_add_executable(ChatClient
    ${CLIENT_SOURCES}
    Client/stickers.qrc
)

# LIÊN KẾT CÁC THƯ VIỆN CẦN THIẾT CHO CLIENT
target_link_libraries(ChatClient PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
)

# CÀI ĐẶT SERVER
install(TARGETS chattcp1
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# TRIỂN KHAI SERVER
qt_generate_deploy_app_script(
    TARGET chattcp1
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

# CÀI ĐẶT CLIENT
install(TARGETS ChatClient
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# TRIỂN KHAI CLIENT
qt_generate_deploy_app_script(
    TARGET ChatClient
    OUTPUT_SCRIPT deploy_script_client
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script_client})
