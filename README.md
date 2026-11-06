# TaskPaper

A plain-text task outliner for Omarchy. Open it and type: the document is an ordinary
`.taskpaper` (or `.txt`) file, and the styling follows what you write.

```
Inbox:
	- this is a task
	- this one is @done(2026-11-05)
	- this one is @tagged
	- this one is @taggedwithvalue(3)
```

`- ` starts a task, a line ending in `:` is a project, anything else is a note, `@tag` and
`@tag(value)` are tags. Indent with tabs. Edits are saved to the file as you type
(`~/Documents/Tasks.taskpaper` the first time, then the last file you opened).

Links (`https://…`, `www.…`, `mailto:`) are underlined and open in your browser when clicked.

`taskpaper [file]` opens a specific file. Search syntax: `@done`, `not @done`,
`@due < today`, `project Inbox`, `task milk`, joined with `and` / `or`.
Saved searches live in the file under a `Searches:` project as `@search(query)`.

## Keys (TaskPaper's, with Ctrl for Command)

| Key | Action |
| --- | --- |
| Ctrl+Enter or Ctrl+D | Toggle done (adds `@done(today)`) |
| Ctrl+. | Fold / unfold the item |
| Ctrl+Alt+9 / Ctrl+Alt+0 | Fold all / unfold all |
| Tab / Shift+Tab | Indent / outdent |
| Ctrl+Alt+Up / Down | Move item with its children |
| Ctrl+Alt+Right / Left | Zoom in / out |
| Ctrl+Alt+Home | Home: the whole document |
| Ctrl+F | Search |
| Ctrl+Shift+F | Save current search |
| Ctrl+Alt+S | Toggle sidebar |
| Ctrl+= / Ctrl+- / Ctrl+0 | Bigger / smaller / reset text |
| Ctrl+O / Ctrl+Shift+S | Open / Save As |
| Ctrl+K | Show all keys |
| Ctrl+Q | Quit |

Omarchy's single-key `?` and `Q` would type into a document, so help and quit take Ctrl.

Text is Omarchy's monospace font (iA Writer Mono S by default) at 10pt.
