idf_component_register(
    SRCS
        "garage_lvgl.c"
        "ui_comp_uibtnmin1.c"
        "ui_comp.c"
        "ui_Screen1.c"
        "ui.c"
        "ui_comp_hook.c"
        "ui_helpers.c"
        "ui_img_skuska2_png.c"
    INCLUDE_DIRS "."
    REQUIRES lvgl esp_lcd esp_timer freertos heap
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC LV_CONF_INCLUDE_SIMPLE=1)
