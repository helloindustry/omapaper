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
| Ctrl+Shift+K | Delete the whole line (or every selected line; a folded item takes its hidden lines with it) |
| Ctrl+Shift+A | Archive the selection, or every `@done` item if nothing is selected |
| Ctrl+9 | Collapse the item (Ctrl+. toggles it) |
| Ctrl+Shift+9 / Ctrl+Shift+0 | Collapse / expand everything |
| Tab / Shift+Tab (or Ctrl+] / Ctrl+[) | Indent / outdent |
| Ctrl+Shift+Up / Down | Move item with its children |
| Ctrl+Shift+I / Ctrl+Shift+O | Focus in / out (zoom) |
| Ctrl+H | Home: the whole document |
| Ctrl+F | Find and replace (top right; Enter = next, Shift+Enter = previous, Esc closes) |
| Ctrl+G / Ctrl+Shift+G | Find next / previous |
| Ctrl+Shift+F | Filter with the search bar |
| Ctrl+Shift+E | Save the current search |
| Ctrl+B | Toggle sidebar |
| Ctrl+= / Ctrl+- / Ctrl+0 | Bigger / smaller / reset text to Omarchy's size |
| Ctrl+O / Ctrl+S / Ctrl+Shift+S | Open / Save now / Save As |
| Ctrl+P | Print what is on screen to the default printer |
| Ctrl+/ | Show all keys |
| Ctrl+Q | Quit |

Typing `@` and a letter offers a tag that is already in the document (`@M` suggests `@MIT`): Tab, Right, or Enter accepts it, and typing on replaces it.

Ctrl+click adds more carets, and typing, Backspace, Delete, arrows, Enter and Tab then act at all of them (Esc or a plain click goes back to one). Triple-click selects a line; dragging from the third click extends by whole lines. Drag the edge of the sidebar to resize it, or drag it nearly shut to collapse it.

Every key is Ctrl+key or Ctrl+Shift+key (TaskPaper's where known). Reopening a file puts the cursor back where you left it and restores which items were folded.

Omarchy's single-key `?` and `Q` would type into a document, so help and quit take Ctrl.

Text is Omarchy's monospace font (iA Writer Mono S by default) at the size of Omarchy's terminal (read from foot.ini).
