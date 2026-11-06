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
| Ctrl+D (or Ctrl+Enter) | Toggle done (adds `@done(today)`) |
| Ctrl+Shift+A | Archive the selection, or every `@done` item if nothing is selected |
| Ctrl+9 / Ctrl+0 | Collapse / expand the item (Ctrl+. toggles it) |
| Ctrl+Alt+9 / Ctrl+Alt+0 | Collapse / expand the item and everything inside it |
| Ctrl+Shift+9 / Ctrl+Shift+0 | Collapse / expand everything |
| Tab / Shift+Tab (or Ctrl+] / Ctrl+[) | Indent / outdent |
| Ctrl+Alt+Up / Down | Move item with its children |
| Ctrl+Alt+] / Ctrl+Alt+[ | Focus in / out (zoom) |
| Ctrl+H | Home: the whole document |
| Ctrl+F | Find and replace (top right; Enter = next, Shift+Enter = previous, Esc closes) |
| Ctrl+G / Ctrl+Shift+G | Find next / previous |
| Ctrl+Shift+F | Filter with the search bar |
| Ctrl+Alt+F | Save the current search |
| Ctrl+Alt+S (or Ctrl+B) | Toggle sidebar |
| Ctrl+= / Ctrl+- | Bigger / smaller text |
| Ctrl+O / Ctrl+S / Ctrl+Shift+S | Open / Save now / Save As |
| Ctrl+K | Show all keys |
| Ctrl+Q | Quit |

Keys follow TaskPaper with Ctrl for Command and Alt for Option. Reopening a file puts the cursor back where you left it.

Omarchy's single-key `?` and `Q` would type into a document, so help and quit take Ctrl.

Text is Omarchy's monospace font (iA Writer Mono S by default) at 10pt.
