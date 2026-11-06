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
    color: theme.background

    Material.theme: theme.dark ? Material.Dark : Material.Light
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    readonly property real fs: backend.fontSize // the size everything else scales from
    property bool sidebarVisible: true
    property bool helpVisible: false
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
    // TaskPaper's keys, with Ctrl for Command and Alt for Option. Where the original's key is not documented
    // (focus, move, sidebar) the closest Ctrl+Alt form is used; Omarchy's own bindings all use Super.
    Shortcut { sequence: "Ctrl+Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "Ctrl+/"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Ctrl+O"; context: Qt.ApplicationShortcut; onActivated: backend.openDialog() }
    Shortcut { sequence: "Ctrl+P"; context: Qt.ApplicationShortcut; onActivated: backend.print() }
    Shortcut { sequence: "Ctrl+S"; context: Qt.ApplicationShortcut; onActivated: backend.save() }
    Shortcut { sequence: "Ctrl+Shift+S"; context: Qt.ApplicationShortcut; onActivated: backend.saveAsDialog() }
    Shortcut { sequences: ["Ctrl+=", "Ctrl++"]; context: Qt.ApplicationShortcut; onActivated: backend.changeFontSize(1) }
    Shortcut { sequence: "Ctrl+-"; context: Qt.ApplicationShortcut; onActivated: backend.changeFontSize(-1) }
    Shortcut { sequence: "Ctrl+F"; context: Qt.ApplicationShortcut; onActivated: win.findVisible ? win.closeFind() : win.openFind() }
    Shortcut { sequence: "Ctrl+G"; context: Qt.ApplicationShortcut; onActivated: win.findAgain(false) }
    Shortcut { sequence: "Ctrl+Shift+G"; context: Qt.ApplicationShortcut; onActivated: win.findAgain(true) }
    Shortcut { sequence: "Ctrl+Shift+F"; context: Qt.ApplicationShortcut; onActivated: { win.searchOpen = true; search.forceActiveFocus(); search.selectAll() } }
    Shortcut { sequence: "Ctrl+Alt+F"; context: Qt.ApplicationShortcut; onActivated: backend.saveSearch() }
    Shortcut { sequences: ["Ctrl+Alt+S", "Ctrl+B"]; context: Qt.ApplicationShortcut; onActivated: win.sidebarVisible = !win.sidebarVisible }
    Shortcut { sequences: ["Ctrl+D", "Ctrl+Return", "Ctrl+Enter"]; context: Qt.ApplicationShortcut; onActivated: backend.toggleDone(editor.selectionStart, editor.selectionEnd) }
    Shortcut { sequence: "Ctrl+Shift+K"; context: Qt.ApplicationShortcut; onActivated: backend.deleteLines(editor.selectionStart, editor.selectionEnd) }
    Shortcut { sequence: "Ctrl+Shift+A"; context: Qt.ApplicationShortcut; onActivated: backend.archive(editor.selectionStart, editor.selectionEnd) }
    Shortcut { sequence: "Ctrl+9"; context: Qt.ApplicationShortcut; onActivated: backend.foldItems(editor.selectionStart, editor.selectionEnd, true, false) }
    Shortcut { sequence: "Ctrl+0"; context: Qt.ApplicationShortcut; onActivated: backend.changeFontSize(0) }
    Shortcut { sequence: "Ctrl+Alt+9"; context: Qt.ApplicationShortcut; onActivated: backend.foldItems(editor.selectionStart, editor.selectionEnd, true, true) }
    Shortcut { sequence: "Ctrl+Alt+0"; context: Qt.ApplicationShortcut; onActivated: backend.foldItems(editor.selectionStart, editor.selectionEnd, false, true) }
    Shortcut { sequences: ["Ctrl+Shift+9", "Ctrl+("]; context: Qt.ApplicationShortcut; onActivated: backend.foldAll(true) }
    Shortcut { sequences: ["Ctrl+Shift+0", "Ctrl+)"]; context: Qt.ApplicationShortcut; onActivated: backend.foldAll(false) }
    Shortcut { sequence: "Ctrl+."; context: Qt.ApplicationShortcut; onActivated: backend.toggleFold(editor.cursorPosition) }
    Shortcut { sequences: ["Ctrl+]", "Ctrl+Alt+Right"]; context: Qt.ApplicationShortcut; onActivated: backend.indent(editor.selectionStart, editor.selectionEnd, 1) }
    Shortcut { sequences: ["Ctrl+[", "Ctrl+Alt+Left"]; context: Qt.ApplicationShortcut; onActivated: backend.indent(editor.selectionStart, editor.selectionEnd, -1) }
    Shortcut { sequence: "Ctrl+Alt+Up"; context: Qt.ApplicationShortcut; onActivated: backend.moveLines(editor.selectionStart, editor.selectionEnd, -1) }
    Shortcut { sequence: "Ctrl+Alt+Down"; context: Qt.ApplicationShortcut; onActivated: backend.moveLines(editor.selectionStart, editor.selectionEnd, 1) }
    Shortcut { sequence: "Ctrl+Alt+]"; context: Qt.ApplicationShortcut; onActivated: backend.zoomIn(editor.cursorPosition) }
    Shortcut { sequence: "Ctrl+Alt+["; context: Qt.ApplicationShortcut; onActivated: backend.zoomOut() }
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
                    anchors.topMargin: 8
                    clip: true
                    model: backend.sidebar
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}

                    delegate: Item {
                        id: row
                        required property var modelData
                        readonly property bool header: modelData.kind === "header"
                        readonly property bool current: modelData.kind === "project" ? backend.zoomBlock === modelData.block && backend.query === ""
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
                            color: row.header ? theme.muted : theme.foreground
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
                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            enabled: !row.header
                            onTapped: {
                                if (row.modelData.kind === "all") backend.showAll()
                                else if (row.modelData.kind === "project") backend.showProject(row.modelData.block)
                                else backend.search(row.modelData.query)
                            }
                        }
                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            enabled: row.modelData.kind === "search"
                            onTapped: menu.popup()
                        }
                        Menu {
                            id: menu
                            MenuItem { text: qsTr("Delete Search"); onTriggered: backend.deleteBlock(row.modelData.block) }
                        }
                    }
                }
            }

            // Drag to resize the sidebar, drag it nearly shut to collapse it, double-click to toggle it.
            Item {
                Layout.preferredWidth: 6
                Layout.fillHeight: true
                HoverHandler { cursorShape: Qt.SplitHCursor } // the resize cursor tells you the edge can be dragged
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.SplitHCursor
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

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // The search bar; typing filters the document as you go.
                Rectangle {
                    visible: win.searchOpen || backend.query !== ""
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.round(win.fs * 3.6)
                    color: theme.sidebar
                    TextInput {
                        id: search
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
                        onTextEdited: backend.query = text
                        onActiveFocusChanged: if (!activeFocus && backend.query === "") win.searchOpen = false
                        Keys.onEscapePressed: { backend.query = ""; win.searchOpen = false; editor.forceActiveFocus() }
                        Keys.onReturnPressed: editor.forceActiveFocus()
                        Text {
                            visible: !search.text && !search.activeFocus
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Search")
                            font: search.font
                            color: theme.muted
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
                        focus: true
                        background: null
                        wrapMode: TextEdit.Wrap
                        font.family: backend.fontFamily
                        font.pointSize: win.fs
                        color: theme.foreground
                        selectionColor: theme.accent
                        selectedTextColor: theme.accentForeground
                        leftPadding: 32
                        rightPadding: 24
                        topPadding: 16
                        bottomPadding: 16
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
                                // The caret that was there stays as an extra; the click becomes the main caret.
                                const base = editor.ctrlBase === clicked ? editor.previousPos : editor.ctrlBase
                                backend.addCursor(base, clicked)
                                editor.cursorPosition = clicked
                            }
                        }
                        Keys.onPressed: event => {
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
                                width: 2
                                height: line.height
                                color: theme.accent
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

                        // Links open on a click and show a pointing hand, as in TaskPaper.
                        property bool overLink: false
                        HoverHandler {
                            onPointChanged: editor.overLink = backend.linkAt(editor.positionAt(point.position.x, point.position.y)) !== ""
                        }
                        HoverHandler { enabled: editor.overLink; cursorShape: Qt.PointingHandCursor }
                        TapHandler {
                            acceptedModifiers: Qt.NoModifier
                            onTapped: eventPoint => {
                                backend.clearCursors()
                                const url = backend.linkAt(editor.positionAt(eventPoint.position.x, eventPoint.position.y))
                                if (url !== "") Qt.openUrlExternally(url)
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
                    visible: backend.notice !== "" && backend.error === ""
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.round(win.fs * 2.8)
                    color: theme.sidebar
                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        verticalAlignment: Text.AlignVCenter
                        text: backend.notice
                        font.family: backend.fontFamily
                        font.pointSize: win.fs - 1.5
                        color: theme.muted
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

        // Every key, on one screen.
        Rectangle {
            visible: win.helpVisible
            anchors.fill: parent
            color: Qt.alpha(theme.background, 0.94)
            focus: visible
            Keys.onEscapePressed: win.helpVisible = false
            TapHandler { onTapped: win.helpVisible = false }

            GridLayout {
                anchors.centerIn: parent
                columns: 2
                columnSpacing: 32
                rowSpacing: 6
                Repeater {
                    model: [
                        [qsTr("Toggle done"), "Ctrl+D"],
                        [qsTr("Archive done items / selection"), "Ctrl+Shift+A"],
                        [qsTr("Delete line"), "Ctrl+Shift+K"],
                        [qsTr("Collapse item (Ctrl+. toggles)"), "Ctrl+9"],
                        [qsTr("Collapse / expand completely"), "Ctrl+Alt+9 / 0"],
                        [qsTr("Collapse / expand all"), "Ctrl+Shift+9 / 0"],
                        [qsTr("Indent / outdent"), "Tab / Shift+Tab"],
                        [qsTr("Move up / down"), "Ctrl+Alt+Up / Down"],
                        [qsTr("Focus in / out"), "Ctrl+Alt+] / ["],
                        [qsTr("Home (whole document)"), "Ctrl+H"],
                        [qsTr("Find and replace"), "Ctrl+F"],
                        [qsTr("Find next / previous"), "Ctrl+G / Ctrl+Shift+G"],
                        [qsTr("Filter (search bar)"), "Ctrl+Shift+F"],
                        [qsTr("Save search"), "Ctrl+Alt+F"],
                        [qsTr("Toggle sidebar"), "Ctrl+Alt+S"],
                        [qsTr("Bigger / smaller / reset text"), "Ctrl+= / Ctrl+- / Ctrl+0"],
                        [qsTr("Open / Save / Save As"), "Ctrl+O / S / Shift+S"],
                        [qsTr("Print"), "Ctrl+P"],
                        [qsTr("Undo / redo"), "Ctrl+Z / Ctrl+Shift+Z"],
                        [qsTr("This list"), "Ctrl+/"],
                        [qsTr("Quit"), "Ctrl+Q"]
                    ]
                    delegate: Item {
                        required property var modelData
                        required property int index
                        Layout.columnSpan: 2
                        Layout.preferredWidth: 520
                        Layout.preferredHeight: Math.round(win.fs * 2.0)
                        Text { text: modelData[0]; color: theme.foreground; font.family: backend.fontFamily; font.pointSize: win.fs - 0.75 }
                        Text { anchors.right: parent.right; text: modelData[1]; color: theme.muted; font.family: backend.fontFamily; font.pointSize: win.fs - 0.75 }
                    }
                }
            }
        }
    }
}
