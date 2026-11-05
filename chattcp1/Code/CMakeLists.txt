
cmake_minimum_required(VERSION 3.19)

project(chattcp1 LANGUAGES CXX)

# Tim cac thu vien Qt 6
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Network
)

qt_standard_project_setup()
set(CMAKE_AUTORCC ON)
# ==============================
# CHUONG TRINH SERVER
# ==============================
qt_add_executable(chattcp1
    WIN32 MACOSX_BUNDLE
    Server/main.cpp
    Server/mainwindow.cpp
    Server/mainwindow.h
    Server/mainwindow.ui
    Server/chatserver.h
    Server/chatserver.cpp
)

target_link_libraries(chattcp1 PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
)

# ==============================
# CHUONG TRINH CLIENT
# ==============================

qt_add_executable(ChatClient
    Client/main_client.cpp
    Client/stickers.qrc
)


target_link_libraries(ChatClient PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
)

# ==============================
# CAI DAT SERVER
# ==============================
install(TARGETS chattcp1
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET chattcp1
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)

install(SCRIPT ${deploy_script})
