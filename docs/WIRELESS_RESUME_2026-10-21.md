# 無線版の開発再開と実機導入

> 0.4系の時点別履歴。現在は0.5系で解決済みの項目があるため、再開は [現行要約](RESUME.md) と [現行方式](HOTSPOT_TRUST.md) を優先する。

2026-10-21 / R08。対象: AtomS3R + Atomic Voice Base A149、Arduino-ESP32 / PlatformIO、Windowsホスト0.4.0-preview.2（APIキー再入力後の手動会話成功、呼びかけ再登録を進行中）。

## GitHubとの整合性・スライド

`git ls-remote origin refs/heads/main` でGitHubのmainが `3283ad2d70c6fa6839aeb9398b76c075f7dddb88` と確認でき、ローカルの開発基点と一致。ローカルmainにのみスライド保管の `cd58beb` が1件追加された状態から再開。未公開差分はスライドとその保管手順・README・TODO・除外解除の5ファイルで、開発コードの競合はなかった。

GitHub CLIの既定認証はHTTP 401。Gitによるリモート参照照会は成功した。今回pushやPR操作は行っていない。

`docs/architecture-slide.pptx` はGitへ保存済み。SHA256 `3DE43ECC02C08145EDB15DF4FCCD4E87C1956B746A0DAD59C9CE26D07A27C241` を再確認。内容は変更していない。詳細は [スライド保管手順](ARCHITECTURE_SLIDE_PRESERVATION.md)。

## PC上の再検証と変更

- Python: `python -m unittest discover -s tests -p 'test_*.py' -v`、34件成功。TEMP/TMPは既存の案件内 `.pio/merge-check-temp` を使用。
- Node24.15.0: server内で `node node_modules/typescript/bin/tsc` と `node --test --test-timeout=10000 dist/test/*.test.js`、28件成功。TLS認証・模擬PCM往復を含む。外部API未使用。
- C++: `python tools/run_host_tests.py --suite <suite>`、rules27 / gemini-ui6 / face-ui27 / wake-storage29 / wake-enrollment13 / wake-enrollment-ui28 / wifi-ui4 / wireless-link10、計144項目成功。
- Windows MSVCの警告をエラーにする設定でwireless-linkが `strncpy` の非推奨警告C4996により停止した。`tests/wireless_stubs/Fakes.h` の代替strlcpyを、長さを制限したmemcpyと終端文字の設定へ変更。警告抑制は追加せず、再実行成功。実機用コードは変更していない。
- 配布EXEの `--ui-self-test` は10項目PASS、apiCalls=0。`.pio/device-check/wireless-resume-host-self-test-20261021-r01.json` に保存。
- 配布manifestのランチャーソース6件は5件がバイト一致、`tools/run_lan_probe.py` はCRLFへ正規化すると一致。server/package-lock.jsonも一致。

## 再ビルド・固定候補

前記録の `.pio/firmware-snapshots/gemini-wireless-v1.bin` は再開時に存在しなかったため、現在の追跡ソースから再ビルドした。前回候補と同一ハッシュとは扱わない。

PlatformIO `run -e atoms3r-gemini-device` 成功。固定依存は `platformio.ini`（Espressif32 6.11.0、Arduino-ESP32 2.0.17、M5Unified 0.2.26、M5GFX 0.2.33、WiFiManager 2.0.17、WebSockets 2.7.3等）に記載。PlatformIO管理領域のロック更新のみ追加権限で実行。

- 候補: `.pio/firmware-snapshots/gemini-wireless-resume-20261021-r01.bin`
- サイズ: 1,606,672 bytes
- SHA256: `16799F01945F35AD0723AE4F0B6E0FC9E62DC9881951159A927E6A7E4C4A60E3`
- RAM: 58,872 / 327,680 bytes、ELF Flash: 1,606,301 / 3,145,728 bytes。

## 実機導入

利用者が候補のアプリ限定導入と専用認証鍵のNVS保存を明示承認。USB接続依頼後にCOM4（VID0x303A/PID0x1001）を確認。

`tools/inspect_device_partitions.py` で実機パーティション表を確認し、`.pio/device-check/partitions-wireless-resume-20261021-r01.bin` に保存。app0は0x10000、容量0x300000。NVSは0x9000、SPIFFSは0x610000。

`tools/flash_application_only.py` に候補・SHA256・実機表を指定し、COM4の0x10000へ書込み成功。`Hash of data verified` を確認。書換え範囲は0x10000〜0x198FFFで、NVS/SPIFFS/パーティション表は対象外。実際の設定・登録読込みは起動応答で別途確認する。

書込み直後のGEM INFO照会では応答を取得できず、利用者へリセット短押しを依頼した。利用者の実施回答後、COM4で以下を確認し、`.pio/device-check/wireless-resume-boot-20261021-r03.json` へ固定状態と数値のみ保存した。生ログ・認証鍵は取得・保存していない。

- `GEM READY v=2 input=16000 output=24000 mono=1 bits=16`: アプリ起動・音声形式。
- `GEM WAKE available=1`: 保存済み呼びかけテンプレートの読込み。実際の検出精度は別途。
- `GEM LINK TLS`: 無線TLSモード。
- `GEM STATE configured=1 wifi=1 pc=0`: Wi-Fi接続済み、PC認証接続は未成立。ホットスポット接続済みという意味ではない。
- `GEM UI stage=3 mode=2 asleep=1 mic=0`: スリープ・マイク停止。
- `GEM RESULT NONE`: この起動で会話試験未実施。20秒の観察中に起動障害を示す固定文字列は検出されなかった。

PCのWi-Fi Direct仮想アダプタは当初Downだった。利用者がホットスポットをオンのままと回答した後の再確認で、`ローカル エリア接続* 2`（ifIndex6）がUp、IPv4 `192.168.137.1/24` がPreferredになったことを確認。2.4GHz設定は利用者へ依頼済みで、帯域の独立確認は未実施。

利用者へWiFiManagerで端末の接続先をPCホットスポットへ変更する手順を案内し、「登録コードが表示された」との回答を受領。SSID・パスワード・登録コードは取得していない。この回答だけではPCとのTLS認証成功とは扱わない。Windowsホスト0.4.0-preview.1の起動、Wi-Fi方式と192.168.137.1の選択、コードとAPIキーのホスト画面への直接入力・開始を案内し、結果待ち。

## ホスト開始後の接続切り分け

利用者はホスト開始後も端末が接続しないと回答。PCのTCP48443は192.168.137.1でListenしており、実行ファイルはユーザー別のハッシュ付き固定パスのNode。端末のGEM INFOはTLS/wifi=1/pc=0/WAKE available=1/asleep=1/mic=0で、PCとの認証接続は未成立だった。

対象Nodeの既存受信規則を文字列で確認すると、TCP・UDPともEnabled=True、Inbound、Allow、Private/Publicだった。当初、ActionのJSON数値2をBlockと読み違えて利用者へ拒否規則と説明したが、文字列の再確認後に訂正した。Windows受信規則は変更していない。変更準備として編集したファイアウォール用スクリプトは元へ戻し、Git差分0を確認。既存の許可規則はホットスポット限定ではないが、ホスト自身の待受アドレスは192.168.137.1である。

利用者がWindowsホットスポットの接続デバイス数を0台と回答。端末がPCホットスポットへ未接続であることが、現時点の阻害要因。wifi=1だけでは接続先を特定できず、以前のWi-Fiへ接続している可能性は推測に留める。

利用者へ画面ボタンを押しながらリセットする操作を案内し、スマホのWi-Fi一覧にStackchan-Setupが表示されたことを確認。スマホからWiFiManagerを開き、PCホットスポット画面と同じネットワーク名・パスワードを直接設定するよう案内中。スマホ自身をPCホットスポットへ接続せず、接続台数1台への変化を確認する。SSID・パスワード・端末認証コードは取得しない。

## TLS認証成功・実質問の未解決事項

PCホットスポットと同じネットワークへ設定し直した後、利用者は接続デバイス数1台と回答。USBの固定状態でも `GEM WIFI CONNECTED`、`GEM PAIR SAVED`、`GEM LINK TLS`、`GEM STATE configured=1 wifi=1 pc=1` を確認。認証情報の保存とTLSによるPC認証接続が成立した。記録は `.pio/device-check/wireless-join-20261021-r01.json`。Windowsの受信規則は変更していない。

利用者は待受の顔になるが呼びかけても開始音が鳴らないと回答。観測でもmode=4/asleep=0/mic=1、呼びかけのMATCHは未取得。登録の上書きやしきい値変更は行っていない。

手動質問では主表示が `Release to ask` となり、ビープ後の発話後に `Not ready` が出るとの利用者回答。以前の案内の `Release to record` は補足表示の文字列であり、主表示と区別して訂正した。実ログは以下で、入力音声のPC送信後まで進んだが回答音声は得られなかった。

- gen1: FAIL、input=24,320、output=0、frames=0、ms=4,543、played=0、fault=3。
- gen2: FAIL、input=25,280、output=0、frames=0、ms=4,860、played=0。
- `.pio/device-check/wireless-manual-question-20261021-r01.json` に固定状態と数値のみ保存。音声・会話本文・キーは保存しない。

fault=3は端末側のプロトコル失敗分類であり、それだけでは認証・利用枠・モデル・通信などGemini側の原因を確定できない。ホスト0.4.0-preview.1はturn-failedの原因分類を捨てていたため、0.4.0-preview.2で既存の安全な分類を引き継ぐよう改善した。未知のコード・生のエラー本文・キーは表示しない。新規Python2件とNode1件の回帰を追加し、実装前の失敗と実装後の成功を確認。さらに配布フォルダとアプリの版番号の整合性回帰を追加し、最終Python37件/Node29件成功。呼びかけ検出と回答取得失敗を別々に扱い、現在はいずれも未解決。

0.4.0-preview.2の最終EXEは自己診断10項目PASS、apiCalls=0。成果物 `releases/StackchanHost-0.4.0-preview.2-win-x64/StackchanHost.exe`、53,208,913 bytes、SHA256 `afa3bbaaa09141dbd918c1269ad4acd35df08b8bcd22d832fa7c9bdd52db5bd8`。ランチャー6ソースのmanifestハッシュ一致、配布入力1367件、機密名の混入0を確認。旧0.4.0-preview.1 EXEのSHA256 `dfea1b5bb6dcd7bdbcdcbef81350b21d3445283d985aa309eeb781ba74265073` は変化なし。保存済み設定は読取り・書換えせず、旧ホスト停止/新版起動と1回の手動質問を利用者へ案内中。

ビルド初回は失敗ログ出力のcp932エラーにより原因表示が止まった。UTF-8で再実行し、ビルド側だけ旧版番号が固定され旧EXEの更新を試みてWinError5になっていたと確認。版番号をsettings.VERSIONへ共通化して別フォルダへ生成し直した。旧EXEはハッシュで保持を確認。アプリの自己診断では外部APIを使っていない。

## 新版での認証エラー分類

利用者が0.4.0-preview.2で手動質問した結果、「Gemini APIキーの認証に失敗しました。キーを確認してください。」および「端末を認証して接続しました。質問できます。」と報告。前者はGemini問い合わせのAUTH分類、後者は端末とPC間のTLS認証であり、別の認証段階。矛盾や回答成功とは扱わない。HTTP状態番号は今回の報告に含まれておらず未取得。

回答取得失敗はGeminiのAPIキー認証で阻害されていると判明した。キーの入力誤り・失効などの詳細理由は未確認。キーの取得・復号・ログ保存は行わず、利用者へホスト停止、キーのホスト画面への直接再入力、再開後の手動質問1回を依頼中。新たなファームウェア変更・登録上書き・Windows受信規則変更は行わない。呼びかけ検出は別問題として未解決のまま保持。

## API認証復旧・手動会話成功

利用者がAPIキーの直接再入力手順後に「回答音声が聞こえた」と回答。COM4のGEM INFOでも `GEM RESULT PASS gen=1 input=24000 output=64560 frames=135 ms=18149 played=1` を確認。TLS/PAIR READY/wifi=1/pc=1/待受mode=4/asleep=0/mic=1。記録は `.pio/device-check/wireless-manual-success-20261021-r01.json`。実機音声送信と回答再生は成功した。口パクの目視確認、長時間安定性、再起動後保持の合格とは区別する。APIキーそのものは取得していない。

その後の呼びかけ2回は、利用者がいずれも開始音なしと回答。観測には `Wake distance: 387`、`545`、後続に`513`、`488`があり、MATCHはなかった。音声取得から照合までは動作しており、保存パターンとの一致判定で止まっていると推測する。各距離と個々の発話の対応、端末内の実際のしきい値は未取得。記録は `.pio/device-check/wireless-wake-check-20261021-r01.json`。しきい値変更は行っていない。

利用者が「3回の再登録を進める」と明示承認。成功前は旧登録保持、整合確認と保存に成功した際に置き換える方針を説明済み。

スリープでの約5秒長押し登録を案内したが、利用者はSettings表示後も押し続け、Pair with PCへ移行したと回答。端末はPAIR REQUIRED、ENROLL result=NONE/STEP none、WAKE available=1で、呼びかけの3回登録はまだ開始されていない。10秒側の端末認証鍵更新操作へ進んだ状態。Settingsは共通の主表示で、登録案内のRelease: learn callは補足表示のため、以後は表示待ちでなく5〜6秒で離す具体的な操作へ改める。

USB観測は一度ClearCommErrorで切断され、同じCOM4への再接続でPAIR REQUIREDを取得。原因究明は進めていない。利用者へホスト停止→現在の端末コードをホストへ直接入力→再開を案内し、PC認証の回復を待つ。旧呼びかけの置換え・新ファームウェア導入・Windows受信規則変更は行っていない。

## 次の検証

### アドレス選択の一時的な阻害と復帰

利用者から「192.168.18.27、192.168.18.8しか選択できない」と報告。実際の `desktop/network.py` の `local_addresses()` を実行し、この2件だけを返すことを再現した。192.168.137.1を含むことの確認は `HOTSPOT_ADDRESS_NOT_AVAILABLE` で失敗。同時にOSのWi-Fi DirectアダプタはDisconnected、IPv4は169.254.*で、先に確認した192.168.137.1は存在しなかった。

アプリの一覧検出結果はOSの接続状態と一致していたため、アドレスの手入力やアプリの検出処理変更は行わない。ホットスポットが稼働状態を失った原因は未確定。無接続時の省電力自動停止は仮説として、利用者へ再有効化と該当設定があれば無効化、その直後の再検出を依頼した。利用者は「192.168.137.1が表示された」と回答。同じ `local_addresses()` の確認を再実行し、192.168.137.1を含み `HOTSPOT_ADDRESS_AVAILABLE` へ復帰した。自動停止設定を実際に変更したかは未確認で、原因確定とは扱わない。192.168.18.*をホットスポット側として代用しない。現在は192.168.137.1を選んでホスト開始する段階。

1. 起動、TLSモード、Wi-Fi、呼びかけ登録の読込み、スリープ中マイク停止は確認済み。
2. PCホットスポット稼働を確認済み。端末接続先の変更と登録コード表示は利用者回答あり。実機TLS認証はホスト開始後に確認。
3. 登録コードは端末画面からPCホストへ直接入力する。チャット・記録へ送らない。
4. 必要な受信許可は `tools/configure_wireless_firewall.ps1` の計画表示でホットスポット側だけを対象に確認してから適用する。
5. 手動質問はAPIキー再入力後に実機PASS/played=1と利用者聴取を確認済み。現在は10秒長押しで更新されたコードをPCへ登録し直す段階。その後、承認済みの呼びかけ3回再登録、呼びかけ会話、回答後の再受付、再起動後の自動再接続を個別に確認する。PC上の模擬試験と区別し、未実施項目を合格にしない。

ホットスポットは利用者が有効化した。CodexによるWindows設定変更・受信規則変更は未実施。利用者の物理操作による実機質問音声送信2件を確認したが、Gemini側の実際の呼出し数・失敗原因はまだ取得していない。観測側はGEM INFOのみで録音を開始しない。
