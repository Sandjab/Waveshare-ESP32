
set(DEVICE_DIR ./device)
file(GLOB_RECURSE DEVICE_C_SRCS ${DEVICE_DIR}/*.c)

set(LED_DIR ./led_strip)
file(GLOB_RECURSE LED_C_SRCS ${LED_DIR}/*.c)



set(MJPEG_DIR ./mjpeg)
file(GLOB_RECURSE MJPEG_C_SRCS ${MJPEG_DIR}/*.c)

set(MUSIC_DIR ./music)
file(GLOB_RECURSE MUSIC_C_SRCS ${MUSIC_DIR}/*.c)

set(MSC_DIR ./msc)
file(GLOB_RECURSE MSC_C_SRCS ${MSC_DIR}/*.c)

idf_component_register(SRCS "main.c" ${DEVICE_C_SRCS} ${LED_C_SRCS} ${MJPEG_C_SRCS} ${MUSIC_C_SRCS} ${MSC_C_SRCS}
                    INCLUDE_DIRS "." device led_strip  mjpeg music msc)

