# システム構成説明スライドの保管・復旧

保管記録 R01 / 2026-10-20

## 目的・対象

利用者が復元した `docs/architecture-slide.pptx` をGit履歴へ保存し、所在をREADMEから確認できるようにする。スライドの内容・内部版数は変更しない。

- 保存先: `C:\Users\thoma\SynologyDrive\H\Doc\Src\Codex\projects\ai-stackchan\AI_StackChan_Ex\Stackchan-chatgpt\docs\architecture-slide.pptx`
- 復元時サイズ: 51,055 bytes
- 復元時SHA256: `3DE43ECC02C08145EDB15DF4FCCD4E87C1956B746A0DAD59C9CE26D07A27C241`
- 依存関係: Git（履歴保存・復旧）、PowerPointまたは互換ビューア（表示）。
- 永久ピン留め: 案件フォルダ・docs・対象ファイルでWindowsのPinned属性を確認。

## 保管方針

`.gitignore` はバックアップ設定ではなく、未追跡ファイルをGitの対象外にする設定。対象スライドは明示的な除外解除規則を設定し、コミットへ含める。追跡開始後は削除・変更もGit差分に現れる。

`GITHUB_WIRELESS_SYNC_2026-10-20.md` の「既存スライドを変更・追加しない」は当時の公開作業の範囲を示す履歴。今回の利用者指示により、このPPTXを新たにGit管理する。SVGの復元・作成は行わない。

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

## 検証範囲・残る作業

PPTXのZIP構造・主要XMLの読込み、保存前後のSHA256一致、Git除外状態・コミット内のバイト一致を確認する。スライドの表示・構成説明の現行実装との一致は未検証。

ローカルGit履歴は同じPC上の復旧手段であり、PCやディスク全体の喪失対策には別の保存先も必要。この作業ではGitHubへのpushは実施しない。遠隔保存は次回の公開時に内容を確認して行う。
