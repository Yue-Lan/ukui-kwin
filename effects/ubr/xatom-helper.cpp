/*
 * KWin Style UKUI
 *
 * Copyright (C) 2020, KylinSoft Co., Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Authors: Yue Lan <lanyue@kylinos.cn>
 *
 */

#include <kwinglobals.h>
#include "xatom-helper.h"

#include <limits.h>

#include <xcb/xcb_util.h>

static XAtomHelper *global_instance = nullptr;

XAtomHelper *XAtomHelper::getInstance()
{
    if (!global_instance)
        global_instance = new XAtomHelper;

    if (!KWin::connection())
        return global_instance;

    if (!global_instance->m_motifWMHintsAtom) {
        QString tmp("_MOTIF_WM_HINTS");
        xcb_intern_atom_cookie_t cookie1 = xcb_intern_atom(KWin::connection(), false, tmp.length(), tmp.toUtf8());
        tmp = "_UNITY_GTK_BORDER_RADIUS";
        xcb_intern_atom_cookie_t cookie2 = xcb_intern_atom(KWin::connection(), false, tmp.length(), tmp.toUtf8());
        tmp = "_KWIN_UKUI_DECORAION";
        xcb_intern_atom_cookie_t cookie3 = xcb_intern_atom(KWin::connection(), false, tmp.length(), tmp.toUtf8());

        xcb_intern_atom_reply_t *reply1 = xcb_intern_atom_reply(KWin::connection(), cookie1, nullptr);
        global_instance->m_motifWMHintsAtom = reply1->atom;
        free(reply1);

        xcb_intern_atom_reply_t *reply2 = xcb_intern_atom_reply(KWin::connection(), cookie2, nullptr);
        global_instance->m_unityBorderRadiusAtom = reply2->atom;
        free(reply2);

        xcb_intern_atom_reply_t *reply3 = xcb_intern_atom_reply(KWin::connection(), cookie3, nullptr);
        global_instance->m_ukuiDecorationAtion = reply3->atom;
        free(reply3);
    }

    return global_instance;
}

bool XAtomHelper::isFrameLessWindow(int winId)
{
    auto hints = getInstance()->getWindowMotifHint(winId);
    if (hints.flags == MWM_HINTS_DECORATIONS && hints.functions == 1) {
        return true;
    }
    return false;
}

bool XAtomHelper::isWindowDecorateBorderOnly(int winId)
{
    return isWindowMotifHintDecorateBorderOnly(getInstance()->getWindowMotifHint(winId));
}

bool XAtomHelper::isWindowMotifHintDecorateBorderOnly(const MotifWmHints &hint)
{
    bool isDeco = false;
    if (hint.flags & MWM_HINTS_DECORATIONS) {
        if (hint.decorations == MWM_DECOR_BORDER) {
            isDeco = true;
        }
    }
    return isDeco;
}

bool XAtomHelper::isUKUICsdSupported()
{
    // fixme:
    return false;
}

bool XAtomHelper::isUKUIDecorationWindow(int winId)
{
    if (m_ukuiDecorationAtion == 0)
        return false;

    uchar *data;

    bool isUKUIDecoration = false;

    xcb_get_property_cookie_t cookie = xcb_get_property(KWin::connection(), false, winId, m_ukuiDecorationAtion, XCB_ATOM_ANY, 0, 1);
    xcb_get_property_reply_t *reply = xcb_get_property_reply(KWin::connection(), cookie, nullptr);
    if (!reply)
        return false;

    data = (uchar *)xcb_get_property_value(reply);
    free(reply);
    if (data) {
        isUKUIDecoration = data[0];
        //free(data);
    }

    return isUKUIDecoration;
}

UnityCorners XAtomHelper::getWindowBorderRadius(int winId)
{
    UnityCorners corners;
    corners.topLeft = 0;
    corners.topRight = 0;
    corners.bottomLeft = 0;
    corners.bottomRight = 0;

    uchar *data;

    xcb_get_property_cookie_t cookie = xcb_get_property(KWin::connection(), false, winId, m_unityBorderRadiusAtom, XCB_ATOM_CARDINAL, 0, sizeof(UnityCorners)/sizeof(ulong));
    xcb_get_property_reply_t *reply = xcb_get_property_reply(KWin::connection(), cookie, nullptr);
    if (!reply)
        return corners;

    if (xcb_get_property_value_length(reply) != 4 * sizeof(int)) {
        free(reply);
        return corners;
    }

    data = (uchar*)xcb_get_property_value(reply);
    free(reply);

    if (data) {
        corners.topLeft = static_cast<ulong>(data[0]);
        corners.topRight = static_cast<ulong>(data[1*sizeof (int)]);
        corners.bottomLeft = static_cast<ulong>(data[2*sizeof (int)]);
        corners.bottomRight = static_cast<ulong>(data[3*sizeof (int)]);
        //free(data);
    }

    return corners;
}

void XAtomHelper::setWindowBorderRadius(int winId, const UnityCorners &data)
{
    if (m_unityBorderRadiusAtom == 0)
        return;

    ulong corners[4] = {data.topLeft, data.topRight, data.bottomLeft, data.bottomRight};

    xcb_change_property(KWin::connection(), XCB_PROP_MODE_REPLACE, winId, m_unityBorderRadiusAtom, XCB_ATOM_CARDINAL, 32, sizeof(corners)/sizeof(corners[0]), &corners);
}

void XAtomHelper::setWindowBorderRadius(int winId, int topLeft, int topRight, int bottomLeft, int bottomRight)
{
    if (m_unityBorderRadiusAtom == 0)
        return;

    ulong corners[4] = {(ulong)topLeft, (ulong)topRight, (ulong)bottomLeft, (ulong)bottomRight};

    xcb_change_property(KWin::connection(), XCB_PROP_MODE_REPLACE, winId, m_unityBorderRadiusAtom, XCB_ATOM_CARDINAL, 32, sizeof(corners)/sizeof(corners[0]), &corners);
}

void XAtomHelper::setUKUIDecoraiontHint(int winId, bool set)
{
    if (m_ukuiDecorationAtion == 0)
        return;

    xcb_change_property(KWin::connection(), XCB_PROP_MODE_REPLACE, winId, m_ukuiDecorationAtion, XCB_ATOM_ANY, 8, 1, &set);
}

void XAtomHelper::setWindowMotifHint(int winId, const MotifWmHints &hints)
{
    if (m_unityBorderRadiusAtom == 0)
        return;

    xcb_change_property(KWin::connection(), XCB_PROP_MODE_REPLACE, winId, m_motifWMHintsAtom, m_motifWMHintsAtom,
                        32, sizeof (MotifWmHints)/ sizeof (ulong), &hints);
}

MotifWmHints XAtomHelper::getWindowMotifHint(int winId)
{
    MotifWmHints hints;

    if (m_motifWMHintsAtom == 0)
        return hints;

    uchar *data;

    xcb_get_property_cookie_t cookie = xcb_get_property(KWin::connection(), false, winId, m_motifWMHintsAtom, XCB_ATOM_ANY, 0, sizeof (MotifWmHints)/ sizeof (ulong));
    xcb_get_property_reply_t *reply = xcb_get_property_reply(KWin::connection(), cookie, nullptr);
    if (!reply)
        return hints;

    if (reply->length != 5) {
        free(reply);
        return hints;
    }

    data = (uchar*)xcb_get_property_value(reply);
    free(reply);
    if (!data)
        return hints;
    hints.flags = data[0];
    hints.functions = data[sizeof(int)];
    hints.decorations = data[sizeof(int)*2];
    hints.input_mode = data[sizeof(int)*3];
    hints.status = data[sizeof(int)*4];

    return hints;
}

XAtomHelper::XAtomHelper(QObject *parent) : QObject(parent)
{
    if (!KWin::connection())
        return;

    QString tmp("_MOTIF_WM_HINTS");
    xcb_intern_atom_cookie_t cookie1 = xcb_intern_atom(KWin::connection(), false, tmp.length(), tmp.toUtf8());
    tmp = "_UNITY_GTK_BORDER_RADIUS";
    xcb_intern_atom_cookie_t cookie2 = xcb_intern_atom(KWin::connection(), false, tmp.length(), tmp.toUtf8());
    tmp = "_KWIN_UKUI_DECORAION";
    xcb_intern_atom_cookie_t cookie3 = xcb_intern_atom(KWin::connection(), false, tmp.length(), tmp.toUtf8());

    xcb_intern_atom_reply_t *reply1 = xcb_intern_atom_reply(KWin::connection(), cookie1, nullptr);
    m_motifWMHintsAtom = reply1->atom;
    free(reply1);

    xcb_intern_atom_reply_t *reply2 = xcb_intern_atom_reply(KWin::connection(), cookie2, nullptr);
    m_unityBorderRadiusAtom = reply2->atom;
    free(reply2);

    xcb_intern_atom_reply_t *reply3 = xcb_intern_atom_reply(KWin::connection(), cookie3, nullptr);
    m_ukuiDecorationAtion = reply3->atom;
    free(reply3);
}

xcb_atom_t XAtomHelper::registerUKUICsdNetWmSupportAtom()
{
    // fixme:
    return 0;
}

void XAtomHelper::unregisterUKUICsdNetWmSupportAtom()
{
    // fixme:
}

bool XAtomHelper::isShowMinimizeButton(int winId)
{
    MotifWmHints hint = getInstance()->getWindowMotifHint(winId);
    if (!(hint.flags & MWM_HINTS_FUNCTIONS)) {
        return true;
    }
    if (hint.functions & MWM_FUNC_ALL) {
        return true;
    }
    if (!(hint.functions & MWM_FUNC_MINIMIZE)) {
        return false;
    }
    return true;
}
