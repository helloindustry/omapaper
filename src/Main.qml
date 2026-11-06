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

    property bool sidebarVisible: true
    property bool helpVisible: false

    // Keys follow TaskPaper with Ctrl in place of Command. Omarchy owns Super, so nothing here uses it,
    // and the single-letter keys of Omarchy's own apps (Q, ?) would type into the document, so they are Ctrl+Q and Ctrl+/.
    Shortcut { sequence: "Ctrl+Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequences: ["Ctrl+/", "F1"]; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Ctrl+O"; context: Qt.ApplicationShortcut; onActivated: backend.openDialog() }
    Shortcut { sequence: "Ctrl+F"; context: Qt.ApplicationShortcut; onActivated: { search.forceActiveFocus(); search.selectAll() } }
    Shortcut { sequence: "Ctrl+Shift+F"; context: Qt.ApplicationShortcut; onActivated: backend.saveSearch() }
    Shortcut { sequence: "Ctrl+Alt+S"; context: Qt.ApplicationShortcut; onActivated: win.sidebarVisible = !win.sidebarVisible }
    Shortcut { sequence: "Ctrl+D"; context: Qt.ApplicationShortcut; onActivated: backend.toggleDone(editor.selectionStart, editor.selectionEnd) }
    Shortcut { sequence: "Ctrl+Alt+Right"; context: Qt.ApplicationShortcut; onActivated: backend.zoomIn(editor.cursorPosition) }
    Shortcut { sequence: "Ctrl+Alt+Left"; context: Qt.ApplicationShortcut; onActivated: backend.zoomOut() }
    Shortcut { sequence: "Ctrl+Alt+Up"; context: Qt.ApplicationShortcut; onActivated: backend.moveLines(editor.selectionStart, editor.selectionEnd, -1) }
    Shortcut { sequence: "Ctrl+Alt+Down"; context: Qt.ApplicationShortcut; onActivated: backend.moveLines(editor.selectionStart, editor.selectionEnd, 1) }
    Shortcut { sequence: "Ctrl+Return"; context: Qt.ApplicationShortcut; onActivated: backend.toggleFold(editor.cursorPosition) }
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
                        height: header ? 36 : 26

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
                            font.pixelSize: row.header ? 12 : 14
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
                    Layout.preferredHeight: 36
                    color: theme.sidebar
                    TextInput {
                        id: search
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        verticalAlignment: TextInput.AlignVCenter
                        font.family: backend.fontFamily
                        font.pixelSize: 14
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
                    Layout.preferredHeight: 28
                    color: theme.sidebar
                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideLeft
                        text: backend.zoomPath
                        font.family: backend.fontFamily
                        font.pixelSize: 13
                        color: theme.muted
                    }
                    TapHandler { onTapped: backend.zoomOut() }
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    TextArea {
                        id: editor
                        focus: true
                        background: null
                        wrapMode: TextEdit.Wrap
                        font.family: backend.fontFamily
                        font.pixelSize: 15
                        color: theme.foreground
                        selectionColor: theme.accent
                        selectedTextColor: theme.accentForeground
                        leftPadding: 24
                        rightPadding: 24
                        topPadding: 16
                        bottomPadding: 16
                        tabStopDistance: fontMetrics.advanceWidth(" ") * 4
                        persistentSelection: true
                        placeholderText: qsTr("Start typing. “- ” makes a task, a line ending in “:” makes a project, and “@tag” tags it.")
                        placeholderTextColor: theme.muted
                        Component.onCompleted: backend.attach(textDocument)

                        FontMetrics { id: fontMetrics; font: editor.font }

                        Keys.onReturnPressed: event => {
                            if (editor.selectionStart === editor.selectionEnd && backend.newline(editor.cursorPosition))
                                event.accepted = true
                        }
                        Keys.onTabPressed: event => { backend.indent(editor.selectionStart, editor.selectionEnd, 1); event.accepted = true }
                        Keys.onBacktabPressed: event => { backend.indent(editor.selectionStart, editor.selectionEnd, -1); event.accepted = true }

                        // Disclosure triangles in the left margin, one per item that has children.
                        Repeater {
                            model: backend.folds
                            delegate: Text {
                                required property var modelData
                                readonly property rect line: { editor.contentHeight; editor.width; return editor.positionToRectangle(modelData.pos) }
                                x: 6
                                y: line.y
                                height: line.height
                                verticalAlignment: Text.AlignVCenter
                                text: modelData.folded ? "▸" : "▾"
                                font.pixelSize: 15
                                color: modelData.folded ? theme.accent : theme.muted
                                TapHandler { onTapped: backend.toggleFoldBlock(modelData.block) }
                            }
                        }

                        Connections {
                            target: backend
                            function onCursorRequested(position) { editor.cursorPosition = position }
                            function onMoved(start, end) { editor.select(start, end) }
                        }
                    }
                }

                Rectangle {
                    visible: backend.error !== ""
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    color: theme.sidebar
                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        verticalAlignment: Text.AlignVCenter
                        text: backend.error
                        font.family: backend.fontFamily
                        font.pixelSize: 13
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
                        [qsTr("Toggle done"), "Ctrl+D"],
                        [qsTr("Indent / outdent"), "Tab / Shift+Tab"],
                        [qsTr("Move up / down"), "Ctrl+Alt+Up / Down"],
                        [qsTr("Zoom in / out"), "Ctrl+Alt+Right / Left"],
                        [qsTr("Show all"), "Ctrl+Alt+Home"],
                        [qsTr("Fold / unfold"), "Ctrl+Return"],
                        [qsTr("Fold all / unfold all"), "Ctrl+Alt+9 / 0"],
                        [qsTr("Search"), "Ctrl+F"],
                        [qsTr("Save search"), "Ctrl+Shift+F"],
                        [qsTr("Toggle sidebar"), "Ctrl+Alt+S"],
                        [qsTr("Open"), "Ctrl+O"],
                        [qsTr("Undo / redo"), "Ctrl+Z / Ctrl+Shift+Z"],
                        [qsTr("This help"), "Ctrl+/ or F1"],
                        [qsTr("Quit"), "Ctrl+Q"]
                    ]
                    delegate: Item {
                        required property var modelData
                        required property int index
                        Layout.columnSpan: 2
                        Layout.preferredWidth: 460
                        Layout.preferredHeight: 22
                        Text { text: modelData[0]; color: theme.foreground; font.family: backend.fontFamily; font.pixelSize: 14 }
                        Text { anchors.right: parent.right; text: modelData[1]; color: theme.muted; font.family: backend.fontFamily; font.pixelSize: 14 }
                    }
                }
            }
        }
    }
}
