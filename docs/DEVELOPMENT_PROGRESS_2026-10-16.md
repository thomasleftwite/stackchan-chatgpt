# 開発進捗: ローカルPC経由のGemini音声応答

版: R01 / 記録日: 2026-10-16（日本時間）

## 目的と対象

M5AtomS3R + Atomic Voice Base A149（旧称Atomic Echo Base）の質問音声をWindows PCで受け、Gemini Liveの回答音声を端末で再生する構成について、会話が成立した段階の成果と残作業を記録する。案件ルートは `C:\Users\thoma\SynologyDrive\H\Doc\Src\Codex\projects\ai-stackchan\AI_StackChan_Ex\Stackchan-chatgpt`。

現在の端末はGemini端末版R05 / protocol v2、USBはCOM4。PCサービスは0.2.0-dev。実装基準は `20f3e05`（可変長録音）と `f5b73aa`（録音開始表示とPC稼働確認の修正）。Arduino / PlatformIOを使用し、SPIFFS・NVS・保存済みWi-Fi・呼びかけ登録を保持してアプリ本体だけ更新した。

## 現在できること

端末マイクの16,000 Hz・16-bit signed little-endian PCM monoを認証付きLAN WebSocketでPCへ送り、PCがGemini Live `gemini-3.8-live`へ中継し、24,000 Hz PCM回答を端末スピーカーで再生する。GoogleのAPIキーはPCにだけ置く。OpenAIキーは不要。

録音は長押し・離す操作で開始し、約1秒の準備と環境音測定後にSpeak nowを表示する。発話末尾の約0.7秒無音で送信し、送信全体は最大5秒、回答は最大10秒。無発話はNo speechで終了し、APIへ送信しない。録音と再生を切り替える1質問1回答の構成で、会話履歴はまだ保持しない。

PCの未設定・Wi-Fi切断・稼働通知の失効時にRelease to recordへ残る不具合を修正済み。PCは5秒ごとに稼働通知を送り、端末は20秒で失効を検出する。正常停止通知とボタンを離すまでの待機上限も追加した。

## 確認した根拠

| 段階 | 結果・範囲 | 資料 |
|---|---|---|
| 端末単体 | 確認音と自声の録音・再生をユーザーが確認 | [単体音声](LOCAL_AUDIO_VALIDATION_2026-10-16.md) |
| LAN往復 | 16kHz上りと24kHz下りのPCM往復、ユーザーが自声を自然に聴取 | [LAN検証](LAN_PCM_VALIDATION_2026-10-16.md) |
| 実機・実API | 固定3秒の質問から日本語回答再生に成功 | [初回実機検証](GEMINI_DEVICE_VALIDATION_2026-10-16.md) |
| 可変長・無発話 | 調整後の無発話2回がEMPTY、ユーザーもNo speechを確認 | [可変長録音](VARIABLE_UTTERANCE_VALIDATION_2026-10-16.md) |
| R05実質問 | 入力37,120サンプル（2.32秒）、回答56,400サンプル（2.35秒）/118フレーム、実再生終了を確認。ユーザーもSpeak nowへの移行と回答を確認 | [録音開始の修正](RECORD_START_FIX_2026-10-16.md) |
| 今回の利用報告 | Start-Gemini-Device.cmdが一度終了したが、再起動後は問題なく動作し、会話が成立しているとユーザーが報告 | 本書の残課題 |
| 今回の統合前確認 | PC26/26、C++27/27、録音UI6項目、atoms3r-gemini-deviceビルド成功 | [検証履歴](validation.md) |

R05の13,743msはアップロード開始から回答の実再生終了までであり、発話終了から応答開始までの遅延ではない。今回の作業ではAPI呼出し・録音・端末書込み・PCサービス再起動を行っていない。現在の稼働はユーザー報告に基づき、稼働状態を再取得していない。

導入済みR05イメージは `.pio/firmware-snapshots/gemini-device-r05.bin`、1,140,272 bytes、SHA256 `D741F1CE3BA58FFCD5258E69B0CE645C33E53724845029C077C84799D6E74104`。今回のビルド報告は静的RAM50,308 bytes / Flash1,139,909 bytes。生成イメージはGitHub公開対象外。

## 再現方法と依存関係

- 同じ家庭内LANにPCと端末を接続し、USB COM4を使用する。案件ルートの [Start-Gemini-Device.cmd](../Start-Gemini-Device.cmd)を実行し、Gemini PC readyを確認する。
- 長押し→Release to recordで離す→開始音→約1秒静かに待つ→Speak now表示後に質問する。約0.7秒の無音で送信される。無言ならNo speechで待機へ戻る。
- 検証用PCサービスはstop操作または最長約10分で終了する。終了後や端末リセット後は起動用ファイルを再実行する。自動起動・Windowsサービス化は未実装。
- Node.jsは22.9.0以降（検証環境24.15.0）。依存はpackage-lock.jsonで固定し、@google/genai 2.30.0 / Express 5.3.0 / ws 8.22.0。PythonはPlatformIO同梱環境を起動ファイルから使用する。端末依存はplatformio.iniと[依存記録](dependencies.md)を参照。
- オフライン確認はserverで `npm test`、案件ルートで `python tools/run_host_tests.py` と `python tools/run_host_tests.py --suite gemini-ui`。端末ビルドは `pio run -e atoms3r-gemini-device`。実機書込みを伴わない。
- [PC起動・設定の詳細](../server/README.md)、[再開要約](RESUME.md)、[TODO](../TODO.md)を参照。

## 残課題と次の段階

1. **起動用ファイルが一度終了した原因は未解明。** エラー内容・終了コード・経過時間の記録がなく、正常な約10分の期限終了とも異常終了とも判定できない。再起動後の動作回復は確認したが、不具合解消とは扱わない。再発時には終了時刻・起動後の経過時間・終了段階・秘密情報を除いたエラー種別/終了コードを確認する。今回は調査・修正を加えない。
2. 複数回の質問、環境音・距離・声量の違い、長時間利用と通信切断からの復帰、応答遅延・音質を継続評価する。
3. 会話履歴を伴う複数ターン、通常版の呼びかけとの統合、録音中の連続送信、遅延改善は未実装。全二重・AEC・発話割込みも未対応。
4. LAN内は平文WS。WSS/TLSとクラウド公開は未検証。AI Studio Starter対象外を受け、ユーザー指定でクラウド公開・課金設定変更は保留する。

## GitHubへの記録

今回の対象は `codex/variable-utterance` の可変長録音・録音開始修正と本進捗資料。実キー・端末トークン・設定ファイル・音声・生成物を公開対象から除外する。統合前に既存未公開2コミットの118種類のblobと作業ファイル114件を検査し、実キー/保存済み端末トークンの一致0件。秘密設定は保存時SHA256と一致し、設定/生成物のignoreと未追跡を別々に確認した。PR作成・mainへのマージと設定保持の確認結果は、完了後に本節へ追記する。
