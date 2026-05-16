
set(DEVICE_DIR ./device)
file(GLOB_RECURSE DEVICE_C_SRCS ${DEVICE_DIR}/*.c)

set(LED_DIR ./led_strip)
file(GLOB_RECURSE LED_C_SRCS ${LED_DIR}/*.c)

set(KNOB_DIR ./knob)
file(GLOB_RECURSE KNOB_C_SRCS ${KNOB_DIR}/*.c)

set(MJPEG_DIR ./mjpeg)
file(GLOB_RECURSE MJPEG_C_SRCS ${MJPEG_DIR}/*.c)

set(MUSIC_DIR ./music)
file(GLOB_RECURSE MUSIC_C_SRCS ${MUSIC_DIR}/*.c)

idf_component_register(SRCS "main.c" ${DEVICE_C_SRCS} ${LED_C_SRCS} ${MJPEG_C_SRCS} ${MUSIC_C_SRCS} ${KNOB_C_SRCS}
                    INCLUDE_DIRS "." device led_strip  mjpeg music knob)

idf_build_get_property(build_components BUILD_COMPONENTS)
if(tinyusb IN_LIST build_components)
   set(tinyusb_name tinyusb) # Local component
else()
    set(tinyusb_name espressif__tinyusb) # Managed component
endif()


# Pass tusb_config.h to the example
idf_component_get_property(tusb_lib ${tinyusb_name} COMPONENT_LIB)
target_include_directories(${tusb_lib} PUBLIC "./knob")
target_sources(${tusb_lib} PUBLIC "./knob/usb_descriptors.c")