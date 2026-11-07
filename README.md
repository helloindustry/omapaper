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

If the file changes on disk (say, another computer editing it through a sync service), the open window updates in place within a moment, keeping your caret, folds and zoom. Edits in different places merge; if you both edit the same spot, yours is kept and theirs is saved beside it as `name (conflict …)`.

`taskpaper [file]` opens a specific file. Click a tag in the document or the sidebar to search for it. Ctrl+click adds another tag to the search (`@a and @b`), and Ctrl+Shift+click filters one out (`not @a`); do the same again to take it back out of the search. Search syntax: `@done`, `not @done`,
`@due < today`, `not @tag` (hides those items and everything under them), `project Inbox`, `task milk`, joined with `and` / `or`.
Saved searches live in the file under a `Searches:` project as `@search(query)`.

## Keys (TaskPaper's, with Ctrl for Command)

| Key | Action |
| --- | --- |
| Ctrl+D (or Ctrl+Enter) | Toggle done (adds `@done(today)`) |
| Ctrl+Shift+K | Delete the whole line (or every selected line; a folded item takes its hidden lines with it) |
| Ctrl+Shift+A | Archive the selection, or every `@done` item if nothing is selected |
| Ctrl+, (or Ctrl+9) | Collapse the item |
| Ctrl+. | Expand the item |
| Ctrl+Shift+, | Collapse everything |
| Ctrl+Alt+Shift+, | Collapse everything except this item |
| Ctrl+Shift+. | Expand everything |
| Ctrl+Shift+9 / Ctrl+Shift+0 | Collapse / expand everything |
| Tab / Shift+Tab (or Ctrl+] / Ctrl+[) | Indent / outdent |
| Ctrl+Shift+Up / Down | Move item with its children |
| Ctrl+Shift+M | Pick a project to move the selection (and everything beneath it) into |
| Ctrl+Shift+Right / Ctrl+Shift+Left | Focus in / out (zoom), as TaskPaper's Option-Command-Right / Left |
| Ctrl+H | Home: the whole document |
| Ctrl+F | Find and replace (top right; Enter = next, Shift+Enter = previous, Esc closes) |
| Ctrl+G / Ctrl+Shift+G | Find next / previous |
| Ctrl+Shift+F | Filter with the search bar |
| Ctrl+Shift+E | Save the current search |
| Ctrl+Shift+B | Toggle sidebar |
| Ctrl+B / Ctrl+I / Ctrl+Shift+X | Markdown bold / italic / strikethrough on the selection, or on the word at the caret when nothing is selected (Omawrite's keys) |
| Ctrl+K | Markdown link: the selection becomes the label, and a URL on the clipboard the target |
| Ctrl+= / Ctrl+- / Ctrl+0 | Bigger / smaller / reset text to Omarchy's size |
| Ctrl+O / Ctrl+S / Ctrl+Shift+S | Open / Save now / Save As |
| Ctrl+P | Print what is on screen to the default printer |
| Ctrl+/ | Show all keys. Type to filter them, Down and Enter to run one, Esc to close |
| Ctrl+Q | Quit |

Typing `@` and a letter offers a tag that is already in the document (`@M` suggests `@MIT`): Tab, Right, or Enter accepts it, and typing on replaces it.

Ctrl+click adds more carets, and typing, Backspace, Delete, arrows, Enter and Tab then act at all of them (Esc or a plain click goes back to one). Triple-click selects a line; dragging from the third click extends by whole lines. Drag the edge of the sidebar to resize it, or drag it nearly shut to collapse it.

Every key is Ctrl+key or Ctrl+Shift+key (TaskPaper's where known). Reopening a file puts the cursor back where you left it and restores which items were folded, which tags are collapsed in the sidebar, and whether the sidebar is open.

Omarchy's single-key `?` and `Q` would type into a document, so help and quit take Ctrl.

Text is Omarchy's monospace font (iA Writer Mono S by default) at the size of Omarchy's terminal (read from foot.ini).

## Markdown (only if you type it)

`**bold**`, `*italic*` / `_italic_`, `***both***`, `~~strike~~` and `` `code` `` are styled as you type, with the markers dimmed. The text is never changed. Markers need a non-space inside and a word edge outside, so `2*3*4`, `snake_case`, URLs and tags in ordinary TaskPaper lines look exactly as before.
