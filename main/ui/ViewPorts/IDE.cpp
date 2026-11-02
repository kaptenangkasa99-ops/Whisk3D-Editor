#include "ViewPorts/IDE.h"
#include "w3dGraphics.h"                 // w3dEngine (abstraccion grafica)
#include "WhiskUI/draw/glesdraw.h"       // W3dPantallaAlto
#include "WhiskUI/theme/colores.h"       // ListaColores / ColorID
#include "WhiskUI/text/bitmapText.h"     // RenderBitmapText
#include "W3dClipboard.h"                // portapapeles del sistema (Core): copiar/pegar con navegador/notas
#include "WhiskUI/core/UI.h"             // marginGS / RenglonHeightGS / LetterWidthGS / GlobalScale / borderGS
#include "objects/Textures.h"            // Textures[0] = atlas de la fuente
#include "render/OpcionesRender.h"       // g_redraw (render event-driven)
#include "script/W3dScript.h"            // W3dScriptDatos (rutas de scripts de los objetos)
#include "ViewPorts/Notificaciones.h"    // toasts de guardado / refresh
#include "ViewPorts/Properties.h"        // invalidar propiedades expuestas por scripts guardados
#include "w3dFilesystem.h"               // ReadTextFile / ListDir (colector + abrir)
#include "io/W3dContenedor.h"            // los .lua del proyecto v4 viven ADENTRO del .w3d
#include "W3dLang.h"                     // T() para los textos de la barra
#include "variables.h"                   // w3dPath (el proyecto abierto)
#include <stdio.h>                       // guardado atomico: fopen/fwrite/rename
#include <string.h>
#include <filesystem>
#ifdef __ANDROID__
#include <SDL.h>
extern bool g_uiTapEnCurso;
#endif

namespace gfx = w3dEngine;

#ifdef __ANDROID__
static void IdeMostrarTecladoAndroid() { SDL_StartTextInput(); }
static void IdeOcultarTecladoAndroid() { SDL_StopTextInput(); }
#else
static void IdeMostrarTecladoAndroid() {}
static void IdeOcultarTecladoAndroid() {}
#endif

// ---------------------------------------------------------------------------
//  Colores del RESALTADO (fijos, legibles sobre el fondo oscuro; la paleta del
//  tema no tiene roles de sintaxis, mismo criterio que los [ERROR] de Console)
// ---------------------------------------------------------------------------
static const float kIdeComentario[4] = { 0.52f, 0.66f, 0.50f, 1.0f }; // verde grisado
static const float kIdeString[4]     = { 0.93f, 0.78f, 0.38f, 1.0f }; // amarillo/naranja
static const float kIdeKeyword[4]    = { 0.52f, 0.72f, 0.98f, 1.0f }; // azul
static const float kIdeNumero[4]     = { 0.55f, 0.88f, 0.94f, 1.0f }; // celeste
static const float kIdeSeleccion[4]  = { 0.22f, 0.36f, 0.55f, 1.0f }; // fondo de la seleccion
static const float kIdeAutocompletadoFondo[4] = { 0.055f, 0.065f, 0.08f, 1.0f };
static const float kIdeAutocompletadoBorde[4] = { 0.32f, 0.38f, 0.46f, 1.0f };

// clipboard INTERNO: respaldo si el clipboard de SDL no esta (Symbian / headless)
static std::string gIDEClipInterno;

// ---------------------------------------------------------------------------
//  helpers chicos (C++03, sin to_string)
// ---------------------------------------------------------------------------
static std::string IdeNum(int v) {
    char buf[16];
    sprintf(buf, "%d", v);
    return std::string(buf);
}
static bool IdeTerminaEn(const std::string& s, const char* suf) {
    size_t ls = s.size(), lf = 0;
    while (suf[lf]) lf++;
    if (lf > ls) return false;
    for (size_t i = 0; i < lf; i++) {
        char a = s[ls - lf + i], b = suf[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a + 32); // comparar sin caso (.LUA)
        if (a != b) return false;
    }
    return true;
}
static std::string IdeNormalizar(const std::string& r) {
    std::string s = r;
    for (size_t i = 0; i < s.size(); i++) if (s[i] == '\\') s[i] = '/';
    return s;
}
// las dos rutas nombran el MISMO archivo? (exacta, o una es sufijo de la otra
// con borde de '/': "contenido/juego.lua" == "/abs/proj/contenido/juego.lua")
static bool IdeMismoArchivo(const std::string& a, const std::string& b) {
    std::string na = IdeNormalizar(a), nb = IdeNormalizar(b);
    if (na == nb) return true;
    if (na.size() > nb.size()) {
        return nb.size() > 0 && na.size() > nb.size() &&
               na.compare(na.size() - nb.size(), nb.size(), nb) == 0 &&
               na[na.size() - nb.size() - 1] == '/';
    }
    if (nb.size() > na.size()) {
        return na.size() > 0 &&
               nb.compare(nb.size() - na.size(), na.size(), na) == 0 &&
               nb[nb.size() - na.size() - 1] == '/';
    }
    return false;
}
static std::string IdeCarpetaProyecto() {
    if (w3dPath.empty()) return std::string();
    size_t s = w3dPath.find_last_of("/\\");
    return (s == std::string::npos) ? std::string(".") : w3dPath.substr(0, s);
}

std::string IDENombreScript(const std::string& ruta) {
    std::string r = IdeNormalizar(ruta);
    std::string dir = IdeNormalizar(IdeCarpetaProyecto());
    if (!dir.empty() && r.size() > dir.size() + 1 &&
        r.compare(0, dir.size(), dir) == 0 && r[dir.size()] == '/')
        return r.substr(dir.size() + 1);        // relativo al proyecto
    size_t s = r.find_last_of('/');
    return (s == std::string::npos) ? r : r.substr(s + 1); // solo el nombre
}

// ---------------------------------------------------------------------------
//  COLECTOR de scripts del proyecto: los .lua referenciados por las escenas
//  cargadas (scriptDatos de todo el arbol, en orden de arbol) + los *.lua de
//  contenido/. Sin repetidos (comparados con IdeMismoArchivo). No se reusa el
//  colector de GuardarVersion porque ese re-parsea el .w3d del DISCO; aca el
//  arbol vivo ya tiene las rutas resueltas (y cubre lo editado sin guardar).
// ---------------------------------------------------------------------------
static void IdeAgregarRuta(const std::string& ruta, std::vector<std::string>* out) {
    if (!IdeTerminaEn(ruta, ".lua")) return; // .luac (bytecode) no se edita como texto
    for (size_t i = 0; i < out->size(); i++)
        if (IdeMismoArchivo((*out)[i], ruta)) return;
    out->push_back(ruta);
}
static void IdeColectarEscena(Object* o, std::vector<std::string>* out) {
    if (!o) return;
    if (o->scriptDatos)
        for (size_t i = 0; i < o->scriptDatos->scripts.size(); i++)
            IdeAgregarRuta(o->scriptDatos->scripts[i].ruta, out);
    for (size_t i = 0; i < o->Childrens.size(); i++)
        IdeColectarEscena(o->Childrens[i], out);
}
void IDEColectarScripts(std::vector<std::string>* rutas) {
    rutas->clear();
    IdeColectarEscena(SceneCollection, rutas);
    // DE DONDE SALEN LOS .lua QUE NO CUELGAN DE NINGUN OBJETO:
    //  - proyecto v4 (el .w3d es un contenedor): de la carpeta scripts/ de ADENTRO.
    //    Ya no se escanea contenido/ en el disco, que con el contenedor no existe
    //    (y si existe es la carpeta VIEJA del proyecto v3 que quedo al lado: listarla
    //    mostraba scripts fantasma que no son los que el proyecto usa).
    //  - proyecto v3 (JSON plano con los assets sueltos): contenido/, como siempre.
    if (W3dContenedorHayMontado()) {
        std::vector<std::string> ents;
        W3dContenedorListarCarpeta("scripts/", ents);
        for (size_t i = 0; i < ents.size(); i++)
            if (IdeTerminaEn(ents[i], ".lua")) IdeAgregarRuta(ents[i], rutas);
        return;
    }
    std::string dir = IdeCarpetaProyecto();
    if (dir.empty()) return;
    std::vector<std::string> carpetas;
    carpetas.push_back(dir + "/scripts"); // v5 loose-folder project
    carpetas.push_back(dir + "/contenido"); // v3 compatibility
    for (size_t c = 0; c < carpetas.size(); c++) {
        std::vector<w3dFileSystem::DirEntry> ents;
        if (!w3dFileSystem::ListDir(carpetas[c], ents)) continue;
        for (size_t i = 0; i < ents.size(); i++)
            if (!ents[i].isDir && IdeTerminaEn(ents[i].name, ".lua"))
                IdeAgregarRuta(carpetas[c] + "/" + ents[i].name, rutas);
    }
}

// ---------------------------------------------------------------------------
//  LEXER lua por linea. Ver las LIMITACIONES en IDE.h: el estado entre lineas
//  es SOLO el comentario de bloque --[[ ]] (sin niveles [==[), y el string
//  largo [[...]] se pinta solo dentro de su linea.
// ---------------------------------------------------------------------------
static bool IdeEsIdent(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}
static bool IdeEsKeyword(const std::string& p) {
    static const char* kw[] = {
        "and", "break", "do", "else", "elseif", "end", "false", "for",
        "function", "goto", "if", "in", "local", "nil", "not", "or",
        "repeat", "return", "then", "true", "until", "while", 0 };
    for (int i = 0; kw[i]; i++) if (p == kw[i]) return true;
    return false;
}
static void IdeMarcar(std::vector<unsigned char>* tipos, size_t d, size_t h, unsigned char t) {
    if (!tipos) return;
    for (size_t i = d; i < h && i < tipos->size(); i++) (*tipos)[i] = t;
}
void IDE::LexLinea(const std::string& s, bool* enBloque, std::vector<unsigned char>* tipos) {
    const size_t n = s.size();
    if (tipos) tipos->assign(n, 0);
    bool bloque = enBloque ? *enBloque : false;
    size_t i = 0;
    while (i < n) {
        if (bloque) { // dentro de --[[ ... : comentario hasta el "]]"
            size_t fin = s.find("]]", i);
            if (fin == std::string::npos) { IdeMarcar(tipos, i, n, 1); i = n; }
            else { IdeMarcar(tipos, i, fin + 2, 1); i = fin + 2; bloque = false; }
            continue;
        }
        char c = s[i];
        if (c == '-' && i + 1 < n && s[i + 1] == '-') {           // comentario
            if (i + 3 < n && s[i + 2] == '[' && s[i + 3] == '[') { // --[[ (bloque)
                size_t fin = s.find("]]", i + 4);
                if (fin == std::string::npos) { IdeMarcar(tipos, i, n, 1); bloque = true; i = n; }
                else { IdeMarcar(tipos, i, fin + 2, 1); i = fin + 2; }
            } else { IdeMarcar(tipos, i, n, 1); i = n; }           // -- hasta el final
            continue;
        }
        if (c == '"' || c == '\'') {                               // string "..." '...'
            size_t j = i + 1;
            while (j < n) {
                if (s[j] == '\\') { j += 2; continue; }            // escape
                if (s[j] == c) { j++; break; }
                j++;
            }
            if (j > n) j = n;
            IdeMarcar(tipos, i, j, 2); i = j; continue;
        }
        if (c == '[' && i + 1 < n && s[i + 1] == '[') {            // [[...]] (misma linea)
            size_t fin = s.find("]]", i + 2);
            size_t j = (fin == std::string::npos) ? n : fin + 2;
            IdeMarcar(tipos, i, j, 2); i = j; continue;
        }
        if (c >= '0' && c <= '9' && (i == 0 || !IdeEsIdent(s[i - 1]))) { // numero
            size_t j = i;
            if (c == '0' && i + 1 < n && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
                j = i + 2;
                while (j < n && ((s[j] >= '0' && s[j] <= '9') ||
                                 (s[j] >= 'a' && s[j] <= 'f') || (s[j] >= 'A' && s[j] <= 'F'))) j++;
            } else {
                while (j < n && ((s[j] >= '0' && s[j] <= '9') || s[j] == '.' ||
                                 s[j] == 'e' || s[j] == 'E' ||
                                 ((s[j] == '+' || s[j] == '-') && (s[j - 1] == 'e' || s[j - 1] == 'E')))) j++;
            }
            IdeMarcar(tipos, i, j, 4); i = j; continue;
        }
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') { // palabra
            size_t j = i;
            while (j < n && IdeEsIdent(s[j])) j++;
            if (IdeEsKeyword(s.substr(i, j - i))) IdeMarcar(tipos, i, j, 3);
            i = j; continue;
        }
        i++;
    }
    if (enBloque) *enBloque = bloque;
}

void IDE::RecalcularBloques() {
    bloqueAlInicio.assign(lineas.size(), 0);
    bool b = false;
    for (size_t i = 0; i < lineas.size(); i++) {
        bloqueAlInicio[i] = b ? 1 : 0;
        LexLinea(lineas[i], &b, NULL); // solo avanza el estado
    }
}

struct IdeApiInfo {
    const char* nombre;
    const char* firma;
    const char* descripcion;
};

static const IdeApiInfo kIdeApi[] = {
    { "key", "key(name)", "True while a keyboard key is held." },
    { "keyPressed", "keyPressed(name)", "True on the frame a key is pressed." },
    { "buttonPressed", "buttonPressed(name)", "True on the frame a gamepad button is pressed." },
    { "stick", "stick(name)", "Returns the x and y values of a controller stick." },
    { "touch", "touch()", "Returns primary touch x, y, and active state." },
    { "mouse", "mouse()", "Returns mouse x, y, and active state." },
    { "finger", "finger(index)", "Returns touch x, y, and active state for a 1-based finger." },
    { "button", "button(name)", "Returns the current gamepad button state." },
    { "random", "random([min], [max])", "Returns a pseudo-random number in the requested range." },
    { "print", "print(...)", "Writes values to the engine log." },
    { "object", "object(name)", "Finds a script reference or scene object by name." },
    { "option", "option(name)", "Reads an option field from this script instance." },
    { "property", "property(name)", "Reads a property exposed by this script." },
    { "parameter", "parameter(name [, default])", "Reads a numeric property with an optional fallback." },
    { "shared", "shared(key)", "Reads a value shared between scripts." },
    { "setShared", "setShared(key, value)", "Stores a value shared between scripts." },
    { "changeScene", "changeScene(name [, restart])", "Switches to another project scene." },
    { "changeSceneAsync", "changeSceneAsync(name [, activateWhenReady [, callback]])", "Loads a scene asynchronously." },
    { "sceneAsyncReadyLoad", "sceneAsyncReadyLoad(name)", "Checks whether a scene has finished loading." },
    { "config", "config(key [, default])", "Reads a persistent setting or its fallback." },
    { "setConfig", "setConfig(key [, value])", "Stores a persistent setting." },
    { "saveConfig", "saveConfig()", "Writes persistent settings to disk." },
    { "loadConfig", "loadConfig()", "Loads persistent settings from disk." },
    { "silence", "silence(enabled)", "Enables or disables global audio mute." },
    { "isMuted", "isMuted()", "Returns whether global audio is muted." },
    { "info", "info(...)", "Writes an info-level log entry." },
    { "logInfo", "logInfo(...)", "Writes a warning-level log entry." },
    { "error", "error(...)", "Writes an error-level log entry." },
    { "debug", "debug(...)", "Writes a debug-level log entry." },
    { "esDebug", "esDebug()", "Returns whether debug logging is enabled." },
    { "logQuantity", "logQuantity(...)", "Writes a quantity/value log entry." },
    { "logLine", "logLine(...)", "Writes a line to the engine log." },
    { "type", "type(obj)", "Returns an object's type." },
    { "name", "name(obj)", "Returns an object's name." },
    { "Position", "Position(obj)", "Returns an object's world position as x, y, z." },
    { "SetPosition", "SetPosition(obj, x, y, z)", "Sets an object's absolute position." },
    { "mover", "mover(obj, dx, dy, dz)", "Moves an object by a relative offset." },
    { "rotation", "rotation(obj)", "Returns an object's rotation in degrees." },
    { "setRotation", "setRotation(obj, x, y, z)", "Sets an object's absolute rotation in degrees." },
    { "rotate", "rotate(obj, x, y, z)", "Rotates an object by relative angles in degrees." },
    { "rotateTowards", "rotateTowards(obj, dx, dz [, factor])", "Turns an object toward an X/Z direction." },
    { "scaleObject", "scaleObject(obj)", "Returns an object's scale as x, y, z." },
    { "setScale", "setScale(obj, sx, sy, sz)", "Sets an object's absolute scale." },
    { "scale", "scale(obj, factor)", "Applies a relative uniform scale." },
    { "visible", "visible(obj)", "Returns whether an object is visible." },
    { "setVisible", "setVisible(obj, visible)", "Sets an object's visibility." },
    { "animator", "animator(obj, animationId [, startFrame])", "Plays or queues a vertex animation." },
    { "pos3", "pos3(obj)", "Returns an object's position as a 3D vector." },
    { "setPos3", "setPos3(obj, x, y, z)", "Sets an object's position using x, y, z." },
    { "color", "color(light)", "Returns a light's normalized RGB tint." },
    { "setColor", "setColor(light, r, g, b)", "Sets a light's RGB tint." },
    { "energy", "energy(light)", "Returns a light's brightness." },
    { "setEnergy", "setEnergy(light, value)", "Sets a light's normalized brightness." },
    { "grupoVertices", "grupoVertices(obj, groupName)", "Accesses a named mesh vertex group." },
    { "verticePos", "verticePos(obj, groupName, index)", "Reads a vertex position from a group." },
    { "setVerticePos", "setVerticePos(obj, groupName, index, x, y, z)", "Sets a vertex position in a group." },
    { "setVerticeColor", "setVerticeColor(obj, groupName, index, r, g, b, a)", "Sets a vertex RGBA color." },
    { "setVertices", "setVertices(obj, groupName, data)", "Replaces vertex data for a mesh group." },
    { "screen", "screen()", "Returns the game UI canvas dimensions." },
    { "posPx", "posPx(obj)", "Returns a 2D object's position in canvas pixels." },
    { "setPosPx", "setPosPx(obj, x, y)", "Sets a 2D object's position in canvas pixels." },
    { "getScreenPx", "getScreenPx(obj)", "Returns a 2D object's size in pixels." },
    { "setScreenPx", "setScreenPx(obj, w, h)", "Sets a 2D object's size in pixels." },
    { "setText", "setText(obj, text)", "Sets the text of a UI text or button object." },
    { "setTextura", "setTextura(obj, path)", "Sets an image object's texture path." },
    { "show", "show(obj [, visible])", "Shows an object, or sets its visibility." },
    { "setOpacity", "setOpacity(obj, alpha)", "Sets UI opacity from 0 to 1." },
    { "setFontScreenSize", "setFontScreenSize(obj, size)", "Sets a text object's screen-size scale." },
    { "sound", "sound(path [, volume [, pitch [, loop]]])", "Plays a sound and returns its voice handle." },
    { "stopSound", "stopSound(handle [, fadeSec])", "Stops a sound handle, optionally fading it out." },
    { "beep", "beep(frequency, ms [, volume])", "Plays a synthesized tone." },
    { "isPressed", "isPressed(obj)", "True for one frame when a UI object is pressed." },
    { "isDown", "isDown(obj)", "True while a UI object is held." },
    { "isColliding", "isColliding(a, b)", "Tests overlap between two UI/game regions." },
    { "clamp", "clamp(value, min, max)", "Clamps a number to a range." },
    { "isInside", "isInside(area [, x [, y]])", "Tests whether a point is inside an area." },
    { "getScale", "getScale(obj)", "Returns a UI object's visual scale." },
    { "getSafeArea", "getSafeArea()", "Returns the screen safe-area rectangle." },
    { "fade", "fade(obj, ...)", "Applies a UI fade or transition." },
    { "getUIBox", "getUIBox(obj)", "Returns the rendered UI box as x, y, width, height." },
    { "screenOf", "screenOf(obj)", "Returns screen-space data for a UI object." },
    { "setRail", "setRail(obj, railName)", "Assigns a camera rail to an object." },
    { "setRailNode", "setRailNode(obj, nodeIndex)", "Moves an object to a rail node." },
    { "setRailLookAt", "setRailLookAt(obj, target)", "Sets the look-at target for a rail." },
    { "railOf", "railOf(obj)", "Returns an object's assigned rail." },
    { "lensOf", "lensOf(obj)", "Returns an object's camera lens data." },
    { "setLens", "setLens(obj, lensData)", "Sets an object's camera lens data." },
    { "cameraXZ", "cameraXZ(obj)", "Returns camera X/Z orientation data." },
    { "target", "target(obj)", "Returns camera or tracking target data." },
    { "controllers", "controllers()", "Returns the number of connected controllers." },
    { "controller", "controller(index)", "Returns the name and type of a 1-based controller." },
    { "instantiate", "instantiate(name [, x, y, z])", "Clones a project prefab at an optional position." },
    { "setSector", "setSector(obj [, sector])", "Selects a 1-based PVS sector; zero shows the full mesh." },
    { "visibilityCell", "visibilityCell(obj)", "Returns the active visibility cell." },
    { "setVisibilityAnchor", "setVisibilityAnchor(zone, object)", "Sets the object used to select a visibility cell." },
    { "setVisibilityCurve", "setVisibilityCurve(zone, t [, rail])", "Sets visibility progress along a curve." },
    { "setCollection", "setCollection(collection, mode)", "Enables or disables static collection batching." },
    { "emitter", "emitter(obj [, count])", "Emits a particle burst immediately." },
    { "quot", "quot()", "Exits the current game." },
    { "velocity", "velocity(obj [, vx, vy [, vz]])", "Reads or sets an object's physics velocity." },
    { "accelerate", "accelerate(obj, factor [, cap])", "Scales velocity and returns the resulting speed." },
    { "collisionBox", "collisionBox(obj [, width, height [, depth]])", "Reads or sets an object's collision box." },
    { "bounce", "bounce(obj, other [, ...])", "Bounces an object against overlapping objects." },
    { "bounceWithin", "bounceWithin(obj, area [, sides])", "Bounces within an area or numeric bounds." },
    { "bounceInside", "bounceInside(obj, area)", "Bounces inside all sides of an area." }
};

static const IdeApiInfo* IdeBuscarApi(const std::string& nombre) {
    for (size_t i = 0; i < sizeof(kIdeApi) / sizeof(kIdeApi[0]); i++)
        if (nombre == kIdeApi[i].nombre) return &kIdeApi[i];
    return NULL;
}

static size_t IdeEspaciosIniciales(const std::string& linea) {
    size_t i = 0;
    while (i < linea.size() && (linea[i] == ' ' || linea[i] == '\t')) i++;
    return i;
}

static size_t IdeAbrirCorcheteLargo(const std::string& linea, size_t inicio,
                                    int* nivelesIgual) {
    if (inicio >= linea.size() || linea[inicio] != '[') return std::string::npos;
    size_t i = inicio + 1;
    while (i < linea.size() && linea[i] == '=') i++;
    if (i >= linea.size() || linea[i] != '[') return std::string::npos;
    *nivelesIgual = (int)(i - inicio - 1);
    return i + 1;
}

static size_t IdeCerrarCorcheteLargo(const std::string& linea, size_t inicio,
                                     int nivelesIgual) {
    std::string cierre = "]";
    cierre.append((size_t)nivelesIgual, '=');
    cierre += "]";
    const size_t fin = linea.find(cierre, inicio);
    return fin == std::string::npos ? fin : fin + cierre.size();
}

static void IdeIndentPalabras(const std::string& linea, int* comentarioEq,
                              int* cadenaEq, std::vector<std::string>* palabras,
                              bool* protegerLinea) {
    palabras->clear();
    *protegerLinea = *comentarioEq >= 0 || *cadenaEq >= 0;
    size_t i = 0;
    while (i < linea.size()) {
        if (*comentarioEq >= 0 || *cadenaEq >= 0) {
            const bool comentario = *comentarioEq >= 0;
            const int nivelesIgual = comentario ? *comentarioEq : *cadenaEq;
            const size_t fin = IdeCerrarCorcheteLargo(linea, i, nivelesIgual);
            if (fin == std::string::npos) return;
            i = fin;
            if (comentario) *comentarioEq = -1;
            else *cadenaEq = -1;
            if (*protegerLinea) return;
            continue;
        }
        if (linea[i] == '-' && i + 1 < linea.size() && linea[i + 1] == '-') {
            int nivelesIgual = 0;
            const size_t inicioComentario = IdeAbrirCorcheteLargo(linea, i + 2, &nivelesIgual);
            if (inicioComentario != std::string::npos) {
                *comentarioEq = nivelesIgual;
                i = inicioComentario;
                continue;
            }
            return;
        }
        if (linea[i] == '"' || linea[i] == '\'') {
            const char delimiter = linea[i++];
            while (i < linea.size()) {
                if (linea[i] == '\\') { i += (i + 1 < linea.size()) ? 2 : 1; continue; }
                if (linea[i++] == delimiter) break;
            }
            continue;
        }
        int nivelesIgual = 0;
        const size_t inicioCadena = IdeAbrirCorcheteLargo(linea, i, &nivelesIgual);
        if (inicioCadena != std::string::npos) {
            *cadenaEq = nivelesIgual;
            i = inicioCadena;
            continue;
        }
        if ((linea[i] >= 'a' && linea[i] <= 'z') ||
            (linea[i] >= 'A' && linea[i] <= 'Z') || linea[i] == '_') {
            const size_t inicio = i++;
            while (i < linea.size() && IdeEsIdent(linea[i])) i++;
            const std::string palabra = linea.substr(inicio, i - inicio);
            if (IdeEsKeyword(palabra)) palabras->push_back(palabra);
            continue;
        }
        i++;
    }
}

static std::string IdeDetalleApi(const std::string& nombre) {
    const IdeApiInfo* info = IdeBuscarApi(nombre);
    if (!info) return std::string();
    std::string firma(info->firma);
    const size_t inicioArgs = firma.find('(');
    const size_t nombreLen = strlen(info->nombre);
    if (inicioArgs != std::string::npos && inicioArgs >= nombreLen)
        firma.erase(0, nombreLen);
    return firma + " - " + info->descripcion;
}

static std::string IdeTruncar(const std::string& texto, size_t maximo) {
    if (texto.size() <= maximo) return texto;
    if (maximo <= 3) return texto.substr(0, maximo);
    return texto.substr(0, maximo - 3) + "...";
}

struct IdeAutocompletadoLayout {
    int x, y, listW, detailW, width, height;
    std::vector<std::string> detailLines;
};

static void IdeEnvolverTexto(const std::string& texto, size_t maxChars,
                             std::vector<std::string>* lineas) {
    lineas->clear();
    if (maxChars == 0) return;
    size_t inicio = 0;
    while (inicio < texto.size()) {
        size_t fin = inicio + maxChars;
        if (fin < texto.size()) {
            size_t espacio = texto.rfind(' ', fin);
            if (espacio != std::string::npos && espacio >= inicio)
                fin = espacio;
        }
        if (fin == inicio) fin = inicio + maxChars;
        lineas->push_back(texto.substr(inicio, fin - inicio));
        inicio = fin;
        while (inicio < texto.size() && texto[inicio] == ' ') inicio++;
    }
    if (lineas->empty()) lineas->push_back(std::string());
}

static IdeAutocompletadoLayout IdeCalcularAutocompletadoLayout(
        const IDE* ide, int letraW, int lh, int gutterW, int top) {
    IdeAutocompletadoLayout layout = { 0, 0, 0, 0, 0, 0, std::vector<std::string>() };
    if (!ide || ide->autoCompleteItems.empty() || letraW <= 0 || lh <= 0) return layout;

    const int availableW = ide->width - gutterW - 2 * marginGS;
    const int availableChars = availableW > 0 ? availableW / letraW : 1;
    int maxNameChars = 1;
    for (size_t i = 0; i < ide->autoCompleteItems.size(); i++) {
        int chars = (int)ide->autoCompleteItems[i].size();
        if (chars > maxNameChars) maxNameChars = chars;
    }
    const int maxListChars = availableChars > 12 ? availableChars / 3 : availableChars;
    if (maxNameChars > maxListChars) maxNameChars = maxListChars;
    if (maxNameChars < 1) maxNameChars = 1;
    layout.listW = maxNameChars * letraW + 2 * marginGS;

    const std::string nombre = ide->autoCompleteItems[
        (size_t)ide->autoCompleteSelection < ide->autoCompleteItems.size()
            ? (size_t)ide->autoCompleteSelection : 0];
    const std::string detalle = IdeDetalleApi(nombre);
    const size_t separador = detalle.find(" - ");
    const std::string firma = nombre +
        (separador == std::string::npos ? detalle : detalle.substr(0, separador));
    const std::string descripcion = separador == std::string::npos
        ? std::string() : detalle.substr(separador + 3);
    const int detailAvailableChars = availableChars -
        (layout.listW + 2 * marginGS) / letraW;
    const int detailChars = detailAvailableChars > 0 ? detailAvailableChars : 1;
    size_t preferredDetailChars = firma.size();
    if (descripcion.size() > preferredDetailChars) preferredDetailChars = descripcion.size();
    if (preferredDetailChars > (size_t)detailChars) preferredDetailChars = (size_t)detailChars;
    if (preferredDetailChars < 1) preferredDetailChars = 1;
    layout.detailW = (int)preferredDetailChars * letraW + 2 * marginGS;
    if (layout.listW + layout.detailW > availableW)
        layout.detailW = availableW > layout.listW ? availableW - layout.listW : 1;
    layout.width = layout.listW + layout.detailW;

    IdeEnvolverTexto(firma, (size_t)(layout.detailW > 2 * marginGS
        ? (layout.detailW - 2 * marginGS) / letraW : 1), &layout.detailLines);
    std::vector<std::string> wrappedDescription;
    IdeEnvolverTexto(descripcion, (size_t)(layout.detailW > 2 * marginGS
        ? (layout.detailW - 2 * marginGS) / letraW : 1), &wrappedDescription);
    layout.detailLines.insert(layout.detailLines.end(),
                              wrappedDescription.begin(), wrappedDescription.end());
    int visibleRows = (int)ide->autoCompleteItems.size();
    if ((int)layout.detailLines.size() > visibleRows)
        visibleRows = (int)layout.detailLines.size();
    layout.height = visibleRows * lh + 2 * marginGS;

    layout.x = gutterW + marginGS + ide->curCol * letraW + ide->PosX;
    if (layout.x + layout.width > ide->width) layout.x = ide->width - layout.width;
    if (layout.x < gutterW) layout.x = gutterW;
    layout.y = top + marginGS + ide->curLin * lh + ide->PosY + lh;
    if (layout.y + layout.height > ide->height)
        layout.y = top + marginGS + ide->curLin * lh + ide->PosY - layout.height;
    if (layout.y < top) layout.y = top;
    return layout;
}

// ---------------------------------------------------------------------------
//  constructor / buffer
// ---------------------------------------------------------------------------
IDE::IDE() {
    sucio = false;
    curLin = 0; curCol = 0;
    haySel = false; selLin = 0; selCol = 0;
    undoUltimaOp = 0;
    lastLineas = -1; lastMaxAncho = 0;
    selDrag = false;
    lineas.push_back(std::string()); // el buffer nunca esta vacio (1 linea vacia)
    BarCrear(); // [0] = icono de tipo/split
    // botones por ROL (no por indice), como la barra del 2D/UV
    Button* b;
    b = new Button(T("Script"), -1);
    b->rol = BRIDE_Script; b->desplegable = true; b->caretMenu = true;
    BarButtons.push_back(b);
    // b = new Button(T("New Lua Class"));
    // b->rol = BRIDE_NewClass;
    // BarButtons.push_back(b);
    // b = new Button(T("Save"));
    // b->rol = BRIDE_Guardar;
    // BarButtons.push_back(b);
    // b = new Button(T("Refresh"));
    // b->rol = BRIDE_Refresh;
    // BarButtons.push_back(b);
    b = new Button(T("File"));
    b->rol = BRIDE_File; b->desplegable = true; b->caretMenu = true;
    BarButtons.push_back(b);
    b = new Button(T("Edit"));
    b->rol = BRIDE_Edit; b->desplegable = true; b->caretMenu = true;
    BarButtons.push_back(b);
    b = new Button(T("Help"));
    b->rol = BRIDE_Help; b->desplegable = true; b->caretMenu = true;
    BarButtons.push_back(b);
    searchMode = 0;
    searchQuerySelected = false;
    autoCompleteSelection = 0;
    autoCompleteVisible = false;
    forceAutoComplete = false;
    RecalcularBloques();
    // con un proyecto cargado, abrir el PRIMER script (el IDE nace mostrando codigo)
    AbrirPrimerScript();
}

IDE::~IDE() {}

void IDE::AbrirPrimerScript() {
    if (!archivo.empty()) return;
    std::vector<std::string> rutas;
    IDEColectarScripts(&rutas);
    if (!rutas.empty()) AbrirArchivo(rutas[0]);
}

void IDE::NuevaClaseLua() {
    if (w3dPath.empty()) {
        Notificar("New Lua Class: save the project first", true);
        return;
    }
    if (W3dContenedorHayMontado()) {
        Notificar("New Lua Class: save the project as v5 before creating scripts", true);
        return;
    }
    std::string dir = IdeCarpetaProyecto() + "/scripts";
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::u8path(dir), ec);
    if (ec) { Notificar("New Lua Class: cannot create scripts folder", true); return; }
    std::string base = dir + "/new_class.lua";
    for (int n = 2; std::filesystem::exists(std::filesystem::u8path(base), ec); n++) {
        char suffix[16]; sprintf(suffix, "_%d", n);
        base = dir + "/new_class" + suffix + ".lua";
    }
    const std::string texto =
        "function start()\n"
        "end\n"
        "\n"
        "function update(dt)\n"
        "end\n";
    if (!w3dFileSystem::WriteTextFile(base, texto)) {
        Notificar("New Lua Class: cannot write the script", true);
        return;
    }
    if (AbrirArchivo(base)) Notificar("Created Lua class " + IDENombreScript(base), false);
}

// texto crudo -> lineas del buffer (sin '\r'; tab -> 4 espacios: la fuente
// pixel no tiene tabulador y el cursor por columnas necesita ancho fijo)
static void IdePartirEnLineas(const std::string& crudo, std::vector<std::string>* out) {
    out->clear();
    std::string linea;
    for (size_t i = 0; i < crudo.size(); i++) {
        char c = crudo[i];
        if (c == '\r') continue;
        if (c == '\n') { out->push_back(linea); linea.clear(); continue; }
        if (c == '\t') { linea += "    "; continue; }
        linea += c;
    }
    out->push_back(linea); // la ultima (o unica) linea; "a\n" -> ["a", ""]
    if (out->empty()) out->push_back(std::string());
}

bool IDE::AbrirArchivo(const std::string& ruta) {
    bool ok = false;
    std::string crudo = w3dFileSystem::ReadTextFile(ruta, &ok);
    if (!ok) {
        Notificar("IDE: no pude leer " + IDENombreScript(ruta), true);
        return false;
    }
    IdePartirEnLineas(crudo, &lineas);
    archivo = ruta;
    sucio = false;
    curLin = 0; curCol = 0; haySel = false; selDrag = false;
    PosX = 0; PosY = 0;
    pilaUndo.clear(); pilaRedo.clear(); undoUltimaOp = 0;
    searchMode = 0; autoCompleteVisible = false; forceAutoComplete = false;
    lastLineas = -1; // fuerza el recalculo del scroll en el proximo Render
    RecalcularBloques();
    g_redraw = true;
    return true;
}

std::string IDE::GetTexto() const {
    std::string t;
    for (size_t i = 0; i < lineas.size(); i++) {
        if (i) t += '\n';
        t += lineas[i];
    }
    return t;
}

void IDE::SetTexto(const std::string& texto) {
    IdePartirEnLineas(texto, &lineas);
    curLin = 0; curCol = 0; haySel = false;
    pilaUndo.clear(); pilaRedo.clear(); undoUltimaOp = 0;
    sucio = true;
    searchMode = 0; autoCompleteVisible = false; forceAutoComplete = false;
    lastLineas = -1;
    RecalcularBloques();
    g_redraw = true;
}

// REGLA: el IDE NUNCA rompe el archivo. Guardado ATOMICO: se escribe TODO a un
// .w3dtmp al lado y recien si salio completo se renombra sobre el .lua. Si el
// disco falla a mitad de camino, el original queda intacto.
bool IDE::Guardar() {
    if (archivo.empty()) {
        Notificar("IDE Save: no Lua script is open", true);
        return false;
    }
    std::string t = GetTexto();
    // ---------------------------------------------------------------------
    //  EL SCRIPT VIVE ADENTRO DEL .w3d: se guarda A SU ENTRADA.
    //  'archivo' aca no es una ruta de disco sino un nombre de entrada
    //  ("scripts/menu.lua"): el fopen de abajo crearia una carpeta scripts/
    //  RELATIVA AL DIRECTORIO ACTUAL (o sea al lado del binario del editor) y el
    //  cambio no llegaria nunca al proyecto. Va al overlay del contenedor, que lo
    //  sirve en caliente a todo el editor y lo hornea al zip al guardar el proyecto.
    // ---------------------------------------------------------------------
    if (W3dEsNombreDeEntrada(archivo)) {
        if (!W3dContenedorEscribirEntrada(archivo, t.empty() ? "" : t.c_str(), t.size())) {
            Notificar("IDE: could not save " + IDENombreScript(archivo) +
                      " within the project", true);
            return false;
        }
        W3dScriptPropiedadesInvalidar();
        sucio = false;
        Notificar(IDENombreScript(archivo) +
              " saved in the project buffer; press Project Save to write the .w3d", false);
        g_redraw = true;
        return true;
    }
    std::string tmp = archivo + ".w3dtmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) {
        Notificar("IDE: I couldn't write. " + IDENombreScript(archivo), true);
        return false;
    }
    size_t esc = t.empty() ? 0 : fwrite(t.c_str(), 1, t.size(), f);
    bool ok = (fclose(f) == 0) && (esc == t.size());
    if (!ok) {
        remove(tmp.c_str());
        Notificar("IDE: failed to save " + IDENombreScript(archivo), true);
        return false;
    }
#if defined(_WIN32) || defined(W3D_SYMBIAN)
    remove(archivo.c_str()); // Windows Y Symbian: rename()/RFs::Rename NO reemplaza un destino que ya existe
                             // (KErrAlreadyExists) -> sin este remove, guardar un .lua que ya existe fallaba SIEMPRE
#endif
    if (rename(tmp.c_str(), archivo.c_str()) != 0) {
        remove(tmp.c_str());
        Notificar("IDE: failed to save " + IDENombreScript(archivo), true);
        return false;
    }
    W3dScriptPropiedadesInvalidar();
    sucio = false;
    Notificar(IDENombreScript(archivo) + " guardado", false);
    g_redraw = true;
    return true;
}

bool IDE::GuardarComo(const std::string& ruta) {
    if (ruta.empty()) return false;
    const std::string anterior = archivo;
    archivo = ruta;
    if (Guardar()) return true;
    archivo = anterior;
    SincronizarBarra();
    return false;
}

void IDE::IniciarGuardarComo() {
    searchQuery = archivo;
    size_t slash = searchQuery.find_last_of("/\\");
    if (slash != std::string::npos) searchQuery = searchQuery.substr(slash + 1);
    if (searchQuery.empty()) searchQuery = "untitled.lua";
    replaceQuery.clear();
    searchQuerySelected = true;
    searchMode = 4;
    autoCompleteVisible = false;
    RecalcularScroll();
    AsegurarCursorVisible();
    IdeMostrarTecladoAndroid();
    g_redraw = true;
}

// REFRESH: guardar + aplicar EN VIVO. Comportamiento elegido (predecible):
//  - juego CORRIENDO (SimActiva): se recargan SOLO los objetos cuyos scripts
//    usan el archivo editado (SimScriptsCambiados): sus lua_State se recrean
//    (variables locales de cero + inicio() de nuevo) y el RESTO del juego
//    (posiciones, otros scripts, escena activa) se PRESERVA tal cual. No se
//    reinicia la partida: es la misma semantica que ya tiene cambiar el .lua
//    de un objeto desde la tarjeta Script con el juego andando.
//  - juego parado: refresh = solo guardar (el proximo Play ya lee lo nuevo).
void IDE::Refresh() {
    if (archivo.empty()) return;
    if (!Guardar()) return;
    // (antes esto estaba bajo #ifndef W3D_SYMBIAN sin razon: SimJuego.cpp -SimActiva/SimScriptsCambiados- SI
    //  compila en el .mmp del N95 -ya se usa desde Whisk3DContainer.cpp-, asi que la recarga en vivo linkea igual.)
    extern bool SimActiva();
    extern void SimScriptsCambiados(Object*);
    if (!SimActiva()) return;
    // juntar primero (SimScriptsCambiados puede agregar a la lista interna de la sim)
    std::vector<Object*> usan;
    std::vector<Object*> pila;
    if (SceneCollection) pila.push_back(SceneCollection);
    while (!pila.empty()) {
        Object* o = pila.back(); pila.pop_back();
        if (o->scriptDatos)
            for (size_t i = 0; i < o->scriptDatos->scripts.size(); i++)
                if (IdeMismoArchivo(o->scriptDatos->scripts[i].ruta, archivo)) {
                    usan.push_back(o);
                    break;
                }
        for (size_t i = 0; i < o->Childrens.size(); i++) pila.push_back(o->Childrens[i]);
    }
    for (size_t i = 0; i < usan.size(); i++) SimScriptsCambiados(usan[i]);
    Notificar("Refresh: " + IdeNum((int)usan.size()) + " live-reloaded object(s)", false);
    g_redraw = true;
}

// ---------------------------------------------------------------------------
//  undo: snapshots del buffer entero (los .lua de un juego son chicos), en una
//  pila AGRUPADA por operacion: una corrida de tipeo = UN undo; backspace
//  sostenido = UN undo; pegar/cortar/enter = un undo cada uno. La pila redo
//  conserva los estados que restaura Ctrl+Z.
// ---------------------------------------------------------------------------
void IDE::Mutar(int op) {
    if (op == 0 || op != undoUltimaOp || pilaUndo.empty()) {
        EstadoTexto e;
        e.lineas = lineas; e.curLin = curLin; e.curCol = curCol;
        pilaUndo.push_back(e);
        if (pilaUndo.size() > 200) pilaUndo.erase(pilaUndo.begin());
    }
    pilaRedo.clear();
    undoUltimaOp = op;
    sucio = true;
}

void IDE::DeshacerTexto() {
    if (pilaUndo.empty()) return;
    EstadoTexto actual;
    actual.lineas = lineas; actual.curLin = curLin; actual.curCol = curCol;
    pilaRedo.push_back(actual);
    EstadoTexto& e = pilaUndo.back();
    lineas = e.lineas;
    curLin = e.curLin; curCol = e.curCol;
    pilaUndo.pop_back();
    haySel = false;
    undoUltimaOp = 0;
    sucio = true; // no se compara contra el disco: deshacer sigue siendo "sin guardar"
    lastLineas = -1;
    RecalcularBloques();
    AsegurarCursorVisible();
    ActualizarAutocompletado();
    g_redraw = true;
}

void IDE::RehacerTexto() {
    if (pilaRedo.empty()) return;
    EstadoTexto actual;
    actual.lineas = lineas; actual.curLin = curLin; actual.curCol = curCol;
    pilaUndo.push_back(actual);
    if (pilaUndo.size() > 200) pilaUndo.erase(pilaUndo.begin());
    EstadoTexto e = pilaRedo.back();
    pilaRedo.pop_back();
    lineas = e.lineas;
    curLin = e.curLin; curCol = e.curCol;
    haySel = false;
    undoUltimaOp = 0;
    sucio = true;
    lastLineas = -1;
    RecalcularBloques();
    AsegurarCursorVisible();
    ActualizarAutocompletado();
    g_redraw = true;
}

// ---------------------------------------------------------------------------
//  cursor / seleccion
// ---------------------------------------------------------------------------
void IDE::NormalizarSel(int* l0, int* c0, int* l1, int* c1) const {
    if (selLin < curLin || (selLin == curLin && selCol <= curCol)) {
        *l0 = selLin; *c0 = selCol; *l1 = curLin; *c1 = curCol;
    } else {
        *l0 = curLin; *c0 = curCol; *l1 = selLin; *c1 = selCol;
    }
}

void IDE::CursorA(int lin, int col, bool shift) {
    if (shift && !haySel) { selLin = curLin; selCol = curCol; haySel = true; }
    if (lin < 0) lin = 0;
    if (lin >= (int)lineas.size()) lin = (int)lineas.size() - 1;
    if (col < 0) col = 0;
    if (col > (int)lineas[lin].size()) col = (int)lineas[lin].size();
    curLin = lin; curCol = col;
    if (!shift) haySel = false;
    if (haySel && selLin == curLin && selCol == curCol) haySel = false;
    undoUltimaOp = 0; // moverse corta el grupo de tipeo del undo
    AsegurarCursorVisible();
    ActualizarAutocompletado();
    g_redraw = true;
}

void IDE::MoverCursor(int dLin, int dCol, bool shift) {
    int lin = curLin, col = curCol;
    if (dCol) {
        col += dCol;
        if (col < 0) { // cruzar al final de la linea anterior
            if (lin > 0) { lin--; col = (int)lineas[lin].size(); }
            else col = 0;
        } else if (col > (int)lineas[lin].size()) { // cruzar al inicio de la siguiente
            if (lin + 1 < (int)lineas.size()) { lin++; col = 0; }
            else col = (int)lineas[lin].size();
        }
    }
    if (dLin) lin += dLin; // CursorA clampea la columna a la linea nueva
    CursorA(lin, col, shift);
}

void IDE::Home(bool shift) { CursorA(curLin, 0, shift); }
void IDE::End(bool shift)  { CursorA(curLin, (int)lineas[curLin].size(), shift); }

void IDE::PaginaArribaAbajo(int dir, bool shift) {
    const int lh = (RenglonHeightGS > 0) ? RenglonHeightGS : 12;
    int visibles = (height - EditorTopOffset()) / lh;
    if (visibles < 1) visibles = 1;
    CursorA(curLin + dir * visibles, curCol, shift);
}

void IDE::SeleccionarTodo() {
    selLin = 0; selCol = 0;
    curLin = (int)lineas.size() - 1;
    curCol = (int)lineas[curLin].size();
    haySel = !(curLin == 0 && curCol == 0);
    undoUltimaOp = 0;
    g_redraw = true;
}

void IDE::DeseleccionarTodo() {
    if (!haySel) return;
    haySel = false;
    g_redraw = true;
}

void IDE::Buscar() {
    searchQuery = TextoSeleccionado();
    searchQuerySelected = !searchQuery.empty();
    replaceQuery.clear();
    searchMode = 1;
    autoCompleteVisible = false;
    RecalcularScroll();
    AsegurarCursorVisible();
    IdeMostrarTecladoAndroid();
    g_redraw = true;
}

void IDE::BuscarReemplazar() {
    searchQuery = TextoSeleccionado();
    searchQuerySelected = !searchQuery.empty();
    replaceQuery.clear();
    searchMode = 2;
    autoCompleteVisible = false;
    RecalcularScroll();
    AsegurarCursorVisible();
    IdeMostrarTecladoAndroid();
    g_redraw = true;
}

bool IDE::BuscarSiguiente() {
    if (searchQuery.empty()) return false;
    const std::string texto = GetTexto();
    size_t cursor = 0;
    for (int l = 0; l < curLin; l++) cursor += lineas[l].size() + 1;
    cursor += curCol;
    size_t pos = texto.find(searchQuery, cursor);
    if (pos == std::string::npos) pos = texto.find(searchQuery);
    if (pos == std::string::npos) return false;
    int startLine = 0, startCol = 0;
    for (size_t i = 0; i < pos; i++) {
        if (texto[i] == '\n') { startLine++; startCol = 0; }
        else startCol++;
    }
    CursorA(startLine, startCol, false);
    int endLine = startLine, endCol = startCol + (int)searchQuery.size();
    while (endCol > (int)lineas[endLine].size() && endLine + 1 < (int)lineas.size()) {
        endCol -= (int)lineas[endLine].size() + 1;
        endLine++;
    }
    CursorA(endLine, endCol, true);
    return true;
}

void IDE::ReemplazarCoincidencias() {
    if (searchQuery.empty()) return;
    std::string texto = GetTexto();
    size_t pos = 0;
    bool changed = false;
    while ((pos = texto.find(searchQuery, pos)) != std::string::npos) {
        if (!changed) Mutar(0);
        texto.replace(pos, searchQuery.size(), replaceQuery);
        pos += replaceQuery.size();
        changed = true;
    }
    if (!changed) return;
    IdePartirEnLineas(texto, &lineas);
    curLin = 0; curCol = 0; haySel = false;
    lastLineas = -1;
    RecalcularBloques();
    AsegurarCursorVisible();
    ActualizarAutocompletado();
    g_redraw = true;
}

void IDE::ActualizarAutocompletado() {
    autoCompleteItems.clear();
    if (searchMode) {
        autoCompleteVisible = false;
        return;
    }
    int start = curCol;
    const std::string& line = lineas[curLin];
    while (start > 0 && IdeEsIdent(line[start - 1])) start--;
    const std::string prefix = line.substr(start, curCol - start);
    forceAutoComplete = forceAutoComplete && prefix.empty();
    if (prefix.empty() && !forceAutoComplete) {
        autoCompleteVisible = false;
        return;
    }
    bool inBlock = (curLin < (int)bloqueAlInicio.size()) ? (bloqueAlInicio[curLin] != 0) : false;
    std::vector<unsigned char> kinds;
    LexLinea(line, &inBlock, &kinds);
    if (curCol > 0 && (kinds[curCol - 1] == 1 || kinds[curCol - 1] == 2)) {
        autoCompleteVisible = false;
        return;
    }
    for (size_t i = 0; i < sizeof(kIdeApi) / sizeof(kIdeApi[0]); i++) {
        const std::string candidate(kIdeApi[i].nombre);
        if (candidate.compare(0, prefix.size(), prefix) != 0) continue;
        if (candidate == prefix) continue;
        autoCompleteItems.push_back(candidate);
        if (autoCompleteItems.size() == 6) break;
    }
    autoCompleteVisible = !autoCompleteItems.empty();
    if (autoCompleteSelection >= (int)autoCompleteItems.size()) autoCompleteSelection = 0;
}

std::string IDE::DetalleAutocompletado(size_t index) const {
    if (index >= autoCompleteItems.size()) return std::string();
    return IdeDetalleApi(autoCompleteItems[index]);
}

bool IDE::AceptarAutocompletado() {
    if (!autoCompleteVisible || autoCompleteItems.empty()) return false;
    int start = curCol;
    const std::string& line = lineas[curLin];
    while (start > 0 && IdeEsIdent(line[start - 1])) start--;
    selLin = curLin; selCol = start; haySel = start != curCol;
    InsertarTexto(autoCompleteItems[autoCompleteSelection]);
    forceAutoComplete = false;
    autoCompleteVisible = false;
    return true;
}

bool IDE::FormatearLua() {
    std::vector<std::string> formateadas;
    formateadas.reserve(lineas.size());
    int comentarioEq = -1;
    int cadenaEq = -1;
    int profundidad = 0;
    for (size_t i = 0; i < lineas.size(); i++) {
        const std::string& original = lineas[i];
        const size_t inicio = IdeEspaciosIniciales(original);
        const std::string cuerpo = original.substr(inicio);
        if (cuerpo.empty()) {
            formateadas.push_back(std::string());
            continue;
        }
        std::vector<std::string> palabras;
        bool protegerLinea = false;
        IdeIndentPalabras(cuerpo, &comentarioEq, &cadenaEq, &palabras, &protegerLinea);
        if (protegerLinea) {
            formateadas.push_back(original);
            continue;
        }
        bool primeraCierra = !palabras.empty() &&
            (palabras[0] == "end" || palabras[0] == "until" ||
             palabras[0] == "else" || palabras[0] == "elseif");
        if (primeraCierra && profundidad > 0) profundidad--;
        std::string linea((size_t)profundidad * 4, ' ');
        linea += cuerpo;
        formateadas.push_back(linea);
        for (size_t p = 0; p < palabras.size(); p++) {
            const std::string& palabra = palabras[p];
            if (p == 0 && primeraCierra) {
                if (palabra == "else") profundidad++;
                continue;
            }
            if (palabra == "function" || palabra == "then" ||
                palabra == "do" || palabra == "repeat") {
                profundidad++;
            } else if (palabra == "end" || palabra == "until") {
                if (profundidad > 0) profundidad--;
            } else if (palabra == "else" || palabra == "elseif") {
                if (profundidad > 0) profundidad--;
                if (palabra == "else") profundidad++;
            }
        }
    }
    if (formateadas == lineas) {
        Notificar("Lua is already formatted", false);
        return false;
    }
    Mutar(0);
    const int oldCurIndent = (int)IdeEspaciosIniciales(lineas[curLin]);
    const int oldSelIndent = haySel ? (int)IdeEspaciosIniciales(lineas[selLin]) : 0;
    lineas.swap(formateadas);
    const int newCurIndent = (int)IdeEspaciosIniciales(lineas[curLin]);
    if (curCol >= oldCurIndent) curCol += newCurIndent - oldCurIndent;
    else curCol = (curCol < newCurIndent) ? curCol : newCurIndent;
    if (haySel) {
        const int newSelIndent = (int)IdeEspaciosIniciales(lineas[selLin]);
        if (selCol >= oldSelIndent) selCol += newSelIndent - oldSelIndent;
        else selCol = (selCol < newSelIndent) ? selCol : newSelIndent;
    }
    if (curCol < 0) curCol = 0;
    if (curCol > (int)lineas[curLin].size()) curCol = (int)lineas[curLin].size();
    if (haySel) {
        if (selCol < 0) selCol = 0;
        if (selCol > (int)lineas[selLin].size()) selCol = (int)lineas[selLin].size();
    }
    lastLineas = -1;
    RecalcularBloques();
    ActualizarAutocompletado();
    AsegurarCursorVisible();
    Notificar("Lua indentation formatted", false);
    g_redraw = true;
    return true;
}

std::string IDE::TextoSeleccionado() const {
    if (!haySel) return std::string();
    int l0, c0, l1, c1;
    NormalizarSel(&l0, &c0, &l1, &c1);
    if (l0 == l1) return lineas[l0].substr(c0, c1 - c0);
    std::string t = lineas[l0].substr(c0);
    for (int l = l0 + 1; l < l1; l++) { t += '\n'; t += lineas[l]; }
    t += '\n'; t += lineas[l1].substr(0, c1);
    return t;
}

// saca la seleccion del buffer SIN tocar el undo (los llamadores agrupan)
static void IdeBorrarRango(std::vector<std::string>* lineas, int l0, int c0, int l1, int c1) {
    if (l0 == l1) {
        (*lineas)[l0].erase(c0, c1 - c0);
        return;
    }
    (*lineas)[l0] = (*lineas)[l0].substr(0, c0) + (*lineas)[l1].substr(c1);
    lineas->erase(lineas->begin() + l0 + 1, lineas->begin() + l1 + 1);
}

void IDE::BorrarSeleccion() {
    if (!haySel) return;
    Mutar(0); // operacion propia (siempre corta grupo)
    int l0, c0, l1, c1;
    NormalizarSel(&l0, &c0, &l1, &c1);
    IdeBorrarRango(&lineas, l0, c0, l1, c1);
    curLin = l0; curCol = c0; haySel = false;
    lastLineas = -1;
    RecalcularBloques();
    AsegurarCursorVisible();
    g_redraw = true;
}

// ---------------------------------------------------------------------------
//  edicion
// ---------------------------------------------------------------------------
void IDE::InsertarChar(char c) {
    if (haySel) {
        Mutar(0); // reemplazo de la seleccion: UNA operacion
        int l0, c0, l1, c1;
        NormalizarSel(&l0, &c0, &l1, &c1);
        IdeBorrarRango(&lineas, l0, c0, l1, c1);
        curLin = l0; curCol = c0; haySel = false;
        undoUltimaOp = 1; // lo que se siga tipeando se agrupa con este snapshot
    } else {
        Mutar(1); // tipeo: agrupa la corrida
    }
    lineas[curLin].insert((size_t)curCol, 1, c);
    curCol++;
    RecalcularBloques();
    ActualizarAutocompletado();
    AsegurarCursorVisible();
    g_redraw = true;
}

void IDE::InsertarTexto(const std::string& t) {
    Mutar(0);
    if (haySel) {
        int l0, c0, l1, c1;
        NormalizarSel(&l0, &c0, &l1, &c1);
        IdeBorrarRango(&lineas, l0, c0, l1, c1);
        curLin = l0; curCol = c0; haySel = false;
    }
    std::vector<std::string> nuevas;
    IdePartirEnLineas(t, &nuevas);
    std::string cola = lineas[curLin].substr((size_t)curCol);
    lineas[curLin] = lineas[curLin].substr(0, (size_t)curCol) + nuevas[0];
    for (size_t i = 1; i < nuevas.size(); i++) {
        curLin++;
        lineas.insert(lineas.begin() + curLin, nuevas[i]);
    }
    curCol = (int)lineas[curLin].size();
    lineas[curLin] += cola;
    lastLineas = -1;
    RecalcularBloques();
    ActualizarAutocompletado();
    AsegurarCursorVisible();
    g_redraw = true;
}

void IDE::Enter() {
    if (haySel) { InsertarTexto("\n"); return; }
    Mutar(0);
    std::string cola = lineas[curLin].substr((size_t)curCol);
    lineas[curLin].erase((size_t)curCol);
    curLin++;
    lineas.insert(lineas.begin() + curLin, cola);
    curCol = 0;
    lastLineas = -1;
    RecalcularBloques();
    AsegurarCursorVisible();
    g_redraw = true;
}

void IDE::TabKey() { InsertarTexto("    "); } // 4 espacios (la fuente no tiene tab)

void IDE::Backspace() {
    if (haySel) { BorrarSeleccion(); return; }
    if (curCol == 0 && curLin == 0) return;
    Mutar(2); // los backspace seguidos se agrupan
    if (curCol > 0) {
        lineas[curLin].erase((size_t)(curCol - 1), 1);
        curCol--;
    } else { // unir con la linea anterior
        curCol = (int)lineas[curLin - 1].size();
        lineas[curLin - 1] += lineas[curLin];
        lineas.erase(lineas.begin() + curLin);
        curLin--;
        lastLineas = -1;
    }
    RecalcularBloques();
    ActualizarAutocompletado();
    AsegurarCursorVisible();
    g_redraw = true;
}

void IDE::DelForward() {
    if (haySel) { BorrarSeleccion(); return; }
    if (curCol >= (int)lineas[curLin].size() && curLin + 1 >= (int)lineas.size()) return;
    Mutar(3); // los delete seguidos se agrupan
    if (curCol < (int)lineas[curLin].size()) {
        lineas[curLin].erase((size_t)curCol, 1);
    } else { // unir con la linea siguiente
        lineas[curLin] += lineas[curLin + 1];
        lineas.erase(lineas.begin() + curLin + 1);
        lastLineas = -1;
    }
    RecalcularBloques();
    ActualizarAutocompletado();
    g_redraw = true;
}

// ---------------------------------------------------------------------------
//  clipboard: el del SISTEMA via W3dClipboard (Core) -> compartido con navegador/notas
//  en TODAS las plataformas (SDL en PC/web, CClipboard en Symbian). gIDEClipInterno es
//  respaldo local (headless / si el sistema no responde).
// ---------------------------------------------------------------------------
void IDE::Copiar() {
    if (!haySel) return;
    gIDEClipInterno = TextoSeleccionado();
    w3dEngine::W3dClipboardSet(gIDEClipInterno);   // portapapeles del sistema (cualquier app lo puede pegar)
}

void IDE::Cortar() {
    if (!haySel) return;
    Copiar();
    BorrarSeleccion();
}

void IDE::Pegar() {
    std::string t = w3dEngine::W3dClipboardGet(); // el clipboard del SISTEMA (de cualquier programa: navegador, notas)
    if (t.empty()) t = gIDEClipInterno; // headless / si el sistema no dio nada
    if (t.empty()) return;
    // solo ASCII imprimible + saltos (la fuente pixel del editor es ASCII)
    std::string filtrado;
    for (size_t i = 0; i < t.size(); i++) {
        char c2 = t[i];
        if (c2 == '\n' || c2 == '\t' || (c2 >= 32 && (unsigned char)c2 < 127)) filtrado += c2;
    }
    if (!filtrado.empty()) InsertarTexto(filtrado);
}

// ---------------------------------------------------------------------------
//  teclado: el ruteo de PC (controles.cpp) le da al IDE enfocado por hover la
//  PRIMERA palabra: si TeclaEditor consume, ningun atajo global corre.
// ---------------------------------------------------------------------------
bool IDE::TeclaEditor(int tecla, bool repeticion) {
    (void)repeticion;
    bool shift = LShiftPressed;
    if (searchMode) {
        if (tecla == W3dK_ESCAPE) {
            searchMode = 0;
            IdeOcultarTecladoAndroid();
            RecalcularScroll();
            AsegurarCursorVisible();
            g_redraw = true;
            return true;
        }
        if (tecla == W3dK_BACKSPACE) {
            std::string& query = (searchMode == 3) ? replaceQuery : searchQuery;
            if (searchMode != 3 && searchQuerySelected) {
                query.clear();
                searchQuerySelected = false;
            } else if (!query.empty()) query.erase(query.size() - 1);
            g_redraw = true;
            return true;
        }
        if (tecla == W3dK_RETURN || tecla == W3dK_KP_ENTER) {
            if (searchMode == 1) {
                if (!BuscarSiguiente()) Notificar("Find: no match", true);
                searchMode = 0;
                RecalcularScroll();
                AsegurarCursorVisible();
            } else if (searchMode == 2) {
                searchMode = 3;
                searchQuerySelected = false;
            } else if (searchMode == 4) {
                const std::string nombre = searchQuery;
                searchMode = 0;
                RecalcularScroll();
                AsegurarCursorVisible();
                IDEGuardarComoDesdeUI(this, nombre);
                g_redraw = true;
                return true;
            } else {
                ReemplazarCoincidencias();
                searchMode = 0;
                RecalcularScroll();
                AsegurarCursorVisible();
            }
            g_redraw = true;
            return true;
        }
        if (tecla >= 32 && tecla < 127) return true;
        return true;
    }
    if (LCtrlPressed) {
        switch (tecla) {
            case W3dK_C: Copiar(); return true;
            case W3dK_X: Cortar(); return true;
            case W3dK_V: Pegar(); return true;
            case W3dK_A: SeleccionarTodo(); return true;
            case W3dK_Z: DeshacerTexto(); return true;             // undo DEL TEXTO, no el global
            case W3dK_Y: RehacerTexto(); return true;
            case W3dK_F: Buscar(); return true;
            case W3dK_H: BuscarReemplazar(); return true;
            case W3dK_N: IDENuevoDesdeUI(this); return true;
            case W3dK_O: IDEAbrirDesdeUI(this); return true;
            case 32:
                forceAutoComplete = true;
                ActualizarAutocompletado();
                g_redraw = true;
                return true;
            case W3dK_S:
                if (shift || archivo.empty()) IniciarGuardarComo();
                else Guardar();
                return true;                                      // guarda el .lua, no el proyecto
            case W3dK_HOME: CursorA(0, 0, shift); return true;     // inicio del archivo
            case W3dK_END:
                CursorA((int)lineas.size() - 1, (int)lineas.back().size(), shift);
                return true;
        }
        return false; // otros Ctrl+... siguen su camino normal
    }
    switch (tecla) {
        case W3dK_UP:
            if (autoCompleteVisible) {
                autoCompleteSelection = (autoCompleteSelection + (int)autoCompleteItems.size() - 1) %
                                        (int)autoCompleteItems.size();
                g_redraw = true;
                return true;
            }
            MoverCursor(-1, 0, shift); return true;
        case W3dK_DOWN:
            if (autoCompleteVisible) {
                autoCompleteSelection = (autoCompleteSelection + 1) % (int)autoCompleteItems.size();
                g_redraw = true;
                return true;
            }
            MoverCursor(+1, 0, shift); return true;
        case W3dK_LEFT:  MoverCursor(0, -1, shift); return true;
        case W3dK_RIGHT: MoverCursor(0, +1, shift); return true;
        case W3dK_HOME:  Home(shift); return true;
        case W3dK_END:   End(shift); return true;
        case W3dK_PGUP:  PaginaArribaAbajo(-1, shift); return true;
        case W3dK_PGDN:  PaginaArribaAbajo(+1, shift); return true;
        case W3dK_RETURN:
        case W3dK_KP_ENTER: Enter(); return true;
        case W3dK_TAB:
            if (AceptarAutocompletado()) return true;
            TabKey(); return true;
        case W3dK_BACKSPACE: Backspace(); return true;
        case W3dK_DELETE:    DelForward(); return true;
        case W3dK_ESCAPE:
            if (autoCompleteVisible) {
                autoCompleteVisible = false; forceAutoComplete = false; g_redraw = true; return true;
            }
            if (haySel) { haySel = false; g_redraw = true; return true; }
            return false; // sin seleccion, el ESC sigue siendo global (pausa, etc.)
    }
    // imprimibles: el CARACTER entra por SDL_TEXTINPUT (respeta el layout del
    // teclado); aca solo se CONSUME el keydown para que no dispare atajos
    // del editor (g/r/s/x moverian la escena mientras escribis codigo)
    if (tecla >= 32 && tecla < 127) return true;
    return false; // F11, modificadores, etc: siguen su camino
}

void IDE::TextoTipeado(const char* utf8) {
    if (!utf8) return;
    for (const char* p = utf8; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (searchMode) {
            if (c >= 32 && c < 127) {
                std::string& query = (searchMode == 3) ? replaceQuery : searchQuery;
                if (searchMode != 3 && searchQuerySelected) {
                    query.clear();
                    searchQuerySelected = false;
                }
                if (query.size() < 128) query += (char)c;
                g_redraw = true;
            }
        } else if (c >= 32 && c < 127) InsertarChar((char)c);
        // fuera de ASCII (acentos, multibyte) se ignora: la fuente es ASCII
    }
}

void IDE::event_key_down(int tecla, bool repeticion) {
    // ruta normal de viewports (Symbian / fallback): mismas teclas de edicion
    TeclaEditor(tecla, repeticion);
}

// ---------------------------------------------------------------------------
//  mouse / touch
// ---------------------------------------------------------------------------
int IDE::GutterW() const {
    int n = (int)lineas.size(), d = 1;
    while (n >= 10) { n /= 10; d++; }
    if (d < 2) d = 2;
    return borderGS + marginGS + d * LetterWidthGS + marginGS;
}

// click en el CONTENIDO: posiciona el cursor (shift extiende la seleccion) y
// arma el drag de seleccion. mx/my en coords de VENTANA.
void IDE::ClickContenido(int mx, int my, bool shift) {
    const int lh = (RenglonHeightGS > 0) ? RenglonHeightGS : 12;
    int lx = mx - x, ly = my - y;
    if (autoCompleteVisible && !autoCompleteItems.empty()) {
        const int letterW = LetterWidthGS > 0 ? LetterWidthGS : 6;
        const IdeAutocompletadoLayout layout = IdeCalcularAutocompletadoLayout(
            this, letterW, lh, GutterW(), EditorTopOffset());
        if (lx >= layout.x && lx < layout.x + layout.width &&
            ly >= layout.y && ly < layout.y + layout.height) {
            if (lx < layout.x + layout.listW) {
                int row = (ly - layout.y - marginGS) / lh;
                if (row >= 0 && row < (int)autoCompleteItems.size()) {
                    autoCompleteSelection = row;
                    AceptarAutocompletado();
                }
            }
            return;
        }
    }
#ifdef __ANDROID__
    if (g_uiTapEnCurso) SDL_StartTextInput();
#endif
    int textX0 = GutterW() + marginGS + PosX;
    int textY0 = EditorTopOffset() + marginGS + PosY;
    int lin = (ly - textY0) / lh;
    int col = (lx - textX0 + LetterWidthGS / 2) / (LetterWidthGS > 0 ? LetterWidthGS : 1);
    CursorA(lin, col, shift);
    selDrag = true;
    ViewPortClickDown = true; // el drag de seleccion congela el foco por hover
    g_redraw = true;
}

void IDE::button_left() {
    // agarre de la scrollbar (patron Console; en PC el click de contenido entra
    // por LayoutClickUI -> ClickContenido, no por aca)
    if (mouseOverScrollY) mouseOverScrollYpress = true;
    if (mouseOverScrollX) mouseOverScrollXpress = true;
}

void IDE::event_mouse_motion(int mx, int my) {
    if (leftMouseDown && selDrag) {
        // arrastrar = extender la seleccion hasta el punto del mouse
        ViewPortClickDown = true;
        const int lh = (RenglonHeightGS > 0) ? RenglonHeightGS : 12;
        int lx = mx - x, ly = my - y;
        int textX0 = GutterW() + marginGS + PosX;
        int textY0 = EditorTopOffset() + marginGS + PosY;
        int lin = (ly - textY0) / lh;
        int col = (lx - textX0 + LetterWidthGS / 2) / (LetterWidthGS > 0 ? LetterWidthGS : 1);
        CursorA(lin, col, true);
        g_redraw = true;
    } else if (middleMouseDown) {
        // boton del medio = arrastrar el contenido (scroll 1:1, como la Console)
        ViewPortClickDown = true;
        ScrollByTouch(mx - lastMouseX, my - lastMouseY);
        g_redraw = true;
    }
}

#ifndef W3D_SYMBIAN
// IMPRESCINDIBLE (regla de la casa): soltar libera ViewPortClickDown. Sin esto
// el foco por hover queda CLAVADO en este viewport (bug conocido de Console).
void IDE::mouse_button_up(int boton) {
    ViewPortClickDown = false;
    if (boton == W3dMB_IZQ) {
        selDrag = false;
        mouseOverScrollYpress = false;
        mouseOverScrollXpress = false;
    }
}

void IDE::event_mouse_wheel(float dy, int mx, int my) {
    if (BarScrollHorizontal(mx, my, (int)(dy * 40))) return; // sobre la barra superior
    int paso = (int)(dy * 6 * GlobalScale);
    if (LShiftPressed) ScrollByTouch(paso, 0); // shift+rueda = horizontal
    else               ScrollByTouch(0, paso);
    g_redraw = true;
}
#endif

bool IDE::event_finger_scroll(int px, int py, int dx, int dy) {
    (void)px; (void)py;
    ScrollByTouch(dx, dy);
    g_redraw = true;
    return true;
}

// ---------------------------------------------------------------------------
//  scroll
// ---------------------------------------------------------------------------
void IDE::RecalcularScroll() {
    const int lh = (RenglonHeightGS > 0) ? RenglonHeightGS : 12;
    int n = (int)lineas.size();
    ResizeScrollbar(width, height, lastMaxAncho, -(n * lh + marginGS * 2), EditorTopOffset());
}

void IDE::Resize(int newW, int newH) {
    ViewportBase::Resize(newW, newH);
    ResizeBorder(newW, newH);
    RecalcularScroll();
}

void IDE::AsegurarCursorVisible() {
    const int lh = (RenglonHeightGS > 0) ? RenglonHeightGS : 12;
    int visH = height - EditorTopOffset() - marginGS * 2;
    int visW = width - GutterW() - marginGS * 2 - GlobalScale * 9; // colchon de la barra vertical
    int cy = curLin * lh;
    if (cy + PosY < 0) PosY = -cy;
    if (visH > lh && cy + PosY > visH - lh) PosY = visH - lh - cy;
    if (PosY > 0) PosY = 0;
    int cx = curCol * LetterWidthGS;
    if (cx + PosX < 0) PosX = -cx;
    if (visW > LetterWidthGS && cx + PosX > visW - LetterWidthGS) PosX = visW - LetterWidthGS - cx;
    if (PosX > 0) PosX = 0;
    // el clamp contra MaxPosX/MaxPosY fino lo hace ResizeScrollbar en el Render
}

int IDE::EditorTopOffset() const {
    const int lh = (RenglonHeightGS > 0) ? RenglonHeightGS : 12;
    return BarTopOffset() + (searchMode ? lh + marginGS * 2 : 0);
}

// ---------------------------------------------------------------------------
//  render
// ---------------------------------------------------------------------------
void IDE::SincronizarBarra() {
    Button* b = BarRolBtn(BarButtons, BRIDE_Script);
    if (!b) return;
    std::string t = archivo.empty() ? std::string(T("Script")) : IDENombreScript(archivo);
    if (sucio) t += " *"; // asterisco = cambios sin guardar
    if (b->text != t) b->text = t;
}

// un rectangulo lleno en coords locales del viewport (sin textura)
static void IdeQuad(float x0, float y0, float x1, float y1, const float* rgba) {
    gfx::Color4fv(rgba);
    float quad[12] = { x0, y0, x1, y0, x1, y1, x0, y0, x1, y1, x0, y1 };
    gfx::VertexPointer2f(0, quad);
    gfx::DrawTrianglesArray(6);
}

static void IdeTexto(int px, int py, const std::string& txt, const float* rgba) {
    gfx::Color4fv(rgba);
    gfx::PushMatrix();
    gfx::Translatef((GLfloat)px, (GLfloat)py, 0);
    RenderBitmapText(txt, textAlign::left);
    gfx::PopMatrix();
}

static const float* IdeColorDeTipo(unsigned char t) {
    switch (t) {
        case 1: return kIdeComentario;
        case 2: return kIdeString;
        case 3: return kIdeKeyword;
        case 4: return kIdeNumero;
    }
    return ListaColores[static_cast<int>(ColorID::blanco)];
}

void IDE::Render() {
    SincronizarBarra();
    const int glY = W3dPantallaAlto - y - height;

    // fondo (mismo patron que los demas viewports)
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
    gfx::DisableArray(gfx::TexCoordArray);

    const int lh = (RenglonHeightGS > 0) ? RenglonHeightGS : 12;
    const int top = EditorTopOffset();
    const int LW = (LetterWidthGS > 0) ? LetterWidthGS : 6;
    const int n = (int)lineas.size();
    const int gutterW = GutterW();

    // medir el contenido y recalcular el scroll SOLO si cambio (patron Console)
    int maxLen = 0;
    for (int i = 0; i < n; i++)
        if ((int)lineas[i].size() > maxLen) maxLen = (int)lineas[i].size();
    int maxAncho = gutterW + marginGS + maxLen * LW + marginGS + GlobalScale * 9;
    if (n != lastLineas || maxAncho != lastMaxAncho) {
        lastLineas = n; lastMaxAncho = maxAncho;
        RecalcularScroll();
    }

    int hVis = height - top; if (hVis < 0) hVis = 0;
    const int textX0 = gutterW + marginGS + PosX;
    const int py0 = top + marginGS + PosY;
    int primera = (0 - (py0)) / lh - 1; if (primera < 0) primera = 0;
    int ultima = primera + hVis / lh + 2; if (ultima > n) ultima = n;

    // ---- 1) quads: seleccion + caret + separador del gutter (sin textura) ----
    gfx::Scissor(x + gutterW, glY, (width > gutterW) ? width - gutterW : 0, hVis);
    if (haySel) {
        int l0, c0, l1, c1;
        NormalizarSel(&l0, &c0, &l1, &c1);
        for (int l = (l0 > primera ? l0 : primera); l <= l1 && l < ultima; l++) {
            int a = (l == l0) ? c0 : 0;
            int b = (l == l1) ? c1 : (int)lineas[l].size();
            int py = py0 + l * lh;
            float x0 = (float)(textX0 + a * LW);
            float x1 = (float)(textX0 + b * LW);
            if (b <= (int)lineas[l].size() && a == b) x1 = x0 + LW * 0.4f; // linea vacia: mostrarla igual
            IdeQuad(x0, (float)py, x1, (float)(py + lh), kIdeSeleccion);
        }
    }
    { // caret (sin parpadeo: el render es event-driven). Verde si el foco esta aca.
        int py = py0 + curLin * lh;
        float cx = (float)(textX0 + curCol * LW);
        // OJO: Scrollable tiene un MIEMBRO viewPortActive que sombrea al global -> '::'
        const float* cc = (::viewPortActive == (ViewportBase*)this)
                              ? ListaColores[static_cast<int>(ColorID::accent)]
                              : ListaColores[static_cast<int>(ColorID::grisUI)];
        float grosor = (float)((GlobalScale > 0) ? GlobalScale : 1);
        IdeQuad(cx, (float)py, cx + grosor, (float)(py + lh), cc);
    }
    // separador vertical del gutter (a lo alto de todo el contenido)
    gfx::Scissor(x, glY, width, hVis);
    IdeQuad((float)(gutterW - GlobalScale), (float)top, (float)gutterW, (float)height,
            ListaColores[static_cast<int>(ColorID::grisLinea)]);

    // ---- 2) texto (fuente): numeros de linea + codigo coloreado ----
    gfx::BindTexture(Textures[0]->iID);
    gfx::EnableArray(gfx::TexCoordArray);
    gfx::Enable(gfx::Texture2D);
    gfx::Enable(gfx::Blend);
    gfx::BlendAlpha();
#ifndef W3D_SYMBIAN
    gfx::TexFilter(false);
#endif

    // numeros de linea (solo scroll VERTICAL: el gutter no se mueve con PosX)
    gfx::Scissor(x, glY, gutterW - GlobalScale, hVis);
    for (int l = primera; l < ultima; l++) {
        std::string num = IdeNum(l + 1);
        int nx = gutterW - marginGS - (int)num.size() * LW; // alineado a la derecha
        // la linea del CURSOR se resalta en BLANCO (el resto en gris); asi se ve en que renglon estamos
        IdeTexto(nx, py0 + l * lh, num, ListaColores[static_cast<int>(l == curLin ? ColorID::blanco : ColorID::grisUI)]);
    }

    // el codigo, por tramos de color del lexer
    gfx::Scissor(x + gutterW, glY, (width > gutterW) ? width - gutterW : 0, hVis);
    if (archivo.empty() && n == 1 && lineas[0].empty()) {
        IdeTexto(textX0, py0, "(there are no .lua scripts in the project)",
                 ListaColores[static_cast<int>(ColorID::grisUI)]);
    }
    std::vector<unsigned char> tipos;
    for (int l = primera; l < ultima; l++) {
        const std::string& s = lineas[l];
        if (s.empty()) continue;
        bool blq = (l < (int)bloqueAlInicio.size()) ? (bloqueAlInicio[l] != 0) : false;
        LexLinea(s, &blq, &tipos);
        int py = py0 + l * lh;
        size_t i = 0;
        while (i < s.size()) {
            size_t j = i;
            while (j < s.size() && tipos[j] == tipos[i]) j++;
            IdeTexto(textX0 + (int)i * LW, py, s.substr(i, j - i), IdeColorDeTipo(tipos[i]));
            i = j;
        }
    }

    gfx::Scissor(x, glY, width, height);
    if (searchMode) {
        std::string status;
        if (searchMode == 4)
            status = "Save as: " + searchQuery + "  (Enter to choose location)";
        else if (searchMode == 3)
            status = "Replace '" + searchQuery + "' with: " + replaceQuery;
        else
            status = "Find: " + searchQuery;
        if (searchMode == 2) status = "Find: " + searchQuery + "  (Enter for replacement)";
        IdeTexto(marginGS, BarTopOffset() + marginGS, status,
                 ListaColores[static_cast<int>(ColorID::blanco)]);
    }
    if (autoCompleteVisible && !autoCompleteItems.empty()) {
        const IdeAutocompletadoLayout layout = IdeCalcularAutocompletadoLayout(
            this, LW, lh, gutterW, top);
        IdeQuad((float)layout.x, (float)layout.y,
                (float)(layout.x + layout.width), (float)(layout.y + layout.height),
                kIdeAutocompletadoFondo);
        IdeQuad((float)layout.x, (float)layout.y,
                (float)(layout.x + layout.width), (float)(layout.y + 1),
                kIdeAutocompletadoBorde);
        IdeQuad((float)layout.x, (float)(layout.y + layout.height - 1),
                (float)(layout.x + layout.width), (float)(layout.y + layout.height),
                kIdeAutocompletadoBorde);
        IdeQuad((float)layout.x, (float)layout.y,
                (float)(layout.x + 1), (float)(layout.y + layout.height),
                kIdeAutocompletadoBorde);
        IdeQuad((float)(layout.x + layout.width - 1), (float)layout.y,
                (float)(layout.x + layout.width), (float)(layout.y + layout.height),
                kIdeAutocompletadoBorde);
        IdeQuad((float)(layout.x + layout.listW), (float)layout.y,
                (float)(layout.x + layout.listW + 1),
                (float)(layout.y + layout.height), kIdeAutocompletadoBorde);

        const int listChars = layout.listW > 2 * marginGS
            ? (layout.listW - 2 * marginGS) / LW : 1;
        for (size_t i = 0; i < autoCompleteItems.size(); i++) {
            int rowY = layout.y + marginGS + (int)i * lh;
            if ((int)i == autoCompleteSelection)
                IdeQuad((float)(layout.x + 1), (float)rowY,
                        (float)(layout.x + layout.listW), (float)(rowY + lh),
                        kIdeSeleccion);
            const std::string& nombre = autoCompleteItems[i];
            IdeTexto(layout.x + marginGS, rowY,
                     IdeTruncar(nombre, (size_t)listChars),
                     ListaColores[static_cast<int>(ColorID::blanco)]);
        }

        const int detailX = layout.x + layout.listW + marginGS;
        const size_t detailChars = layout.detailW > 2 * marginGS
            ? (size_t)((layout.detailW - 2 * marginGS) / LW) : 1;
        for (size_t i = 0; i < layout.detailLines.size(); i++) {
            const int rowY = layout.y + marginGS + (int)i * lh;
            const float* color = i == 0
                ? ListaColores[static_cast<int>(ColorID::blanco)]
                : ListaColores[static_cast<int>(ColorID::grisUI)];
            IdeTexto(detailX, rowY,
                     IdeTruncar(layout.detailLines[i], detailChars), color);
        }
    }

    gfx::Disable(gfx::ScissorTest);

    RenderBar();
    DibujarBordes(this);
    DibujarScrollbar(this);
#ifdef W3D_SYMBIAN
    gfx::EnableArray(gfx::NormalArray); // baseline que asume la escena
#endif
}

// ---------------------------------------------------------------------------
//  captura global (PC): el IDE bajo el foco por hover se queda con el teclado
// ---------------------------------------------------------------------------
static IDE* IdeEnfocado() {
    if (!viewPortActive || !viewPortActive->isLeaf()) return NULL;
    if (viewPortActive->ViewportKind() != 8) return NULL;
    return (IDE*)viewPortActive;
}

bool IDETecladoCaptura(int tecla, bool repeticion) {
    IDE* ide = IdeEnfocado();
    if (!ide) return false;
    return ide->TeclaEditor(tecla, repeticion);
}

bool IDETextoTexto(const char* utf8) {
    IDE* ide = IdeEnfocado();
    if (!ide) return false;
    ide->TextoTipeado(utf8);
    return true; // enfocado: el texto es del IDE aunque se haya filtrado todo
}
