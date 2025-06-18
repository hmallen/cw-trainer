idf_component_register(SRCS "wifi_companion.c" "trainer_protocol.c" "web_server.c"
                    INCLUDE_DIRS ".")

spiffs_create_partition_image(spiffs spiffs_image FLASH_IN_PROJECT)

#spiffs_create_partition_image(spiffs FLASH_IN_PROJECT
#                              PARTITION_LABEL spiffs
#                              FLASH_IN_PROJECT TRUE
#                              FILES "spiffs_image")