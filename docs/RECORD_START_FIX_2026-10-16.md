# 録音開始前にRelease表示へ残る不具合

版: R01 / 日付: 2026-10-16（日本時間）。対象: COM4のAtomS3R + Atomic Voice Base、Gemini端末版R05。

目的: 「長押しして離してもRelease to recordのまま、次の短押しで音だけ鳴って初期画面へ戻る」というユーザー報告を修正する。案件ルートは `C:\Users\thoma\SynologyDrive\H\Doc\Src\Codex\projects\ai-stackchan\AI_StackChan_Ex\Stackchan-chatgpt`。

## 再現と原因

実際の `src/LocalGemini.cpp` と `AudioDevice.cpp` を、ボタン・Wi-Fi・時刻・画面だけを置換したC++ホスト環境で実行する `tests/gemini_ui_test.cpp` を追加した。`python tools/run_host_tests.py --suite gemini-ui` で、2.1秒長押し→離す→短押しを駆動する。修正前は未設定/切断の2条件とも次の結果になり、繰り返しても再現した。

```text
FAIL not configured: hold/release stuck=1, micRequests=0, nextClickOnlyBeepsAndRestoresIdle=1
FAIL WiFi lost: hold/release stuck=1, micRequests=0, nextClickOnlyBeepsAndRestoresIdle=1
```

原因は `run(true)` の設定/Wi-Fi確認で早期returnする際、直前のRelease表示を更新していないこと。録音も発話判定も始まらず、次の短押しだけが確認音とidle表示を実行するため症状が一致する。PC一時サービスは今回の確認時に既に終了し、前回の許可済み数値ログに録音/API処理はなかった。ただし、ユーザー操作時のRAM設定消失・Wi-Fi切断のどちらが発生していたかまでは当時のログだけで確定できない。

ボタン離し検出の問題も候補とした。押下が継続するホスト試験は上限なく待つことを再現したため、これも10秒で案内へ移るようにした。実際のボタン接点/入力の故障を確認したという意味ではない。

## 修正

- 録音開始条件をRelease表示前と離した後の両方で確認。未設定はStart PC service、切断はWiFi disconnected、PC停止はPC service stoppedを表示し、追加クリックを不要にする。
- PCツールがサーバ稼働中に5秒ごとにGEM PINGを送る。20秒届かなければ端末がPC停止を表示し録音を開始しない。GEM INFOの単なる状態確認はPC稼働扱いにしない。
- ツールの正常停止時はGEM STOPを送り、端末のRAMトークンと設定済み状態をクリアする。異常終了時もheartbeat期限で停止を表示する。初回接続先/トークンは従来どおりUSB/RAMだけで、永続保存しない。
- ボタンを離せない場合はRelease表示の開始から10秒でButton still held / Release and retryを表示。発話しきい値・環境音測定・PCM形式・API処理は変更しない。
- `GEM STATE configured=0/1 wifi=0/1 pc=0/1` と定義済み停止理由だけを公開する。キー・トークン・接続先・音声をログへ出さない。暫定DEBUGログは追加していない。

## 検証と再現

ホスト試験は設定なし、Wi-Fi切断、PCの期限切れ、準備完了から録音要求、heartbeat切れ/復帰/停止、押し続けの6項目すべて成功。準備完了の試験では偽マイクに1回だけ録音を要求し、意図的な録音失敗後も画面を復帰する。実際のGPIO、録音品質、実API成功をこの試験だけで保証しない。

既存C++発話/PCM/環境音の27件も成功。ツールのPython構文と許可リストの検査成功。TypeScript/SDK/プロトコルに変更なし。既存のPC26件合格と、前工程の固定長Gemini成功は各記録を参照。

R05イメージ: `.pio/firmware-snapshots/gemini-device-r05.bin`、1,140,272B、SHA256 `D741F1CE3BA58FFCD5258E69B0CE645C33E53724845029C077C84799D6E74104`。ビルド成功、静的RAM50,308B/Flash1,139,909B。依存・ボード・復旧元は [可変長録音記録](VARIABLE_UTTERANCE_VALIDATION_2026-10-16.md)。

COM4へアプリ限定で書込み、消去0x10000〜0x126fff、esptoolハッシュ検証成功。設定領域と登録データを保持した。APIなしの実機合成入力24,000→出力12,001サンプル/26フレーム、実再生終了まで2,324msで成功。PC稼働中はconfigured=1/wifi=1/pc=1、stopでGEM PC OFFLINEを受信し、再オープン後は0/0/0を確認した。

同じ長押し/離す操作で、ユーザーがSpeak nowへの移行と日本語回答の聴取を確認。環境音RMS65/しきい値750、収集67,840サンプル/4,297msから37,120サンプル（2.32秒）を切り出し、末尾無音で送信。RMS896/peak5,670/clip0。Gemini処理7,907ms、回答56,400サンプル/24kHz/118フレーム、送信開始から実再生終了13,743msでPASS。今回のLive接続は1回。これは短い実質問1回であり、長時間/切断/音質の定量評価ではない。

結果は `.pio/device-check/gemini-start-fixture-r01.json` / `gemini-start-live-r01.json` の数値/状態だけに保存。音声と本文はPCへ保存しない。実質問後にstopし、端末のGEM PC OFFLINEと正常終了を確認済み。サービス停止後のStart PC service表示は仕様で、再試験は起動用ファイルを開く。

最終確認: 元の秘密設定が保存時チェックポイントと一致し、秘密設定/生成物はignoredかつ未追跡。公開候補113ファイルに実キー/保存済み端末トークンの一致0件、Git差分の空白エラーなし。今回の修正はcodex/variable-utteranceへローカル保存し、追加pushはしていない。

予防策: 状態不足で早期returnする分岐にも画面遷移の検査が必要だった。実際のloop/runを実行するホスト回帰試験を追加し、未設定/切断だけでなく正常な録音要求が1回発行されることも検査する。暫定DEBUGログや捨てるべき試作物の残存はない。

起動用ファイルは引き続き案件ルートのStart-Gemini-Device.cmd。PCサービスが起動して端末にGemini PC readyが出てから長押し→離す→開始音後約1秒静かに待つ→Speak now表示後に発話する。サービスは約10分で自動終了する検証用で、終了後は起動用ファイルを再実行する。
