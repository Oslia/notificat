# AWS IoT Core MQTT テスト

`main/core/SystemMQTT.cpp` は、設定で有効化する実機診断コードである。
Wi-Fi 接続と SNTP 同期の完了後、MQTT 3.1.1、相互 TLS 認証、ポート 8883 で接続する。
サーバー証明書とホスト名の検証は有効のまま使用する。

## 認証情報の保存先

認証情報は、通常の設定用 `nvs` とは別の `cert_nvs` パーティションに保存する。

| 項目 | 値 |
| --- | --- |
| パーティション名 | `cert_nvs` |
| 種別 / サブタイプ | `data` / `nvs` |
| オフセット | `0x610000` |
| サイズ | `0x6000`（24 KiB） |
| NVS namespace | `cert_key` |
| NVS キー | `cert`（デバイス証明書）、`key`（秘密鍵）、`ca1`（Amazon Root CA 1） |
| 保存形式 | NUL 終端の PEM 文字列 |

OTA スロットの直後に `cert_nvs` を配置し、その後の `0x616000` に `storage` を配置する。
既存の `nvs` と OTA スロットの位置・サイズ、および `storage` のサイズは変更しない。
全パーティションは 16 MiB フラッシュの範囲内に収まる。パーティションの分離自体は暗号化ではない。

### イメージの生成

プロジェクトルートで、ESP-IDF 環境を有効化して実行する。
ローカルの `cert_nvs.csv` は次の構成を使用する。パスは実際のファイル名に置き換える。

```csv
key,type,encoding,value
cert_key,namespace,,
cert,file,string,./certs/<device-certificate>.pem.crt
key,file,string,./certs/<device-private>.pem.key
ca1,file,string,./certs/AmazonRootCA1.pem
```

```sh
python "$IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py" generate cert_nvs.csv cert_nvs.bin 0x6000
```

`certs/`、`cert_nvs.csv`、生成した `cert_nvs.bin` は Git の管理対象外である。
生成イメージにも秘密鍵が含まれるため、配布用ファームウェアには同梱しない。
以前の `key_nvs.bin` は使わず、専用パーティションのサイズで再生成する。

### 実機への書き込み

1. `idf.py build` で新しいパーティション表とファームウェアを生成する。
2. `idf.py -p <PORT> flash` で書き込む。全フラッシュ消去は不要である。
3. モニターを閉じた状態で、認証情報を専用領域に書き込む。

```sh
python -m esptool --chip esp32s3 --port <PORT> write_flash 0x610000 cert_nvs.bin
idf.py -p <PORT> monitor
```

`<PORT>` は実際のシリアルポートに置き換える。上記のアドレスは本リポジトリの
`partitions.csv` に対応する。書き込みは `cert_nvs` 内の認証情報を置き換える。
通常の `idf.py flash` には認証イメージを含めないため、以後のファームウェア更新で
認証情報を書き直す必要はない。全フラッシュ消去を行った場合は再度書き込む。

以前の配置から移行する場合は、`storage` の開始位置が変わるため、パーティション表だけでなく
FATFS イメージも含めて `idf.py flash` を実行し、その後で認証情報を書き込む。
旧配置の認証情報は自動移行されない。旧 `storage` に実機で追加したファイルがある場合は、
書き込み前に退避する。通常の Wi-Fi・アラーム・地域設定は位置が変わらない `nvs` に保持される。

## AWS 側の準備

- AWS IoT Data-ATS endpoint のホスト名、client ID、テスト用トピックを用意する。
- デバイス証明書を有効化し、必要な IoT ポリシーを関連付ける。
- 別のクライアントと同じ client ID を同時に使用しない。

ポリシーでは、使用するリージョンとアカウントに対して次のリソースを許可する。

| Action | Resource ARN の末尾 |
| --- | --- |
| `iot:Connect` | `client/<client-id>` |
| `iot:Publish`, `iot:Receive` | `topic/<test-topic>` |
| `iot:Subscribe` | `topicfilter/<test-topic>` |

ARN の先頭は `arn:aws:iot:<region>:<account-id>:` である。

## テスト設定と実行

1. `idf.py menuconfig` → `Notificat MQTT` でテストを有効にする。
2. Endpoint には `https://` やポート番号を付けず、ホスト名を入力する。
3. ポリシーで許可した client ID と、ワイルドカードを含まないトピックを入力する。
4. NVS パーティション名を `cert_nvs` にする。以前の設定が `nvs` の場合は変更する。
5. ビルドと書き込みを行い、AWS IoT コンソールの MQTT test client でも同じトピックを購読する。
6. デバイスの Settings で Wi-Fi に接続する。

MQTT テストと天気サービスは同時に動作する。天気は従来の専用ワーカーで HTTPS 通信を行い、
MQTT は ESP-MQTT の内部タスクで接続を維持する。テストを無効にしても天気サービスは動作する。
実機では MQTT の送受信中に天気が更新されること、Wi-Fi 再接続後に両方が復帰すること、
メモリ不足や UI の停止が発生しないことを確認する。

## 成功の判定

- `CONNECTED`: TLS / MQTT 接続が成功した。
- `SUBACK`: テスト用トピックの購読が承認された。
- `PUBACK`: QoS 1 のテスト送信をブローカーが確認した。
- `RX`: 同じトピックに送った `{"message":"hello from notificat"}` を受信した。
- コンソールで同じメッセージを確認し、`{"source":"console","test":"reply"}` を送信して、
  デバイスの `RX` ログでも確認する。PUBACK と RX の順序は前後する場合がある。

接続ごとに一度送信し、受信メッセージには自動応答しない。
最大 1,024 バイトのメッセージは断片を結合して記録し、それを超えるメッセージは破棄する。
受信内容は診断ログのみに表示し、既存の通知 UI には渡さない。
テスト用トピックには秘密情報を送信しない。

## 問題の切り分け

接続前の `Endpoint=... client_id=...` と `Device certificate SHA256=...` を確認する。
指紋は実機の NVS から読み込んだ証明書の DER データから計算するため、古い認証イメージが
残っていないか照合できる。秘密鍵は出力しない。
`tls=0x8008` は相手による接続終了を示す。このログだけでは認証・認可の失敗理由は特定できない。
CONNACK のコードは MQTT 接続拒否イベントの場合のみ表示する。
AWS 側の拒否理由は、対象リージョンで IoT Core の Logs を有効化し、CloudWatch の
`AWSIotLogsV2`（または指定したロググループ）で確認する。
失敗時の `eventType`、`clientId`、`principalId`、`reason`、`details` を照合する。

- `Waiting for Wi-Fi and SNTP` で止まる場合は、Wi-Fi 接続と時刻同期を確認する。
- 認証情報の読み込みに失敗する場合は、パーティション表、書き込み先、NVS キーと文字列形式を確認する。
- 接続エラーの場合は、ログの TLS エラーと CONNACK、endpoint、証明書の有効状態、client ID の権限を確認する。
- 購読または受信のみ失敗する場合は、`Subscribe` の topicfilter と `Receive` の topic 権限を確認する。
- 初期化失敗後に設定や認証情報を修正した場合は再起動する。接続断は MQTT クライアントが再試行する。

`Error create mqtt task` は、接続認証ではなく ESP-MQTT のタスク生成段階の失敗である。
現在の SDK では MQTT タスクの既定スタック 6 KiB と管理領域を内部 RAM に確保する。
認証情報の PEM 文字列は PSRAM に優先配置する。タスク生成に失敗した場合は、
天気通信の一時的なメモリ使用が解消する余地を設けるため、5 秒間隔で最大 6 回試行する。
`Before MQTT task start` と `MQTT task start failed` の `internal free`、`largest`、
`minimum` はバイト単位であり、内部 RAM の空き合計・最大連続領域・最小空き容量を示す。
PSRAM の空きだけではタスク生成の可否を判断できない。再試行後も失敗する場合は、
これらのログから常時使用量と断片化を調査する。6 回とも失敗した場合は初期化を終了し、
クライアントと認証情報を解放する。

参考: [AWS 通信プロトコル](https://docs.aws.amazon.com/iot/latest/developerguide/protocols.html)、
[AWS 購読権限とテスト](https://docs.aws.amazon.com/iot/latest/developerguide/iot-dc-testconn-subscribe.html)。

共通 API とアプリ側の登録方法は [アプリ共通 MQTT サービス](mqtt.md) を参照する。
