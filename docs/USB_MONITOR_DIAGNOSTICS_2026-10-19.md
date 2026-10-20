# USB監視ツールの終了理由・状態行取得の診断

版: R02 / 2026-10-19（日本時間）。対象: Windows PCのtools/run_gemini_device.py、AtomS3R + Atomic Voice Base A149、COM4、Stage2 R01。目的: 会話成功後に監視ツールが終了コード1となる問題を分類し、再生時の入力停止・切断復帰を根拠別に検証する。今回のファームウェア変更・書込みはなし。

## 変更と再現

既存ツールでは例外詳細を一律に抑止し、reportにも終了理由がない。実mainを模擬USB例外で実行するtests/test_gemini_coordinator.pyで、serial_error/serial_openを保存する期待がFAILすることを確認。これは診断情報欠落の再現であり、過去のUSB故障原因の再現ではない。

終了報告をversion 2とし、固定コードreason/stage、elapsedMs、code、serverCode、deviceStopConfirmed、serialDetail（固定分類のみ）を追加。例外本文・ポート名・アドレス・トークン・音声・回答本文は出力しない。期限・stop・stdin閉鎖・USB失敗・サーバ終了・停止応答なしを区別する。USB書込みに2秒上限、INFOの再照会、停止応答の確認、一次エラーを消さない子プロセス終了処理を追加。

USB read_untilのタイムアウトで途中までの行が返る場合、以前は行片ごとに正規表現へ渡していた。SerialLinesで分割行を512 byteまで再結合し、長すぎる行を次の改行まで破棄する。追加試験でread_untilの実装パターンを模擬した6行の読込みが324回のI/Oとなることを確認し、最大512 byteの一括読込みと複数行キューへ変更した。一括読込みは同じ試験で2回以内となる。これは読込み方法の改善であり、接触不良や過去のUSB切断を修復するものではない。許可リストは維持。状態行は出力の重複を抑止しつつ、reportには最大2,000件まで繰返しを保存する。

## ホスト検証

`python -m unittest discover -s tests -p test_gemini_coordinator.py -v`: 14/14成功。実mainを実行し、USB open/monitor失敗、書込み期限、正常期限、stop、stdin終了、サーバ終了、停止未確認、終了コード異常、不正イベント、分割/過長の行と秘密情報非出力を検証。機器/通信/APIは模擬。

`python tools/run_host_tests.py --suite face-ui`: 27/27成功。追加4項目で検出有効時の送信/回答/再生/700ms待ちの停止、取引終了後の再検出、PC明示停止と接続復帰を実際のLocalGemini.cpp経路で確認。機器/検出器は模擬のため実音声精度の根拠ではない。

## 実機で得た証拠

- 30秒ローカル合成試験: 入力24,000→出力12,001/26フレーム、端末PASS/played=1/2,605ms。duration_limit、全体30,390ms、PC/子サーバ終了コード0、PC OFFLINE応答あり。証跡: .pio/device-check/stage2-coordinator-fixture-20261019-r01.json。
- 260秒予定の待受試験: 合成往復PASS/2,624ms後、33,469msでserial_error / monitor、終了コード1。子サーバ片付け後コード0、停止通知応答は未確認。ユーザーはReadyにした後も画面はReadyのままだったと報告。証跡: .pio/device-check/stage2-idle-sleep-20261019-r01.json。
- その後の再照会: 起動時WAKE available=1、READY、Sleep/mic=0を取得。ROM表示とresetコード0x15を観測。ローカルEspressif SDKのesp32s3/rom/rtc.hでは21=USB_UART_CHIP_RESET。ただしこの照会はopen後にDTR/RTSを変更しており、照会操作でリセットが起きた可能性を除外できない。前のUSBエラーで端末が再起動した証拠とは扱わない。証跡: .pio/device-check/stage2-usb-failure-state-20261019-r01.json。
- 操作なし75秒比較: 合成往復PASS/2,637ms、duration_limit、75,218ms、両終了コード0、停止応答あり。serialDetail=null。証跡: .pio/device-check/stage2-usb-repro-20261019-r02.json。

上記は全てローカルfixture。Google/Gemini呼出し0、マイク質問の自動開始なし、音声保存なし。最初のエラー分類はUSB通信まで絞れたが、Windowsのどの処理が失敗したか、接続/電源/端末/ドライバのどれが原因かは未確定。過去の終了事象と同一原因とも断定しない。

## 待受・停止の追加結果

- Ready比較試験: ユーザーが「Sleepingに戻り、勝手な開始もなかった」と回答。約3分後の自動スリープと今回の待受区間での意図しない開始なしを利用者目視として基本確認。USB記録にはReady/mic=1の遷移が残らず、マイクの停止タイミングや全区間の誤検出を端末ログだけで保証しない。
- 同試験の停止: 約238,468msでstopを入力、PC OFFLINE応答が2秒以内に得られずdevice_stop_unconfirmed / device_stop、code=1、serverCode=0。USB例外ではない終了を区別できた。証跡: .pio/device-check/stage2-awake-repro-20261019-r03.json。
- 一括読込み変更後: 60秒予定の試験を開始したが、ユーザーの調査保留指示を受け18,719msでstop。合成音往復PASS/2,667ms、user_stop / complete、code=0、serverCode=0、PC OFFLINE応答あり。証跡: .pio/device-check/stage2-bulk-usb-20261019-r04.json。60秒安定性試験の完了とは扱わない。

## 利用者指示による原因究明の保留

2026-10-19、利用者から「USBコネクタの接触不良で、USBコネクションが切れ、再接続する期間で発生しているようです。この異常終了の原因究明は保留して良い」と指示あり。接触不良に伴う切断/再接続は利用者の観察に基づく有力な可能性として記録し、電気的/機器ログで確定した原因とは区別する。以後の原因追跡・追加再現試験は保留。実行中の試験サービスは正常停止済み。

今回の成果は安全な終了分類・分割行/一括読込み改善、ホスト診断14件と顔UI27件成功、自動スリープのユーザー確認。ファームウェア・端末設定の変更、API呼出し、USB接触不良の修復は行っていない。実機の再生中検出停止/残響評価、切断復帰と精度の定量評価は残項目。新たな自動再接続や自動音声再送は実装していない。
