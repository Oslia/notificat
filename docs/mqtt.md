# アプリ共通 MQTT サービス

`SystemManager` が `SystemMQTT` を初期化し、各アプリは `AppContext::mqtt` を参照する。
接続、証明書、再接続、購読状態、送受信キューを一元管理する。アプリごとに MQTT クライアントを作らない。

## 設定

`idf.py menuconfig` → `Notificat MQTT` → `Enable shared AWS IoT MQTT service` を有効にする。
既存のテスト設定を有効にすると共通サービスも有効になる。テストを無効にして共通サービスだけを使用できる。
接続と証明書の準備は [MQTT テスト](mqtt_test.md) を参照する。

## アプリでの利用

以下はアプリが保持するメンバーと処理の例である。トピックは完全な名前を渡す。
コールバックは AppShell の 100 ms 周期 LVGL タイマーから実行されるため、LVGL の更新が可能である。
長時間処理や待機を行わない。受信データのポインターはコールバック実行中のみ有効であり、保持する場合はコピーする。

```cpp
#include "core/SystemMQTT.hpp"

SystemMQTT::Subscription subscription_ = 0;

// onEnter などで登録する。失敗時はエラーを表示するなどして対処する。
auto err = context.mqtt.subscribe(
    "notificat/weather/update",
    [](const MqttMessage &message, void *owner) {
        auto *app = static_cast<MyApp *>(owner);
        app->handleMessage(message.payload, message.size);
    }, this, subscription_);

// ボタン操作などから送信する。payload は終端 NUL を含めない。
constexpr char payload[] = "{\"action\":\"refresh\"}";
err = context.mqtt.publish("notificat/weather/request", payload, sizeof(payload) - 1);
// 空のメッセージなら publish(topic, nullptr, 0)。

// onLeave で UI や所有者を破棄する前に解除する。
if (subscription_ != 0) {
    context.mqtt.unsubscribe(subscription_);
    subscription_ = 0;
}
```

`MyApp` と `handleMessage` は利用側で実装する。登録済みハンドルを上書きせず、解除してから再登録する。
画面外でも受信したい機能は、画面より長く生存するサービスが起動時に購読を登録する。
画面遷移自体は自動解除しない。登録した所有者が解除を担当する。

## 動作と制約

- 購読は完全一致、QoS 1。`+` / `#` は未対応。最大 16 登録で、テストも 1 登録を使用する。
- 同じトピックへの複数登録はブローカー上で共有する。最後の登録が解除されると購読解除を要求する。
- `subscribe` の成功はローカル登録の完了。`subscribed(handle)` が true になると SUBACK による承認済みである。
- 再接続時は登録済みトピックを再購読する。拒否された購読は 5 秒間隔で再試行する。
- payload はバイナリとして長さ付きで扱い、最大 1,024 バイト。受信断片は結合後に 1 回のコールバックへ渡す。
- トピックは UTF-8 で最大 256 バイト、区切り `/` は最大 7 個。発行の QoS は 0 / 1 に対応する。
- 送信・受信キューは各 4 件。送信キュー満杯は `ESP_ERR_NO_MEM`、切断中は `ESP_ERR_INVALID_STATE` を返す。
- 受信キュー満杯またはサイズ超過はログを残して破棄する。未登録の受信内容はアプリへ配送しない。
- `publish` の `ESP_OK` はキュー受付であり、配送保証ではない。ESP-MQTT outbox は 8 KiB に制限し、拒否はログに記録する。
- 中央キュー内で切断をまたいだ送信は破棄する。既に ESP-MQTT に渡した QoS 1 メッセージは再送される場合がある。
- QoS 1 の重複やデバイスでの処理完了確認が必要な場合は、payload に要求 ID を含めてアプリ側で管理する。
- 解除後は古い受信キューからその登録を呼び出さない。同じスロットを再登録しても以前の受信は渡さない。
- コールバックと解除は同期する。他のタスクから解除する場合は、コールバックが必要とする別のロックを保持して待たない。

管理ワーカーのスタックは 4 KiB、ESP-MQTT の既定スタックは 6 KiB。バッファと登録表は PSRAM を使用する。
天気の HTTPS 通信と並行して動作するため、内部 RAM の余裕は実機ログで確認する。
起動時に初期化に失敗した場合は原因を修正して再起動する。

## 検証

ホスト上の断片結合・サイズ境界・UTF-8 検証:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -I main main/core/tests/mqtt_routing_test.cpp -o /tmp/mqtt-routing-test
/tmp/mqtt-routing-test
idf.py build
```

実機では 2 登録で同一トピックを受信し、一方を解除しても他方が受信することを確認する。
さらに Wi-Fi 再接続、画面退出直前の受信、天気との同時通信を確認する。
