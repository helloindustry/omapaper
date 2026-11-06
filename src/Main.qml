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

    // Keys follow TaskPaper with Ctrl in place of Command. Omarchy owns Super, so nothing here uses it,
    // and the single-letter keys of Omarchy's own apps (Q, ?) would type into the document, so they are Ctrl+Q and Ctrl+/.
    Shortcut { sequence: "Ctrl+Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequences: ["Ctrl+K", "Ctrl+/", "F1"]; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Ctrl+Shift+S"; context: Qt.ApplicationShortcut; onActivated: backend.saveAsDialog() }
    Shortcut { sequences: ["Ctrl+=", "Ctrl++"]; context: Qt.ApplicationShortcut; onActivated: backend.changeFontSize(1) }
    Shortcut { sequence: "Ctrl+-"; context: Qt.ApplicationShortcut; onActivated: backend.changeFontSize(-1) }
    Shortcut { sequence: "Ctrl+0"; context: Qt.ApplicationShortcut; onActivated: backend.changeFontSize(0) }
    Shortcut { sequence: "Ctrl+O"; context: Qt.ApplicationShortcut; onActivated: backend.openDialog() }
    Shortcut { sequence: "Ctrl+F"; context: Qt.ApplicationShortcut; onActivated: { search.forceActiveFocus(); search.selectAll() } }
    Shortcut { sequence: "Ctrl+Shift+F"; context: Qt.ApplicationShortcut; onActivated: backend.saveSearch() }
    Shortcut { sequence: "Ctrl+Alt+S"; context: Qt.ApplicationShortcut; onActivated: win.sidebarVisible = !win.sidebarVisible }
    Shortcut { sequences: ["Ctrl+Return", "Ctrl+Enter", "Ctrl+D"]; context: Qt.ApplicationShortcut; onActivated: backend.toggleDone(editor.selectionStart, editor.selectionEnd) }
    Shortcut { sequence: "Ctrl+Alt+Right"; context: Qt.ApplicationShortcut; onActivated: backend.zoomIn(editor.cursorPosition) }
    Shortcut { sequence: "Ctrl+Alt+Left"; context: Qt.ApplicationShortcut; onActivated: backend.zoomOut() }
    Shortcut { sequence: "Ctrl+Alt+Up"; context: Qt.ApplicationShortcut; onActivated: backend.moveLines(editor.selectionStart, editor.selectionEnd, -1) }
    Shortcut { sequence: "Ctrl+Alt+Down"; context: Qt.ApplicationShortcut; onActivated: backend.moveLines(editor.selectionStart, editor.selectionEnd, 1) }
    Shortcut { sequence: "Ctrl+Shift+A"; context: Qt.ApplicationShortcut; onActivated: backend.archive(editor.selectionStart, editor.selectionEnd) }
    Shortcut { sequences: ["Ctrl+.", "Ctrl+Alt+Return"]; context: Qt.ApplicationShortcut; onActivated: backend.toggleFold(editor.cursorPosition) }
    Shortcut { sequence: "Ctrl+Alt+9"; context: Qt.ApplicationShortcut; onActivated: backend.foldAll(true) }
    Shortcut { sequence: "Ctrl+Alt+0"; context: Qt.ApplicationShortcut; onActivated: backend.foldAll(false) }
    Shortcut { sequence: "Ctrl+Alt+Home"; context: Qt.ApplicationShortcut; onActivated: backend.showAll() }
    Shortcut { sequence: "Ctrl+]"; context: Qt.ApplicationShortcut; onActivated: backend.indent(editor.selectionStart, editor.selectionEnd, 1) }
    Shortcut { sequence: "Ctrl+["; context: Qt.ApplicationShortcut; onActivated: backend.indent(editor.selectionStart, editor.selectionEnd, -1) }

    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        RowLayout {
            anchors.fill: parent
            spacing: 0

            // Projects, tags, and saved searches.
            Rectangle {
                visible: win.sidebarVisible
                Layout.preferredWidth: 240
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
                            x: 16 + (row.modelData.level || 0) * 14
                            width: parent.width - x - 16
                            text: row.modelData.label
                            elide: Text.ElideRight
                            font.family: backend.fontFamily
                            font.pointSize: win.fs - (row.header ? 2 : 0.75)
                            font.bold: row.header
                            color: row.header ? theme.muted : theme.foreground
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

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // The search bar; typing filters the document as you go.
                Rectangle {
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
                        Keys.onEscapePressed: { backend.query = ""; editor.forceActiveFocus() }
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
                        tabStopDistance: fontMetrics.advanceWidth(" ") * 4
                        persistentSelection: true
                        placeholderText: qsTr("Start typing. “- ” makes a task, a line ending in “:” makes a project, and “@tag” tags it.")
                        placeholderTextColor: theme.muted
                        Component.onCompleted: backend.attach(textDocument)

                        // Links open on a click and show a pointing hand, as in TaskPaper.
                        property bool overLink: false
                        HoverHandler {
                            onPointChanged: editor.overLink = backend.linkAt(editor.positionAt(point.position.x, point.position.y)) !== ""
                        }
                        HoverHandler { enabled: editor.overLink; cursorShape: Qt.PointingHandCursor }
                        TapHandler {
                            onTapped: eventPoint => {
                                const url = backend.linkAt(editor.positionAt(eventPoint.position.x, eventPoint.position.y))
                                if (url !== "") Qt.openUrlExternally(url)
                            }
                        }

                        FontMetrics { id: fontMetrics; font: editor.font }

                        // A Keys handler accepts the key unless told otherwise, so say so when the default Return should run.
                        Keys.onReturnPressed: event => {
                            event.accepted = editor.selectionStart === editor.selectionEnd && backend.newline(editor.cursorPosition)
                        }
                        Keys.onTabPressed: event => { backend.indent(editor.selectionStart, editor.selectionEnd, 1); event.accepted = true }
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
                        [qsTr("Toggle done"), "Ctrl+Enter / Ctrl+D"],
                        [qsTr("Fold / unfold"), "Ctrl+."],
                        [qsTr("Fold all / unfold all"), "Ctrl+Alt+9 / 0"],
                        [qsTr("Archive selection (or all done)"), "Ctrl+Shift+A"],
                        [qsTr("Indent / outdent"), "Tab / Shift+Tab"],
                        [qsTr("Move up / down"), "Ctrl+Alt+Up / Down"],
                        [qsTr("Zoom in / out"), "Ctrl+Alt+Right / Left"],
                        [qsTr("Home (whole document)"), "Ctrl+Alt+Home"],
                        [qsTr("Search"), "Ctrl+F"],
                        [qsTr("Save search"), "Ctrl+Shift+F"],
                        [qsTr("Toggle sidebar"), "Ctrl+Alt+S"],
                        [qsTr("Bigger / smaller / reset text"), "Ctrl+= / Ctrl+- / Ctrl+0"],
                        [qsTr("Open"), "Ctrl+O"],
                        [qsTr("Save As"), "Ctrl+Shift+S"],
                        [qsTr("Undo / redo"), "Ctrl+Z / Ctrl+Shift+Z"],
                        [qsTr("This list"), "Ctrl+K"],
                        [qsTr("Quit"), "Ctrl+Q"]
                    ]
                    delegate: Item {
                        required property var modelData
                        required property int index
                        Layout.columnSpan: 2
                        Layout.preferredWidth: 520
                        Layout.preferredHeight: Math.round(win.fs * 2.2)
                        Text { text: modelData[0]; color: theme.foreground; font.family: backend.fontFamily; font.pointSize: win.fs - 0.75 }
                        Text { anchors.right: parent.right; text: modelData[1]; color: theme.muted; font.family: backend.fontFamily; font.pointSize: win.fs - 0.75 }
                    }
                }
            }
        }
    }
}
