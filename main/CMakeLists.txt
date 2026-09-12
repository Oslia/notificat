idf_component_register(SRCS "notificat.c"
                       INCLUDE_DIRS ".")


fatfs_create_spiflash_image(storage ../storage FLASH_IN_PROJECT PRESERVE_TIME)