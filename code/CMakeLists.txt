cmake_minimum_required(VERSION 3.19)

project(chattcp1 LANGUAGES CXX)

# Tim cac thu vien Qt 6 (Sql: luu lich su chat bang SQLite o Client)
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Network
    Sql
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

# Lay tat ca file .cpp/.h trong thu muc Client/ de gom cac module moi:
# ConnectionDialog, ConnectionProfile, SessionConnection, ChatHistory,
# HistoryStore, HistoryView, historyimage.h, JsonChannel va cac file *protocol.h
file(GLOB CLIENT_SOURCES CONFIGURE_DEPENDS
    Client/*.cpp
    Client/*.h
)

qt_add_executable(ChatClient
    ${CLIENT_SOURCES}
    Client/stickers.qrc
)

target_link_libraries(ChatClient PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
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

# ==============================
# CAI DAT CLIENT
# (script deploy se copy ca driver SQLite: sqldrivers/qsqlite)
# ==============================
install(TARGETS ChatClient
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET ChatClient
    OUTPUT_SCRIPT deploy_script_client
    NO_UNSUPPORTED_PLATFORM_ERROR
)

install(SCRIPT ${deploy_script_client})
