# 第3段階R05: 短い呼びかけの最低有声時間の整合

2026-10-19（日本時間）/ R05。対象: AtomS3R + Atomic Voice Base A149、Arduino-ESP32 2.0.17、PlatformIO atoms3r-gemini-device。依存固定版はplatformio.iniを参照。目的: [R04](WAKE_STAGE3_R04.md)で連続取得した呼びかけが有声340msとしてSHORTになった問題を修正する。

## 変更と根拠

R04の最初の登録録音は、保持17600・有声5440・末尾を切った特徴窓12800サンプル。16kHzで有声340ms、特徴窓800msに相当し、音量判定は通っているが登録側だけの最低有声400msで終了した。従来LocalWakeのSpeechGateは最低200msであるため、登録と照合の録音にも同じ200msを適用する。これに加え、特徴計算に必要な窓400ms以上を録音側でも明示的に要求する。

ビープ完了後700msのマイク停止は共通の登録/照合経路で維持する。その後の初期400ms破棄と600ms環境音測定、RMS/雑音/クリップ/最大3秒、別発話によるDTW照合、明示的な保存操作、失敗・取消時の旧登録保護も変更しない。音声と特徴量を記録せず、Googleへは送信しない。BUILD enrollment=5で導入版を識別する。

## 検証

修正前に、観測されたサンプル数と一致する模擬音声（原音声そのものではない）と、有声200ms/窓400msの境界の2項目が失敗することを確認。修正後に両方成功。有声200ms未満で窓だけ長い場合と、有声200msでも窓が400ms未満の場合を拒否する。

- 録音品質13/13、登録UI16/16、FaceUI27/27、質問UI6/6、保存21/21、PC監視17/17、状態許可リスト2/2。合計102項目成功。
- atoms3r/atoms3r-gemini-deviceの2構成ビルド成功。端末版RAM57,992 bytes、ELF flash1,470,197 bytes。
- 再現: tools/run_host_tests.pyのwake-enrollment/wake-enrollment-ui/face-ui/gemini-ui/wake-storageと、unittestのtest_enrollment_status.py/test_gemini_coordinator.py。サーバ・共通ルール・表示デモは今回変更せず再実行していない。

## 導入と実機確認

候補は.pio/firmware-snapshots/gemini-face-stage3-r05.bin、1,470,560 bytes、SHA256 64CAB8CBB51F7493A976B642C3FBF41A5EB904A2404E33F55239FF472807DB88。既存R01〜R04を保持する。COM4のアプリ0x10000だけへ書込み成功（消去0x10000〜0x177fff、Hash of data verified、終了コード0）。SPIFFS/NVS/設定/旧登録領域へは書き込んでいない。

仕様/品質の独立した2軸の読取レビューでは、修正必須の欠陥なし。200msと400msの条件、既存候補APIとの整合を確認した。実機の受理改善や誤検出影響の検証とは区別する。

次の確認はSleepingで約7秒長押し→離す→Settingsで短押し→Learning callを確認して従来の呼びかけを1回発話。STEP verifyが記録された場合だけ再発話照合へ進み、STEP saveになった場合だけ保存操作を案内する。実登録保存・再起動後の保持・通常の呼びかけ検出精度は未確認。
