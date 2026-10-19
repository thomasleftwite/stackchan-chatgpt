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

自動リセット後にBUILD enrollment=5、Sleep/mic=0を確認。継続監視は.pio/device-check/face-stage3-r05-live-20261019-r01.json（単一セッション82829、最大10分）。最初の実発話はCAL rms=138 threshold=750 ms=1040 gap=728、AUDIO samples=18560 voiced=5440 ms=3876からSTEP verifyへ進んだ。利用者もTry your call表示を確認。有声340msの同じ長さを受理して照合待ちに進む改善を実機確認した。照合と保存の成功はこの時点では未確認。

続く照合録音はCAL rms=210 threshold=750 ms=1040 gap=729、AUDIO samples=17280 voiced=6080 ms=3651でSTEP saveへ進んだ。別発話の照合を実機で通過した。利用者はSettings表示を報告したが、返信を挟む間に無操作30秒でCANCEL/Sleepとなり保存されなかった。これは照合失敗とは区別する。以後は途中返信を挟まず、最終SettingsのClick: save / hold: noで30秒以内に短押しする一連の手順を案内する。

次の一連操作では最初の録音がCAL161/750/ms1040/gap729、samples18560/voiced7680で通過。照合側は発話待ちに入る前のCAL rms=1004 threshold=3012 ms=1040 gap=729でNOISY、samples=0/voiced=0となった。利用者はTry again表示を報告。これは短い発話やDTW不一致ではなく準備時の判定で、入力音の発生源は未確定。700ms待ちと雑音基準は変更せず、開始音停止後さらに約3秒無言で待ち、録音案内がある場合だけ話す再確認を案内した。
