# Notificat UML 設計図

## この文書について

現在のソースコードを基準に、主要クラスの責務と実行時の連携を Mermaid で表現する。GitHub の Markdown 画面でそのまま描画できる。メンバー、引数、戻り値は設計理解に必要なものへ絞っており、完全な API 一覧ではない。実装と図が異なる場合はソースコードを正とする。

## システム構成

```mermaid
flowchart TB
    Main[app_main] --> Storage[FATFS / NVS]
    Main --> Display[BSP Display / LVGL]
    Display --> Splash[SplashScreen]
    Main --> Bootstrap[SystemBootstrap]
    Bootstrap --> SystemManager
    Main --> AppShell

    AppShell --> AppManager
    AppManager --> HomeApp
    AppManager --> AlarmApp
    AppManager --> WeatherApp
    AppManager --> DeviceSettingsApp
    DeviceSettingsApp --> WifiSettingsView

    SystemManager --> AppContext
    SystemManager --> SystemEventBus
    SystemManager --> AlarmService
    SystemManager --> NotificationService
    SystemManager --> SettingsService
    SystemManager --> TimeService
    SystemManager --> WeatherService
    SystemManager --> WifiService

    AlarmService --> CoreS3AlarmOutput
    WeatherService --> OpenMeteoProvider
    OpenMeteoProvider --> OpenMeteo[(Open-Meteo API)]
    WifiService --> SNTP[(SNTP)]
```

## UI クラス図

```mermaid
classDiagram
    class App {
        <<abstract>>
        +init(AppContext)
        +onEnter(lv_obj_t)
        +onLeave()
        +onSystemEvents(SystemEventMask)
    }

    class HomeApp
    class AlarmApp
    class WeatherApp
    class DeviceSettingsApp
    class WifiSettingsView {
        +init(WifiService)
        +show(lv_obj_t, ExitCallback, context)
        +hide()
        +onSystemEvents(SystemEventMask)
    }
    class AppManager {
        -AppRegistry registry_
        -App active_app_
        +init(AppContext)
        +activate(AppId, lv_obj_t)
        +dispatchSystemEvents(SystemEventMask)
    }
    class AppRegistry {
        -AppDescriptor entries
        +add(AppId, title, App)
        +find(AppId) App
        +count() size_t
    }
    class AppDescriptor {
        +AppId id
        +string title
        +App app
    }
    class AppContext
    class AppShell

    App <|-- HomeApp
    App <|-- AlarmApp
    App <|-- WeatherApp
    App <|-- DeviceSettingsApp
    DeviceSettingsApp *-- WifiSettingsView
    AppManager *-- AppRegistry
    AppRegistry *-- AppDescriptor
    AppManager *-- HomeApp
    AppManager *-- AlarmApp
    AppManager *-- WeatherApp
    AppManager *-- DeviceSettingsApp
    AppShell --> AppManager
    HomeApp ..> AppContext
    AlarmApp ..> AppContext
    WeatherApp ..> AppContext
    DeviceSettingsApp ..> AppContext
```

## サービスとプラットフォームのクラス図

```mermaid
classDiagram
    class SystemManager {
        -SystemEventBus event_bus_
        -AppContext context_
        -SystemManagerState state_
        +instance() SystemManager
        +init() esp_err_t
        +context() AppContext
    }
    class AppContext {
        +AlarmService alarms
        +NotificationService notifications
        +SettingsService settings
        +TimeService time
        +WeatherService weather
        +WifiService wifi
        +SystemEventBus events
    }
    class SystemEventBus {
        -atomic pending_
        +publish(SystemEvent)
        +consume() SystemEventMask
    }
    class AlarmService {
        +snapshot() AlarmSnapshot
        +add(hour, minute, repeatDays) esp_err_t
        +update(id, hour, minute, repeatDays) esp_err_t
        +remove(id) esp_err_t
        +set_enabled(id, enabled) esp_err_t
        +dismiss() esp_err_t
        +snooze(minutes) esp_err_t
    }
    class AlarmOutput {
        <<interface>>
        +set_active(bool)
    }
    class CoreS3AlarmOutput {
        +instance() CoreS3AlarmOutput
        +set_active(bool)
    }
    class NotificationService {
        +snapshot() NotificationSnapshot
        +post(message)
        +mark_all_read()
    }
    class SettingsService {
        +region_count() uint8_t
        +region() RegionInfo
        +set_region(index) esp_err_t
    }
    class TimeService {
        +apply_timezone(timezone)
        +start_time_sync() esp_err_t
        +sync_state() TimeSyncState
        +local_time() SystemTime
    }
    class WifiService {
        +scan() esp_err_t
        +connect(ssid, password) esp_err_t
        +disconnect() esp_err_t
        +snapshot(WifiSnapshot)
    }
    class WeatherService {
        +set_region(RegionInfo)
        +request_refresh()
        +snapshot() WeatherSnapshot
    }
    class WeatherProvider {
        <<interface>>
        +fetch(WeatherLocation, WeatherForecastData) esp_err_t
    }
    class OpenMeteoProvider {
        +instance() OpenMeteoProvider
        +fetch(WeatherLocation, WeatherForecastData) esp_err_t
    }

    SystemManager *-- SystemEventBus
    SystemManager *-- AppContext
    AppContext o-- AlarmService
    AppContext o-- NotificationService
    AppContext o-- SettingsService
    AppContext o-- TimeService
    AppContext o-- WeatherService
    AppContext o-- WifiService
    AppContext o-- SystemEventBus
    AlarmService --> TimeService : local time
    AlarmService --> NotificationService : post
    AlarmService --> AlarmOutput : sound command
    AlarmOutput <|.. CoreS3AlarmOutput
    WeatherService --> WeatherProvider : fetch
    WeatherService --> WifiService : connection state
    WeatherProvider <|.. OpenMeteoProvider
    SettingsService --> SystemEventBus : publish
    TimeService --> SystemEventBus : publish
    WifiService --> SystemEventBus : publish
    WeatherService --> SystemEventBus : publish
    AlarmService --> SystemEventBus : publish
```

## データモデル

```mermaid
classDiagram
    class AlarmSnapshot {
        +uint8_t alarm_count
        +uint8_t enabled_count
        +bool ringing
        +AlarmItem alarms
    }
    class AlarmItem {
        +uint32_t id
        +uint8_t hour
        +uint8_t minute
        +uint8_t repeat_days
        +bool enabled
    }
    class WeatherSnapshot {
        +WeatherStatus status
        +string region_name
        +int64_t updated_at
        +WeatherForecastData forecast
    }
    class WeatherForecastData {
        +CurrentWeather current
        +HourlyWeather hourly
        +DailyWeather daily
    }
    class WifiSnapshot {
        +WifiConnectionState connection_state
        +WifiError last_error
        +bool scan_in_progress
        +WifiNetwork networks
    }
    class RegionInfo {
        +string name
        +string timezone
        +double latitude
        +double longitude
    }

    AlarmSnapshot *-- AlarmItem
    WeatherSnapshot *-- WeatherForecastData
    WifiSnapshot *-- WifiNetwork
    SettingsService --> RegionInfo
```

## 起動シーケンス

```mermaid
sequenceDiagram
    participant Main as app_main
    participant FS as FATFS / NVS
    participant BSP as Display BSP / LVGL
    participant Boot as SystemBootstrap
    participant SM as SystemManager
    participant Shell as AppShell
    participant Apps as AppManager

    Main->>FS: FATFS mount and NVS init
    Main->>BSP: display and LVGL start
    Main->>BSP: show cat splash
    Main->>Boot: system_bootstrap_init()
    Boot->>SM: init()
    SM->>SM: initialize settings, time, alarm, weather, Wi-Fi
    Main->>Main: keep splash for 1500 ms
    Main->>Shell: app_shell_init()
    Shell->>Apps: init(AppContext)
    Shell->>Apps: activate(Home)
```

## アプリ切替シーケンス

```mermaid
sequenceDiagram
    actor User
    participant Shell as AppShell
    participant Registry as AppRegistry
    participant Manager as AppManager
    participant Current as Current App
    participant Next as Selected App

    User->>Shell: swipe upward from bottom edge
    Shell->>Registry: enumerate registered apps
    Shell-->>User: show launcher
    User->>Shell: tap an app
    Shell->>Manager: activate(AppId, content)
    Manager->>Current: onLeave()
    Manager->>Shell: clean content objects
    Manager->>Registry: find(AppId)
    Registry-->>Manager: App instance
    Manager->>Next: onEnter(content)
```

## 天気更新シーケンス

```mermaid
sequenceDiagram
    participant WiFi as WifiService
    participant SM as SystemManager
    participant WS as WeatherService task
    participant Provider as OpenMeteoProvider
    participant Bus as SystemEventBus
    participant Shell as AppShell
    participant UI as HomeApp / WeatherApp

    WiFi->>SM: connected callback
    SM->>WS: request_refresh()
    WS->>WiFi: snapshot()
    WiFi-->>WS: Connected
    WS->>Provider: fetch(location)
    Provider-->>WS: WeatherForecastData
    WS->>Bus: publish(WeatherChanged)
    Shell->>Bus: consume()
    Shell->>UI: onSystemEvents(mask)
    UI->>WS: snapshot()
    UI->>UI: redraw latest values
```

## アラーム発火シーケンス

```mermaid
sequenceDiagram
    participant Task as AlarmService task
    participant Time as TimeService
    participant Notify as NotificationService
    participant Audio as AlarmOutput
    participant Bus as SystemEventBus
    participant Shell as AppShell
    actor User

    loop every second
        Task->>Time: local_time()
        Time-->>Task: SystemTime
        Task->>Task: compare enabled schedules
    end
    Task->>Notify: post(message)
    Task->>Audio: set_active(true)
    Task->>Bus: publish(AlarmChanged)
    Shell->>Bus: consume()
    Shell-->>User: show full-screen alarm overlay
    alt Stop
        User->>Task: dismiss()
    else Snooze
        User->>Task: snooze(5)
    end
    Task->>Audio: set_active(false)
```

## 天気サービス状態遷移

```mermaid
stateDiagram-v2
    [*] --> NotLoaded
    NotLoaded --> WaitingForNetwork: Wi-Fi unavailable
    NotLoaded --> Loading: refresh requested
    WaitingForNetwork --> Loading: Wi-Fi connected
    Loading --> Available: fetch succeeded
    Loading --> Error: fetch failed
    Available --> Loading: one hour elapsed
    Available --> NotLoaded: region changed
    Error --> Loading: five minute retry
    Error --> NotLoaded: region changed
```

## アラーム実行状態遷移

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Ringing: schedule matched
    Ringing --> Idle: dismiss or ten minute timeout
    Ringing --> Snoozed: snooze
    Snoozed --> Ringing: snooze time reached
    Snoozed --> Idle: alarm removed or disabled
```

## 設計上の制約

- LVGL API は UI タスクまたは LVGL lock 内からだけ操作する。
- サービスは UI オブジェクトを保持せず、値コピーの snapshot を公開する。
- FreeRTOS タスクと ESP イベントから更新する共有状態は mutex または atomic で保護する。
- HTTPS response や画像キャッシュは PSRAM を優先し、LCD/SPI DMA 用の内部 RAM を残す。
- アプリの追加・削除は `AppManager`、`AppRegistry`、`AppContext`、ビルド対象、文書を同時に更新する。
