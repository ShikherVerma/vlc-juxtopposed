// Copyright (C) 2026 VideoLAN; GPL-2.0-or-later
import QtQuick
import QtQuick.Effects
import VLC.Style

Rectangle {
    id: root

    property bool shadow: true
    // Qt Quick popups live in the window Overlay, outside the application root.
    // Never sample the Overlay itself (that would recursively capture this popup).
    property Item backdrop: Window.window?.contentItem.children[0] ?? null
    property color tint: VLCStyle.popupTint(theme.palette?.isDark ?? VLCStyle.palette.isDark)

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.ComboBox
        enabled: true
        hovered: false
        pressed: false
        focused: false
    }

    radius: VLCStyle.dropdownPopup_radius
    color: backdrop && backdrop !== root.parent
           ? Qt.alpha(tint, (theme.palette?.isDark ?? VLCStyle.palette.isDark) ? 0.95 : 0.75) : tint

    ShaderEffectSource {
        id: sample
        visible: false
        sourceItem: root.backdrop === root.parent ? null : root.backdrop
        sourceRect: {
            if (!sourceItem) return Qt.rect(0, 0, 0, 0)
            const p = root.mapToItem(sourceItem, 0, 0)
            return Qt.rect(p.x, p.y, root.width, root.height)
        }
        textureSize: Qt.size(root.width, root.height)
        live: root.visible
    }

    Rectangle {
        id: mask
        anchors.fill: parent
        radius: root.radius
        visible: false
        layer.enabled: true
        color: "white"
    }

    MultiEffect {
        anchors.fill: parent
        z: -1
        source: sample
        visible: sample.sourceItem !== null
        blurEnabled: true
        blurMax: VLCStyle.popup_blurRadius
        blur: 1.0
        autoPaddingEnabled: false
        maskEnabled: true
        maskSource: mask
    }

    RoundedRectangleShadow {
        visible: root.shadow
        z: -2
        blurRadius: VLCStyle.popup_shadowRadius
        yOffset: VLCStyle.popup_shadowOffset
        color: theme.shadow
    }
}
