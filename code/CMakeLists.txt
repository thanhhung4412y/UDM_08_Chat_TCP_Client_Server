cmake_minimum_required(VERSION 3.19)
project(chattcp1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets Network Sql)
qt_standard_project_setup()
set(CMAKE_AUTORCC ON)

add_library(chat_ui STATIC Client/chattheme.cpp Client/chattheme.h Client/chatshell.cpp Client/chatshell.h)
target_link_libraries(chat_ui PUBLIC Qt6::Widgets)
target_include_directories(chat_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Client ${CMAKE_CURRENT_SOURCE_DIR}/Shared)

add_library(video_transfer STATIC
    Shared/jsonchannel.cpp Shared/jsonchannel.h Shared/jsonframer.h
    Shared/videoclient.cpp Shared/videoclient.h
    Shared/videorelay.cpp Shared/videorelay.h Shared/videoprotocol.h
    Shared/historyprotocol.h
)
target_link_libraries(video_transfer PUBLIC Qt6::Core Qt6::Network)
target_include_directories(video_transfer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Shared)

add_library(chat_history STATIC
    Client/historystore.cpp Client/historystore.h
    Client/historyview.cpp Client/historyview.h
    Client/historyimage.h
    Client/chathistory.cpp Client/chathistory.h
    Client/historysearchdialog.cpp Client/historysearchdialog.h
)
target_link_libraries(chat_history PUBLIC Qt6::Core Qt6::Widgets Qt6::Sql)
target_include_directories(chat_history PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Client ${CMAKE_CURRENT_SOURCE_DIR}/Shared)

add_library(chat_connection STATIC
    Client/connectionprofile.cpp Client/connectionprofile.h
    Client/connectiondialog.cpp Client/connectiondialog.h
    Client/passworddialog.cpp Client/passworddialog.h
    Client/passwordvisibility.h Client/recoverydialog.cpp Client/recoverydialog.h
    Client/sessionconnection.cpp Client/sessionconnection.h)
target_link_libraries(chat_connection PUBLIC video_transfer Qt6::Widgets)
target_include_directories(chat_connection PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Client)

add_library(chat_recall STATIC Client/recallclient.cpp Client/recallclient.h Client/editmessagedialog.cpp Client/editmessagedialog.h Client/receiptclient.cpp Client/receiptclient.h)
target_link_libraries(chat_recall PUBLIC chat_history chat_connection)
add_library(account_server STATIC
    Server/emailservice.cpp Server/emailservice.h Server/gmailsender.cpp Server/gmailsender.h
    Server/gmailconfig.cpp Server/gmailconfig.h
    Server/recallstore.cpp Server/recallstore.h
    Server/chatserver.cpp Server/chatserver.h Server/accountstore.cpp Server/accountstore.h
    Server/authservice.cpp Server/authservice.h Server/tlsidentity.cpp Server/tlsidentity.h)
target_link_libraries(account_server PUBLIC video_transfer Qt6::Sql Qt6::Gui)
target_include_directories(account_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Server)
if(WIN32)
    target_link_libraries(account_server PRIVATE ncrypt crypt32)
endif()

qt_add_executable(chattcp1 WIN32 MACOSX_BUNDLE
    Shared/branding.qrc
    Server/main.cpp Server/mainwindow.cpp Server/mainwindow.h Server/mainwindow.ui
    Server/gmailsettingsdialog.cpp Server/gmailsettingsdialog.h)
target_link_libraries(chattcp1 PRIVATE account_server Qt6::Widgets)
target_include_directories(chattcp1 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Server ${CMAKE_CURRENT_SOURCE_DIR}/Shared)
if(WIN32)
    target_link_libraries(chattcp1 PRIVATE crypt32)
endif()

qt_add_executable(ChatClient Client/main_client.cpp Client/stickers.qrc Shared/branding.qrc)
target_link_libraries(ChatClient PRIVATE video_transfer chat_history chat_connection chat_ui chat_recall)

include(GNUInstallDirs)
install(TARGETS chattcp1 ChatClient BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
qt_generate_deploy_app_script(TARGET chattcp1 OUTPUT_SCRIPT server_deploy NO_UNSUPPORTED_PLATFORM_ERROR)
qt_generate_deploy_app_script(TARGET ChatClient OUTPUT_SCRIPT client_deploy NO_UNSUPPORTED_PLATFORM_ERROR)
install(SCRIPT ${server_deploy})
install(SCRIPT ${client_deploy})

option(BUILD_TESTING "Build automated tests (requires Qt6 Test)" OFF)
include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(group_history_tests tests/group_history_tests.cpp)
    target_link_libraries(group_history_tests PRIVATE account_server chat_connection Qt6::Test)
    add_test(NAME group_history_tests COMMAND group_history_tests)
    set_tests_properties(group_history_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(email_tests tests/email_tests.cpp)
    target_link_libraries(email_tests PRIVATE account_server chat_connection Qt6::Test)
    add_test(NAME email_tests COMMAND email_tests)
    set_tests_properties(email_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(transfer_tests tests/transfer_tests.cpp)
    target_include_directories(transfer_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Server)
    target_link_libraries(transfer_tests PRIVATE account_server chat_history Qt6::Test Qt6::Gui)
    add_test(NAME transfer_tests COMMAND transfer_tests)
    qt_add_executable(offline_tests tests/offline_tests.cpp)
    target_link_libraries(offline_tests PRIVATE video_transfer Qt6::Test)
    add_test(NAME offline_tests COMMAND offline_tests)
    qt_add_executable(history_tests tests/history_tests.cpp Client/stickers.qrc)
    target_link_libraries(history_tests PRIVATE chat_history video_transfer Qt6::Test)
    add_test(NAME history_tests COMMAND history_tests)
    set_tests_properties(history_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(history_probe tests/history_probe.cpp)
    target_link_libraries(history_probe PRIVATE chat_history)
    add_dependencies(history_tests history_probe)
    qt_add_executable(connection_tests tests/connection_tests.cpp)
    target_link_libraries(connection_tests PRIVATE account_server chat_connection chat_history Qt6::Test)
    target_include_directories(connection_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Server)
    add_test(NAME connection_tests COMMAND connection_tests)
    set_tests_properties(connection_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(auth_tests tests/auth_tests.cpp)
    target_link_libraries(auth_tests PRIVATE account_server chat_connection chat_history Qt6::Test)
    add_test(NAME auth_tests COMMAND auth_tests)
    set_tests_properties(auth_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(password_tests tests/password_tests.cpp)
    target_link_libraries(password_tests PRIVATE account_server chat_connection chat_history Qt6::Test)
    add_test(NAME password_tests COMMAND password_tests)
    set_tests_properties(password_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(ui_tests tests/ui_tests.cpp)
    target_link_libraries(ui_tests PRIVATE chat_ui chat_history chat_connection Qt6::Test)
    add_test(NAME ui_tests COMMAND ui_tests)
    set_tests_properties(ui_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(profile_tests tests/profile_tests.cpp)
    target_link_libraries(profile_tests PRIVATE account_server chat_connection chat_ui Qt6::Test)
    add_test(NAME profile_tests COMMAND profile_tests)
    set_tests_properties(profile_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(search_tests tests/search_tests.cpp Client/stickers.qrc)
    target_link_libraries(search_tests PRIVATE chat_history Qt6::Test)
    add_test(NAME search_tests COMMAND search_tests)
    set_tests_properties(search_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(recall_tests tests/recall_tests.cpp Client/stickers.qrc)
    target_link_libraries(recall_tests PRIVATE account_server chat_recall Qt6::Test)
    add_test(NAME recall_tests COMMAND recall_tests)
    set_tests_properties(recall_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(edit_tests tests/edit_tests.cpp Client/stickers.qrc)
    target_link_libraries(edit_tests PRIVATE account_server chat_recall Qt6::Test)
    add_test(NAME edit_tests COMMAND edit_tests)
    set_tests_properties(edit_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(receipt_tests tests/receipt_tests.cpp Client/stickers.qrc)
    target_link_libraries(receipt_tests PRIVATE account_server chat_recall Qt6::Test)
    add_test(NAME receipt_tests COMMAND receipt_tests)
    set_tests_properties(receipt_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(search_preview tests/search_preview.cpp)
    target_link_libraries(search_preview PRIVATE chat_history chat_ui)
    qt_add_executable(ui_preview tests/ui_preview.cpp Client/stickers.qrc)
    target_link_libraries(ui_preview PRIVATE chat_ui chat_history chat_connection chat_recall)
endif()
