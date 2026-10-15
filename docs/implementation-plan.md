# 実装方針

ユーザーの実装指示に基づき、Stackchan-chatgpt配下に独立したArduino/PlatformIOプロジェクトを作る。既存firmwareは変更しない。これまでの調査資料もこのフォルダへ移した。

- AtomS3R + Atomic Echo Base、C++、WiFiManager。
- 待機・録音登録は端末内のみ。SimpleVoxによるMFCC/DTW比較。
- ボタンまたはウェイクワードで会話開始。半二重、開始音後から音声送信。
- OpenAI Realtime文字起こし → Gemini REST回答 → OpenAI Realtime音声生成。
- メモリを抑えるためAPI接続は処理段階ごとに切り替える。接続・転写待ち・再生には上限時間を設ける。
- 音声は24 kHz PCM。ウェイク検出/登録のみ16 kHz。
- Wi-FiはWiFiManager、会話設定はYAML、APIキーは別JSON。CA検証を必須とする。
- 設定/登録データが不正なら会話送信を止めてエラーを表示。未登録はボタン開始可能。
- 設定例、導入手順、検証手順、依存ライブラリの出典を同梱する。

検証: PlatformIOビルド、状態遷移/入力上限等のホストテスト、独立レビュー。実機やAPIキーがない場合は実機・実API検証を未実施と明示する。書き込みはユーザー指示があるまで行わない。
