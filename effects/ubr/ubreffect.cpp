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

#include "ubreffect.h"
#include "xatom-helper.h"
#include "ubrtexturehelper.h"
#include "shaderhelper.h"

#include <kwineffects.h>
#include <kwinglutils.h>
#include <kwinglplatform.h>
#include <KWindowSystem>

#include <QPainterPath>
#include <QX11Info>
#include <QDebug>

#include <QMetaMethod>

static QList<KWin::EffectWindow *> maximizedWindows;

Q_DECLARE_METATYPE(UnityCorners)

typedef void (*ToplevelSetDepth)(void *, int);

static ToplevelSetDepth setDepthfunc = nullptr;

UBREffect::UBREffect(QObject *parent, const QVariantList &args)
{
    // try get Toplevel::setDepth(), for resolving ubr effect has black corners of opaque window.
    setDepthfunc = (ToplevelSetDepth) QLibrary::resolve("kwin.so." + qApp->applicationVersion(), "_ZN4KWin8Toplevel8setDepthEi");
    qDebug()<<"try get Toplevel::setDepth()"<<setDepthfunc;

    qDebug()<<parent<<args<<"ubr effect active";

    m_ubrShader = ShaderHelper::getShader();
    qRegisterMetaType<UnityCorners>("UnityCorners");

    // use kwindow system?
    //KWin::effects->findWindow()
    qDebug()<<KWin::effects;
    auto effectsManager = KWin::effects;

    auto windowSystem = KWindowSystem::self();

    for (auto id : windowSystem->windows()) {
        auto window = effectsManager->findWindow(id);

        bool isUKUIDecoration = XAtomHelper::getInstance()->isUKUIDecorationWindow(id);
        window->setData(IsUKUIDecoration, isUKUIDecoration);

        KWindowInfo info(id, NET::WMState);
        NETWinInfo netInfo(QX11Info::connection(), id, QX11Info::appRootWindow(), NET::Property(), NET::WM2GTKFrameExtents);
        auto gtkFrameExtent = netInfo.gtkFrameExtents();
        if (gtkFrameExtent.left > 0 || gtkFrameExtent.top > 0 || gtkFrameExtent.right > 0 || gtkFrameExtent.bottom > 0) {
            window->setData(IgnoreUBR, true);
        }

        if (info.hasState(NET::MaxHoriz) || info.hasState(NET::MaxVert)) {
            //qDebug()<<"forbid ubr effect"<<window->caption();
            maximizedWindows.prepend(window);
        }

        if (XAtomHelper::isFrameLessWindow(id)) {
            window->setData(IgnoreUBR, true);
        }

        if (!window->data(UnityBorderRadius).isValid()) {
            auto ubr = XAtomHelper::getInstance()->getWindowBorderRadius(id);
            if (ubr.topLeft == 0) {
                ubr.topLeft = 12;
            }
            if (ubr.topRight == 0) {
                ubr.topRight = 12;
            }
            if (ubr.bottomLeft == 0) {
                ubr.bottomLeft = 12;
            }
            if (ubr.bottomRight == 0) {
                ubr.bottomRight = 12;
            }
            window->setData(UnityBorderRadius, QVariant::fromValue(ubr));
        }

        //qDebug()<<ubr.topLeft<<ubr.topRight<<ubr.bottomLeft<<ubr.bottomRight<<window->caption();

        bool isCSD = XAtomHelper::getInstance()->isWindowDecorateBorderOnly(id);
        window->setData(IsCSD, isCSD);
    }

    connect(windowSystem, &KWindowSystem::windowAdded, this, [=](WId id){
        auto window = effectsManager->findWindow(id);
        qDebug()<<"window added"<<id<<window->caption();

        bool isUKUIDecoration = XAtomHelper::getInstance()->isUKUIDecorationWindow(id);
        window->setData(IsUKUIDecoration, isUKUIDecoration);

        KWindowInfo info(id, NET::WMState);
        NETWinInfo netInfo(QX11Info::connection(), id, QX11Info::appRootWindow(), NET::Property(), NET::WM2GTKFrameExtents);
        auto gtkFrameExtent = netInfo.gtkFrameExtents();
        if (gtkFrameExtent.left > 0 || gtkFrameExtent.top > 0 || gtkFrameExtent.right > 0 || gtkFrameExtent.bottom > 0) {
            window->setData(IgnoreUBR, true);
        }

        if (info.hasState(NET::MaxHoriz) || info.hasState(NET::MaxVert)) {
            //qDebug()<<"forbid ubr effect"<<window->caption();
            maximizedWindows.prepend(window);
        }

        auto motifHints = XAtomHelper::getInstance()->getWindowMotifHint(id);
        qDebug()<<motifHints.flags<<motifHints.decorations<<motifHints.functions;

        // skip frame less window.
        if (XAtomHelper::isFrameLessWindow(id)) {
            qDebug()<<window->caption()<<"is frame less";
            window->setData(IgnoreUBR, true);
        }

        auto ubr = XAtomHelper::getInstance()->getWindowBorderRadius(id);
        if (ubr.topLeft == 0) {
            ubr.topLeft = 12;
        }
        if (ubr.topRight == 0) {
            ubr.topRight = 12;
        }
        if (ubr.bottomLeft == 0) {
            ubr.bottomLeft = 12;
        }
        if (ubr.bottomRight == 0) {
            ubr.bottomRight = 12;
        }
        window->setData(UnityBorderRadius, QVariant::fromValue(ubr));
        //qDebug()<<ubr.topLeft<<ubr.topRight<<ubr.bottomLeft<<ubr.bottomRight<<window->caption();

        bool isCSD = XAtomHelper::getInstance()->isWindowDecorateBorderOnly(id);
        window->setData(IsCSD, isCSD);
    });
    connect(windowSystem, QOverload<WId, NET::Properties, NET::Properties2>::of(&KWindowSystem::windowChanged), this, [=](WId id, NET::Properties properties1, NET::Properties2 properties2){
        //qDebug()<<"window changed"<<id<<properties1<<properties2;
        if (properties1 & NET::Property::WMGeometry) {
            auto window = effectsManager->findWindow(id);
            maximizedWindows.removeOne(window);
            KWindowInfo info(id, NET::WMState);
            if (info.hasState(NET::MaxHoriz) || info.hasState(NET::MaxVert)) {
                //qDebug()<<"forbid ubr effect"<<window->caption();
                maximizedWindows.prepend(window);
            }
        }
    });

    qDebug()<<"ubr effect";

    for (auto window : effectsManager->stackingOrder()) {
        if (window->isFullScreen()) {
            maximizedWindows.prepend(window);
        }
        // FIXME: get window maximized state by xatom.
        //window->readProperty()
    }

    connect(effectsManager, &KWin::EffectsHandler::windowMaximizedStateChanged, this, [=](KWin::EffectWindow *window, bool hMax, bool vMax){
        maximizedWindows.removeOne(window);
        //qDebug()<<window->caption()<<hMax<<vMax;
        if (hMax || vMax) {
            maximizedWindows.prepend(window);
        }
    });

    connect(effectsManager, &KWin::EffectsHandler::windowDeleted, this, [=](KWin::EffectWindow *window){
        maximizedWindows.removeOne(window);

        window->setData(IgnoreUBR, QVariant());
        window->setData(UnityBorderRadius, QVariant());
        window->setData(IsCSD, QVariant());
        window->setData(IsUKUIDecoration, QVariant());
    });
}

UBREffect::~UBREffect()
{
    //ShaderHelper::releaseShaders();

    // clear the dirty texture.
    // NOTE: do not comment this code.
    UBRTextureHelper::getInstance()->release();

    for (auto window : KWin::effects->stackingOrder()) {
        window->setData(IgnoreUBR, QVariant());
        //window->setData(UnityBorderRadius, QVariant());
    }
}

void UBREffect::prePaintScreen(KWin::ScreenPrePaintData &data, int time)
{
    KWin::Effect::prePaintScreen(data, time);
}

void UBREffect::prePaintWindow(KWin::EffectWindow *w, KWin::WindowPrePaintData &data, int time)
{
    return KWin::Effect::prePaintWindow(w, data, time);
}

void UBREffect::drawWindow(KWin::EffectWindow *w, int mask, const QRegion &region, KWin::WindowPaintData &data)
{
    if (!w->data(IsUKUIDecoration).toBool()) {
        return KWin::Effect::drawWindow(w, mask, region, data);
    }

    if (!KWin::effects->isOpenGLCompositing()) {
        return KWin::Effect::drawWindow(w, mask, region, data);
    }

    if (w->data(IgnoreUBR).isValid()) {
        return KWin::Effect::drawWindow(w, mask, region, data);
    }

    if (!w->isManaged()) {
        return KWin::Effect::drawWindow(w, mask, region, data);
    }

    if (!w->isPaintingEnabled() || ((mask & PAINT_WINDOW_LANCZOS))) {
        return KWin::Effect::drawWindow(w, mask, region, data);
    }

    if (w->windowType() != NET::WindowType::Normal && !w->isDialog()) {
        return KWin::Effect::drawWindow(w, mask, region, data);
    }

    if ((mask & PAINT_WINDOW_TRANSFORMED) && !(mask & PAINT_SCREEN_TRANSFORMED)) {
        //return KWin::Effect::drawWindow(w, mask, region, data);
    }

    if (maximizedWindows.contains(w)) {
        return KWin::Effect::drawWindow(w, mask, region, data);
    }

    if (!w->hasAlpha()) {
        //return KWin::Effect::drawWindow(w, mask, region, data);
        if (setDepthfunc) {
            setDepthfunc(w->parent(), 32);
        }
    }

    auto ubr = qvariant_cast<UnityCorners>(w->data(UnityBorderRadius));

    KWin::WindowPaintData tmpData = data;

//    KWin::WindowQuadList windowQuads;
//    KWin::WindowQuadList shadowQuads;
//    KWin::WindowQuadList decorationQuads;

//    for (auto quad : tmpData.quads) {
//        switch (quad.type()) {
//        case KWin::WindowQuadContents: {
//            windowQuads<<quad;
//            break;
//        }
//        case KWin::WindowQuadDecoration: {
//            decorationQuads<<quad;
//            break;
//        }
//        case KWin::WindowQuadError: {
//            break;
//        }
//        case KWin::WindowQuadShadow:
//        case KWin::WindowQuadShadowTop:
//        case KWin::WindowQuadShadowTopRight:
//        case KWin::WindowQuadShadowRight:
//        case KWin::WindowQuadShadowBottomRight:
//        case KWin::WindowQuadShadowBottom:
//        case KWin::WindowQuadShadowBottomLeft:
//        case KWin::WindowQuadShadowLeft:
//        case KWin::WindowQuadShadowTopLeft: {
//            shadowQuads<<quad;
//        }
//        default: {
//            break;
//        }
//        }
//    }

    // draw window shadow
//    tmpData.quads = shadowQuads;
//    KWin::Effect::drawWindow(w, mask, region, tmpData);

//    // draw decoration
//    tmpData.quads = decorationQuads;
//    KWin::Effect::drawWindow(w, mask, region, tmpData);

//    // draw normal area

//    tmpData.quads = windowQuads;

//    tmpData.quads.clear();
//    tmpData.quads<<decorationQuads<<windowQuads;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };

    auto textureTopLeft = UBRTextureHelper::getInstance()->getTexture(ubr.topLeft);
    glActiveTexture(GL_TEXTURE10);
    textureTopLeft->bind();
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glActiveTexture(GL_TEXTURE0);

    auto textureTopRight = UBRTextureHelper::getInstance()->getTexture(ubr.topRight);
    glActiveTexture(GL_TEXTURE11);
    textureTopRight->bind();
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glActiveTexture(GL_TEXTURE0);

    auto textureBottomLeft = UBRTextureHelper::getInstance()->getTexture(ubr.bottomLeft);
    glActiveTexture(GL_TEXTURE12);
    textureBottomLeft->bind();
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glActiveTexture(GL_TEXTURE0);

    auto textureBottomRight = UBRTextureHelper::getInstance()->getTexture(ubr.bottomRight);
    glActiveTexture(GL_TEXTURE13);
    textureBottomRight->bind();
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glActiveTexture(GL_TEXTURE0);

    tmpData.shader = m_ubrShader;
    KWin::ShaderManager::instance()->pushShader(m_ubrShader);

    m_ubrShader->setUniform("topleft", 10);
    m_ubrShader->setUniform("scale", QVector2D(w->width()*1.0/textureTopLeft->width(), w->height()*1.0/textureTopLeft->height()));

    m_ubrShader->setUniform("topright", 11);
    m_ubrShader->setUniform("scale1", QVector2D(w->width()*1.0/textureTopRight->width(), w->height()*1.0/textureTopRight->height()));

    m_ubrShader->setUniform("bottomleft", 12);
    m_ubrShader->setUniform("scale2", QVector2D(w->width()*1.0/textureBottomLeft->width(), w->height()*1.0/textureBottomLeft->height()));

    m_ubrShader->setUniform("bottomright", 13);
    m_ubrShader->setUniform("scale3", QVector2D(w->width()*1.0/textureBottomRight->width(), w->height()*1.0/textureBottomRight->height()));

    KWin::Effect::drawWindow(w, mask, region, tmpData);

    KWin::ShaderManager::instance()->popShader();

    glActiveTexture(GL_TEXTURE10);
    textureTopLeft->unbind();
    glActiveTexture(GL_TEXTURE0);

    glActiveTexture(GL_TEXTURE11);
    textureTopRight->unbind();
    glActiveTexture(GL_TEXTURE0);

    glActiveTexture(GL_TEXTURE12);
    textureBottomLeft->unbind();
    glActiveTexture(GL_TEXTURE0);

    glActiveTexture(GL_TEXTURE13);
    textureBottomRight->unbind();
    glActiveTexture(GL_TEXTURE0);

    glDisable(GL_BLEND);

    return;
}
