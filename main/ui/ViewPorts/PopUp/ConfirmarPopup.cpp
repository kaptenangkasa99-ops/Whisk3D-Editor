#include "w3dGraphics.h"            // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include "ConfirmarPopup.h"
#include "ViewPorts/LayoutInput.h"  // LayoutKey
#include "WhiskUI/core/UI.h"             // metricas (RenglonHeightGS/borderGS/marginGS/gapGS/...) + RenderBitmapText
#include "WhiskUI/text/bitmapText.h"
#include "WhiskUI/widgets/Button.h"
#include "objects/Textures.h"
#include "objects/ObjectMode.h"     // Eliminar
#include "variables.h"              // InteractionMode, ObjActivo, ObjSelects
#include <cstdio>

ConfirmarPopup* confirmarPopup = NULL;

static const float ROJO_BORRAR[4] = { 0.92f, 0.26f, 0.26f, 1.0f }; // rojo del boton de borrar (fijo: la paleta no tiene un rojo claro)

ConfirmarPopup::ConfirmarPopup() : PopUpBase("Confirmar") {
    onSi = NULL;
    onExtra = NULL;
    tresOpciones = false;
    foco = 1; // por defecto resaltado en "Si" (la accion principal)
    btnSi = new Button(T("Yes"));
    btnNo = new Button(T("No"));
    btnExtra = new Button(T("Discard"));
    btnSi->adaptar = false; btnSi->centrado = true;
    btnNo->adaptar = false; btnNo->centrado = true;
    btnExtra->adaptar = false; btnExtra->centrado = true;
    btnSi->colorTexto = ROJO_BORRAR; // borrar = ROJO
}

ConfirmarPopup::~ConfirmarPopup() {
    delete btnSi; delete btnNo; delete btnExtra;
}

void ConfirmarPopup::Abrir(const std::string& msg, void (*cb)()) {
    mensaje = msg;
    onSi = cb;
    onExtra = NULL;
    tresOpciones = false;
    btnNo->text = T("No");
    btnSi->text = T("Yes");
    btnSi->colorTexto = ROJO_BORRAR;
    foco = 1;
    PopUpActive = this; // el layout (ancho/posicion) se calcula en Render con las metricas ACTUALES
}

void ConfirmarPopup::AbrirTresOpciones(const std::string& msg, const std::string& textoExtra,
                                      void (*cbExtra)(), const std::string& textoSi,
                                      void (*cbSi)()) {
    mensaje = msg;
    onExtra = cbExtra;
    onSi = cbSi;
    tresOpciones = true;
    btnNo->text = T("Cancel");
    btnExtra->text = textoExtra;
    btnSi->text = textoSi;
    btnSi->colorTexto = NULL;
    foco = 2;
    PopUpActive = this;
}

// parte 's' en renglones de <= maxChars, cortando en un espacio si puede (la fuente es monoespaciada).
// SIN limite de renglones: el mensaje entra completo (por eso el popup crece en alto).
static void WrapMensaje(const std::string& s, int maxChars, std::vector<std::string>& out) {
    out.clear();
    if (maxChars < 1) maxChars = 1;
    size_t pos = 0;
    while (pos < s.size()) {
        while (pos < s.size() && s[pos] == ' ') pos++;      // saltar espacios
        if (pos >= s.size()) break;
        if ((int)(s.size() - pos) <= maxChars) { out.push_back(s.substr(pos)); break; }
        size_t brk = s.rfind(' ', pos + maxChars);           // espacio mas a la derecha que entre
        if (brk != std::string::npos && brk > pos) { out.push_back(s.substr(pos, brk - pos)); pos = brk + 1; }
        else { out.push_back(s.substr(pos, maxChars)); pos += maxChars; } // palabra sin espacios: partir
    }
    if (out.empty()) out.push_back(s);
}

// BARRA al fondo, ANCHO COMPLETO de la pantalla (simula los soft keys de Symbian). Se recalcula cada frame
// para adaptarse al tamaño/orientacion actual (y porque al abrir las metricas pueden no estar listas).
// padding UNIFORME en los 4 lados (el contenido = N renglones del mensaje + gap + fila de botones)
int ConfirmarPopup::Padding() const { return borderGS + GlobalScale * 3; } // borde + ~3px de aire (antes marginGS+borderGS = demasiado)
int ConfirmarPopup::LineHeight() const { return LetterHeightGS + GlobalScale * 3; } // renglon de mensaje (con aire)
int ConfirmarPopup::AltoBarra()  const {
    int nL = lineas.empty() ? 1 : (int)lineas.size();       // el mensaje ocupa N renglones (word-wrap)
    return Padding() * 2 + nL * LineHeight() + gapGS + (RenglonHeightGS + bordersGS);
}

void ConfirmarPopup::Layout() {
    // ancho del dialogo primero (independiente del alto), para saber cuantos caracteres entran por renglon
#ifdef W3D_SYMBIAN
    int w = MenuPantallaW; // barra al FONDO, ancho COMPLETO (ahi estan los soft keys)
#else
    int w = MenuPantallaW / 2;
    if (w < 200) w = MenuPantallaW; // pantallas muy chicas: ancho completo
#endif
    // word-wrap del mensaje al ancho INTERIOR (w - padding a ambos lados). Asi nunca se corta.
    int cw = (CharacterWidthGS > 0) ? CharacterWidthGS : 1;
    int maxChars = (w - Padding() * 2) / cw;
    WrapMensaje(mensaje, maxChars, lineas);
    int h = AltoBarra(); // usa lineas.size() (ya calculado)
    popUpWindow->Resize(w, h);
#ifdef W3D_SYMBIAN
    x = 0;
    y = MenuPantallaH - h;
#else
    x = (MenuPantallaW - w) / 2;
    y = (MenuPantallaH - h) / 2;
#endif
}

void ConfirmarPopup::Render() {
    Layout();
    initView();
    w3dEngine::BindTexture(Textures[0]->iID);

    const float* fondo = ListaColores[static_cast<int>(ColorID::background)];
    const float* blanco = ListaColores[static_cast<int>(ColorID::blanco)];
    const float* verde = ListaColores[static_cast<int>(ColorID::accent)];

    // fondo + BORDE VERDE
    w3dEngine::Color4f(fondo[0], fondo[1], fondo[2], 1.0f);
    popUpWindow->Render(false);
    w3dEngine::Color4f(verde[0], verde[1], verde[2], 1.0f);
    popUpWindow->RenderBorder(false);

    int w = popUpWindow->width;
    int pad = Padding(); // mismo padding arriba/abajo/lados

    // mensaje, centrado, en N renglones (word-wrap) apilados desde arriba
    int lineH = LineHeight();
    int ty = pad;
    for (size_t i = 0; i < lineas.size(); i++) {
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)pad, (GLfloat)ty, 0);
        w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
        RenderBitmapText(lineas[i].c_str(), textAlign::center, w - pad * 2);
        w3dEngine::PopMatrix();
        ty += lineH;
    }

    // botones debajo del mensaje
    int izq = pad;
    int contW = w - pad * 2;
    int nL = lineas.empty() ? 1 : (int)lineas.size();
    int btnY = pad + nL * lineH + gapGS;

    int columnas = tresOpciones ? 3 : 2;
    int anchoBoton = (contW - gapGS * (columnas - 1)) / columnas;
    btnNo->Resize(anchoBoton);
    btnSi->Resize(anchoBoton); // Si queda ROJO siempre (colorTexto)
    if (tresOpciones) btnExtra->Resize(anchoBoton);
    btnNo->focoMenu = tresOpciones && foco == 0;
    btnExtra->focoMenu = tresOpciones && foco == 1;
    btnSi->focoMenu = tresOpciones && foco == 2;

    Button* botones[3] = { btnNo, btnExtra, btnSi };
    for (int col = 0; col < columnas; col++) {
        Button* boton = tresOpciones ? botones[col] : (col == 0 ? btnNo : btnSi);
        int bx = izq + col * (anchoBoton + gapGS);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)bx, (GLfloat)btnY, 0);
        boton->Render();
        w3dEngine::PopMatrix();
        boton->sx = x + bx; boton->sy = y + btnY;
    }

    endView();
}

bool ConfirmarPopup::Motion(int mx, int my) {
    Layout();
    btnNo->hover = btnNo->Contains(mx, my); // ilumina el boton al pasar el mouse (igual que cualquier boton)
    btnSi->hover = btnSi->Contains(mx, my);
    if (tresOpciones) btnExtra->hover = btnExtra->Contains(mx, my);
    return true;
}

bool ConfirmarPopup::Click(int mx, int my) {
    Layout(); // asegura posicion/tamaño actuales para el hit-test
    if (btnNo->Contains(mx, my)) { Cerrar(); return true; }
    if (tresOpciones && btnExtra->Contains(mx, my)) {
        void (*cb)() = onExtra; Cerrar(); if (cb) cb(); return true;
    }
    if (btnSi->Contains(mx, my)) { void (*cb)() = onSi; Cerrar(); if (cb) cb(); return true; }
    // click adentro de la barra (sin botón): lo consume (modal); afuera -> el caller cierra
    return Contains(mx, my);
}

bool ConfirmarPopup::Tecla(int tecla) {
    if (tresOpciones) {
        if (tecla == LayoutKey::Left || tecla == LayoutKey::Right) {
            foco = (foco + (tecla == LayoutKey::Right ? 1 : 2)) % 3;
            return true;
        }
        if (tecla == LayoutKey::Cancel) { Cerrar(); return true; }
        if (tecla == LayoutKey::Enter || tecla == LayoutKey::Accept) {
            void (*cb)() = (foco == 1) ? onExtra : (foco == 2 ? onSi : NULL);
            Cerrar(); if (cb) cb(); return true;
        }
        return true;
    }
    switch (tecla) {
        case LayoutKey::Right:   // flecha derecha / soft key derecho = Si
        case LayoutKey::Enter:   // ENTER (teclado PC) = Si: el boton de borrar es el default resaltado
        case LayoutKey::Accept:  // OK / soft key derecho (Symbian) = Si (borrar)
            { void (*cb)() = onSi; Cerrar(); if (cb) cb(); } return true;
        case LayoutKey::Left:    // flecha izquierda / soft key izquierdo = No
        case LayoutKey::Cancel:  // Esc / C = No (cancela)
            Cerrar(); return true;
    }
    return true; // modal: no le roban teclas
}

// ----------------------------------------------------------------

static void ConfirmarBorradoSi()    { Eliminar(false); } // el borrado real (con su undo) al confirmar
static void ConfirmarBorradoSiCol() { Eliminar(true); }  // desde el outliner: incluye colecciones

void AbrirConfirmarBorrado(bool incluirCollecciones) {
    if (InteractionMode != ObjectMode) return;          // Edit Mode tiene su propio menu Delete
    if (!HayObjetosSeleccionados(incluirCollecciones)) return;

    int n = 0; Object* uno = NULL;
    for (size_t i = 0; i < ObjSelects.size(); i++)
        if (ObjSelects[i] && ObjSelects[i]->select) { n++; uno = ObjSelects[i]; }

    char buf[128];   // snprintf: el nombre lo tipea el usuario (sprintf desbordaba con nombres largos)
    if (n == 1 && uno) snprintf(buf, sizeof(buf), T("Delete \"%s\"?"), uno->name.c_str());
    else if (n > 1)    snprintf(buf, sizeof(buf), T("Delete %d objects?"), n);
    else               snprintf(buf, sizeof(buf), "%s", T("Delete selected?")); // ej. una coleccion (no esta en ObjSelects)

    if (!confirmarPopup) confirmarPopup = new ConfirmarPopup();
    confirmarPopup->Abrir(buf, incluirCollecciones ? ConfirmarBorradoSiCol : ConfirmarBorradoSi);
}
