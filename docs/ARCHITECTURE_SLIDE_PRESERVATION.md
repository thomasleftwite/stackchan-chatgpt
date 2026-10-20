# システム構成説明スライドの保管・復旧

保管・改訂記録 R02 / 2026-10-21

## 目的・対象

利用者が復元した `docs/architecture-slide.pptx` をGit履歴へ保存し、所在をREADMEから確認できるようにする。今回は利用者の依頼により、元版を保管して現行Windowsホスト0.5.0-preview.1の内容へ改訂する。

- 保存先: `C:\Users\thoma\SynologyDrive\H\Doc\Src\Codex\projects\ai-stackchan\AI_StackChan_Ex\Stackchan-chatgpt\docs\architecture-slide.pptx`
- 復元時サイズ: 51,055 bytes
- 復元時SHA256: `3DE43ECC02C08145EDB15DF4FCCD4E87C1956B746A0DAD59C9CE26D07A27C241`
- 依存関係: Git（履歴保存・復旧）、PowerPointまたは互換ビューア（表示）。
- 永久ピン留め: 案件フォルダ・docs・対象ファイルでWindowsのPinned属性を確認。

## 現行スライドR02

- 更新先は上記と同じ `docs/architecture-slide.pptx`。40,441 bytes、SHA256 `89C408D88B9F5F347756D467C666B5FCBA0EC0258E54DBC82DA945FA1771BCD3`。
- 元版の保管先: `docs/archive/2026-10-21/architecture-slide-before-hotspot-trust.pptx`。上記の復元時SHA256と一致し、改訂前HEADのPPTXともバイト一致。
- 端末・PC・Geminiの3区分、1枚、13⅓×7½インチ、58個の編集可能図形、Yu Gothicを維持。USB設定転送中心の説明を、2.4GHzホットスポット・HTTP/WS TCP48443・コード不要・サブネット受付制限へ変更した。
- APIキーのPC限定使用、3回の呼びかけ登録、16kHz入力/24kHz回答、ビープ終了から700msの受付を記載。呼びかけ・音声・口パク・端末リセット後復帰の基本確認と、長期精度・電源断・別PC・Piの未検証を区別する。信頼の範囲の詳細と根拠は発表者ノートにも残した。
- 根拠: `docs/HOTSPOT_TRUST.md` R04、`src/LocalGemini.cpp`、`include/WirelessLink.h`、`desktop/wireless.py`、`desktop/host_app.py`、`server/gemini-server.ts`、`docs/WAKE_STAGE3_R07.md`。
- 改訂に使用した依存: バンドル済み `@oai/artifact-tool` 2.8.99、Node24.19.0、Python、presentation skill 26.1015.12040。閲覧はPowerPointまたは互換ビューア。

## 保管方針

`.gitignore` はバックアップ設定ではなく、未追跡ファイルをGitの対象外にする設定。対象スライドは明示的な除外解除規則を設定し、コミットへ含める。追跡開始後は削除・変更もGit差分に現れる。

`GITHUB_WIRELESS_SYNC_2026-10-20.md` の「既存スライドを変更・追加しない」は当時の公開作業の範囲を示す履歴。復元時の指示によりGit管理し、今回の更新依頼により内容を改訂した。SVGの復元・作成は行わない。

作業整理や生成物の清掃でこのファイルを削除・移動・上書きしない。改訂時は元の履歴を残して変更内容をコミットする。Synology Driveの同期中に別PCと同時編集しない。

## 確認・復旧手順

案件フォルダで実行する。

```powershell
git ls-files --error-unmatch -- docs/architecture-slide.pptx
git log --oneline -- docs/architecture-slide.pptx
Get-FileHash -LiteralPath docs/architecture-slide.pptx -Algorithm SHA256
```

削除後、HEADに保存された版へ戻す場合は、対象に残すべき編集がないことを確認してから実行する。

```powershell
git restore --source=HEAD --staged --worktree -- docs/architecture-slide.pptx
```

復旧後はハッシュとビューア表示を確認する。上記ハッシュは復元時の版にのみ適用する。

## 検証結果・残る注意点

2026-10-21に元版と改訂版の全1枚を画像表示し、配置・折返し・接続矢印を比較。最終PPTXを再読込みして再描画し、編集時プレビューと画像ハッシュが一致した。パッケージ整合性・寸法・見出し/箇条書き配置・フォント検査は指摘0件、再インポート成功。図形数・ノート・現行説明を確認した。PowerPoint実アプリでの表示確認は未実施。

検証レポートは案件内 `.pio/architecture-slide-update-20261021/validation-r02.json`（Git除外）。画像は同フォルダの `before.png` / `final.png`。検証ツールのレポートで合格を確認した一方、描画を含むNode親プロセスは完了後も終了コード1を返したため、終了コードだけを成功根拠にしていない。最終ファイルのハッシュ・構造・再描画結果を別途確認した。

この資料更新ではファームウェア・ホストアプリを変更せず、実機操作・API呼出しは行っていない。実機の成功範囲は既存記録と利用者回答に基づく。

ローカルGit履歴は同じPC上の復旧手段であり、PCやディスク全体の喪失対策には別の保存先も必要。この作業ではGitHubへのpushは実施しない。遠隔保存は次回の公開時に内容を確認して行う。
