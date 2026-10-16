# 再開用の短い要約

更新: 2026-10-16（日本時間）/ R01。目的: この長い会話を読み直さず、現在の状態から続ける。

## 現在地

- 対象はM5AtomS3R + Atomic Voice Base A149、USBはCOM4。Arduino/PlatformIOの開発版。
- **実機の16kHz/16bit/mono質問音声→Windows PC→Gemini Live gemini-3.8-live→24kHz回答の端末再生が1回成功。ユーザーが日本語回答を確認した。** 詳細な測定値・書込みSHA・証拠は [実機検証](GEMINI_DEVICE_VALIDATION_2026-10-16.md) を参照。
- 端末はGemini端末版R01 / protocol v2。通常版の呼びかけ待機は現在動かさない。設定領域・Wi-Fi・呼びかけ登録を保持してアプリ本体だけ更新済み。
- PC一時サービスは前回試験後に停止済み。今回の要約・GitHub同期では端末操作、追加録音、API呼出し、クラウド操作はしない。
- クラウド公開は保留。AI StudioでStarter対象外を確認したため、ユーザーがローカルPC検証を選択した。課金を有効化しない。

## 起動と設定

- 実機用は案件ルートの [Start-Gemini-Device.cmd](../Start-Gemini-Device.cmd)。同一家庭内LANとCOM4が必要。PCの接続先・端末トークンをUSBで端末RAMへ渡す。リセット後は再実行する。
- 端末ボタンをRelease to recordまで長押し→離す→開始音後の3秒間で質問。短押しは確認音、通信/再生中の短押しは中止。PC画面のstopまたは約10分で終了する検証用サービス。
- Geminiキーは既存の非公開 `data/secrets.json` に設定済み。OpenAIキーは不要。キーを端末へ渡さない。`.env`、秘密設定/バックアップ、音声、`.pio`、dist、node_modulesは公開しない。
- PC単独用Start-Gemini-PC.cmdは既定127.0.0.1で、実機のUSB設定を行わない。操作の詳細は [server/README](../server/README.md)。

## 検証と残作業

- 直近の合格: PC25件、C++19件、対象ファームウェアビルド、アプリ書込みハッシュ、APIなし可変長音声、実機Gemini回答再生。今回の同期前回帰もPC25/25・C++19/19、通常版/Gemini端末版のビルド2/2成功。
- 現版は3秒一括録音・1質問1回答・最大10秒回答の半二重。会話履歴、可変発話長、連続録音、ウェイク統合、AEC/割込みは未対応。
- LAN内平文WS。WSS/TLS、クラウド公開、長時間・切断復帰・音質/認識精度の定量評価は未検証。1回成功を製品完成とは扱わない。
- 次の候補は可変発話長、複数ターン、ウェイク統合、遅延改善。範囲を確認して順に進める。詳細な状態は [TODO](../TODO.md)。

## 再開時に読むもの

1. 本書→ [実機Gemini検証](GEMINI_DEVICE_VALIDATION_2026-10-16.md) → [server/README](../server/README.md)。
2. 経緯が必要な場合だけ [日次引継ぎ](HANDOFF_2026-10-16.md) と [検証履歴](validation.md)。旧時点の「未実装」を現在の状態と混同しない。
3. 依存はplatformio.ini、server/package.json/lock。PC: Node.js24.15.0、@google/genai2.30.0。実機ビルド環境: atoms3r-gemini-device。

## 推奨Skillと取扱い

- ファームウェア・ビルド・端末操作: microcontroller-embedded-development。
- 会話の引継ぎ整理: mattpocock-skills:handoff。生の会話や秘密情報を保存せず既存資料を参照する。
- 実マイクをGeminiへ送る試験はユーザーの明示許可を取得済み。追加のクラウド公開・課金変更はその許可に含めない。実機の録音開始は物理操作のみ。
- 同期前に秘密設定のignoreと未追跡を別々に確認し、Git切替/マージ後も内容保持を確認する。

作業基準: `e6ae4a1`（実機Gemini成功）。GitHub統合先: [thomasleftwite/stackchan-chatgpt](https://github.com/thomasleftwite/stackchan-chatgpt)、main。同期の最終結果は本書への追記とGitHubのPR状態を参照。
