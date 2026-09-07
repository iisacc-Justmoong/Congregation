pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import LVRS 1.0 as LV

Rectangle {
    id: root

    property var menuActions: ({})
    property var shortcutTexts: ({})
    property string applicationVersion: ""
    property bool updateSupported: false
    property color barColor: LV.Theme.window

    readonly property bool topLevelMenuOpened: fileContextMenu.opened || editContextMenu.opened || windowContextMenu.opened || helpContextMenu.opened
    readonly property bool anyMenuOpened: topLevelMenuOpened || toolsContextMenu.opened || shapeKindContextMenu.opened || keyboardShortcutsContextMenu.opened || shortcutFileContextMenu.opened || shortcutEditContextMenu.opened || shortcutToolsContextMenu.opened || shortcutShapeKindContextMenu.opened || shortcutWindowContextMenu.opened

    implicitHeight: LV.Theme.controlHeightSm
    color: barColor

    onVisibleChanged: {
        if (!visible)
            closeAllMenus();
    }

    function actionFor(actionId) {
        if (!root.menuActions || root.menuActions[actionId] === undefined)
            return null;
        return root.menuActions[actionId];
    }

    function shortcutFor(shortcutId) {
        if (!shortcutId || !root.shortcutTexts || root.shortcutTexts[shortcutId] === undefined)
            return "";
        return String(root.shortcutTexts[shortcutId]);
    }

    function actionEntry(actionId, shortcutId, ownerMenuId) {
        const action = root.actionFor(actionId);
        const shortcut = root.shortcutFor(shortcutId);
        return {
            label: action ? action.text : "",
            shortcut: shortcut,
            keyVisible: shortcut.length > 0,
            showIconSlot: false,
            showChevron: false,
            hasChildItems: false,
            enabled: action ? action.enabled : false,
            selected: action ? action.checked === true : false,
            ownerMenuId: ownerMenuId || "",
            onTriggered: function () {
                root.triggerAction(actionId);
            }
        };
    }

    function referenceEntry(label, shortcutId) {
        const shortcut = root.shortcutFor(shortcutId);
        return {
            label: label,
            shortcut: shortcut,
            keyVisible: shortcut.length > 0,
            showIconSlot: false,
            showChevron: false,
            hasChildItems: false,
            enabled: false
        };
    }

    function submenuEntry(label, submenuId, ownerMenuId) {
        return {
            label: label,
            showIconSlot: false,
            showChevron: true,
            hasChildItems: true,
            closeOnTrigger: false,
            submenuId: submenuId,
            ownerMenuId: ownerMenuId
        };
    }

    function dividerEntry() {
        return {
            type: "divider"
        };
    }

    function helpMenuEntries() {
        const entries = [];
        if (root.updateSupported) {
            entries.push(root.actionEntry("checkForUpdates", "", "help"));
            entries.push(root.dividerEntry());
        }
        entries.push(root.submenuEntry(qsTr("Keyboard Shortcuts"), "keyboardShortcuts", "help"));
        entries.push({
            label: qsTr("Congregation %1").arg(root.applicationVersion),
            showIconSlot: false,
            showChevron: false,
            hasChildItems: false,
            enabled: false,
            ownerMenuId: "help"
        });
        return entries;
    }

    function triggerAction(actionId) {
        const action = root.actionFor(actionId);
        if (!action || action.enabled === false)
            return;
        root.closeAllMenus();
        action.trigger();
    }

    function allMenus() {
        return [fileContextMenu, editContextMenu, windowContextMenu, helpContextMenu, toolsContextMenu, shapeKindContextMenu, keyboardShortcutsContextMenu, shortcutFileContextMenu, shortcutEditContextMenu, shortcutToolsContextMenu, shortcutShapeKindContextMenu, shortcutWindowContextMenu];
    }

    function topLevelMenus() {
        return [fileContextMenu, editContextMenu, windowContextMenu, helpContextMenu];
    }

    function closeMenu(menu) {
        if (menu && menu.opened)
            menu.close();
    }

    function closeMenuList(menus, exceptMenu) {
        for (let index = 0; index < menus.length; ++index) {
            const menu = menus[index];
            if (menu !== exceptMenu)
                root.closeMenu(menu);
        }
    }

    function closeEditSubmenus(exceptMenu) {
        root.closeMenuList([toolsContextMenu, shapeKindContextMenu], exceptMenu);
    }

    function closeShortcutDetailMenus(exceptMenu) {
        root.closeMenuList([shortcutFileContextMenu, shortcutEditContextMenu, shortcutToolsContextMenu, shortcutShapeKindContextMenu, shortcutWindowContextMenu], exceptMenu);
    }

    function closeHelpSubmenus(exceptMenu) {
        root.closeShortcutDetailMenus(null);
        root.closeMenuList([keyboardShortcutsContextMenu], exceptMenu);
    }

    function closeSubmenusOwnedBy(ownerMenuId) {
        switch (ownerMenuId) {
        case "edit":
            root.closeEditSubmenus(null);
            break;
        case "help":
            root.closeHelpSubmenus(null);
            break;
        case "keyboardShortcuts":
            root.closeShortcutDetailMenus(null);
            break;
        default:
            break;
        }
    }

    function closeAllSubmenus() {
        root.closeEditSubmenus(null);
        root.closeHelpSubmenus(null);
    }

    function closeAllMenus(exceptMenu) {
        root.closeAllSubmenus();
        root.closeMenuList(root.topLevelMenus(), exceptMenu || null);
    }

    function openTopLevelMenu(menu, anchorItem, toggleCurrent) {
        if (!menu || !anchorItem)
            return;
        const alreadyOpened = menu.opened;
        root.closeAllMenus(menu);
        if (alreadyOpened && toggleCurrent) {
            menu.close();
            return;
        }
        if (!menu.opened)
            menu.openFor(anchorItem, 0, anchorItem.height);
    }

    function submenuForId(submenuId) {
        switch (submenuId) {
        case "tools":
            return toolsContextMenu;
        case "shapeKind":
            return shapeKindContextMenu;
        case "keyboardShortcuts":
            return keyboardShortcutsContextMenu;
        case "shortcutFile":
            return shortcutFileContextMenu;
        case "shortcutEdit":
            return shortcutEditContextMenu;
        case "shortcutTools":
            return shortcutToolsContextMenu;
        case "shortcutShapeKind":
            return shortcutShapeKindContextMenu;
        case "shortcutWindow":
            return shortcutWindowContextMenu;
        default:
            return null;
        }
    }

    function openSubmenu(submenuId, anchorItem) {
        const menu = root.submenuForId(submenuId);
        if (!menu || !anchorItem)
            return;

        switch (submenuId) {
        case "tools":
        case "shapeKind":
            root.closeEditSubmenus(menu);
            break;
        case "keyboardShortcuts":
            root.closeShortcutDetailMenus(null);
            break;
        case "shortcutFile":
        case "shortcutEdit":
        case "shortcutTools":
        case "shortcutShapeKind":
        case "shortcutWindow":
            root.closeShortcutDetailMenus(menu);
            break;
        default:
            break;
        }

        if (!menu.opened)
            menu.openFor(anchorItem, anchorItem.width + LV.Theme.gap2, 0);
    }

    function popupContainsGlobalPoint(popup, globalX, globalY) {
        if (!popup || !popup.opened || !popup.parent || !popup.parent.mapFromGlobal)
            return false;
        const localPoint = popup.parent.mapFromGlobal(globalX, globalY);
        return localPoint.x >= popup.x && localPoint.x <= popup.x + popup.width && localPoint.y >= popup.y && localPoint.y <= popup.y + popup.height;
    }

    function containsGlobalPoint(globalX, globalY) {
        const barPoint = root.mapFromGlobal(globalX, globalY);
        if (barPoint.x >= 0 && barPoint.x <= root.width && barPoint.y >= 0 && barPoint.y <= root.height) {
            return true;
        }

        const menus = root.allMenus();
        for (let index = 0; index < menus.length; ++index) {
            if (root.popupContainsGlobalPoint(menus[index], globalX, globalY))
                return true;
        }
        return false;
    }

    function handleGlobalMenuPress(eventData) {
        if (!root.anyMenuOpened || !eventData)
            return;
        const globalX = eventData.globalX !== undefined ? Number(eventData.globalX) : Number(eventData.x);
        const globalY = eventData.globalY !== undefined ? Number(eventData.globalY) : Number(eventData.y);
        if (!Number.isFinite(globalX) || !Number.isFinite(globalY))
            return;
        if (!root.containsGlobalPoint(globalX, globalY))
            root.closeAllMenus();
    }

    component ApplicationContextMenu: LV.ContextMenu {
        modal: false
        itemWidth: LV.Theme.scaleMetric(220)
        showIconSlot: false
        dismissOnGlobalPress: false
        dismissOnGlobalContextRequest: false
        closePolicy: Controls.Popup.CloseOnEscape
        itemDelegate: applicationMenuItemDelegate
    }

    component MenuBarButton: Controls.Button {
        id: menuButton

        property var popupMenu: null

        implicitHeight: root.implicitHeight
        topPadding: LV.Theme.gap2
        bottomPadding: LV.Theme.gap2
        leftPadding: LV.Theme.gap8
        rightPadding: LV.Theme.gap8
        spacing: LV.Theme.gapNone
        hoverEnabled: true
        flat: true

        contentItem: LV.Label {
            text: menuButton.text
            style: body
            color: menuButton.enabled ? LV.Theme.bodyColor : LV.Theme.disabledColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            implicitHeight: root.implicitHeight
            color: menuButton.down || menuButton.hovered || (menuButton.popupMenu && menuButton.popupMenu.opened) ? LV.Theme.surfaceAlt : "transparent"
        }

        Accessible.name: text

        onClicked: root.openTopLevelMenu(popupMenu, menuButton, true)
        onHoveredChanged: {
            if (hovered && root.topLevelMenuOpened && popupMenu && !popupMenu.opened)
                root.openTopLevelMenu(popupMenu, menuButton, false);
        }
    }

    Component {
        id: applicationMenuItemDelegate

        LV.MenuItem {
            id: menuItem

            property var modelData: ({})
            readonly property var entry: modelData.entry

            width: parent ? parent.width : implicitWidth
            state: modelData.state === undefined ? defaultState : modelData.state
            label: modelData.label || ""
            key: modelData.shortcut || ""
            keyVisible: modelData.keyVisible === true
            showIconSlot: modelData.showIconSlot !== false
            iconName: modelData.iconName || ""
            iconSource: modelData.iconSource || ""
            showChevron: modelData.showChevron === true
            hasChildItems: modelData.hasChildItems === true
            expanded: false
            selectionDirection: "right"
            enabled: modelData.enabled !== false
            Accessible.name: label

            onClicked: {
                if (menuItem.entry && menuItem.entry.submenuId) {
                    modelData.trigger();
                    root.openSubmenu(menuItem.entry.submenuId, menuItem);
                    return;
                }
                modelData.trigger();
            }

            onHoveredChanged: {
                if (!hovered || !menuItem.entry)
                    return;
                if (menuItem.entry.submenuId)
                    root.openSubmenu(menuItem.entry.submenuId, menuItem);
                else
                    root.closeSubmenusOwnedBy(menuItem.entry.ownerMenuId || "");
            }
        }
    }

    Row {
        id: menuButtonRow

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        spacing: LV.Theme.gapNone

        MenuBarButton {
            objectName: "windowsFileMenuButton"
            height: menuButtonRow.height
            text: qsTr("File")
            popupMenu: fileContextMenu
        }

        MenuBarButton {
            objectName: "windowsEditMenuButton"
            height: menuButtonRow.height
            text: qsTr("Edit")
            popupMenu: editContextMenu
        }

        MenuBarButton {
            objectName: "windowsWindowMenuButton"
            height: menuButtonRow.height
            text: qsTr("Window")
            popupMenu: windowContextMenu
        }

        MenuBarButton {
            objectName: "windowsHelpMenuButton"
            height: menuButtonRow.height
            text: qsTr("Help")
            popupMenu: helpContextMenu
        }
    }

    LV.EventListener {
        enabled: root.anyMenuOpened
        trigger: "globalPressed"
        acceptedButtons: Qt.AllButtons
        includeUiHit: true
        action: function (eventData) {
            root.handleGlobalMenuPress(eventData);
        }
    }

    LV.EventListener {
        enabled: root.anyMenuOpened
        trigger: "globalContextRequested"
        acceptedButtons: Qt.AllButtons
        includeUiHit: true
        action: function (eventData) {
            root.handleGlobalMenuPress(eventData);
        }
    }

    ApplicationContextMenu {
        id: fileContextMenu
        objectName: "windowsFileContextMenu"
        items: [root.actionEntry("newCanvas", "newCanvas", "file"), root.actionEntry("openImage", "openImage", "file"), root.actionEntry("saveImageAs", "saveImageAs", "file"), root.actionEntry("clearCanvas", "clearCanvas", "file"), root.dividerEntry(), root.actionEntry("quit", "quit", "file")]
    }

    ApplicationContextMenu {
        id: editContextMenu
        objectName: "windowsEditContextMenu"
        itemWidth: LV.Theme.scaleMetric(280)
        items: [root.actionEntry("preferences", "preferences", "edit"), root.dividerEntry(), root.actionEntry("undo", "undo", "edit"), root.actionEntry("redo", "redo", "edit"), root.actionEntry("pasteImage", "pasteImage", "edit"), root.dividerEntry(), root.actionEntry("addLayer", "addLayer", "edit"), root.actionEntry("deleteCurrentLayer", "deleteCurrentLayer", "edit"), root.dividerEntry(), root.submenuEntry(qsTr("Tools"), "tools", "edit"), root.submenuEntry(qsTr("Shape Kind"), "shapeKind", "edit"), root.dividerEntry(), root.actionEntry("decreaseBrushSize", "decreaseBrushSize", "edit"), root.actionEntry("increaseBrushSize", "increaseBrushSize", "edit")]
        onClosed: root.closeEditSubmenus(null)
    }

    ApplicationContextMenu {
        id: windowContextMenu
        objectName: "windowsWindowContextMenu"
        itemWidth: LV.Theme.scaleMetric(240)
        items: [root.actionEntry("fitCanvasToWindow", "fitCanvasToWindow", "window"), root.actionEntry("resetCanvasView", "resetCanvasView", "window"), root.dividerEntry(), root.actionEntry("minimizeWindow", "minimizeWindow", "window"), root.actionEntry("toggleFullScreen", "toggleFullScreen", "window")]
    }

    ApplicationContextMenu {
        id: helpContextMenu
        objectName: "windowsHelpContextMenu"
        items: root.helpMenuEntries()
        onClosed: root.closeHelpSubmenus(null)
    }

    ApplicationContextMenu {
        id: toolsContextMenu
        objectName: "windowsToolsContextMenu"
        itemWidth: LV.Theme.scaleMetric(200)
        items: [root.actionEntry("brushTool", "brushTool", "tools"), root.actionEntry("eraserTool", "eraserTool", "tools"), root.actionEntry("handPanTool", "handPanTool", "tools"), root.actionEntry("moveTool", "moveTool", "tools"), root.actionEntry("zoomTool", "zoomTool", "tools"), root.actionEntry("shapeTool", "shapeTool", "tools"), root.actionEntry("fillTool", "fillTool", "tools"), root.actionEntry("textTool", "textTool", "tools")]
    }

    ApplicationContextMenu {
        id: shapeKindContextMenu
        objectName: "windowsShapeKindContextMenu"
        itemWidth: LV.Theme.scaleMetric(240)
        items: [root.actionEntry("rectangleShape", "rectangleShape", "shapeKind"), root.actionEntry("ellipseShape", "ellipseShape", "shapeKind"), root.actionEntry("triangleShape", "triangleShape", "shapeKind"), root.actionEntry("diamondShape", "diamondShape", "shapeKind"), root.actionEntry("starShape", "starShape", "shapeKind"), root.actionEntry("rectangleBubbleShape", "rectangleBubbleShape", "shapeKind"), root.actionEntry("ellipseBubbleShape", "ellipseBubbleShape", "shapeKind")]
    }

    ApplicationContextMenu {
        id: keyboardShortcutsContextMenu
        objectName: "windowsKeyboardShortcutsContextMenu"
        items: [root.referenceEntry(qsTr("Preferences"), "preferences"), root.dividerEntry(), root.submenuEntry(qsTr("File"), "shortcutFile", "keyboardShortcuts"), root.submenuEntry(qsTr("Edit"), "shortcutEdit", "keyboardShortcuts"), root.submenuEntry(qsTr("Tools"), "shortcutTools", "keyboardShortcuts"), root.submenuEntry(qsTr("Shape Kind"), "shortcutShapeKind", "keyboardShortcuts"), root.submenuEntry(qsTr("Window"), "shortcutWindow", "keyboardShortcuts")]
        onClosed: root.closeShortcutDetailMenus(null)
    }

    ApplicationContextMenu {
        id: shortcutFileContextMenu
        objectName: "windowsShortcutFileContextMenu"
        items: [root.referenceEntry(qsTr("New Canvas"), "newCanvas"), root.referenceEntry(qsTr("Open Image"), "openImage"), root.referenceEntry(qsTr("Save Image As"), "saveImageAs"), root.referenceEntry(qsTr("Clear Canvas"), "clearCanvas"), root.referenceEntry(qsTr("Quit Congregation"), "quit")]
    }

    ApplicationContextMenu {
        id: shortcutEditContextMenu
        objectName: "windowsShortcutEditContextMenu"
        itemWidth: LV.Theme.scaleMetric(280)
        items: [root.referenceEntry(qsTr("Undo"), "undo"), root.referenceEntry(qsTr("Redo"), "redo"), root.referenceEntry(qsTr("Paste Image"), "pasteImage"), root.referenceEntry(qsTr("Add Layer"), "addLayer"), root.referenceEntry(qsTr("Delete Current Layer"), "deleteCurrentLayer"), root.referenceEntry(qsTr("Decrease Brush Size"), "decreaseBrushSize"), root.referenceEntry(qsTr("Increase Brush Size"), "increaseBrushSize")]
    }

    ApplicationContextMenu {
        id: shortcutToolsContextMenu
        objectName: "windowsShortcutToolsContextMenu"
        itemWidth: LV.Theme.scaleMetric(200)
        items: [root.referenceEntry(qsTr("Brush"), "brushTool"), root.referenceEntry(qsTr("Eraser"), "eraserTool"), root.referenceEntry(qsTr("Hand Pan"), "handPanTool"), root.referenceEntry(qsTr("Move"), "moveTool"), root.referenceEntry(qsTr("Zoom"), "zoomTool"), root.referenceEntry(qsTr("Shape"), "shapeTool"), root.referenceEntry(qsTr("Fill"), "fillTool"), root.referenceEntry(qsTr("Text"), "textTool")]
    }

    ApplicationContextMenu {
        id: shortcutShapeKindContextMenu
        objectName: "windowsShortcutShapeKindContextMenu"
        itemWidth: LV.Theme.scaleMetric(240)
        items: [root.referenceEntry(qsTr("Rectangle"), "rectangleShape"), root.referenceEntry(qsTr("Ellipse"), "ellipseShape"), root.referenceEntry(qsTr("Triangle"), "triangleShape"), root.referenceEntry(qsTr("Diamond"), "diamondShape"), root.referenceEntry(qsTr("Star"), "starShape"), root.referenceEntry(qsTr("Rectangle Bubble"), "rectangleBubbleShape"), root.referenceEntry(qsTr("Ellipse Bubble"), "ellipseBubbleShape")]
    }

    ApplicationContextMenu {
        id: shortcutWindowContextMenu
        objectName: "windowsShortcutWindowContextMenu"
        itemWidth: LV.Theme.scaleMetric(240)
        items: [root.referenceEntry(qsTr("Fit Canvas to Window"), "fitCanvasToWindow"), root.referenceEntry(qsTr("Reset Canvas View"), "resetCanvasView"), root.referenceEntry(qsTr("Minimize"), "minimizeWindow"), root.referenceEntry(qsTr("Enter Full Screen"), "toggleFullScreen")]
    }
}
