# Windowsからの端末AP設定 実証記録

2026-10-25 / R01 / ブランチ`codex/host-ap-provisioning`。
対象仕様: [HOST_AP_PROVISIONING_R01.md](HOST_AP_PROVISIONING_R01.md)。基準コミットは`260428b`。

## 結果

**このPCの有線インターネット構成で、スマホを使わず、端末APへの一時接続→WiFiManagerへの設定送信→PCのWi-Fi復帰→ホットスポット参加→既存ホストへの接続を基本確認した。**

設定送信後にAP接続を20秒維持した試行で、受付応答、接続台数0→1、PCの復帰、ホットスポット設定の前後一致を確認。利用者は端末表示が「Start PC Service」であること、続いて既存ホストを開始した後に「PC Online / Sleeping」へ変わったことを確認した。

これは検証用ツールによる実証。**製品ホストの新ボタンへの組込みは未実施**で、RC1のホストコード・配布EXE・端末ファームウェアは変更していない。会話・音声・口パク・再起動後の接続は今回試験していない。

## 許可と変更範囲

- 利用者が次段階への続行を指示し、端末をAP待機、PCをホットスポット有効にした。
- AP一時接続・読取りを実証後、利用者が明示的に「端末の設定送信を許可します」と指示。
- 既存WiFiManagerの`POST /wifisave`へ接続先SSID・パスワードだけを送信。新旧の接続先情報の内容は保存・表示しない。
- AP名・APパスワード、ファームウェア、登録済み呼びかけ、NVS/SPIFFSの他の内容は変更対象外。Wi-Fi資格情報の従来経路による保存以外に端末への書込み操作は行っていない。
- PCでは一時プロファイルで接続し、プロファイルの永続保存APIは呼ばない。毎回`finally`で元のWi-Fi未接続状態へ戻した。ホットスポットの停止・再設定、端末リセット、USB制御・監視、クラウドAPI呼出しはツールから行っていない。
- ホットスポットの手動有効化・再有効化、省電力自動オフの無効化、既存ホスト開始は利用者が行った。

## 対象環境と依存

| 項目 | 確認内容 |
|---|---|
| PC OS | Windows NTバージョン10.0.26200.0 |
| 診断ランタイム | Python 3.15.1 x64、Windows PowerShell 5.1、Python標準ライブラリのみ |
| 実行権限 | Windows管理者昇格なし。Codexの制限外で実行許可を得た読取り・一時接続・送信コマンドで確認 |
| 上流 | 有線Ethernet、InternetAccess |
| Wi-Fi | Native Wi-Fiが列挙した物理インターフェース1件、初期状態は未接続 |
| ホットスポット | 2.4GHz / WPA2、初期接続台数0。資格情報をメモリ内で取得できることを確認 |
| 端末 | 利用者がAP待機させた既存AtomS3R + Atomic Voice Base。ファームウェア再取得・書込みなし |

ドライバ型番・版数は今回取得していない。対象PCでの観測として扱い、同じWindowsビルドの全機種での動作保証とはしない。診断に用いたPythonと、RC1配布EXEのランタイムは異なるため、EXE組込み後の再検証が必要。

## 試行と観測

| 試行 | 結果 |
|---|---|
| 制限内のWindows API読取り | WinRT型は利用可能だがネットワーク取得で`0x80073D54`。制限外・通常ユーザーでは取得成功。Windows API自体が非対応という判断はしない。 |
| AP探索 | 対象SSIDのエントリ1件・BSSID数1・接続可能を確認。他SSID・MACは報告しない。 |
| AP読取り1 | AP接続・IPv4取得・PC復帰成功。フォーム検証は診断側が相対actionを扱わなかったため不合格。ローカル固定版ソースに合わせて修正。 |
| AP読取り2 | HTTP200、POSTフォームの`s`/`p`確認に成功。切替前・AP接続中・復帰後の有線InternetAccessとホットスポットOnを確認。 |
| 設定送信の事前確認 | ホットスポットOffを検出し、切替・送信前に停止。利用者が再有効化し、自動オフ設定も無効化。停止原因は未確定。 |
| 設定送信1 | 1回POST、HTTP200、PC復帰成功。ただし45秒の観測中に接続台数は増えず、端末はWiFi setupのままと利用者確認。この時点ではWi-Fi接続成功と扱わない。 |
| 失敗後のAP読取り | 端末側スキャンに対象ホットスポットあり、SSID入力欄がPCの設定と一致。Windows側はWPA2。認証失敗・AP不在・一般接続失敗の既知メッセージはページに現れず、原因は特定できなかった。 |
| 設定送信2 | 同じ設定を1回POST。HTTP200と`Saving Credentials`応答を確認後、AP接続を20秒維持。PC復帰後に接続台数0→1。ホットスポット資格情報の前後一致もメモリ内比較で確認。 |
| 表示・既存ホスト接続 | 利用者の最新確認は、まずStart PC Service、その後既存ホストの「接続して開始」操作でPC Online / Sleeping。接続台数だけとは別の利用者確認として記録。 |

WiFiManagerの`processConfigPortal()`には受付応答後の約2秒待機があり、現行端末は接続タイムアウト15秒。20秒維持はこの処理時間を覆ってPCの無線状態を安定させる試験条件。**直後離脱1回失敗と20秒維持1回成功の比較だけでは、待機不足が根本原因だったとは断定できない。** 無線チャネルやドライバの過渡状態は未測定。新実装は即時切断を避け、端末到達の確認を含む有限時間の待機を設けて再試験する。

ホットスポットOn/InternetAccessは複数時点のサンプル観測であり、パケット断のない連続通信を証明していない。端末の同名AP/偽装APに対する個体認証の限界も仕様R01のとおり残る。

## 再現手順と診断物

プロジェクトルートで、Windowsの通常ユーザーから実行する。診断出力は状態・件数・成否だけ。例外本文・HTTP本文・資格情報を出力しない。

- [Windows設定読取り](../tools/probe_windows_provisioning.ps1): `powershell.exe -NoProfile -NonInteractive -File tools/probe_windows_provisioning.ps1`。接続・設定変更なし。
- [AP診断](../tools/probe_stackchan_ap.py): `python tools/probe_stackchan_ap.py`は探索のみ。Wi-Fi APIの権限によってはWindowsの位置情報許可が必要。
- `python tools/probe_stackchan_ap.py --connect-readonly`: 一時接続→フォーム/スキャン結果の成否だけ読取り→未接続へ復帰。設定送信なし。
- `python tools/probe_stackchan_ap.py --provision-once`: **明示的に許可された対象にだけ実行する設定変更操作**。稼働中ホットスポット情報を1回送信し、20秒維持して復帰、最大45秒程度の参加観測を行う。コマンド内部で自動再送はしない。タイムアウトは未確定扱い。
- [オフライン試験](../tests/test_provisioning_probe.py): `python -m unittest discover -s tests -p test_provisioning_probe.py -v`。12件成功。フォーム送信先の制限、Win32構造体サイズ、失敗時復帰、複数APの拒否、事前条件不成立、未検証フォームからの送信禁止、受付本文確認、応答不明時の非再送、報告への資格情報非混入を確認。

この診断は有線InternetAccess、ホットスポットOn/2.4GHz、物理Wi-Fi1件が未接続、対象BSSID1件を要求する。上流Wi-Fiを切り替えて復元する実装ではない。プロセス強制終了・OS障害時の自動復帰も保証していない。製品UIからはまだ呼び出さない。

記録・仕様・README・ToDo・RESUMEのローカルリンク46件は不明0件。PowerShell構文解析エラー0件、差分の空白エラーなし。既知のAPIキー/トークン形式の検査は0件で、実際の個人設定を読み出して比較する監査は行っていない。既存個人設定ファイルは変更対象に含めていない。

## 次の作業と未検証

1. この有線PC構成を初期対応範囲とするか決定し、製品ホストの接続処理・単一候補の自動選択・待受開始順序・中止と復帰へ組み込む。
2. 端末の参加・PC到達を段階表示し、AP消失、保存応答、PC自身のヘルスチェックを接続成功と誤認しない。
3. ホストの既存声・キー設定を保った配布EXEで実機試験する。ホストを先に開始する本来の順序、繰り返し、設定失敗、アプリ中止、端末通常再起動、会話/音声/口パク/待受復帰を別々に確認する。
4. 上流Wi-Fiの復元・別PC・複数アダプタは未対応/未検証として別途扱う。端末ファームウェア変更が必要という結論には至っていない。

一次資料: ローカルWiFiManager 2.0.17の`handleWifi()`/`handleWifiSave()`/`processConfigPortal()`、[Microsoft Native Wi-Fiの接続](https://learn.microsoft.com/en-us/windows/win32/api/wlanapi/nf-wlanapi-wlanconnect)、[一時プロファイル](https://learn.microsoft.com/en-us/windows/win32/api/wlanapi/ne-wlanapi-wlan_connection_mode)、[ネットワーク構造体](https://learn.microsoft.com/en-us/windows/win32/api/wlanapi/ns-wlanapi-wlan_available_network)、[ホットスポット管理](https://learn.microsoft.com/en-us/uwp/api/windows.networking.networkoperators.networkoperatortetheringmanager)。
