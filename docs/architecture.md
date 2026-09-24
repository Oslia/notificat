# Notificat アーキテクチャ

## 設計方針

UI 表現と機能実装を分離する。`*App` と `*View` は LVGL オブジェクトの生成とユーザー操作を担当し、デバイス全体で共有する状態は `features/` と `platform/` のサービスが所有する。サービスから LVGL API を呼び出してはならない。

各コンテンツアプリは独立したライフサイクルを持つ。あるアプリから別のアプリのクラスや画面オブジェクトを直接参照せず、共有が必要な情報はサービスのスナップショットとして受け取る。

## 現在の構成

```text
app_main
 ├─ ストレージ／ディスプレイ初期化
 ├─ SplashScreen（rsc/notificat.bmp）
 ├─ SystemBootstrap（LVGL ロック外でサービス初期化）
 └─ AppShell
     ├─ グローバルステータスバー
     ├─ 下端スワイプ・アプリランチャー
     ├─ AppManager / AppRegistry
     │   ├─ HomeApp            （現在時刻、現在の天気概要）
     │   ├─ AlarmApp           （アラーム画面）
     │   ├─ WeatherApp         （時間別、7日間予報）
     │   └─ DeviceSettingsApp  （地域、ネットワーク設定）
     │       └─ WifiSettingsView
     └─ SystemManager
         ├─ AppContext / SystemEventBus
         ├─ AlarmService       （NVS 保存、バックグラウンド判定、スヌーズ）
         │   └─ AlarmOutput
         │       └─ CoreS3AlarmOutput（スピーカー、I2S）
         ├─ NotificationService（機能横断通知の境界）
         ├─ SettingsService    （地域、座標、NVS 永続化）
         ├─ TimeService        （タイムゾーン、SNTP、ローカル時刻）
         ├─ WifiService        （検索、接続、切断、接続状態）
         └─ WeatherService     （更新周期、地域と天気スナップショット）
             └─ WeatherProvider
                 └─ OpenMeteoProvider（HTTPS、JSON 変換）
```

## 責務の境界

| コンポーネント | 責務 | 担当しないこと |
| --- | --- | --- |
| `AppShell` | 共通ステータスバー、コンテンツ領域、アプリ切替ジェスチャー | 個別機能のデータ取得・保存 |
| `AppManager` | アプリの静的所有、初期化、`onEnter` / `onLeave`、表示切替 | ランチャーボタンの個別定義 |
| `AppRegistry` | アプリ ID、表示名、インスタンスの一元登録 | アプリ画面の描画 |
| `AppContext` | UI に必要なサービス参照を明示的に渡す | サービスの生成・状態所有 |
| `SystemEventBus` | バックグラウンドの変更通知をビットマスクで集約 | イベントデータ本体の保存 |
| `*App`, `*View` | LVGL 描画、入力、サービスへのコマンド発行 | グローバル状態の所有、通信処理 |
| `SystemManager` | システムサービスの初期化順序を一元管理 | 画面描画、継続的な業務ロジック |
| `AlarmService` | アラーム設定の NVS 保存、ローカル時刻によるバックグラウンド判定、スヌーズ、発火状態 | アラーム画面の描画、I2S の具体的な実装 |
| `AlarmOutput` | 発火出力をハードウェア実装から分離するインターフェース | 時刻判定、アラーム保存 |
| `CoreS3AlarmOutput` | スピーカーの遅延初期化とアラーム音の非同期再生 | アラームのスケジュール判断、LVGL 描画 |
| `NotificationService` | 機能横断通知の集約 | 各アプリ固有画面の描画 |
| `SettingsService` | 地域設定と NVS 永続化 | 時刻・天気の描画 |
| `TimeService` | タイムゾーン適用、SNTP 同期、ローカル時刻提供 | Wi-Fi 接続、時計画面の描画 |
| `WifiService` | Wi-Fi ドライバー、検索・接続・切断、状態スナップショット | 設定画面の描画 |
| `WeatherService` | 対象地域、更新スケジュール、現在値・予報データの所有 | HTTPS・JSON の具体的な実装、天気画面の描画 |
| `WeatherProvider` | 外部天気サービスを交換可能にする取得インターフェース | 更新周期、UI 表現 |
| `OpenMeteoProvider` | Open-Meteo への HTTPS 要求と JSON からドメイン型への変換 | キャッシュ、LVGL 描画 |

## 状態と非同期処理

- Wi-Fi 状態は `Off`、`Disconnected`、`Connecting`、`Connected`、`Disconnecting`、`Error` で表現する。
- Wi-Fi ドライバーはディスプレイバッファ確保後、ローディング画面表示中に LVGL ロック外で一度だけ初期化する。接続、検索、IP 取得は ESP-IDF のイベント処理で非同期に進行する。
- IP 取得イベントを受けると `TimeService` に SNTP 同期を要求する。
- サービスは `SystemEventBus` に変更を通知する。`AppShell` が LVGL タイマー内でイベントを取得し、アクティブなアプリは最新スナップショットを読む。
- UI は `AppContext` からサービスを参照し、サービス側のイベントハンドラーから LVGL を直接操作しない。
- 地域変更時は `SettingsService` に保存した後、`TimeService` と `WeatherService` に反映する。
- `WeatherService` は専用 FreeRTOS タスクで通信し、成功後 1 時間、失敗後 5 分で次の取得を行う。Wi-Fi 接続と地域変更はタスク通知で即時取得を要求する。
- HTTP 通信中に地域が変更された場合は世代番号を比較し、古い地域の応答を破棄する。
- 天気スナップショットは mutex で保護し、UI は値コピーだけを受け取る。サービスとプロバイダーから LVGL API を呼び出さない。
- 取得対象は現在値、24 時間分の時間別配列、7 日分の日別配列である。サービスは時間別配列を 3 時間間隔の 8 件に整形して保持する。
- HTTPS 応答バッファと mbedTLS の動的メモリは PSRAM を利用し、LCD/SPI DMA が必要とする内部 RAM のピークを抑える。
- `AlarmService` は 1 秒周期で `TimeService` のローカル時刻を確認する。時刻が無効な間は判定せず、同じアラームを同一分内に複数回発火させない。
- アラーム発火は `SystemEventBus` で通知し、`AppShell` が現在のアプリより上に全画面オーバーレイを表示する。停止とスヌーズの操作は再び `AlarmService` へコマンドとして渡す。
- スピーカーは最初の発火時に専用タスクと I2S／codec を初期化する。音声出力の失敗はスケジュール判定や全画面通知を停止させない。

## ソースコードの配置

- `main/core/`: `AppContext`、システムイベント、サービス初期化の調停。
- `main/platform/`: Wi-Fi、時刻、NVS など ESP-IDF に近い機能。
- `main/features/`: 機能単位のサービス、状態モデル、専用 UI。
- `main/ui/`: 共通の LVGL シェル、テーマ、アプリライフサイクルとレジストリ。
- `main/notificat.c`: NVS、FAT、ディスプレイ、スプラッシュ画面の起動処理。
- `docs/`: 要件、アーキテクチャ、UI 方針。

## 実装状況と次の境界

現在は画面骨格、地域保存、Wi-Fi の検索・接続・切断・保存済み設定への再接続、SNTP 同期、アラームの保存／判定／出力、Open-Meteo からの天気取得までを実装している。ホームは現在の天気を表示し、天気アプリは現在値、3 時間間隔、7 日間のタブを持つ。

`NotificationService` は現在、最新メッセージと未読件数を集約する段階である。次の段階では通知履歴と既読操作を実装する。MQTT は `AppContext::mqtt` の共通サービスを利用し、購読と送信を一元管理する。天気の HTTPS 通信とは並行して動作する。詳細は [MQTT](mqtt.md) を参照する。UI はすでに `AppContext` を介してサービスを参照しており、新しい機能も同じ依存注入規則に従う。
