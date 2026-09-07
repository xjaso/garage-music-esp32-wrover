file(GLOB COMPONENT_SOURCES "*.c")
idf_component_register(SRCS ${COMPONENT_SOURCES}
    PRIV_REQUIRES bt garage_ui avrcp_common_utils
    INCLUDE_DIRS "." "${IDF_PATH}/examples/bluetooth/bluedroid/classic_bt/common/avrcp_utils/include")
