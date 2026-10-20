# 第3段階R02: ビープ終了後の待ちとNOISY判定の診断

最新の診断は[第3段階R03](WAKE_STAGE3_R03.md)。R02の無言試験1回は改善したが、再開後の登録準備でNOISYが再発した。0.7秒待ちは維持し、最後の判定を後から再照会できるようにする。

2026-10-19（日本時間）/ R02。対象: AtomS3R + Atomic Voice Base A149、Arduino-ESP32 2.0.17、PlatformIO atoms3r-gemini-device。目的: R01の登録準備が無言でもNOISYになる現象を切り分ける。R02の無言試験1回は正常なNO_SPEECHを確認。発生源の確定と実登録の照合/保存は未完了。基本操作・保存保護は[第3段階R01](WAKE_STAGE3_R01.md)を継承する。

## 確認した現象と仮説

R01で登録画面への移行・無操作取消・長押し取消を確認。その後の最初の録音操作で、準備中にNOISYとなりSleepingへ戻った。開始音の後さらに約2秒無言で待つ手順でも同じ結果となり、利用者も無言のまま戻ったと報告した。いずれも照合・保存へ進んでいない。証跡は.pio/device-check/face-stage3-enrollment-stepwise-20261019-r01.json。準備/NOISYは58,610/60,297msと128,328/128,578msで観測。状態照会時刻の差は録音時間そのものではない。

利用者から「自分のビープ音を拾っているのではないか」との指摘。実装はAudioDevice.cueでビープ再生終了を待った後、すぐマイクを起動していた。最初の0.4秒分のマイク入力を捨て、続く0.6秒分で環境音を測るが、ビープ終了後の独立した待ちはなかった。残響/切替直後の音、録音取得の連続性、実環境音と判定値の3候補のうち、今回は残響仮説を先に比較する。ビープが直接原因と確定したわけではない。

## 変更範囲

- 登録/照合の各録音だけ、ビープ終了後700msはマイクを起動しない。既存のマイク起動後の0.4秒分の破棄と0.6秒分の環境音測定は維持。通常質問の録音経路は変更しない。
- 700ms中も画面更新、状態照会、取消を処理する。取消した場合はマイクを起動しない。
- 環境音測定完了時にGEM ENROLL CAL rms=<数値> threshold=<数値> ms=<数値> gap=<数値>だけを出す。rmsはDC除去した各フレーム音量の90パーセンタイル、msはマイク起動後の測定経過、gapはビープ終了からマイク起動完了までの経過。音声/特徴量/キーは保存しない。
- AmbientGateの雑音判定は変更しない。threshold=max(750, noise*3)で、threshold<=3000を要求する。NOISYはnoiseが1000を超えたことを意味するが、その発生源は数値だけでは特定できない。
- PC監視の状態許可リストへ、この有界な数値行だけを追加する。配布済みEXEは今回は再生成しないため、診断はプロジェクト内の観測ツールで行う。

## 検証

マイク起動とビープ再生の時刻を扱うテストを先に追加し、R01では「再生終了後700msまでマイクを起動しない」の1項目が失敗することを確認した。R02適用後は成功。これは待ち時間の保証の検証で、実機NOISYの原因証明ではない。

- 登録UI: 13/13成功。700ms待ち、待ち中取消、既存の録音/照合/保存/保護を含む。
- 録音品質: 10/10成功。雑音判定値の数値診断も確認。
- 既存FaceUI27、質問UI6、保存21、PC監視17が成功。
- 数値診断の許可リスト2項目成功。文字や追加データ、過大な桁数を拒否。
- 今回実行分は合計96項目成功。端末版・通常版のビルド2/2成功。依存はplatformio.ini固定版のまま。

再現はtools/run_host_tests.pyのwake-enrollment-ui/wake-enrollment/face-ui/gemini-ui/wake-storageを各実行し、python -m unittest discover -s tests -p test_gemini_coordinator.py -qとtest_enrollment_status.pyも実行する。ビルドはpio run -e atoms3r-gemini-device -e atoms3r。サーバ・共通ルール・表示デモに今回の変更はなく、今回再実行には含めない。

## 候補と導入・再試験

候補: .pio/firmware-snapshots/gemini-face-stage3-r02.bin、1,469,552 bytes、SHA256 9BD102955755F0C15FEB49976DB2FE7381DEE13E5AD4797A6482BF8D22264663。R01候補は保持。導入は委譲済みの権限内でflash_application_only.pyを用い、COM4/0x10000のアプリだけを更新する。SPIFFS/NVS/設定/既存登録は書込み対象外。導入結果は追記する。

COM4への書込みは成功。消去範囲0x10000〜0x176fff、1,469,552 bytes、Hash of data verified、終了コード0。今回は自動リセット後の状態照会でstage=3/Sleep/mic=0、configured=0/wifi=0/pc=0、CAPTURE/RESULT NONEを取得し、通常起動を確認した。追加の手動ダウンロードモード操作は不要だった。証跡.pio/device-check/face-stage3-r02-flash-20261019-r01.json、face-stage3-r02-boot-20261019-r01.json。無言比較試験は確認中。

比較試験は、まず何も話さない状態で登録準備を実施し、CALのrms/threshold/ms/gapと、NO_SPEECHかNOISYかを確認する。無言試験でNO_SPEECHになれば次に実際の呼びかけで録音→照合→明示保存→リセット後保持を確認する。Googleへの送信は行わない。R02だけで改善しても、残響が原因だったとの断定には限界がある。USB接触不良の原因究明保留と、追加の常時PC監視をしない合意は維持する。

## 無言比較試験の結果と再開（2026-10-19）

最初の無言試験監視はusb_errorで終了し、利用者は短時間の表示を読めなかったと回答。続く状態照会で、端末に残ったGEM ENROLL CAL rms=94 threshold=750 ms=1631 gap=729とENROLL result=NO_SPEECHを取得。Sleep/mic=0、CAPTURE/RESULT NONEも確認した。ビープ終了からマイク起動まで729msを確保でき、環境音判定を通過して無発話で終了した。証跡.pio/device-check/face-stage3-r02-after-quiet-20261019-r01.json。R01の無言NOISYから改善した比較1回であり、残響が唯一の原因だったとは断定しない。登録保存は行っていない。

次の登録監視の起動が自動承認レビューの利用上限で実行されず中断したが、利用者の再開指示後、USB監視を再開できた。起動時はSleep/mic=0、PC/Wi-Fi未接続。続きは物理ボタンによる録音→別発話照合→明示保存。今回の状態専用監視は最長10分で、常時監視ではない。音声/特徴量は保存しない。再開後の証跡.pio/device-check/face-stage3-r02-registration-20261019-r02.json。
