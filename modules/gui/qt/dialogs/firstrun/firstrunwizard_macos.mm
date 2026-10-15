/*****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *****************************************************************************/

#import <AppKit/AppKit.h>

#include "firstrunwizard.hpp"

void FirstRunWizard::setVisible(bool visible)
{
    QWizard::setVisible(visible);
    if (!visible)
        return;

    // QWidget's Cocoa WId is its NSView. Only change this wizard's own
    // top-level native window, never its transient parent (the main window).
    NSView *view = reinterpret_cast<NSView *>(winId());
    NSWindow *window = view.window;
    if (!window || window.contentView != view)
    {
        msg_Warn(p_intf, "First-run wizard native window unavailable");
        return;
    }

    // AppKit otherwise refuses an external Quit while this modal sheet is
    // open, before Qt receives QEvent::Quit. Keep WindowModal, but allow Quit
    // to reach QuitSpy and the normal core shutdown. This is not Finish:
    // no staged choices or consent are committed on application termination.
    const bool prevented = window.preventsApplicationTerminationWhenModal;
    window.preventsApplicationTerminationWhenModal = NO;
    msg_Dbg(p_intf, "First-run wizard modal termination prevention: %d -> %d",
            prevented, bool(window.preventsApplicationTerminationWhenModal));
}
