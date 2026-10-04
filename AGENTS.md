# AI Agent Instructions

- Do not run project builds unless the user explicitly asks for a build in the current conversation.
- Preserve CRLF line endings when creating or editing source and text files.
- 開始任務前必須讀取根目錄 `HANDOFF.md`，依當次需求確認目前狀態。
- 每完成一個可交接階段，必須先依 `HANDOFF.md` 的更新規則更新該文件，再回報完成或進入下一階段；暫停、遇到 blocker 或交接時亦同。所有 AI 共用此文件。
