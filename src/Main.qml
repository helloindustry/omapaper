import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 960
    height: 680
    minimumWidth: 480
    minimumHeight: 320
    visible: true
    title: backend.title + " — TaskPaper"
    property bool caretThemed: backend.caretThemed() // the theme's accent by default; an undocumented shortcut switches to the blue, and it is remembered
    readonly property color caretColor: caretThemed ? theme.accent : "#00b3ff" // iA Writer caret blue, sampled from the app
    readonly property real caretWidth: 2.2
    color: theme.background

    Material.theme: theme.dark ? Material.Dark : Material.Light
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    readonly property real fs: backend.fontSize
    FontMetrics { id: lineMetrics; font.family: backend.fontFamily; font.pointSize: win.fs }
    readonly property real barHeight: Math.ceil(lineMetrics.lineSpacing) // one regular line // the size everything else scales from
    property bool sidebarVisible: backend.sidebarShown() // remembered between runs
    onSidebarVisibleChanged: backend.setSidebarShown(sidebarVisible)
    property bool helpVisible: false
    property int deleteBlock: -1 // a saved search waiting on y/n
    property string deleteName: ""
    // The "move to project" list: what was selected when it opened, and the projects it can go to.
    property bool moveOpen: false
    property int moveStart: 0
    property int moveEnd: 0
    property int moveCaret: 0
    property var moveItems: []
    property var moveShown: moveItems.filter(item => item.label.toLowerCase().indexOf(moveField.text.toLowerCase()) >= 0)
    function openMove() {
        moveStart = editor.selectionStart
        moveEnd = editor.selectionEnd
        moveCaret = editor.cursorPosition
        moveItems = backend.moveTargets(moveStart, moveEnd)
        moveField.text = ""
        moveList.currentIndex = 0
        moveOpen = true
        moveField.forceActiveFocus()
    }
    function closeMove() {
        moveOpen = false
        editor.forceActiveFocus()
    }
    function chooseMove(index) {
        if (index < 0 || index >= moveShown.length)
            return
        backend.moveToProject(moveStart, moveEnd, moveShown[index].block, moveCaret)
        closeMove()
    }
    // Ctrl+Up / Ctrl+Down move the sidebar's selection (Home, projects, tags, saved searches) and open it, as a click would.
    function sidebarCurrent(row) {
        const state = row.kind === "tag" ? backend.termState(row.query) : 0
        if (state === 1) return true
        if (state === 2) return false
        if (row.kind === "project") return backend.zoomBlock === row.block && backend.query === ""
        if (row.kind === "all") return backend.zoomBlock < 0 && backend.query === ""
        if (row.kind === "header") return false
        return backend.zoomBlock < 0 && backend.query === row.query
    }
    // Ctrl+1 ... Ctrl+9 open the first nine projects, top to bottom, as clicking them in the sidebar would.
    function openProjectNumber(number) {
        const rows = backend.sidebar
        let seen = 0
        for (let i = 0; i < rows.length; ++i) {
            if (rows[i].kind !== "project")
                continue
            if (++seen === number) {
                backend.showProject(rows[i].block)
                list.positionViewAtIndex(i, ListView.Contain)
                return
            }
        }
    }
    function stepSidebar(direction) {
        if (!win.sidebarVisible)
            return
        const rows = backend.sidebar
        let at = -1
        for (let i = 0; i < rows.length && at < 0; ++i)
            if (rows[i].kind !== "header" && sidebarCurrent(rows[i]))
                at = i
        let next = at
        do {
            next += direction
        } while (next >= 0 && next < rows.length && rows[next].kind === "header")
        if (at < 0)
            next = direction > 0 ? rows.findIndex(r => r.kind !== "header") : rows.length - 1 - [...rows].reverse().findIndex(r => r.kind !== "header")
        if (next < 0 || next >= rows.length)
            return
        const row = rows[next]
        if (row.kind === "all") backend.showAll()
        else if (row.kind === "project") backend.showProject(row.block)
        else backend.search(row.query)
        list.positionViewAtIndex(next, ListView.Contain)
    }
    property bool findVisible: false
    property bool searchOpen: false // the filter bar shows only while it is focused or a search is active
    property string findStatus: ""

    // A one-line text box used by the find panel.
    component Field: Rectangle {
        id: field
        property alias text: input.text
        property alias input: input
        property string hint
        signal edited()
        signal submitted(bool backwards)
        signal closed()
        Layout.fillWidth: true
        Layout.preferredHeight: Math.round(win.fs * 2.6)
        color: theme.background
        radius: Math.min(4, theme.radius)
        border.color: input.activeFocus ? theme.accent : theme.selection
        TextInput {
            id: input
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            verticalAlignment: TextInput.AlignVCenter
            font.family: backend.fontFamily
            font.pointSize: win.fs - 0.75
            color: theme.foreground
            selectionColor: theme.accent
            selectedTextColor: theme.accentForeground
            clip: true
            onTextEdited: field.edited()
            Keys.onReturnPressed: event => field.submitted(!!(event.modifiers & Qt.ShiftModifier))
            Keys.onEnterPressed: event => field.submitted(!!(event.modifiers & Qt.ShiftModifier))
            Keys.onEscapePressed: field.closed()
            Text {
                visible: !input.text
                anchors.verticalCenter: parent.verticalCenter
                text: field.hint
                font: input.font
                color: theme.muted
            }
        }
    }

    Connections {
        target: backend
        function onQueryChanged() { if (backend.query === "" && !search.activeFocus) win.searchOpen = false }
    }

    // Double-clicking the edge sizes the sidebar to its longest line.
    TextMetrics { id: rowMetrics; font.family: backend.fontFamily }
    function fitSidebar() {
        let widest = 0
        for (const row of backend.sidebar) {
            const header = row.kind === "header"
            rowMetrics.font.pointSize = win.fs - (header ? 2 : 0.75)
            rowMetrics.font.bold = header
            rowMetrics.text = row.label
            const x = (row.kind === "tag" ? 28 : 16) + (row.level || 0) * 14
            widest = Math.max(widest, x + rowMetrics.advanceWidth + 24)
        }
        win.sidebarVisible = true
        backend.sidebarWidth = Math.max(120, Math.min(Math.ceil(widest), win.width * 0.6))
    }

    function openFind() {
        const selected = editor.selectedText
        if (selected !== "" && selected.indexOf("\n") < 0 && selected.indexOf("\u2029") < 0)
            findField.text = selected
        win.findVisible = true
        findField.input.forceActiveFocus()
        findField.input.selectAll()
    }
    function closeFind() {
        win.findVisible = false
        editor.forceActiveFocus()
    }
    function findFrom(position, backwards) {
        const at = backend.findNext(findField.text, position, backwards)
        win.findStatus = at < 0 && findField.text !== "" ? qsTr("No matches") : ""
        if (at >= 0)
            editor.select(at, at + findField.text.length)
    }
    function findAgain(backwards) {
        if (findField.text === "") { win.openFind(); return }
        win.findFrom(backwards ? editor.selectionStart : editor.selectionEnd, backwards)
    }
    function replaceNow() {
        if (findField.text === "")
            return
        if (replaceAllBox.checked) {
            const n = backend.replaceAll(findField.text, replaceField.text)
            win.findStatus = n === 0 ? qsTr("No matches") : qsTr("%n replaced", "", n)
            return
        }
        // Replace the match that is selected, then move on to the next one.
        let from = editor.selectionEnd
        if (editor.selectedText.toLowerCase() === findField.text.toLowerCase())
            from = backend.replaceRange(editor.selectionStart, editor.selectionEnd, replaceField.text)
        findFrom(from, false)
    }

    // Keys follow TaskPaper with Ctrl in place of Command. Omarchy owns Super, so nothing here uses it,
    // and the single-letter keys of Omarchy's own apps (Q, ?) would type into the document, so they are Ctrl+Q and Ctrl+/.
    // Every key is Ctrl+key or Ctrl+Shift+key: TaskPaper's where known, otherwise the nearest free one.
    // Omarchy's own bindings all use Super, so none of these collide with it.
    Shortcut { sequence: "Ctrl+Alt+Shift+C"; context: Qt.ApplicationShortcut; onActivated: { win.caretThemed = !win.caretThemed; backend.setCaretThemed(win.caretThemed) } }
    Shortcut { sequence: "Ctrl+Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "Ctrl+Shift+M"; context: Qt.ApplicationShortcut; onActivated: win.openMove() }
    Shortcut { sequence: "Ctrl+/"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Ctrl+O"; context: Qt.ApplicationShortcut; onActivated: backend.openDialog() }
    Shortcut { sequence: "Ctrl+P"; context: Qt.ApplicationShortcut; onActivated: backend.print() }
    Shortcut { sequence: "Ctrl+S"; context: Qt.ApplicationShortcut; onActivated: backend.saveNow() }
    Shortcut { sequence: "Ctrl+Shift+S"; context: Qt.ApplicationShortcut; onActivated: backend.saveAsDialog() }
    Shortcut { sequences: ["Ctrl+=", "Ctrl++"]; context: Qt.ApplicationShortcut; onActivated: backend.changeFontSize(1) }
    Shortcut { sequence: "Ctrl+-"; context: Qt.ApplicationShortcut; onActivated: backend.changeFontSize(-1) }
    Shortcut { sequence: "Ctrl+F"; context: Qt.ApplicationShortcut; onActivated: win.findVisible ? win.closeFind() : win.openFind() }
    Shortcut { sequence: "Ctrl+G"; context: Qt.ApplicationShortcut; onActivated: win.findAgain(false) }
    Shortcut { sequence: "Ctrl+Shift+G"; context: Qt.ApplicationShortcut; onActivated: win.findAgain(true) }
    Shortcut { sequence: "Ctrl+Shift+F"; context: Qt.ApplicationShortcut; onActivated: { win.searchOpen = true; search.forceActiveFocus(); search.selectAll() } }
    Shortcut { sequence: "Ctrl+Shift+E"; context: Qt.ApplicationShortcut; onActivated: backend.saveSearch() }
    Shortcut { sequence: "Ctrl+Shift+B"; context: Qt.ApplicationShortcut; onActivated: win.sidebarVisible = !win.sidebarVisible }
    // Markdown, with the keys Omawrite uses.
    Shortcut { sequence: "Ctrl+B"; context: Qt.ApplicationShortcut; enabled: editor.activeFocus; onActivated: backend.toggleWrap(editor.selectionStart, editor.selectionEnd, "**") }
    Shortcut { sequence: "Ctrl+I"; context: Qt.ApplicationShortcut; enabled: editor.activeFocus; onActivated: backend.toggleWrap(editor.selectionStart, editor.selectionEnd, "*") }
    Shortcut { sequence: "Ctrl+Shift+X"; context: Qt.ApplicationShortcut; enabled: editor.activeFocus; onActivated: backend.toggleWrap(editor.selectionStart, editor.selectionEnd, "~~") }
    Shortcut { sequence: "Ctrl+K"; context: Qt.ApplicationShortcut; enabled: editor.activeFocus; onActivated: backend.insertLink(editor.selectionStart, editor.selectionEnd) }
    Shortcut { sequences: ["Ctrl+D", "Ctrl+Return", "Ctrl+Enter"]; context: Qt.ApplicationShortcut; onActivated: backend.toggleDone(editor.selectionStart, editor.selectionEnd) }
    Shortcut { sequence: "Ctrl+Shift+K"; context: Qt.ApplicationShortcut; onActivated: backend.deleteLines(editor.selectionStart, editor.selectionEnd) }
    Shortcut { sequence: "Ctrl+Shift+A"; context: Qt.ApplicationShortcut; onActivated: backend.archive(editor.selectionStart, editor.selectionEnd) }
    Shortcut { sequence: "Ctrl+1"; context: Qt.ApplicationShortcut; onActivated: { if (!backend.shiftHeld()) win.openProjectNumber(1) } }
    Shortcut { sequence: "Ctrl+2"; context: Qt.ApplicationShortcut; onActivated: { if (!backend.shiftHeld()) win.openProjectNumber(2) } }
    Shortcut { sequence: "Ctrl+3"; context: Qt.ApplicationShortcut; onActivated: { if (!backend.shiftHeld()) win.openProjectNumber(3) } }
    Shortcut { sequence: "Ctrl+4"; context: Qt.ApplicationShortcut; onActivated: { if (!backend.shiftHeld()) win.openProjectNumber(4) } }
    Shortcut { sequence: "Ctrl+5"; context: Qt.ApplicationShortcut; onActivated: { if (!backend.shiftHeld()) win.openProjectNumber(5) } }
    Shortcut { sequence: "Ctrl+6"; context: Qt.ApplicationShortcut; onActivated: { if (!backend.shiftHeld()) win.openProjectNumber(6) } }
    Shortcut { sequence: "Ctrl+7"; context: Qt.ApplicationShortcut; onActivated: { if (!backend.shiftHeld()) win.openProjectNumber(7) } }
    Shortcut { sequence: "Ctrl+8"; context: Qt.ApplicationShortcut; onActivated: { if (!backend.shiftHeld()) win.openProjectNumber(8) } }
    Shortcut { sequence: "Ctrl+9"; context: Qt.ApplicationShortcut; onActivated: {  if (backend.shiftHeld()) backend.foldAll(true, editor.cursorPosition); else win.openProjectNumber(9) } }
    Shortcut { sequence: "Ctrl+0"; context: Qt.ApplicationShortcut; onActivated: {  if (backend.shiftHeld()) backend.foldAll(false, editor.cursorPosition); else backend.changeFontSize(0) } }
    Shortcut { sequence: "Ctrl+,"; context: Qt.ApplicationShortcut; onActivated: { if (backend.shiftHeld()) backend.foldAll(true, editor.cursorPosition); else backend.foldItems(editor.selectionStart, editor.selectionEnd, true, false) } }
    Shortcut { sequence: "Ctrl+."; context: Qt.ApplicationShortcut; onActivated: { if (backend.shiftHeld()) backend.foldAll(false, editor.cursorPosition); else backend.foldItems(editor.selectionStart, editor.selectionEnd, false, false) } }
    Shortcut { sequence: "Ctrl+Alt+Shift+,"; context: Qt.ApplicationShortcut; onActivated: backend.foldOthers(editor.cursorPosition) }
    Shortcut { sequence: "Ctrl+]"; context: Qt.ApplicationShortcut; onActivated: backend.indent(editor.selectionStart, editor.selectionEnd, 1) }
    Shortcut { sequence: "Ctrl+["; context: Qt.ApplicationShortcut; onActivated: backend.indent(editor.selectionStart, editor.selectionEnd, -1) }
    Shortcut { sequence: "Ctrl+Shift+Up"; context: Qt.ApplicationShortcut; onActivated: backend.moveLines(editor.selectionStart, editor.selectionEnd, -1) }
    Shortcut { sequence: "Ctrl+Shift+Down"; context: Qt.ApplicationShortcut; onActivated: backend.moveLines(editor.selectionStart, editor.selectionEnd, 1) }
    Shortcut { sequence: "Ctrl+Shift+Right"; context: Qt.ApplicationShortcut; onActivated: backend.zoomIn(editor.cursorPosition) }
    Shortcut { sequence: "Ctrl+Shift+Left"; context: Qt.ApplicationShortcut; onActivated: backend.zoomOut() }
    Shortcut { sequence: "Ctrl+Up"; context: Qt.ApplicationShortcut; enabled: win.sidebarVisible; onActivated: win.stepSidebar(-1) }
    Shortcut { sequence: "Ctrl+Down"; context: Qt.ApplicationShortcut; enabled: win.sidebarVisible; onActivated: win.stepSidebar(1) }
    Shortcut { sequence: "Ctrl+H"; context: Qt.ApplicationShortcut; onActivated: backend.showAll() }

    Item {
        id: content
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        RowLayout {
            anchors.fill: parent
            spacing: 0

            // Projects, tags, and saved searches.
            Rectangle {
                visible: win.sidebarVisible
                Layout.preferredWidth: backend.sidebarWidth
                Layout.fillHeight: true
                color: theme.sidebar

                ListView {
                    id: list
                    anchors.fill: parent
                    anchors.topMargin: 12
                    anchors.bottomMargin: 6
                    clip: true
                    model: backend.sidebar
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}

                    // After a saved search is clicked, Delete asks "Delete? y/n" in the bottom bar. y deletes; n or Esc cancels.
                    onActiveFocusChanged: if (!activeFocus) win.deleteBlock = -1
                    Keys.onPressed: event => {
                        if (win.deleteBlock >= 0) {
                            if (event.key === Qt.Key_Y) backend.deleteBlock(win.deleteBlock)
                            if (event.key === Qt.Key_Y || event.key === Qt.Key_N || event.key === Qt.Key_Escape) {
                                win.deleteBlock = -1
                                editor.forceActiveFocus()
                            }
                            event.accepted = true
                            return
                        }
                        if (event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace) {
                            for (const r of backend.sidebar)
                                if (r.kind === "search" && r.query === backend.query) {
                                    win.deleteBlock = r.block
                                    win.deleteName = r.label
                                    event.accepted = true
                                    return
                                }
                        }
                        // Anything else is for the editor: hand focus back and pass typed text along.
                        editor.forceActiveFocus()
                        if (event.text.length > 0 && event.text.charCodeAt(0) >= 32 && !(event.modifiers & (Qt.ControlModifier | Qt.AltModifier)))
                            editor.insert(editor.cursorPosition, event.text)
                        event.accepted = event.key !== Qt.Key_Escape
                    }

                    delegate: Item {
                        id: row
                        required property var modelData
                        readonly property bool header: modelData.kind === "header"
                        // 1 when the search includes this tag, 2 when it filters it out (read the query so this updates)
                        readonly property int termState: { backend.query; return modelData.kind === "tag" ? backend.termState(modelData.query) : 0 }
                        readonly property bool current: termState === 1 ? true : termState === 2 ? false : modelData.kind === "project" ? backend.zoomBlock === modelData.block && backend.query === ""
                                                       : modelData.kind === "all" ? backend.zoomBlock < 0 && backend.query === ""
                                                       : modelData.kind === "header" ? false
                                                       : backend.zoomBlock < 0 && backend.query === modelData.query
                        width: ListView.view.width
                        height: Math.round(win.fs * (header ? 3.6 : 2.6))

                        Rectangle {
                            anchors.fill: parent
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6
                            radius: Math.min(4, theme.radius)
                            color: row.current ? theme.selection : hover.hovered && !row.header ? Qt.alpha(theme.selection, 0.5) : "transparent"
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.verticalCenterOffset: row.header ? 6 : 0
                            x: (row.modelData.kind === "tag" ? 28 : 16) + (row.modelData.level || 0) * 14
                            width: parent.width - x - 16
                            text: row.modelData.label
                            elide: Text.ElideRight
                            font.family: backend.fontFamily
                            font.pointSize: win.fs - (row.header ? 2 : 0.75)
                            font.bold: row.header
                            font.strikeout: row.termState === 2
                            color: row.header || row.termState === 2 ? theme.muted : theme.foreground
                        }
                        // Tags that have values fold, with the same arrow as the editor.
                        Text {
                            visible: row.modelData.foldable === true
                            x: 4
                            width: 24
                            height: parent.height
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            text: row.modelData.collapsed ? "▸" : "▾"
                            font.pointSize: win.fs + 6
                            color: row.modelData.collapsed ? theme.accent : theme.muted
                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                            TapHandler { onTapped: backend.toggleTagFold(row.modelData.name) }
                        }
                        HoverHandler { id: hover }
                        // Ctrl+click adds a tag (or one of its values) to the search; Ctrl+Shift+click filters it out.
                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            acceptedModifiers: Qt.ControlModifier
                            enabled: row.modelData.kind === "tag"
                            onTapped: backend.include(row.modelData.query)
                        }
                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            acceptedModifiers: Qt.ControlModifier | Qt.ShiftModifier
                            enabled: row.modelData.kind === "tag"
                            onTapped: backend.exclude(row.modelData.query)
                        }
                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            acceptedModifiers: Qt.NoModifier
                            enabled: !row.header
                            onTapped: {
                                if (row.modelData.kind === "all") backend.showAll()
                                else if (row.modelData.kind === "project") backend.showProject(row.modelData.block)
                                else { backend.search(row.modelData.query); if (row.modelData.kind === "search") list.forceActiveFocus() }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // The search bar; typing filters the document as you go.
                Rectangle {
                    visible: win.searchOpen || backend.query !== ""
                    z: 10 // the tag suggestions hang below this bar, over the editor
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.round(win.fs * 3.6)
                    color: theme.sidebar
                    TextInput {
                        id: search
                        // Typing "@" offers the document's tags; Down/Up choose, Enter or Tab completes, Esc just closes the list.
                        property string tagPrefix: ""
                        property var tagChoices: []
                        property int tagPicked: -1
                        property bool tagsDismissed: false
                        function refreshTags() {
                            const m = text.slice(0, cursorPosition).match(/(^|[\s(])@([\w.\-]*)$/)
                            tagPrefix = m ? m[2] : ""
                            let out = []
                            if (m && !tagsDismissed) {
                                const want = tagPrefix.toLowerCase()
                                for (const row of backend.sidebar)
                                    if (row.kind === "tag" && row.level === 0 && row.name.toLowerCase().indexOf(want) === 0
                                            && !(row.name.toLowerCase() === want))
                                        out.push(row.name)
                            }
                            tagChoices = out.slice(0, 8)
                            tagPicked = -1
                        }
                        function completeTag(name) {
                            // insert/remove edit the text the way typing does, so the binding to the query survives.
                            const at = text.slice(0, cursorPosition).lastIndexOf("@")
                            remove(at, cursorPosition)
                            insert(at, "@" + name)
                            cursorPosition = at + 1 + name.length
                            backend.query = text
                            tagsDismissed = false
                            refreshTags()
                        }
                        onTextEdited: { tagsDismissed = false; backend.query = text }
                        onTextChanged: refreshTags()
                        onCursorPositionChanged: refreshTags()
                        Keys.onPressed: event => {
                            if (tagChoices.length === 0)
                                return
                            if (event.key === Qt.Key_Down) tagPicked = Math.min(tagChoices.length - 1, tagPicked + 1)
                            else if (event.key === Qt.Key_Up && tagPicked >= 0) tagPicked -= 1
                            else if (event.key === Qt.Key_Tab) completeTag(tagChoices[Math.max(tagPicked, 0)])
                            else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && tagPicked >= 0) completeTag(tagChoices[tagPicked])
                            else if (event.key === Qt.Key_Escape) { tagsDismissed = true; refreshTags() }
                            else return
                            event.accepted = true
                        }
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        verticalAlignment: TextInput.AlignVCenter
                        font.family: backend.fontFamily
                        font.pointSize: win.fs - 0.75
                        color: theme.foreground
                        selectionColor: theme.accent
                        selectedTextColor: theme.accentForeground
                        clip: true
                        text: backend.query
                        onActiveFocusChanged: if (!activeFocus && backend.query === "") win.searchOpen = false
                        // Specific key handlers run before Keys.onPressed, so the open suggestion list is answered here too.
                        Keys.onEscapePressed: {
                            if (tagChoices.length > 0) { tagsDismissed = true; refreshTags(); return }
                            backend.query = ""; win.searchOpen = false; editor.forceActiveFocus()
                        }
                        Keys.onReturnPressed: {
                            if (tagChoices.length > 0 && tagPicked >= 0) completeTag(tagChoices[tagPicked])
                            else editor.forceActiveFocus()
                        }
                        Keys.onEnterPressed: {
                            if (tagChoices.length > 0 && tagPicked >= 0) completeTag(tagChoices[tagPicked])
                            else editor.forceActiveFocus()
                        }
                        Text {
                            visible: !search.text && !search.activeFocus
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Search")
                            font: search.font
                            color: theme.muted
                        }
                    }
                    Rectangle {
                        visible: search.activeFocus && search.tagChoices.length > 0
                        x: 12
                        y: parent.height
                        width: 260
                        height: search.tagChoices.length * Math.round(win.fs * 2.2) + 8
                        color: theme.sidebar
                        border.color: theme.selection
                        radius: 4
                        Column {
                            anchors.fill: parent
                            anchors.margins: 4
                            Repeater {
                                model: search.tagChoices
                                delegate: Rectangle {
                                    required property string modelData
                                    required property int index
                                    width: parent.width
                                    height: Math.round(win.fs * 2.2)
                                    radius: 3
                                    color: index === search.tagPicked ? theme.selection : "transparent"
                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        x: 8
                                        textFormat: Text.StyledText
                                        text: "<font color=\"" + theme.accent + "\">@" + modelData.slice(0, search.tagPrefix.length) + "</font>" + modelData.slice(search.tagPrefix.length)
                                        color: theme.foreground
                                        font.family: backend.fontFamily
                                        font.pointSize: win.fs - 0.75
                                    }
                                    TapHandler { onTapped: search.completeTag(modelData) }
                                }
                            }
                        }
                    }
                }

                // Where a zoom has taken you; click to step back out.
                Rectangle {
                    visible: backend.zoomBlock >= 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.round(win.fs * 2.8)
                    color: theme.sidebar
                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideLeft
                        text: backend.zoomPath
                        font.family: backend.fontFamily
                        font.pointSize: win.fs - 1.5
                        color: theme.muted
                    }
                    TapHandler { onTapped: backend.zoomOut() }
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    TextArea {
                        id: editor
                        objectName: "editor"
                        ContextMenu.menu: null // no popup on right-click
                        focus: true
                        background: null
                        wrapMode: TextEdit.Wrap
                        font.family: backend.fontFamily
                        font.pointSize: win.fs
                        color: theme.foreground
                        selectionColor: theme.accent
                        selectedTextColor: theme.accentForeground
                        // The one non-stock touch: a blue caret, a hair thicker than the default.
                        cursorDelegate: Rectangle {
                            id: caret
                            width: win.caretWidth
                            color: win.caretColor
                            antialiasing: true
                            // A custom caret has to blink itself. It stays solid while you type or move, then blinks.
                            Timer {
                                id: blink
                                interval: 530
                                repeat: true
                                running: editor.activeFocus
                                onTriggered: caret.opacity = caret.opacity > 0 ? 0 : 1
                            }
                            Connections {
                                target: editor
                                function onCursorPositionChanged() { caret.opacity = 1; blink.restart() }
                            }
                        }
                        leftPadding: 32
                        rightPadding: 24
                        topPadding: 22
                        bottomPadding: win.barHeight + 6 // the status bar is flush with the window's bottom edge and doubles as the margin
                        // A property read, so the width follows the font size when it changes (a method call would not be re-evaluated).
                        tabStopDistance: fontMetrics.averageCharacterWidth * 2
                        persistentSelection: true
                        selectByMouse: true
                        placeholderText: qsTr("Start typing. “- ” makes a task, a line ending in “:” makes a project, and “@tag” tags it.")
                        placeholderTextColor: theme.muted
                        property int previousPos: 0
                        property int currentPos: 0
                        onCursorPositionChanged: { previousPos = currentPos; currentPos = cursorPosition; backend.rememberCursor(cursorPosition) }
                        Component.onCompleted: backend.attach(textDocument)

                        // Several carets: Ctrl+click adds one (the old caret stays behind), and keys then act on all of them.
                        property int ctrlBase: 0
                        TapHandler {
                            acceptedModifiers: Qt.ControlModifier
                            onPressedChanged: if (pressed) editor.ctrlBase = editor.cursorPosition
                            onTapped: eventPoint => {
                                const clicked = editor.positionAt(eventPoint.position.x, eventPoint.position.y)
                                // Ctrl+click on a tag adds it to the search, as in the sidebar.
                                const tag = editor.tagAtPoint(eventPoint.position.x, eventPoint.position.y)
                                if (tag !== "") {
                                    backend.include(tag)
                                    return
                                }
                                // The caret that was there stays as an extra; the click becomes the main caret.
                                const base = editor.ctrlBase === clicked ? editor.previousPos : editor.ctrlBase
                                backend.addCursor(base, clicked)
                                editor.cursorPosition = clicked
                            }
                        }
                        // Ctrl+Shift+click on a tag filters it out of the search.
                        TapHandler {
                            acceptedModifiers: Qt.ControlModifier | Qt.ShiftModifier
                            onTapped: eventPoint => {
                                const tag = editor.tagAtPoint(eventPoint.position.x, eventPoint.position.y)
                                if (tag !== "") backend.exclude(tag)
                            }
                        }
                        // Ctrl+Shift+Alt+click on a tag removes it.
                        TapHandler {
                            acceptedModifiers: Qt.ControlModifier | Qt.ShiftModifier | Qt.AltModifier
                            onTapped: eventPoint => {
                                if (editor.tagAtPoint(eventPoint.position.x, eventPoint.position.y) !== "")
                                    backend.removeTag(editor.positionAt(eventPoint.position.x, eventPoint.position.y))
                            }
                        }
                        Keys.onPressed: event => {
                            // The editor claims Ctrl+Shift+Left/Right itself (select by word), so a Shortcut never sees them.
                            const ctrlShift = (event.modifiers & Qt.ControlModifier) && (event.modifiers & Qt.ShiftModifier) && !(event.modifiers & Qt.AltModifier)
                            if (win.sidebarVisible && event.modifiers === Qt.ControlModifier && (event.key === Qt.Key_Up || event.key === Qt.Key_Down)) {
                                win.stepSidebar(event.key === Qt.Key_Up ? -1 : 1); event.accepted = true; return
                            }
                            if (ctrlShift && event.key === Qt.Key_Right) { backend.zoomIn(editor.cursorPosition); event.accepted = true; return }
                            if (ctrlShift && event.key === Qt.Key_Left) { backend.zoomOut(); event.accepted = true; return }
                            if ((event.modifiers & Qt.ControlModifier) && (event.modifiers & Qt.ShiftModifier) && (event.modifiers & Qt.AltModifier)
                                    && (event.key === Qt.Key_Comma || event.key === Qt.Key_Less)) { backend.foldOthers(editor.cursorPosition); event.accepted = true; return }
                            // Collapse / expand all. The shortcuts above can miss when the keyboard reports Shift+9 as "(" and
                            // Shift+0 as ")", so the editor answers those keys itself.
                            if (ctrlShift && (event.key === Qt.Key_Comma || event.key === Qt.Key_Less)) { backend.foldAll(true, editor.cursorPosition); event.accepted = true; return }
                            if (ctrlShift && (event.key === Qt.Key_Period || event.key === Qt.Key_Greater)) { backend.foldAll(false, editor.cursorPosition); event.accepted = true; return }
                            if (ctrlShift && (event.key === Qt.Key_9 || event.key === Qt.Key_ParenLeft)) { backend.foldAll(true, editor.cursorPosition); event.accepted = true; return }
                            if (ctrlShift && (event.key === Qt.Key_0 || event.key === Qt.Key_ParenRight)) { backend.foldAll(false, editor.cursorPosition); event.accepted = true; return }
                            if (event.key === Qt.Key_End && acceptSuggestion()) {
                                event.accepted = true
                                return
                            }
                            if (backend.extraCursors.length === 0)
                                return
                            const k = event.key
                            const bare = !(event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier))
                            let pos = -1
                            if (k === Qt.Key_Escape) {
                                backend.clearCursors()
                                event.accepted = true
                                return
                            }
                            if (!bare)
                                return
                            if ([Qt.Key_Backspace, Qt.Key_Delete, Qt.Key_Left, Qt.Key_Right, Qt.Key_Up, Qt.Key_Down, Qt.Key_Home, Qt.Key_End].indexOf(k) >= 0) {
                                if (event.modifiers & Qt.ShiftModifier) { backend.clearCursors(); return } // selecting is single-caret
                                pos = backend.multiKey(editor.cursorPosition, k)
                            } else if (k === Qt.Key_Return || k === Qt.Key_Enter) {
                                pos = backend.multiType(editor.cursorPosition, "\n")
                            } else if (k === Qt.Key_Tab) {
                                pos = backend.multiType(editor.cursorPosition, "\t")
                            } else if (event.text.length > 0 && event.text.charCodeAt(0) >= 32) {
                                pos = backend.multiType(editor.cursorPosition, event.text)
                            }
                            if (pos >= 0) {
                                editor.cursorPosition = pos
                                event.accepted = true
                            }
                        }
                        Repeater {
                            model: backend.extraCursors
                            delegate: Rectangle {
                                required property var modelData
                                readonly property rect line: { editor.contentHeight; editor.width; return editor.positionToRectangle(modelData) }
                                x: line.x
                                y: line.y
                                width: win.caretWidth
                                height: line.height
                                color: win.caretColor
                                SequentialAnimation on opacity {
                                    loops: Animation.Infinite
                                    NumberAnimation { to: 0; duration: 530 }
                                    NumberAnimation { to: 1; duration: 530 }
                                }
                            }
                        }

                        // Triple-click selects the whole line, and dragging from that third press extends by whole lines.
                        property var lineAnchor: null
                        property var lineSel: [0, 0]
                        property int clickCount: 0
                        property double lastPress: 0
                        property point lastPoint
                        PointHandler {
                            acceptedButtons: Qt.LeftButton
                            acceptedModifiers: Qt.NoModifier
                            onActiveChanged: {
                                if (active) {
                                    const now = Date.now()
                                    const p = point.position
                                    const near = Math.abs(p.x - editor.lastPoint.x) < 6 && Math.abs(p.y - editor.lastPoint.y) < 6
                                    editor.clickCount = (now - editor.lastPress < Qt.styleHints.mouseDoubleClickInterval && near) ? editor.clickCount + 1 : 1
                                    editor.lastPress = now
                                    editor.lastPoint = p
                                    if (editor.clickCount === 3) {
                                        const r = backend.lineRange(editor.positionAt(p.x, p.y))
                                        editor.lineAnchor = r
                                        editor.selectByMouse = false // so the editor's own drag selection does not fight this one
                                        editor.lineSel = [r[0], r[1]]
                                        editor.select(r[0], r[1])
                                    }
                                } else {
                                    // The editor collapses a selection when the button comes up; put the line selection back.
                                    if (editor.lineAnchor !== null) {
                                        const keep = editor.lineSel
                                        Qt.callLater(() => editor.select(keep[0], keep[1]))
                                    }
                                    editor.lineAnchor = null
                                    editor.selectByMouse = true
                                }
                            }
                            onPointChanged: {
                                if (!active || editor.lineAnchor === null)
                                    return
                                const r = backend.lineRange(editor.positionAt(point.position.x, point.position.y))
                                const a = editor.lineAnchor
                                editor.lineSel = r[0] < a[0] ? [a[1], r[0]] : [a[0], Math.max(a[1], r[1])]
                                editor.select(editor.lineSel[0], editor.lineSel[1])
                            }
                        }

                        // positionAt snaps to the nearest character, so a click in the blank space after a line would
                        // count as a click on its last tag. A point only counts when it is inside the span's own box.
                        function inSpan(span, x, y) {
                            if (span.length === 0)
                                return false
                            const a = positionToRectangle(span[0]), b = positionToRectangle(span[1])
                            if (y < a.y || y >= b.y + b.height)
                                return false
                            if (a.y === b.y)
                                return x >= a.x && x < b.x
                            return y < a.y + a.height ? x >= a.x : y >= b.y ? x < b.x : true
                        }
                        function tagAtPoint(x, y) {
                            const at = positionAt(x, y)
                            return inSpan(backend.tagSpan(at), x, y) ? backend.tagAt(at) : ""
                        }
                        function linkAtPoint(x, y) {
                            const at = positionAt(x, y)
                            return inSpan(backend.linkSpan(at), x, y) ? backend.linkAt(at) : ""
                        }

                        // Links open on a click, and tags search for themselves like the sidebar's; both show a pointing hand.
                        property bool overLink: false
                        HoverHandler {
                            onPointChanged: {
                                const x = point.position.x, y = point.position.y
                                const at = editor.positionAt(x, y)
                                let span = backend.linkSpan(at)
                                if (!editor.inSpan(span, x, y))
                                    span = backend.tagSpan(at)
                                const over = editor.inSpan(span, x, y)
                                editor.overLink = over
                                if (over) backend.setHover(span[0], span[1])
                                else backend.setHover(-1, -1)
                            }
                            onHoveredChanged: if (!hovered) { editor.overLink = false; backend.setHover(-1, -1) }
                        }
                        HoverHandler { enabled: editor.overLink; cursorShape: Qt.PointingHandCursor }
                        TapHandler {
                            acceptedModifiers: Qt.NoModifier
                            onTapped: eventPoint => {
                                backend.clearCursors()
                                const url = editor.linkAtPoint(eventPoint.position.x, eventPoint.position.y)
                                if (url !== "") {
                                    Qt.openUrlExternally(url)
                                    return
                                }
                                const tag = editor.tagAtPoint(eventPoint.position.x, eventPoint.position.y)
                                if (tag !== "") backend.search(tag)
                            }
                        }

                        FontMetrics { id: fontMetrics; font: editor.font }

                        // A Keys handler accepts the key unless told otherwise, so say so when the default Return should run.
                        Keys.onReturnPressed: event => {
                            if (acceptSuggestion()) { event.accepted = true; return }
                            event.accepted = editor.selectionStart === editor.selectionEnd && backend.newline(editor.cursorPosition)
                        }
                        // A tag suggestion sits after the caret as selected text; Tab, Right, End, or Enter keep it.
                        property int suggestStart: -1
                        property int suggestEnd: -1
                        function acceptSuggestion() {
                            if (suggestEnd < 0 || selectionStart !== suggestStart || selectionEnd !== suggestEnd)
                                return false
                            const end = suggestEnd
                            suggestStart = suggestEnd = -1
                            deselect()
                            cursorPosition = end
                            return true
                        }
                        Connections {
                            target: backend
                            function onSuggestion(start, end) {
                                editor.suggestStart = start
                                editor.suggestEnd = end
                                editor.select(start, end)
                            }
                        }
                        Keys.onRightPressed: event => { if (acceptSuggestion()) event.accepted = true; else event.accepted = false }
                        Keys.onTabPressed: event => { if (!acceptSuggestion()) backend.indent(editor.selectionStart, editor.selectionEnd, 1); event.accepted = true }
                        Keys.onBacktabPressed: event => { backend.indent(editor.selectionStart, editor.selectionEnd, -1); event.accepted = true }

                        // Disclosure triangles in the left margin, one per item that has children.
                        Repeater {
                            model: backend.folds
                            delegate: Text {
                                required property var modelData
                                readonly property rect line: { editor.contentHeight; editor.width; return editor.positionToRectangle(modelData.pos) }
                                x: 4
                                y: line.y
                                width: 24
                                height: line.height
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                text: modelData.folded ? "▸" : "▾"
                                font.pointSize: win.fs + 6
                                color: modelData.folded ? theme.accent : theme.muted
                                HoverHandler { cursorShape: Qt.PointingHandCursor }
                                TapHandler { onTapped: backend.toggleFoldBlock(modelData.block) }
                            }
                        }

                        Connections {
                            target: backend
                            function onCursorRequested(position) { editor.forceActiveFocus(); editor.cursorPosition = position }
                            function onMoved(start, end) { editor.select(start, end) }
                        }
                    }
                }

                Rectangle {
                    visible: backend.error !== ""
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.round(win.fs * 2.8)
                    color: theme.sidebar
                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        verticalAlignment: Text.AlignVCenter
                        text: backend.error
                        font.family: backend.fontFamily
                        font.pointSize: win.fs - 1.5
                        color: "#ff5f4a"
                    }
                }
            }
        }

        // A click anywhere outside the find panel closes it, and still reaches whatever is under it.
        MouseArea {
            anchors.fill: parent
            enabled: win.findVisible
            onPressed: mouse => { win.closeFind(); mouse.accepted = false }
        }

        // Drag the sidebar's edge to resize it, drag it nearly shut to collapse it, double-click to fit it to its text.
        // It straddles the edge and sits above everything, so the scroll bar and the editor can't take the press.
        Item {
            x: (win.sidebarVisible ? backend.sidebarWidth : 0) - 6
            width: 12
            height: parent.height
            z: 100
            HoverHandler { cursorShape: Qt.SplitHCursor }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.SplitHCursor
                preventStealing: true
                onPositionChanged: mouse => {
                    if (!pressed)
                        return
                    const x = mapToItem(content, mouse.x, 0).x
                    if (x < 100) {
                        win.sidebarVisible = false
                    } else {
                        win.sidebarVisible = true
                        backend.sidebarWidth = Math.max(140, Math.min(x, win.width * 0.6))
                    }
                }
                onReleased: backend.saveSidebarWidth()
                onDoubleClicked: { win.fitSidebar(); backend.saveSidebarWidth() }
            }
        }

        // Find and replace, in the top right corner.
        Rectangle {
            id: findPanel
            visible: win.findVisible
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 12
            width: Math.round(win.fs * 30)
            height: findColumn.implicitHeight + 20
            color: theme.sidebar
            radius: theme.radius
            border.color: theme.selection

            ColumnLayout {
                id: findColumn
                anchors.fill: parent
                anchors.margins: 10
                spacing: 6

                Field {
                    id: findField
                    hint: qsTr("Find")
                    onEdited: win.findFrom(editor.selectionStart, false)
                    onSubmitted: backwards => win.findFrom(backwards ? editor.selectionStart : editor.selectionEnd, backwards)
                    onClosed: win.closeFind()
                }
                Field {
                    id: replaceField
                    hint: qsTr("Replace with")
                    onSubmitted: win.replaceNow()
                    onClosed: win.closeFind()
                }
                RowLayout {
                    Layout.fillWidth: true
                    CheckBox {
                        id: replaceAllBox
                        text: qsTr("Replace all")
                        font.family: backend.fontFamily
                        font.pointSize: win.fs - 0.75
                    }
                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignRight
                        text: win.findStatus
                        elide: Text.ElideRight
                        font.family: backend.fontFamily
                        font.pointSize: win.fs - 1.5
                        color: theme.muted
                    }
                    Button {
                        text: qsTr("Replace")
                        flat: true
                        font.family: backend.fontFamily
                        font.pointSize: win.fs - 0.75
                        onClicked: win.replaceNow()
                    }
                }
            }
        }

        // Pick a project to move the selected items into.
        Rectangle {
            visible: win.moveOpen
            anchors.fill: parent
            color: Qt.alpha(theme.background, 0.6)
            TapHandler { onTapped: win.closeMove() }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                y: Math.round(parent.height * 0.15)
                width: Math.min(460, parent.width - 48)
                height: moveColumn.implicitHeight + 16
                color: theme.sidebar
                border.color: theme.selection
                radius: Math.min(4, theme.radius)
                TapHandler { }

                ColumnLayout {
                    id: moveColumn
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6

                    TextInput {
                        id: moveField
                        Layout.fillWidth: true
                        Layout.leftMargin: 8
                        Layout.rightMargin: 8
                        Layout.preferredHeight: Math.round(win.fs * 2.2)
                        verticalAlignment: TextInput.AlignVCenter
                        font.family: backend.fontFamily
                        font.pointSize: win.fs
                        color: theme.foreground
                        selectionColor: theme.accent
                        selectedTextColor: theme.accentForeground
                        clip: true
                        onTextChanged: moveList.currentIndex = 0
                        Keys.onEscapePressed: win.closeMove()
                        Keys.onReturnPressed: win.chooseMove(moveList.currentIndex)
                        Keys.onEnterPressed: win.chooseMove(moveList.currentIndex)
                        Keys.onUpPressed: moveList.currentIndex = Math.max(0, moveList.currentIndex - 1)
                        Keys.onDownPressed: moveList.currentIndex = Math.min(win.moveShown.length - 1, moveList.currentIndex + 1)
                        Text {
                            visible: !moveField.text
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Move to project…")
                            font: moveField.font
                            color: theme.muted
                        }
                    }

                    ListView {
                        id: moveList
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(contentHeight, Math.round(win.fs * 2 * 10))
                        clip: true
                        model: win.moveShown
                        boundsBehavior: Flickable.StopAtBounds
                        currentIndex: 0
                        onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
                        delegate: Item {
                            id: moveRow
                            required property var modelData
                            required property int index
                            width: ListView.view.width
                            height: Math.round(win.fs * 2)
                            Rectangle {
                                anchors.fill: parent
                                radius: Math.min(4, theme.radius)
                                color: moveRow.index === moveList.currentIndex ? theme.selection : "transparent"
                            }
                            Text {
                                x: 8 + moveRow.modelData.level * 14
                                width: parent.width - x - 8
                                anchors.verticalCenter: parent.verticalCenter
                                text: moveRow.modelData.label
                                elide: Text.ElideRight
                                font.family: backend.fontFamily
                                font.pointSize: win.fs - 0.75
                                color: theme.foreground
                            }
                            TapHandler { onTapped: win.chooseMove(moveRow.index) }
                        }
                    }

                    Text {
                        visible: win.moveShown.length === 0
                        Layout.leftMargin: 8
                        text: win.moveItems.length === 0 ? qsTr("No projects to move into") : qsTr("No match")
                        font.family: backend.fontFamily
                        font.pointSize: win.fs - 0.75
                        color: theme.muted
                    }
                }
            }
        }

        // A brief cue along the bottom edge when something happens ("Saved"). It floats, so nothing moves, and it fades by itself.
        Rectangle {
            id: toast
            property string shownText: ""
            readonly property bool active: (backend.notice !== "" || win.deleteBlock >= 0) && backend.error === ""
            z: 5
            anchors.left: parent.left
            anchors.leftMargin: win.sidebarVisible ? backend.sidebarWidth : 0 // only under the editor
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: Math.round(win.fs * 3.6) // the same bar as the search bar at the top
            color: theme.sidebar
            opacity: active ? 1 : 0
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: 180 } }
            Connections {
                target: backend
                function onNoticeChanged() { if (backend.notice !== "") toast.shownText = backend.notice }
            }
            Text {
                anchors.fill: parent
                anchors.leftMargin: 16
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                text: win.deleteBlock >= 0 ? qsTr("Delete “%1”?  y / n").arg(win.deleteName) : toast.shownText
                font.family: backend.fontFamily
                font.pointSize: win.fs - 0.75
                color: theme.foreground
            }
        }

        // Every key, on one screen. Typing filters it, Down picks a row, Enter runs that action.
        Rectangle {
            id: help
            visible: win.helpVisible
            anchors.fill: parent
            color: Qt.alpha(theme.background, 0.94)
            focus: visible

            property string filter: ""
            property int picked: -1
            readonly property real rowHeight: Math.round(win.fs * 1.75)
            // [label, keys, action]. An action of null is something done with the mouse.
            readonly property var all: [
                [qsTr("Toggle done"), "Ctrl+D", () => backend.toggleDone(editor.selectionStart, editor.selectionEnd)],
                [qsTr("Archive done items / selection"), "Ctrl+Shift+A", () => backend.archive(editor.selectionStart, editor.selectionEnd)],
                [qsTr("Delete line"), "Ctrl+Shift+K", () => backend.deleteLines(editor.selectionStart, editor.selectionEnd)],
                [qsTr("Collapse item"), "Ctrl+,", () => backend.foldItems(editor.selectionStart, editor.selectionEnd, true, false)],
                [qsTr("Expand item"), "Ctrl+.", () => backend.foldItems(editor.selectionStart, editor.selectionEnd, false, false)],
                [qsTr("Collapse everything"), "Ctrl+Shift+,", () => backend.foldAll(true, editor.cursorPosition)],
                [qsTr("Expand everything"), "Ctrl+Shift+.", () => backend.foldAll(false, editor.cursorPosition)],
                [qsTr("Collapse everything except this item"), "Ctrl+Alt+Shift+,", () => backend.foldOthers(editor.cursorPosition)],
                [qsTr("Indent"), "Tab", () => backend.indent(editor.selectionStart, editor.selectionEnd, 1)],
                [qsTr("Outdent"), "Shift+Tab", () => backend.indent(editor.selectionStart, editor.selectionEnd, -1)],
                [qsTr("Move up"), "Ctrl+Shift+Up", () => backend.moveLines(editor.selectionStart, editor.selectionEnd, -1)],
                [qsTr("Move down"), "Ctrl+Shift+Down", () => backend.moveLines(editor.selectionStart, editor.selectionEnd, 1)],
                [qsTr("Move to a project"), "Ctrl+Shift+M", () => win.openMove()],
                [qsTr("Focus in"), "Ctrl+Shift+Right", () => backend.zoomIn(editor.cursorPosition)],
                [qsTr("Focus out"), "Ctrl+Shift+Left", () => backend.zoomOut()],
                [qsTr("Open project 1 to 9 (top to bottom)"), "Ctrl+1 … Ctrl+9", null],
                [qsTr("Previous in the sidebar"), "Ctrl+Up", () => win.stepSidebar(-1)],
                [qsTr("Next in the sidebar"), "Ctrl+Down", () => win.stepSidebar(1)],
                [qsTr("Home (whole document)"), "Ctrl+H", () => backend.showAll()],
                [qsTr("Find and replace"), "Ctrl+F", () => win.openFind()],
                [qsTr("Find next"), "Ctrl+G", () => win.findAgain(false)],
                [qsTr("Find previous"), "Ctrl+Shift+G", () => win.findAgain(true)],
                [qsTr("Filter (search bar)"), "Ctrl+Shift+F", () => { win.searchOpen = true; search.forceActiveFocus(); search.selectAll() }],
                [qsTr("Save search"), "Ctrl+Shift+E", () => backend.saveSearch()],
                [qsTr("Add a tag to the search"), "Ctrl+click", null],
                [qsTr("Filter a tag out"), "Ctrl+Shift+click", null],
                [qsTr("Remove a tag from its line"), "Ctrl+Shift+Alt+click", null],
                [qsTr("Toggle sidebar"), "Ctrl+Shift+B", () => { win.sidebarVisible = !win.sidebarVisible }],
                [qsTr("Bold"), "Ctrl+B", () => backend.toggleWrap(editor.selectionStart, editor.selectionEnd, "**")],
                [qsTr("Italic"), "Ctrl+I", () => backend.toggleWrap(editor.selectionStart, editor.selectionEnd, "*")],
                [qsTr("Strikethrough"), "Ctrl+Shift+X", () => backend.toggleWrap(editor.selectionStart, editor.selectionEnd, "~~")],
                [qsTr("Markdown link"), "Ctrl+K", () => backend.insertLink(editor.selectionStart, editor.selectionEnd)],
                [qsTr("Bigger text"), "Ctrl+=", () => backend.changeFontSize(1)],
                [qsTr("Smaller text"), "Ctrl+-", () => backend.changeFontSize(-1)],
                [qsTr("Reset text size"), "Ctrl+0", () => backend.changeFontSize(0)],
                [qsTr("Open"), "Ctrl+O", () => backend.openDialog()],
                [qsTr("Save"), "Ctrl+S", () => backend.saveNow()],
                [qsTr("Save As"), "Ctrl+Shift+S", () => backend.saveAsDialog()],
                [qsTr("Print"), "Ctrl+P", () => backend.print()],
                [qsTr("Undo"), "Ctrl+Z", () => editor.undo()],
                [qsTr("Redo"), "Ctrl+Shift+Z", () => editor.redo()],
                [qsTr("This list"), "Ctrl+/", null],
                [qsTr("Quit"), "Ctrl+Q", () => Qt.quit()]
            ]
            readonly property var shown: {
                const q = filter.toLowerCase()
                return q === "" ? all : all.filter(r => r[0].toLowerCase().indexOf(q) >= 0 || r[1].toLowerCase().indexOf(q) >= 0)
            }

            function reset() { filter = ""; picked = -1 }
            function esc(t) { return t.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;") }
            // The typed part gets the accent colour; everything else is plain.
            function marked(t, color) {
                const at = filter === "" ? -1 : t.toLowerCase().indexOf(filter.toLowerCase())
                if (at < 0)
                    return esc(t)
                return esc(t.slice(0, at)) + "<font color=\"" + color + "\">" + esc(t.slice(at, at + filter.length)) + "</font>" + esc(t.slice(at + filter.length))
            }
            function dismiss() { win.helpVisible = false; editor.forceActiveFocus() }
            function run() {
                const row = shown[picked]
                if (!row || !row[2])
                    return
                dismiss()
                row[2]()
            }
            onVisibleChanged: { reset(); if (visible) forceActiveFocus() }
            // The first match is picked as soon as there is a filter, so Enter alone runs it.
            function setFilter(text) { filter = text; picked = text === "" || shown.length === 0 ? -1 : 0; helpList.positionViewAtBeginning() }

            Keys.onEscapePressed: dismiss()
            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Down) {
                    picked = Math.min(shown.length - 1, picked + 1)
                    helpList.positionViewAtIndex(picked, ListView.Contain)
                } else if (event.key === Qt.Key_Up) {
                    picked = Math.max(filter === "" ? -1 : 0, picked - 1)
                    if (picked >= 0)
                        helpList.positionViewAtIndex(picked, ListView.Contain)
                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    run()
                } else if (event.key === Qt.Key_Backspace) {
                    setFilter(filter.slice(0, -1))
                } else if (event.text.length > 0 && event.text.charCodeAt(0) >= 32 && event.text.charCodeAt(0) !== 127
                           && !(event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier))) {
                    setFilter(filter + event.text)
                } else {
                    return
                }
                event.accepted = true
            }
            TapHandler { onTapped: help.dismiss() }

            // The box is as tall as the whole list, so filtering never moves where anything starts.
            Item {
                id: box
                anchors.centerIn: parent
                width: 520
                height: Math.min(help.parent.height - 48, help.rowHeight * (help.all.length + 2))

                // The typed filter sits on the line above the first row, with a blank line between.
                Text {
                    id: typed
                    visible: help.filter !== ""
                    text: help.filter
                    color: theme.accent
                    font.family: backend.fontFamily
                    font.pointSize: win.fs - 0.75
                    height: help.rowHeight
                    verticalAlignment: Text.AlignVCenter
                    Rectangle {
                        id: filterCaret
                        x: typed.contentWidth
                        anchors.verticalCenter: parent.verticalCenter
                        height: parent.contentHeight
                        width: win.caretWidth
                        color: win.caretColor
                        antialiasing: true
                        Timer {
                            interval: 530
                            repeat: true
                            running: typed.visible
                            onTriggered: filterCaret.opacity = filterCaret.opacity > 0 ? 0 : 1
                        }
                        Connections {
                            target: help
                            function onFilterChanged() { filterCaret.opacity = 1 }
                        }
                    }
                }

                ListView {
                    id: helpList
                    anchors.top: parent.top
                    anchors.topMargin: help.rowHeight * 2
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    clip: true
                    interactive: false
                    model: help.shown
                    delegate: Item {
                        required property var modelData
                        required property int index
                        width: helpList.width
                        height: help.rowHeight
                        Rectangle {
                            anchors.fill: parent
                            anchors.leftMargin: -8
                            anchors.rightMargin: -8
                            radius: 4
                            color: Qt.alpha(theme.accent, 0.22)
                            visible: index === help.picked
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            textFormat: Text.StyledText
                            text: help.marked(modelData[0], theme.accent)
                            color: theme.foreground
                            font.family: backend.fontFamily
                            font.pointSize: win.fs - 0.75
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                            textFormat: Text.StyledText
                            text: help.marked(modelData[1], theme.accent)
                            color: theme.muted
                            font.family: backend.fontFamily
                            font.pointSize: win.fs - 0.75
                        }
                    }
                }
            }
        }
    }
}
