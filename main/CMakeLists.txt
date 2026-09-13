idf_component_register(SRCS
                       "ui/AppShell.cpp"
                       "ui/SplashScreen.cpp"
                       "ui/UiTheme.cpp"
                       "core/SystemBootstrap.cpp"
                       "core/SystemManager.cpp"
                       "features/alarm/AlarmService.cpp"
                       "features/alarm/ui/AlarmApp.cpp"
                       "features/home/ui/HomeApp.cpp"
                       "features/notification/NotificationService.cpp"
                       "features/settings/ui/DeviceSettingsApp.cpp"
                       "features/settings/ui/WifiSettingsView.cpp"
                       "features/weather/WeatherService.cpp"
                       "features/weather/providers/OpenMeteoProvider.cpp"
                       "features/weather/ui/WeatherApp.cpp"
                       "features/weather/ui/WeatherPresentation.cpp"
                       "platform/audio/CoreS3AlarmOutput.cpp"
                       "platform/network/WifiService.cpp"
                       "platform/settings/SettingsService.cpp"
                       "platform/time/TimeService.cpp"
                       "ui/app/App.cpp"
                       "ui/app/AppManager.cpp"
                       "ui/app/AppRegistry.cpp"
                       "notificat.c"
                       INCLUDE_DIRS ".")


fatfs_create_spiflash_image(storage ../rsc FLASH_IN_PROJECT PRESERVE_TIME)
