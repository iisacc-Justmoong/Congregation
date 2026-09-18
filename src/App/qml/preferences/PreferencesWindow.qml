import QtQuick
import QtQuick.Dialogs as Dialogs
import QtQuick.Layouts
import LVRS 1.0 as LV

LV.Window {
    id: preferencesWindow

    objectName: "preferencesWindow"
    visible: false
    width: 480
    height: 360
    title: qsTr("Preferences")

    readonly property url profileImageSource: CongregationProfileImageProcessor.imageSource
    property alias profileName: profileNameField.text
    property alias canInviteOtherUsers: inviteOtherUsersCheckBox.checked
    property string accountEmail: ""
    property bool accountEmailLoading: false
    property bool startWithRecentCanvas: false
    property bool discoverNearbyCongregationUsers: true
    property bool restorePurchasesEnabled: true
    property bool updateCheckEnabled: false
    property bool initialCenteringApplied: false
    property var currentCanvasMemberProfiles: []
    property bool currentUserIsCanvasHost: true
    property string localCanvasState: "idle"
    property string localCanvasError: ""
    property int localCanvasParticipantCount: 0
    property var availableLocalCanvases: []
    property var availableLocalInvitees: []
    readonly property bool localCanvasActive: localCanvasState !== "idle"
    readonly property var displayedCanvasMemberProfiles: CongregationMemberProfileListBuilder.build(currentCanvasMemberProfiles, profileName, profileImageSource, currentUserIsCanvasHost)

    signal restorePurchasesRequested
    signal checkForUpdatesRequested
    signal startWithRecentCanvasRequested(bool enabled)
    signal discoverNearbyCongregationUsersRequested(bool enabled)
    signal inviteCanvasMemberRequested(string sessionId)
    signal deleteCanvasMemberRequested(var profile, int index)
    signal hostCanvasRequested
    signal joinCanvasRequested(string sessionId)
    signal leaveCanvasRequested

    function applyInitialCentering() {
        if (initialCenteringApplied || !transientParent)
            return false;

        const parentCenterX = transientParent.x + transientParent.width / 2;
        const parentCenterY = transientParent.y + transientParent.height / 2;
        x = Math.round(parentCenterX - width / 2);
        y = Math.round(parentCenterY - height / 2);
        initialCenteringApplied = true;
        return true;
    }

    function showGeneralSection() {
        generalSectionButton.checked = true;
    }

    function localCanvasStatusText() {
        if (localCanvasError.length > 0)
            return localCanvasError;
        if (localCanvasState === "hosting")
            return qsTr("Sharing on local network · %1 connected").arg(localCanvasParticipantCount);
        if (localCanvasState === "joining")
            return qsTr("Connecting to nearby canvas…");
        if (localCanvasState === "connected")
            return qsTr("Connected to host canvas");
        return qsTr("Canvas stays on this device");
    }

    function localCanvasJoinItems() {
        const result = [];
        const canvases = availableLocalCanvases || [];
        for (let index = 0; index < Number(canvases.length || 0); ++index) {
            const canvas = canvases[index];
            result.push({
                label: canvas.displayName || qsTr("Nearby canvas"),
                sessionId: String(canvas.sessionId || ""),
                showIconSlot: false,
                showChevron: false
            });
        }
        return result;
    }

    function localCanvasInviteItems() {
        const result = [];
        const invitees = availableLocalInvitees || [];
        for (let index = 0; index < Number(invitees.length || 0); ++index) {
            const invitee = invitees[index];
            result.push({
                label: invitee.displayName || qsTr("Nearby Congregation user"),
                sessionId: String(invitee.sessionId || ""),
                showIconSlot: true,
                iconName: "user",
                showChevron: false
            });
        }
        return result;
    }

    Dialogs.FileDialog {
        id: profileImageDialog

        title: qsTr("Choose profile image")
        nameFilters: [qsTr("Image files (*.png *.jpg *.jpeg *.bmp *.webp)")]
        onAccepted: CongregationProfileImageProcessor.processProfileImage(selectedFile)
    }

    Component {
        id: profileImageMenuItemDelegate

        LV.MenuItem {
            property var modelData: ({})

            label: modelData.label || ""
            itemWidth: profileImageMenu.minimumItemWidth
            state: modelData.state === undefined ? defaultState : modelData.state
            keyVisible: false
            showIconSlot: false
            showChevron: false
            hasChildItems: false
            enabled: modelData.enabled !== false
            Accessible.name: label
            onClicked: modelData.trigger()
        }
    }

    LV.ContextMenu {
        id: profileImageMenu

        itemDelegate: profileImageMenuItemDelegate
        showIconSlot: false
        items: [
            {
                label: qsTr("Select profile image"),
                action: "select",
                showChevron: false
            },
            {
                label: qsTr("Delete profile image"),
                action: "delete",
                showChevron: false,
                enabled: preferencesWindow.profileImageSource.toString().length > 0
            }
        ]
        onItemTriggered: function (index, item) {
            if (item.action === "select") {
                Qt.callLater(function () {
                    profileImageDialog.open();
                });
                return;
            }
            if (item.action === "delete") {
                CongregationProfileImageProcessor.clearProfileImage();
            }
        }
    }

    LV.ContextMenu {
        id: localCanvasJoinMenu

        showIconSlot: false
        items: preferencesWindow.localCanvasJoinItems()
        onItemTriggered: function (index, item) {
            if (item && item.sessionId)
                preferencesWindow.joinCanvasRequested(String(item.sessionId));
        }
    }

    LV.ContextMenu {
        id: localCanvasInviteMenu

        items: preferencesWindow.localCanvasInviteItems()
        onItemTriggered: function (index, item) {
            if (item && item.sessionId)
                preferencesWindow.inviteCanvasMemberRequested(String(item.sessionId));
        }
    }

    Component {
        id: memberProfileDelegate

        LV.ListItem {
            property var modelData: ({})
            readonly property var entry: modelData.entry || ({})

            size: LV.ListItem.Mini
            label: modelData.label || ""
            iconSource: memberList.memberProfileImageSource(entry)
            iconName: iconSource.toString().length > 0 ? "" : (modelData.iconName || "user")
            selected: modelData.selected === true
            enabled: modelData.enabled === true
            rowHorizontalPadding: memberList.itemLabelLeftPadding
            rowVerticalPadding: LV.Theme.gap2
            miniItemWidth: memberList.listWidth
            minItemWidth: memberList.listWidth
            listBackgroundColor: "transparent"
            selectedBackgroundColor: memberList.selectedRowColor
            separatorColor: memberList.separatorColor
            separatorOpacity: memberList.separatorOpacity
            separatorVisible: false
            onClicked: modelData.trigger()
        }
    }

    LV.LabelSegmentedControl {
        id: preferencesSectionHeader

        objectName: "preferencesSectionHeader"
        anchors.top: parent.top
        anchors.topMargin: LV.Theme.gap24
        anchors.horizontalCenter: parent.horizontalCenter

        LV.LabelButton {
            id: generalSectionButton

            objectName: "generalSectionButton"
            text: qsTr("General")
            checkable: true
            autoExclusive: true
            checked: true
            backgroundColor: checked ? LV.Theme.panelBackground12 : "transparent"
        }

        LV.LabelButton {
            id: profileSectionButton

            objectName: "profileSectionButton"
            text: qsTr("Profile")
            checkable: true
            autoExclusive: true
            backgroundColor: checked ? LV.Theme.panelBackground12 : "transparent"
        }

        LV.LabelButton {
            id: membersSectionButton

            objectName: "membersSectionButton"
            text: qsTr("Members")
            checkable: true
            autoExclusive: true
            backgroundColor: checked ? LV.Theme.panelBackground12 : "transparent"
        }
    }

    Item {
        id: generalSettings

        anchors.top: preferencesSectionHeader.bottom
        anchors.topMargin: LV.Theme.gap20
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: generalSectionButton.checked

        LV.VStack {
            id: generalContent

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.leftMargin: LV.Theme.gap24
            alignment: Qt.AlignLeft
            spacing: LV.Theme.gap20

            LV.VStack {
                alignment: Qt.AlignLeft
                spacing: LV.Theme.gap6

                LV.Label {
                    id: accountEmailLabel

                    objectName: "accountEmailLabel"
                    text: qsTr("Account")
                    style: description
                    Accessible.name: text
                }

                LV.Label {
                    id: accountEmailValueLabel

                    objectName: "accountEmailValueLabel"
                    text: preferencesWindow.accountEmailLoading ? qsTr("Loading…") : (preferencesWindow.accountEmail.length ? preferencesWindow.accountEmail : qsTr("Not connected"))
                    style: body
                    Accessible.name: accountEmailLabel.text + ": " + text
                }
            }

            LV.VStack {
                alignment: Qt.AlignLeft
                spacing: LV.Theme.gap6

                LV.Label {
                    text: qsTr("Startup with")
                    style: description
                }

                LV.HStack {
                    spacing: LV.Theme.gap16

                    LV.RadioButton {
                        id: newCanvasRadioButton

                        objectName: "newCanvasRadioButton"
                        text: qsTr("New canvas")
                        autoExclusive: true
                        checked: !preferencesWindow.startWithRecentCanvas
                        onToggled: {
                            if (checked)
                                preferencesWindow.startWithRecentCanvasRequested(false);
                        }
                    }

                    LV.RadioButton {
                        id: recentCanvasRadioButton

                        objectName: "recentCanvasRadioButton"
                        text: qsTr("Recent canvas")
                        autoExclusive: true
                        checked: preferencesWindow.startWithRecentCanvas
                        onToggled: {
                            if (checked)
                                preferencesWindow.startWithRecentCanvasRequested(true);
                        }
                    }
                }
            }

            LV.CheckBox {
                id: discoverNearbyCongregationUsersCheckBox

                objectName: "discoverNearbyCongregationUsersCheckBox"
                text: qsTr("Discover nearby Congregation users")
                checked: preferencesWindow.discoverNearbyCongregationUsers
                onToggled: preferencesWindow.discoverNearbyCongregationUsersRequested(checked)
            }
        }

        LV.HStack {
            id: generalActions

            anchors.left: parent.left
            anchors.leftMargin: LV.Theme.gap24
            anchors.right: parent.right
            anchors.rightMargin: LV.Theme.gap24
            anchors.bottom: parent.bottom
            anchors.bottomMargin: LV.Theme.gap24

            LV.LabelButton {
                id: restorePurchasesButton

                objectName: "restorePurchasesButton"
                text: qsTr("Restore Purchases")
                tone: LV.AbstractButton.Default
                enabled: preferencesWindow.restorePurchasesEnabled
                onClicked: preferencesWindow.restorePurchasesRequested()
            }

            LV.Spacer {}

            LV.LabelButton {
                id: checkForUpdatesButton

                objectName: "checkForUpdatesButton"
                text: qsTr("Check for Updates…")
                tone: LV.AbstractButton.Primary
                enabled: preferencesWindow.updateCheckEnabled
                onClicked: preferencesWindow.checkForUpdatesRequested()
            }
        }
    }

    LV.VStack {
        id: profileSettings

        anchors.top: preferencesSectionHeader.bottom
        anchors.topMargin: LV.Theme.gap20
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: LV.Theme.gap16
        visible: profileSectionButton.checked

        LV.VStack {
            spacing: LV.Theme.gap6

            LV.Label {
                text: qsTr("Profile image")
                style: description
            }

            LV.IconButton {
                id: profileImageButton

                readonly property int avatarSize: LV.Theme.scaleMetric(64)
                readonly property int avatarInset: Math.max(0, Math.round((avatarSize - iconSize) / 2))

                tone: LV.AbstractButton.Borderless
                shapeStyle: profileImageButton.shapeCylinder
                horizontalPadding: avatarInset
                verticalPadding: avatarInset
                iconName: preferencesWindow.profileImageSource.toString().length === 0 ? "user" : ""
                iconSource: preferencesWindow.profileImageSource
                iconSize: LV.Theme.scaleMetric(44)
                implicitWidth: avatarSize
                implicitHeight: avatarSize
                Layout.preferredWidth: avatarSize
                Layout.preferredHeight: avatarSize
                Accessible.name: qsTr("Profile image options")
                onClicked: profileImageMenu.openFor(profileImageButton, 0, profileImageButton.height)
            }
        }

        LV.VStack {
            alignment: Qt.AlignLeft
            spacing: LV.Theme.gap6

            LV.Label {
                text: qsTr("Profile name")
                style: description
            }

            LV.InputField {
                id: profileNameField

                placeholder: qsTr("Profile name")
                Accessible.name: qsTr("Profile name")
            }
        }

        LV.CheckBox {
            id: inviteOtherUsersCheckBox

            text: qsTr("Allow inviting other users")
            checked: false
        }
    }

    Item {
        id: membersSettings

        anchors.top: preferencesSectionHeader.bottom
        anchors.topMargin: LV.Theme.gap20
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: membersSectionButton.checked

        LV.HStack {
            id: localCanvasActions

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.leftMargin: LV.Theme.gap24
            anchors.right: parent.right
            anchors.rightMargin: LV.Theme.gap24
            spacing: LV.Theme.gap8

            LV.Label {
                id: localCanvasStatusLabel

                objectName: "localCanvasStatusLabel"
                text: preferencesWindow.localCanvasStatusText()
                style: description
                elide: Text.ElideRight
                Layout.fillWidth: true
                Accessible.name: text
            }

            LV.LabelButton {
                id: shareCanvasButton

                objectName: "shareCanvasButton"
                visible: !preferencesWindow.localCanvasActive
                text: qsTr("Share canvas")
                tone: LV.AbstractButton.Primary
                onClicked: preferencesWindow.hostCanvasRequested()
            }

            LV.LabelButton {
                id: joinCanvasButton

                objectName: "joinCanvasButton"
                visible: !preferencesWindow.localCanvasActive && Number((preferencesWindow.availableLocalCanvases || []).length || 0) > 0
                text: qsTr("Join nearby…")
                tone: LV.AbstractButton.Default
                onClicked: localCanvasJoinMenu.openFor(joinCanvasButton, 0, joinCanvasButton.height)
            }

            LV.LabelButton {
                id: leaveCanvasButton

                objectName: "leaveCanvasButton"
                visible: preferencesWindow.localCanvasActive
                text: preferencesWindow.localCanvasState === "hosting" ? qsTr("Stop sharing") : qsTr("Leave canvas")
                tone: LV.AbstractButton.Default
                onClicked: preferencesWindow.leaveCanvasRequested()
            }
        }

        LV.List {
            id: memberList

            readonly property bool canDeleteSelectedMember: {
                if (selectedIndex < 0 || selectedIndex >= entryCount)
                    return false;
                return roleValue(entryAt(selectedIndex), "removable", true);
            }

            objectName: "memberList"
            anchors.top: localCanvasActions.bottom
            anchors.topMargin: LV.Theme.gap12
            anchors.bottom: parent.bottom
            anchors.bottomMargin: LV.Theme.gap24
            anchors.horizontalCenter: parent.horizontalCenter
            width: implicitWidth
            listWidth: LV.Theme.scaleMetric(237)
            minimumListHeight: LV.Theme.scaleMetric(231)
            model: preferencesWindow.displayedCanvasMemberProfiles
            labelRole: "displayName"
            defaultItemIconName: "user"
            itemDelegate: memberProfileDelegate
            footerVisible: true
            footerButton1: ({
                    type: "icon",
                    iconName: "add",
                    enabled: (preferencesWindow.localCanvasState === "idle" || preferencesWindow.localCanvasState === "hosting") && Number((preferencesWindow.availableLocalInvitees || []).length || 0) > 0
                })
            footerButton2: ({
                    type: "icon",
                    iconName: "generaldelete",
                    enabled: memberList.canDeleteSelectedMember
                })
            footerButton3: ({
                    type: "icon",
                    iconName: "",
                    iconGlyph: " ",
                    enabled: false
                })

            function memberProfileImageSource(entry) {
                const value = roleValue(entry, "profileImageSource", "");
                return value === null || value === undefined ? "" : String(value);
            }

            onModelChanged: selectedIndex = -1
            onEntryCountChanged: {
                if (selectedIndex >= entryCount)
                    selectedIndex = -1;
            }
            onItemTriggered: function (index) {
                selectedIndex = index;
            }
            onFooterButtonTriggered: function (index) {
                if (index === 0) {
                    localCanvasInviteMenu.openFor(memberList, 0, memberList.height);
                    return;
                }
                if (index === 1 && memberList.canDeleteSelectedMember) {
                    const selectedProfile = entryAt(selectedIndex);
                    const sourceProfile = memberList.roleValue(selectedProfile, "sourceProfile", selectedProfile);
                    const sourceIndex = memberList.roleValue(selectedProfile, "sourceIndex", -1);
                    preferencesWindow.deleteCanvasMemberRequested(sourceProfile, sourceIndex);
                }
            }
        }
    }
}
