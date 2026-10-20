# ホットスポット接続を信頼するWindowsホスト

2026-10-21 / R03 / ホスト0.5.0-preview.1。目的: WiFiManagerでPCホットスポットへ接続した後に、4行の端末登録コードを転記せずサービスを使う。

## 合意と変更範囲

利用者は、ホットスポットのWi-Fiパスワードで参加を確認するため、接続後は端末を信頼してサービスを開始してよいと明示した。この指示を、従来の端末固有PSK・コード転記によるアプリ認証に優先する。

- Windowsホストは、稼働中のWi-Fi Direct仮想アダプタのIPv4とサブネットを検出する。上流の共有Wi-Fiは通常接続の候補にしない。起動直前にも再確認する。
- Node中継はそのIPv4のTCP48443だけへバインドし、HTTPヘルス確認とWebSocketの両方で接続元IPv4をサブネットに照合する。IPv4-mapped IPv6も正規化して判定する。異なる範囲は403で拒否する。
- 通常通信はHTTP/WS。アプリ独自のTLS、Bearerトークン、端末コード入力は使わない。音声通信の無線区間の保護と参加認証はホットスポットのWi-Fi暗号化・パスワードに委ねる。同じホットスポットへ参加できる機器も利用可能になる設計。
- Gemini APIキーはPCだけで使用し、端末へ渡さない。任意の保存には従来のWindows DPAPIを維持する。
- 端末はDHCPゲートウェイをホストとして自動接続する。192.168.137.1へ固定しない。
- 4行コードの表示・入力欄・PC登録解除ボタン・端末の10秒長押しによるコード更新を廃止。5〜6秒で離す呼びかけ登録は維持する。
- 旧端末認証用のNVS、旧pairing.jsonは読取り・削除・書換えせず残す。Wi-Fi設定と呼びかけテンプレートも保持する。
- USB診断は従来のBearer付き経路を維持。汎用サーバーの旧TLS対応も残すが、通常のホットスポット方式では使用しない。

## 成果物・依存関係

- Windows: `releases/StackchanHost-0.5.0-preview.1-win-x64/StackchanHost.exe`、53,208,620 bytes、SHA256 `881bf7649305218ed8854597bf22a3eca97dfde33fcbfd472f3ed27555f9faa8`。
- 端末候補: `.pio/firmware-snapshots/gemini-hotspot-trust-20261021-r01.bin`、1,601,424 bytes、SHA256 `BF936E8863AE7A11587EE1ABD0E6FAC94B64B9D2F196715AE097E6B97CCE3B98`。
- AtomS3R + Atomic Voice Base A149。Espressif32 6.11.0 / Arduino-ESP32 2.0.17、固定ライブラリはplatformio.ini。
- Windows配布ランタイムはPython3.14.4 / Node24.15.0 / PyInstaller6.22.3。API依存はserver/package-lock.json。旧版EXEと端末候補を保持。

## 再現確認

1. Python: 案件内の既存 `.pio/merge-check-temp` をTEMP/TMPに指定し、`python -m unittest discover -s tests -p 'test_*.py'`。39件成功。一度TEMP指定なしで実行してサンドボックスの一時フォルダ権限により13件失敗したが、案件内TEMPで再実行成功。コードの失敗と区別する。
2. server: `npm test`（Node24.15.0）。32件成功。コード・Bearerなしの模擬PCM往復、HTTP/WSの範囲外拒否、IPv4-mapped IPv6、旧経路回帰を含む。ホスト内ループバックを使い、外部API未使用。
3. C++: tools/run_host_tests.pyの8スイート、計145項目成功。無コードでの初期化・接続・再試行・再起動模擬、NVS/呼びかけ状態非変更、既存録音・登録・UI回帰を含む。
4. PlatformIO: atoms3r / atoms3r-gemini-device / atoms3r-face-demoの3構成ビルド成功。
5. 最終EXE: `--ui-self-test --report <案件内JSON>`。11項目PASS、apiCalls=0。コード入力欄なし、コードなし模擬音声往復、設定保護、GUI配置を確認。`.pio/device-check/hotspot-trust-self-test-20261021-r01.json`。
6. 配布manifestのランチャー6ソースハッシュ一致、配布入力1367ファイル、機密設定名の混入0。旧0.4.0-preview.2 EXEのSHA256不変を確認。

## 実機への適用と残る検証

利用者が旧ホスト停止と、新候補のアプリ限定導入を明示承認。COM4の実機パーティション表を再確認し、`.pio/device-check/partitions-hotspot-trust-20261021-r01.bin` に保存。app0=0x10000/容量0x300000に1,601,424 bytesを書き込み、Hash of data verifiedを確認。書換え範囲0x10000〜0x196FFFで、Wi-Fi・NVS・SPIFFSは消去していない。

利用者のリセット/新ホスト起動後、「コード入力なしで接続できた」との回答を受領。端末も `GEM LINK HOTSPOT`、`GEM STATE configured=1 wifi=1 pc=1`、`GEM WAKE available=1`、スリープ/mic=0を確認。`.pio/device-check/hotspot-trust-boot-20261021-r01.json`。新方式の実機導入・基本自動接続は完了。Windows受信規則は変更していない。

旧PSK端末と新ホストの組合せでは通常接続できないため、端末とホストを対応する版へ移行する。

旧ホスト停止→新端末導入→ホスト0.5起動→Wi-Fi設定済み端末のコードなし接続→手動質問→呼びかけ登録/会話→端末リセット後の自動復帰、の順に確認する。呼びかけ3回の再登録は既に利用者承認済みだが、まだ完了していない。Windowsのホットスポット/ファイアウォールはこの変更で自動操作しない。

### 実機の最終確認

利用者が承認済みの呼びかけ3回再登録を行い、Call saved/スリープ復帰を回答。端末でもENROLL result=SAVED/STEP save、最後の登録音声samples=16960/voiced=6400、最後の登録受付gap=701msを確認。全3回の境界を今回の観測で個別収録したとは扱わない。

再登録後の呼びかけで距離188/MATCHを確認し、質問は `PASS gen=3 input=39360 output=79200 frames=165 ms=22175 played=1`。利用者は呼びかけ反応、回答音声、口の動き、待受復帰の全てを確認した。質問受付gapは観測値700/701ms。記録は `.pio/device-check/hotspot-trust-wake-question-20261021-r01.json`。音声・会話本文・APIキーは保存していない。

ホストとホットスポットを起動したまま端末だけを利用者がリセット。再設定・コード入力なしで自動接続し、呼びかけから回答まで成功したと回答。端末でも起動後のHOTSPOT/wifi=1/pc=1/WAKE available=1、照合距離187/MATCH、質問受付gap=700ms、`PASS gen=1 input=24640 output=44881 frames=94 ms=14150 played=1`を確認。記録は `.pio/device-check/hotspot-trust-reboot-20261021-r01.json`。リセット後のWi-Fiと呼びかけ登録保持、基本会話への復帰を確認済み。

コードなし自動接続、呼びかけ再登録・検出、実会話、端末リセット後の復帰は基本確認完了。長期間の検出精度・耐障害性、物理的な電源断試験、別PCへの移行は未検証。ホストは利用者が通常利用できるよう起動したままとし、観測用USBポートは閉じた。Windows受信規則はこの作業で変更していない。

## 将来のRaspberry Pi移行

ゲートウェイとサブネットを使う通信部分はWindowsの固定IPに依存しない。Piへの移行時はAP構築、起動サービス、画面・キー保存を別途対応する。今回Piの導入・設定は行っていない。
Pi 5には2.4/5GHzの内蔵Wi-Fiがある。上流もWi-Fiにする場合は内蔵+USB1本、またはUSB2本が候補で、AP側にはAPモードとOS対応が必要。一系統AP/STA同時動作は機種・ドライバ・同一チャンネル等の条件付きで、今回の検証対象外。

- [Pi 5公式仕様](https://datasheets.raspberrypi.com/rpi5/raspberry-pi-5-product-brief.pdf)
- [公式ホットスポット手順](https://www.raspberrypi.com/tutorials/host-a-hotel-wifi-hotspot/)
- [RaspAP開発元のAP/STA資料](https://github.com/RaspAP/raspap-docs/blob/master/docs/features-experimental/ap-sta.md)
