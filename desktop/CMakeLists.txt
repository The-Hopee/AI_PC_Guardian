qt_policy(SET QTP0004 NEW)

qt_add_executable(
    guardian-desktop
    WIN32
    src/main.cpp
)

target_include_directories(
    guardian-desktop
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/guardian/desktop"
)

qt_add_qml_module(
    guardian-desktop
    URI GuardianApp
    VERSION 1.0
    SOURCES
        src/local_agent_client.cpp
        src/system_metrics_view_model.cpp
        include/guardian/desktop/local_agent_client.hpp
        include/guardian/desktop/system_metrics_view_model.hpp
    QML_FILES
        qml/Main.qml
        qml/MetricCard.qml
)

target_link_libraries(
    guardian-desktop
    PRIVATE
        Guardian::Common
        Guardian::Ipc
        Qt6::Network
        Qt6::Quick
        Qt6::QuickControls2
        guardian_proto_messages
)
