# Wi-Fi設定とUSB不要ホストの公開・統合

2026-10-20。対象ブランチcodex/wireless-host-package → main。

## 公開する内容

Gemini端末版のWiFiManager（固定APパスワード12345678、時間制限なし）、端末固有キーの無線ペアリングと永続化、ホットスポットゲートウェイのTCP48443へ接続するTLS1.2 DHE-PSK中継、Windowsホスト0.4.0-preview.1のソース、回帰テスト、利用説明と検証記録。
既存の顔表示・呼びかけ登録/検出・手動質問・回答再生・USB診断を維持。新端末版の実機導入は承認待ち、Windowsホットスポット/受信許可は未変更。実機の新構成での会話成功とは扱わない。

## 検証

- Python34件、サーバー28件（Node24.15.0）、C++144項目、3構成ビルド成功。
- 最終配布EXEの自己診断10項目成功（TLS音声往復を含む）、外部API0回。
- ZIPはEXEと関連資料の7ファイル。実行モジュールはEXEへ内蔵。SHA256SUMSとの整合、実キー一致0、秘密設定・登録・音声ファイルの非同梱を確認。
- 公開候補ソースで実キー/既知キー形式一致0。秘密設定2件は除外・未追跡を確認し、同期後の保持確認用ハッシュはGit除外領域に保存。
- ZIP/EXE/.pio/.packaging/node_modules/秘密設定はGitへ追加しない。利用者の既存architecture-slide.pptxとarchitecture-slide.svgも変更・追加しない。

## 配布物

releases/StackchanHost-0.4.0-preview.1-win-x64.zip（Git除外、別途利用者へ提供）。サイズ51,559,539 bytes、SHA256 70040f4b3eead175f96a2a6abd244df8ad911fe3c1e1152aeb45e9ae30c7b12f。

## 統合

PR作成とmain統合を実施し、結果を追記する。
