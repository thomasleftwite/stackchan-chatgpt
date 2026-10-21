# 開発記録の要約

2026-10-21 / R01。詳細な失敗・調査・検証証拠は各記録に保持し、日常の再開用記録を短くまとめた。

| 段階 | 到達点 | 根拠 |
|---|---|---|
| 10/15〜16 | ビルド復旧、録音/可変長PCM、Gemini実音声往復 | [旧検証記録](archive/2026-10-21/rc1-before-compaction/docs/validation.md) |
| 顔・登録 | Sleeping/Ready、3回登録、ビープ終了700ms後の受付 | [R07](WAKE_STAGE3_R07.md) |
| Wi-Fi運用 | 端末コード不要、ホットスポット信頼、リセット後の再接続 | [通信仕様・元の詳細](archive/2026-10-21/rc1-before-compaction/docs/HOTSPOT_TRUST.md) |
| 検出調査 | 登録と検出の条件整合、一時マイク失敗の復帰、診断版導入 | [ウェイク診断R01](WAKE_DIAGNOSTIC_FIRMWARE_R01.md) |
| 声の操作 | ホスト声選択・PC試聴、PC確認だけで採用可能へ簡素化 | [声採用修正](STACKCHANHOST_VOICE_PC_ADOPTION_2026-10-21.md) |
| 冒頭欠け | スピーカー有効化後200ms待機。利用者が欠け無しを確認 | [待機修正](REPLY_START_DELAY_2026-10-21.md) |
| RC1 | 端末診断表示撤去、モード中心、Wi-Fi固定ホスト。書込み成功、利用者ok | [RC1](RELEASE_CANDIDATE_0_6_0_2026-10-21.md) |
| 文書・公開 | README/構成スライド更新、開発記録整理、EXE付き公開 | [公開記録](GITHUB_RC1_SYNC_2026-10-21.md) |

現在の残課題と将来構想は[ToDo](../TODO.md)に集約。旧ToDo・README・RESUME・検証・スライドは[整理前保管](archive/2026-10-21/rc1-before-compaction/README.md)に残す。過去時点の未実施を現在の未実施として読み替えない。
