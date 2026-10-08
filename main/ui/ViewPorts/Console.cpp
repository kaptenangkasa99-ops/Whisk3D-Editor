#include "ViewPorts/Console.h"
#include "w3dGraphics.h"                 // w3dEngine (abstraccion grafica)
#include "WhiskUI/draw/glesdraw.h"       // W3dPantallaAlto
#include "WhiskUI/theme/colores.h"       // ListaColores / ColorID / SetColorID
#include "WhiskUI/text/bitmapText.h"     // RenderBitmapText
#include "WhiskUI/core/UI.h"             // marginGS / RenglonHeightGS / GlobalScale / borderGS / dx / dy
#include "objects/Textures.h"            // Textures[0] = atlas de la fuente
#include "render/OpcionesRender.h"       // g_redraw (render event-driven)
#include "w3dlog.h"                      // ring buffer del log del Core
#include "W3dClipboard.h"
#include "script/W3dScript.h"            // W3dScriptUltimoError()
#include <algorithm>
#include <stdio.h>
#include <string>

namespace gfx = w3dEngine;

// Colores de nivel FIJOS (el tema no tiene rol de error/aviso, y accent hoy es
// verde: no sirve para resaltar errores). Elegidos legibles sobre fondo oscuro.
static const float kRojoError[4]    = { 0.95f, 0.35f, 0.30f, 1.0f }; // [ERROR] + banner de lua
static const float kAmarilloWarn[4] = { 0.95f, 0.85f, 0.30f, 1.0f }; // [WARN]
static const float kSeleccion[4]   = { 0.20f, 0.36f, 0.55f, 1.0f };

Console::Console() {
    lastCount = -1;   // fuerza la primera medicion del contenido
    lastMaxAncho = 0;
    lastBannerH = 0;
    showInfo = showWarn = showError = true;
    groupRepeated = false;
    selectionActive = selectionDragging = false;
    selectionAnchorRow = selectionAnchorCol = 0;
    selectionFocusRow = selectionFocusCol = 0;
    BarCrear();
    Button* b = new Button("Clear", IconType::borrar, true);
    b->rol = BAR_CLEAR; BarButtons.push_back(b);
    b = new Button("Info"); b->rol = BAR_INFO; BarButtons.push_back(b);
    b = new Button("Warn"); b->rol = BAR_WARN; BarButtons.push_back(b);
    b = new Button("Error"); b->rol = BAR_ERROR; BarButtons.push_back(b);
    b = new Button("Group"); b->rol = BAR_GROUP; BarButtons.push_back(b);
    SyncFilterButtons();
}

Console::~Console() {}

void Console::ClearLog() {
    w3dLogRingClear();
    lastCount = -1;
    lastMaxAncho = 0;
    lastBannerH = 0;
    PosX = 0;
    PosY = 0;
    selectionActive = selectionDragging = false;
    visibleLines.clear();
    RecalcularScroll();
    g_redraw = true;
}

void Console::SyncFilterButtons() {
    const float* activo = ListaColores[static_cast<int>(ColorID::accent)];
    const float* inactivo = ListaColores[static_cast<int>(ColorID::gris)];
    const float* textoActivo = ListaColores[static_cast<int>(ColorID::blanco)];
    const float* textoInactivo = ListaColores[static_cast<int>(ColorID::grisUI)];
    for (size_t i = 0; i < BarButtons.size(); i++) {
        Button* b = BarButtons[i];
        bool enabled = (b->rol == BAR_INFO) ? showInfo :
                       (b->rol == BAR_WARN) ? showWarn :
                       (b->rol == BAR_ERROR) ? showError :
                       (b->rol == BAR_GROUP) ? groupRepeated : false;
        if (b->rol == BAR_CLEAR) continue;
        b->tinte = enabled ? activo : inactivo;
        b->colorTexto = enabled ? textoActivo : textoInactivo;
    }
}

void Console::RebuildVisibleLines() {
    visibleLines.clear();
    const int count = w3dLogRingCount();
    for (int i = 0; i < count;) {
        const char* raw = w3dLogRingLinea(i);
        LogLine line;
        line.text = raw ? raw : "";
        line.repetitions = 1;
        line.level = (line.text.compare(0, 7, "[ERROR]") == 0) ? 2 :
                     (line.text.compare(0, 6, "[WARN]") == 0) ? 1 : 0;
        int next = i + 1;
        if (groupRepeated) {
            while (next < count && line.text == w3dLogRingLinea(next)) {
                ++line.repetitions;
                ++next;
            }
        }
        bool show = (line.level == 0 && showInfo) ||
                    (line.level == 1 && showWarn) ||
                    (line.level == 2 && showError);
        if (show) {
            if (groupRepeated && line.repetitions > 1) {
                char suffix[24];
                snprintf(suffix, sizeof(suffix), " (x%d)", line.repetitions);
                line.text += suffix;
            }
            visibleLines.push_back(line);
        }
        i = next;
    }
}

bool Console::SelectionBoundsForRow(int row, int& begin, int& end) const {
    if (!selectionActive || visibleLines.empty()) return false;
    int firstRow = selectionAnchorRow, firstCol = selectionAnchorCol;
    int lastRow = selectionFocusRow, lastCol = selectionFocusCol;
    if (firstRow > lastRow || (firstRow == lastRow && firstCol > lastCol)) {
        std::swap(firstRow, lastRow);
        std::swap(firstCol, lastCol);
    }
    if (row < firstRow || row > lastRow) return false;
    const int length = (int)visibleLines[(size_t)row].text.size();
    begin = (row == firstRow) ? firstCol : 0;
    end = (row == lastRow) ? lastCol : length;
    if (begin < 0) begin = 0;
    if (begin > length) begin = length;
    if (end < 0) end = 0;
    if (end > length) end = length;
    return end > begin;
}

void Console::SelectionPosition(int mx, int my, int& row, int& col) const {
    const int lh = (RenglonHeightGS > 0) ? RenglonHeightGS : 12;
    const int contentTop = BarTopOffset() + lastBannerH;
    row = (my - y - contentTop - marginGS - PosY) / lh;
    if (row < 0) row = 0;
    if (row >= (int)visibleLines.size()) row = (int)visibleLines.size() - 1;
    int charWidth = LetterWidthGS > 0 ? LetterWidthGS : 1;
    col = (mx - x - marginGS - borderGS - PosX + charWidth / 2) / charWidth;
    if (col < 0) col = 0;
    int length = (int)visibleLines[(size_t)row].text.size();
    if (col >= length) col = length;
}

void Console::UpdateSelection(int mx, int my) {
    if (visibleLines.empty()) return;
    int row = 0, col = 0;
    SelectionPosition(mx, my, row, col);
    selectionFocusRow = row;
    selectionFocusCol = col;
    g_redraw = true;
}

void Console::CopySelection() {
    if (!selectionActive || visibleLines.empty()) return;
    int firstRow = selectionAnchorRow, firstCol = selectionAnchorCol;
    int lastRow = selectionFocusRow, lastCol = selectionFocusCol;
    if (firstRow > lastRow || (firstRow == lastRow && firstCol > lastCol)) {
        std::swap(firstRow, lastRow);
        std::swap(firstCol, lastCol);
    }
    std::string copied;
    for (int row = firstRow; row <= lastRow; ++row) {
        int begin = 0, end = 0;
        if (SelectionBoundsForRow(row, begin, end))
            copied.append(visibleLines[(size_t)row].text, (size_t)begin, (size_t)(end - begin));
        if (row < lastRow) copied += '\n';
    }
    if (!copied.empty()) w3dEngine::W3dClipboardSet(copied);
}

bool Console::ClickBarButton(int mx, int my) {
    for (size_t i = 1; i < BarButtons.size(); i++) {
        Button* b = BarButtons[i];
        if (!b->visible || !b->Contains(mx, my)) continue;
        if (b->rol == BAR_CLEAR) ClearLog();
        else if (b->rol == BAR_INFO) showInfo = !showInfo;
        else if (b->rol == BAR_WARN) showWarn = !showWarn;
        else if (b->rol == BAR_ERROR) showError = !showError;
        else if (b->rol == BAR_GROUP) groupRepeated = !groupRepeated;
        SyncFilterButtons();
        selectionActive = false;
        lastCount = -1;
        g_redraw = true;
        return true;
    }
    return true;
}

// recalcula el rango del scrollbar con las metricas guardadas. AUTOSCROLL:
// si estabamos pegados al final (PosY == MaxPosY), el final nuevo nos sigue;
// si el usuario scrolleo hacia arriba, se respeta su posicion.
void Console::RecalcularScroll() {
    const int lh = (RenglonHeightGS > 0) ? RenglonHeightGS : 12;
    bool alFinal = (PosY <= MaxPosY); // pegado al fondo? (los clamps garantizan PosY >= MaxPosY)
    int n = (lastCount > 0) ? lastCount : 0;
    ResizeScrollbar(width, height, lastMaxAncho, -(n * lh + marginGS * 2),
                    BarTopOffset() + lastBannerH);
    if (alFinal) PosY = MaxPosY; // re-enganchar al final del log
}

void Console::Resize(int newW, int newH) {
    ViewportBase::Resize(newW, newH);
    ResizeBorder(newW, newH); // el borde sigue el tamano (como Outliner/Properties/UV)
    RecalcularScroll();
}

// dibuja una linea de texto en (px, py) del viewport (coord local, origen arriba-izq)
static void DibujarLinea(int px, int py, const std::string& txt, const float* rgba) {
    gfx::Color4fv(rgba);
    gfx::PushMatrix();
    gfx::Translatef((GLfloat)px, (GLfloat)py, 0);
    RenderBitmapText(txt, textAlign::left);
    gfx::PopMatrix();
}

static void DibujarRect(int x0, int y0, int x1, int y1, const float* rgba) {
    if (x1 <= x0 || y1 <= y0) return;
    gfx::Color4fv(rgba);
    float quad[12] = { (float)x0,(float)y0, (float)x1,(float)y0, (float)x1,(float)y1,
                       (float)x0,(float)y0, (float)x1,(float)y1, (float)x0,(float)y1 };
    gfx::VertexPointer2f(0, quad);
    gfx::DrawTrianglesArray(6);
}

// color de la linea segun su tag ([ERROR]/[WARN]/resto). El tag lo antepone el ring buffer.
static const float* ColorDeLinea(const char* linea) {
    if (linea[0] == '[') {
        if (linea[1] == 'E') return kRojoError;     // ERROR: rojo
        if (linea[1] == 'W') return kAmarilloWarn;  // WARN: amarillo
    }
    return ListaColores[static_cast<int>(ColorID::grisUI)]; // INFO: gris claro
}

void Console::Render() {
    const int glY = W3dPantallaAlto - y - height;

    // fondo (igual patron que los demas viewports)
    gfx::Enable(gfx::ScissorTest);
    gfx::Scissor(x, glY, width, height);
    const float* bg = ListaColores[static_cast<int>(ColorID::background)];
    gfx::ClearColor(bg[0], bg[1], bg[2], bg[3]);
    gfx::Clear(gfx::ColorBuffer | gfx::DepthBuffer);

    gfx::Viewport(x, glY, width, height);
    gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
    gfx::Ortho(0, width, height, 0, -1, 1);
    gfx::MatrixMode(gfx::ModelView); gfx::LoadIdentity();
    gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::Lighting);
    gfx::Disable(gfx::Fog); gfx::Disable(gfx::CullFace);
    gfx::Disable(gfx::Texture2D); gfx::Disable(gfx::Blend);
    gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::ColorArray);
    gfx::EnableArray(gfx::VertexArray);

    const int top = BarTopOffset();
    const int lh = (RenglonHeightGS > 0) ? RenglonHeightGS : 12;
    SyncFilterButtons();

    // ---- BANNER del ultimo error de lua (bien visible, arriba) ----------------
    const char* errLua = W3dScriptUltimoError();
    int bannerH = (errLua && errLua[0]) ? (lh + marginGS) : 0;
    if (bannerH > 0) {
        // fondo del banner ROJO (error) para que salte a la vista
        gfx::Disable(gfx::Texture2D);
        gfx::DisableArray(gfx::TexCoordArray);
        gfx::Color4fv(kRojoError);
        float bx0 = (float)borderGS, by0 = (float)(top);
        float bx1 = (float)(width - borderGS), by1 = (float)(top + bannerH);
        float quad[12] = { bx0,by0, bx1,by0, bx1,by1,  bx0,by0, bx1,by1, bx0,by1 };
        gfx::VertexPointer2f(0, quad);
        gfx::DrawTrianglesArray(6);
    }

    // ---- medir el contenido y recalcular el scroll SOLO si cambio -------------
    // (mismo espiritu que lastContentRows del Outliner: sin esto el scrollbar
    // quedaria viejo cuando el log crece sin que se redimensione el viewport)
    RebuildVisibleLines();
    const int count = (int)visibleLines.size();
    int maxLen = 0;
    for (int i = 0; i < count; i++) {
        int len = (int)visibleLines[(size_t)i].text.size();
        if (len > maxLen) maxLen = len;
    }
    // + un colchon a la derecha para que el final de la linea no quede tapado por la barra vertical
    int maxAncho = marginGS + borderGS + maxLen * LetterWidthGS + marginGS + GlobalScale * 9;
    if (count != lastCount || maxAncho != lastMaxAncho || bannerH != lastBannerH) {
        lastCount = count; lastMaxAncho = maxAncho; lastBannerH = bannerH;
        RecalcularScroll();
    }

    // ---- resaltado de seleccion (se dibuja debajo del texto) -------------------
    const int contentTop = top + bannerH;
    int hVis = height - contentTop; if (hVis < 0) hVis = 0;
    gfx::Scissor(x, glY, width, hVis);
    gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::TexCoordArray);
    const int selectionX = marginGS + borderGS + PosX;
    const int selectionY = contentTop + marginGS + PosY;
    const int charWidth = LetterWidthGS > 0 ? LetterWidthGS : 1;
    for (int i = 0; selectionActive && i < count; i++) {
        int begin = 0, end = 0;
        if (!SelectionBoundsForRow(i, begin, end)) continue;
        int py = selectionY + i * lh;
        if (py + lh <= contentTop || py >= height) continue;
        DibujarRect(selectionX + begin * charWidth, py,
                    selectionX + end * charWidth, py + lh, kSeleccion);
    }

    // ---- TEXTO (fuente): bind del atlas + blend, como el Outliner --------------
    gfx::BindTexture(Textures[0]->iID);
    gfx::EnableArray(gfx::TexCoordArray);
    gfx::Enable(gfx::Texture2D);
    gfx::Enable(gfx::Blend);
    gfx::BlendAlpha();
#ifndef W3D_SYMBIAN
    gfx::TexFilter(false);
#endif

    // el texto del banner (sobre el fondo rojo): negro para contraste
    if (bannerH > 0) {
        gfx::Scissor(x, glY, width, height);
        std::string linea = "Lua: ";
        linea += errLua;
        DibujarLinea(marginGS + borderGS, top + (marginGS / 2), linea,
                     ListaColores[static_cast<int>(ColorID::negro)]);
    }

    // scissor SOLO al area de contenido: al scrollear, las lineas no se pisan
    // con la barra de botones ni con el banner
    gfx::Scissor(x, glY, width, hVis);
    int px = marginGS + borderGS + PosX;
    int py0 = contentTop + marginGS + PosY;
    for (int i = 0; i < count; i++) {
        int py = py0 + i * lh;
        if (py + lh <= contentTop) continue; // arriba del area visible
        if (py >= height) break;             // abajo del area visible
        const std::string& line = visibleLines[(size_t)i].text;
        DibujarLinea(px, py, line, ColorDeLinea(line.c_str()));
    }

    gfx::Disable(gfx::ScissorTest);

    RenderBar();
    DibujarBordes(this);
    DibujarScrollbar(this); // barras v/h (mismo dibujo/agarre que el Outliner)
#ifdef W3D_SYMBIAN
    gfx::EnableArray(gfx::NormalArray); // baseline que asume la escena
#endif
}

// agarre de la barra en el down (patron del Outliner; el ruteo compartido de
// LayoutInput ya calcula mouseOverScrollY/X con la zona del borde del panel)
void Console::button_left() {
    if (mouseOverScrollY) mouseOverScrollYpress = true;
    if (mouseOverScrollX) mouseOverScrollXpress = true;
    if (mouseOverScrollY || mouseOverScrollX) return;
    if (lastMouseY < y + BarTopOffset() + lastBannerH + marginGS) return;
    RebuildVisibleLines();
    selectionActive = !visibleLines.empty();
    selectionDragging = selectionActive;
    if (selectionActive) {
        const int lh = (RenglonHeightGS > 0) ? RenglonHeightGS : 12;
        const int contentTop = BarTopOffset() + lastBannerH;
        selectionAnchorRow = (lastMouseY - y - contentTop - marginGS - PosY) / lh;
        if (selectionAnchorRow < 0) selectionAnchorRow = 0;
        if (selectionAnchorRow >= (int)visibleLines.size())
            selectionAnchorRow = (int)visibleLines.size() - 1;
        SelectionPosition(lastMouseX, lastMouseY,
                          selectionAnchorRow, selectionAnchorCol);
        UpdateSelection(lastMouseX, lastMouseY);
    }
}

// arrastrar con un boton (izq o medio) sobre el CONTENIDO = scroll 1:1, como el
// touch. La scrollbar agarrada NO pasa por aca (LayoutMotionUI consume ese motion),
// y el drag de la barra superior tampoco (gesto lockeado en controles.cpp).
void Console::event_mouse_motion(int mx, int my) {
    if (selectionDragging && leftMouseDown) {
        ViewPortClickDown = true;
        const int edgeStep = (GlobalScale > 0 ? GlobalScale : 1) * 4;
        const int leftEdge = x + borderGS + marginGS;
        const int rightEdge = x + width - borderGS -
                              (scrollY ? 9 * GlobalScale : marginGS);
        const int topEdge = y + BarTopOffset() + lastBannerH + marginGS;
        const int bottomEdge = y + height - borderGS - marginGS;
        int scrollDeltaX = mx < leftEdge ? edgeStep : (mx > rightEdge ? -edgeStep : 0);
        int scrollDeltaY = my < topEdge ? edgeStep : (my > bottomEdge ? -edgeStep : 0);
        if (scrollDeltaX || scrollDeltaY)
            ScrollByTouch(scrollDeltaX, scrollDeltaY);
        UpdateSelection(mx, my);
    } else if (leftMouseDown || middleMouseDown) {
        ViewPortClickDown = true; // el drag congela el foco por hover hasta soltar
        // delta fresco contra el ultimo punto guardado (GuardarMousePos en el down;
        // CheckWarpMouseInViewport lo actualiza despues de cada motion)
        ScrollByTouch(mx - lastMouseX, my - lastMouseY);
        g_redraw = true;
    }
}

// IMPRESCINDIBLE (como en todos los viewports): soltar libera ViewPortClickDown.
// Sin esto, cualquier click sobre la consola dejaba el foco por hover CLAVADO aca
// (las teclas seguian viniendo a la consola aunque muevas el mouse a otro viewport).
void Console::mouse_button_up(int boton) {
    ViewPortClickDown = false;
    if (boton == W3dMB_IZQ) {
        selectionDragging = false;
        mouseOverScrollYpress = false;
        mouseOverScrollXpress = false;
    }
}

void Console::event_key_down(int tecla, bool repeticion) {
    (void)repeticion;
    RebuildVisibleLines();
    if (LCtrlPressed && tecla == W3dK_C) CopySelection();
    else if (LCtrlPressed && tecla == W3dK_A && !visibleLines.empty()) {
        selectionActive = true;
        selectionAnchorRow = 0;
        selectionAnchorCol = 0;
        selectionFocusRow = (int)visibleLines.size() - 1;
        selectionFocusCol = (int)visibleLines.back().text.size();
        g_redraw = true;
    } else if (tecla == W3dK_ESCAPE) {
        selectionActive = false;
        g_redraw = true;
    }
}

#ifndef W3D_SYMBIAN
void Console::event_mouse_wheel(float dy, int mx, int my) {
    if (BarScrollHorizontal(mx, my, (int)(dy * 40))) return; // sobre la barra superior -> scroll de la barra
    // rueda = scroll vertical (arriba = mirar hacia atras en el historial);
    // shift+rueda = horizontal (lineas largas). ScrollByTouch clampea y, al
    // volver al fondo, el autoscroll se re-engancha solo (PosY == MaxPosY).
    int paso = (int)(dy * 6 * GlobalScale);
    if (LShiftPressed) ScrollByTouch(paso, 0);
    else               ScrollByTouch(0, paso);
    g_redraw = true;
}
#endif

// TOUCH: arrastrar 1 dedo sobre el contenido = scroll (v/h), como el Outliner
bool Console::event_finger_scroll(int px, int py, int dx, int dy) {
    (void)px; (void)py;
    ScrollByTouch(dx, dy);
    g_redraw = true;
    return true;
}
