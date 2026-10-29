// ============================================================================
//  W3dEscena.cpp — ver W3dEscena.h. El motor multi-escena COMPARTIDO por el
//  editor (Play) y el runtime compilado. C++03 puro. Comentarios sin acentos.
// ============================================================================
#include "W3dEscena.h"
#include "W3dPaletas.h"
#include "W3dNombres.h"   // LA regla de nombres unicos (compartida por todo el editor)
#include "objects/Objects.h"
#include "objects/UI.h"
#include "objects/LegacyUIScene.h"
#include "objects/Texto2D.h"    // los tipos con campos pal* (fix de referencias
#include "objects/Imagen2D.h"   // al borrar un color de las paletas)
#include "objects/Rect2D.h"
#include "objects/Slice9.h"
#include "objects/Boton2D.h"
#include "base/w3dlog.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <map>
#include <vector>

// ---------------------------------------------------------------------------
//  ESTADO del sistema (una sola simulacion/juego a la vez).
// ---------------------------------------------------------------------------
static std::map<std::string, Object*> gEscenas; // name -> logical scene root
static Object* gEscActiva = 0;                   // active logical scene
static std::map<Object*, bool> gInited;           // scenes that already ran init()
static std::string gInicial;                   // nombre de la escena que arranca (ajuste del proyecto)
static bool gModoMulti = false;                // modo multi-escena declarado (informativo)
static W3dEscenaInitFn gInitFn = 0;            // el init de una escena (lo pone cada build)
// cambio de escena PENDIENTE (se aplica al final del frame)
static bool gHayCambio = false;
static bool gReiniciarPend = false;
static std::string gCambioNombre;
static bool gCambioAsync = false;

struct W3dAsyncSceneRequest {
    bool active;
    bool started;
    bool ready;
    bool activateWhenReady;
    Object* root;
    std::string name;
    size_t total;
    size_t completed;
    lua_State* callbackState;
    int callbackRef;
    unsigned long id;

    W3dAsyncSceneRequest()
        : active(false), started(false), ready(false), activateWhenReady(false), root(0),
          total(0), completed(0), callbackState(0), callbackRef(LUA_NOREF), id(0) {}
};

static W3dAsyncSceneRequest gAsyncScene;
static unsigned long gAsyncSceneId = 0;

void W3dEscenaSetInit(W3dEscenaInitFn fn) { gInitFn = fn; }

static size_t ContarNodosEscena(Object* root) {
    if (!root) return 0;
    size_t count = 1;
    for (size_t i = 0; i < root->Childrens.size(); ++i)
        count += ContarNodosEscena(root->Childrens[i]);
    return count;
}

static void InvocarCallbackAsync(lua_State* L, int callbackRef,
                                 const std::string& name, float progress,
                                 const char* status) {
    if (!L || callbackRef == LUA_NOREF || callbackRef == LUA_REFNIL) return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, callbackRef);
    lua_pushlstring(L, name.c_str(), name.size());
    lua_pushnumber(L, progress);
    lua_pushstring(L, status);
    if (lua_pcall(L, 3, 0, 0) != 0) {
        const char* error = lua_tostring(L, -1);
        w3dLogfE("[scene] async progress callback failed: %s",
                 error ? error : "unknown Lua error");
        lua_pop(L, 1);
    }
}

static void NotificarAsync(const char* status, float progress) {
    if (!gAsyncScene.active) return;
    InvocarCallbackAsync(gAsyncScene.callbackState, gAsyncScene.callbackRef,
                         gAsyncScene.name, progress, status);
}

static void TerminarAsync(const char* status) {
    if (!gAsyncScene.active) return;
    lua_State* L = gAsyncScene.callbackState;
    int callbackRef = gAsyncScene.callbackRef;
    const std::string name = gAsyncScene.name;
    const float progress = gAsyncScene.ready ? 1.0f :
        (gAsyncScene.total ? (float)gAsyncScene.completed / (float)gAsyncScene.total : 0.0f);
    gAsyncScene = W3dAsyncSceneRequest();
    InvocarCallbackAsync(L, callbackRef, name, progress, status);
    if (L && callbackRef != LUA_NOREF && callbackRef != LUA_REFNIL)
        luaL_unref(L, LUA_REGISTRYINDEX, callbackRef);
}

static void CancelarAsync() {
    if (!gAsyncScene.active) return;
    lua_State* L = gAsyncScene.callbackState;
    int callbackRef = gAsyncScene.callbackRef;
    gAsyncScene = W3dAsyncSceneRequest();
    if (L && callbackRef != LUA_NOREF && callbackRef != LUA_REFNIL)
        luaL_unref(L, LUA_REGISTRYINDEX, callbackRef);
}

static void AsyncPrepararPaso() {
    if (!gAsyncScene.active || gAsyncScene.ready) return;
    if (!gAsyncScene.started) {
        gAsyncScene.started = true;
        return;
    }
    const unsigned long id = gAsyncScene.id;
    const size_t batchSize = 32;
    size_t remaining = gAsyncScene.total - gAsyncScene.completed;
    size_t step = remaining < batchSize ? remaining : batchSize;
    gAsyncScene.completed += step;
    const float progress = gAsyncScene.total
        ? (float)gAsyncScene.completed / (float)gAsyncScene.total : 1.0f;
    NotificarAsync("loading", progress);
    if (!gAsyncScene.active || gAsyncScene.id != id || gAsyncScene.completed < gAsyncScene.total)
        return;
    gAsyncScene.ready = true;
    NotificarAsync("ready", 1.0f);
    if (!gAsyncScene.active || gAsyncScene.id != id) return;
    if (gAsyncScene.activateWhenReady) {
        gHayCambio = true;
        gCambioNombre = gAsyncScene.name;
        gReiniciarPend = false;
        gCambioAsync = true;
    }
}

void W3dEscenaRegistrarTodas() {
    CancelarAsync();
    gEscenas.clear();
    gInited.clear();
    gEscActiva = 0;
    gHayCambio = false; gReiniciarPend = false; gCambioNombre.clear();
    gCambioAsync = false;
    if (!SceneCollection) return;
    for (size_t i = 0; i < SceneCollection->Childrens.size(); i++) {
        Object* o = SceneCollection->Childrens[i];
        if (!o) continue;
        if (o->getType() == ObjectType::scene)
            gEscenas[o->name] = o;
        else if (o->getType() == ObjectType::ui && o->Parent == SceneCollection) {
            LegacyUIScene* legacy = dynamic_cast<LegacyUIScene*>(o);
            if (!legacy || legacy->cargada) gEscenas[o->name] = o;
        }
    }
}

bool W3dEscenaHayEscenas() { return !gEscenas.empty(); }
Object* W3dEscenaActivaObjeto() { return gEscActiva; }

Object* W3dEscenaBuscar(const std::string& nombre) {
    std::map<std::string, Object*>::iterator it = gEscenas.find(nombre);
    return (it != gEscenas.end()) ? it->second : 0;
}

void W3dEscenaNombreCambiado(Object* escena, const std::string& nombreAnterior,
                             const std::string& nombreNuevo) {
    if (!escena || nombreAnterior == nombreNuevo) return;
    std::map<std::string, Object*>::iterator old = gEscenas.find(nombreAnterior);
    if (old != gEscenas.end() && old->second == escena)
        gEscenas.erase(old);
    if (escena->getType() == ObjectType::scene ||
        (escena->getType() == ObjectType::ui && escena->Parent == SceneCollection))
        gEscenas[nombreNuevo] = escena;
    if (gInicial == nombreAnterior) gInicial = nombreNuevo;
}

void W3dEscenaUIEn(Object* raiz, std::vector<UI*>& lienzos) {
    if (!raiz) return;
    if (raiz->getType() == ObjectType::ui) {
        lienzos.push_back((UI*)raiz);
        return;
    }
    for (size_t i = 0; i < raiz->Childrens.size(); ++i)
        W3dEscenaUIEn(raiz->Childrens[i], lienzos);
}

UI* W3dEscenaActiva() {
    if (!gEscActiva) return 0;
    std::vector<UI*> lienzos;
    W3dEscenaUIEn(gEscActiva, lienzos);
    return lienzos.empty() ? 0 : lienzos[0];
}

void W3dEscenaSetInicial(const std::string& nombre) { gInicial = nombre; }
const std::string& W3dEscenaInicial() { return gInicial; }
void W3dEscenaSetModo(bool multi) { gModoMulti = multi; }
bool W3dEscenaModo() { return gModoMulti; }

// muestra SOLO 'nueva' entre las escenas registradas (el resto se oculta: dejan de
// correr y de recibir input; el overlay no las dibuja por visible=false).
static void MostrarSolo(Object* nueva) {
    for (std::map<std::string, Object*>::iterator it = gEscenas.begin(); it != gEscenas.end(); ++it) {
        Object* raiz = it->second;
        if (raiz) raiz->visible = raiz == nueva;
    }
}

void W3dEscenaArrancar() {
    if (gEscenas.empty()) return;
    Object* ini = W3dEscenaBuscar(gInicial);
    if (!ini) ini = gEscenas.begin()->second;   // fallback: la primera escena registrada
    MostrarSolo(ini);
    gEscActiva = ini;
    if (gInitFn) gInitFn(ini, false);            // init de la escena inicial (el resto, perezoso)
    gInited[ini] = true;
}

void W3dEscenaPedirCambio(const std::string& nombre, bool reiniciar) {
    CancelarAsync();
    // ignorar un nombre inexistente: no romper el juego por un typo del script
    if (!W3dEscenaBuscar(nombre)) return;
    gHayCambio = true; gCambioNombre = nombre; gReiniciarPend = reiniciar;
    gCambioAsync = false;
}

bool W3dEscenaPedirCambioAsync(const std::string& nombre, bool activarAlTerminar,
                               void* luaState, int callbackIndex) {
    lua_State* L = (lua_State*)luaState;
    int callbackRef = LUA_NOREF;
    if (L && lua_type(L, callbackIndex) == LUA_TFUNCTION) {
        lua_pushvalue(L, callbackIndex);
        callbackRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    Object* root = W3dEscenaBuscar(nombre);
    if (!root) {
        InvocarCallbackAsync(L, callbackRef, nombre, 0.0f, "error");
        if (L && callbackRef != LUA_NOREF && callbackRef != LUA_REFNIL)
            luaL_unref(L, LUA_REGISTRYINDEX, callbackRef);
        return false;
    }

    CancelarAsync();
    gHayCambio = false;
    gReiniciarPend = false;
    gCambioAsync = false;
    gAsyncScene.active = true;
    gAsyncScene.started = false;
    gAsyncScene.ready = false;
    gAsyncScene.activateWhenReady = activarAlTerminar;
    gAsyncScene.root = root;
    gAsyncScene.name = nombre;
    gAsyncScene.total = ContarNodosEscena(root);
    gAsyncScene.completed = 0;
    gAsyncScene.callbackState = L;
    gAsyncScene.callbackRef = callbackRef;
    gAsyncScene.id = ++gAsyncSceneId;
    NotificarAsync("loading", 0.0f);
    return true;
}

bool W3dEscenaAsyncReadyLoad(const std::string& nombre) {
    if (!gAsyncScene.active || !gAsyncScene.ready) return false;
    if (!nombre.empty() && nombre != gAsyncScene.name) return false;
    gHayCambio = true;
    gCambioNombre = gAsyncScene.name;
    gReiniciarPend = false;
    gCambioAsync = true;
    return true;
}

void W3dEscenaAplicarPendiente() {
    AsyncPrepararPaso();
    if (!gHayCambio) return;
    gHayCambio = false;
    Object* nueva = W3dEscenaBuscar(gCambioNombre);
    const bool asyncChange = gCambioAsync;
    gCambioAsync = false;
    if (!nueva) {
        gReiniciarPend = false;
        if (asyncChange) TerminarAsync("error");
        return;
    }
    bool yaInit = (gInited.find(nueva) != gInited.end() && gInited[nueva]);
    MostrarSolo(nueva);            // ocultar la activa anterior, mostrar la nueva
    gEscActiva = nueva;
    // init perezoso (primera vez) o reinicio forzado (changeScene(n, true))
    if (!yaInit || gReiniciarPend) {
        if (gInitFn) gInitFn(nueva, gReiniciarPend && yaInit);   // reiniciar solo si ya estaba iniciada
        gInited[nueva] = true;
    }
    gReiniciarPend = false;
    if (asyncChange) TerminarAsync("active");
}

Object* W3dEscenaRaizDe(Object* o) {
    Object* raizUiLegacy = 0;
    for (Object* p = o; p; p = p->Parent) {
        if (p->getType() == ObjectType::scene) return p;
        if (p->getType() == ObjectType::ui) raizUiLegacy = p;
    }
    if (raizUiLegacy && raizUiLegacy->Parent == SceneCollection)
        return raizUiLegacy;
    return SceneCollection;   // fuera de toda escena UI (3D): la busqueda de refs sigue igual
}

bool W3dEscenaEsDeActiva(Object* o) {
    if (!o) return false;
    if (!o->visible) return false;        // invisible: no corre (regla del editor de siempre)
    if (gEscenas.empty()) return true;    // without scenes, visibility is the only rule
    Object* raiz = W3dEscenaRaizDe(o);
    if (!raiz || (raiz->getType() != ObjectType::ui &&
                  raiz->getType() != ObjectType::scene)) return true;
    return raiz == gEscActiva;
}

void W3dEscenaLimpiar() {
    CancelarAsync();
    // el editor vuelve a EDICION: deshacer el MostrarSolo del Play (que dejo OCULTAS las
    // escenas no-activas con visible=false). Al cargar, TODAS las ventanas UI nacen visibles
    // (el .w3dui no guarda "visible" de la ventana), asi que restaurar a visible=true deja el
    // outliner como recien abierto y el editor puede volver a mostrar/editar cualquier escena.
    for (std::map<std::string, Object*>::iterator it = gEscenas.begin(); it != gEscenas.end(); ++it)
        if (it->second) it->second->visible = true;
    gEscenas.clear();
    gInited.clear();
    gEscActiva = 0;
    gHayCambio = false; gReiniciarPend = false; gCambioNombre.clear();
    gCambioAsync = false;
    // OJO: NO se toca gInicial (es un ajuste del proyecto, lo fija el importador/runtime)
}

// ===========================================================================
//  W3dPaletas — las paletas de colores a NIVEL PROYECTO (ver W3dPaletas.h).
//  Viven aca porque W3dEscena.cpp compila en el editor Y en todos los
//  runtimes standalone (consola/calculadora/whiskpaddle, linux/web/android).
// ===========================================================================
static std::vector<Paleta> gPaletas;
static bool gPaletasInit = false;

std::vector<Paleta>& W3dPaletas() {
    if (!gPaletasInit) { gPaletas.reserve(8); gPaletasInit = true; }
    return gPaletas;
}

void W3dPaletasLimpiar() { W3dPaletas().clear(); }

Paleta* W3dPaletaPorNombre(const std::string& n) {
    if (n.empty()) return 0;
    std::vector<Paleta>& ps = W3dPaletas();
    for (size_t i = 0; i < ps.size(); i++)
        if (ps[i].nombre == n) return &ps[i];
    return 0;
}

// INVARIANTE 1 al adoptar: completar las paletas CORTAS para que todas midan
// igual. El color que falta en el indice c se copia de la primera paleta que
// lo tenga (mismo nombre y valor: un default razonable).
static void PalNormalizarLargos() {
    std::vector<Paleta>& ps = W3dPaletas();
    size_t maxN = 0;
    for (size_t i = 0; i < ps.size(); i++)
        if (ps[i].colores.size() > maxN) maxN = ps[i].colores.size();
    for (size_t i = 0; i < ps.size(); i++)
        for (size_t c = ps[i].colores.size(); c < maxN; c++) {
            PaletaColor pc;
            pc.nombre = "Color";
            pc.rgba[0] = pc.rgba[1] = pc.rgba[2] = pc.rgba[3] = 1.0f;
            for (size_t j = 0; j < ps.size(); j++)
                if (c < ps[j].colores.size()) { pc = ps[j].colores[c]; break; }
            ps[i].colores.push_back(pc);
        }
}

void W3dPaletasAdoptar(const std::vector<Paleta>& nuevas) {
    std::vector<Paleta>& ps = W3dPaletas();
    for (size_t i = 0; i < nuevas.size(); i++) {
        if (ps.size() >= 8) break;                          // tope (reserve del store)
        if (W3dPaletaPorNombre(nuevas[i].nombre)) continue; // merge: la primera gana
        Paleta pa;
        pa.nombre = nuevas[i].nombre;
        ps.push_back(pa);
        // reserve sobre el vector VIVO: los punteros a los rgba quedan estables
        ps.back().colores.reserve(32);
        for (size_t c = 0; c < nuevas[i].colores.size() && c < 32; c++)
            ps.back().colores.push_back(nuevas[i].colores[c]);
    }
    PalNormalizarLargos();
}

Paleta* W3dPaletaEfectiva(Object* o) {
    for (Object* p = o; p; p = p->Parent) {
        if (!p->paleta.empty()) {
            Paleta* pa = W3dPaletaPorNombre(p->paleta);
            if (pa) return pa;   // nombre roto: se sigue subiendo (tolerante)
        }
        if (p->getType() == ObjectType::ui && p->paleta.empty()) {
            // RETROCOMPAT: una raiz UI sin seleccion propia cae a su
            // paletaActiva de siempre (-1 = deseleccionada -> propio). Si el
            // proyecto tiene una paleta con ese nombre, manda la del proyecto
            // (la copia local del UI puede estar vieja).
            UI* u = (UI*)p;
            if (u->paletaActiva >= 0 && u->paletaActiva < (int)u->paletas.size()) {
                Paleta* local = &u->paletas[u->paletaActiva];
                Paleta* proy = W3dPaletaPorNombre(local->nombre);
                return proy ? proy : local;
            }
            return 0;
        }
    }
    return 0;   // nadie eligio: colores propios
}

std::vector<PaletaColor>* W3dColoresEfectivos(Object* o) {
    Paleta* pa = W3dPaletaEfectiva(o);
    return pa ? &pa->colores : 0;
}

// nombre libre de COLOR de paleta. Los nombres van por INDICE y son los MISMOS en todas
// las paletas (invariante), asi que el espacio de nombres es la lista de la paleta 0.
// 'excepto' = indice que puede conservar el suyo (-1 = ninguno).
static bool PalColorExiste(const std::string& n, void* ctx) {
    int excepto = *(int*)ctx;
    std::vector<Paleta>& ps = W3dPaletas();
    if (ps.empty()) return false;
    for (size_t i = 0; i < ps[0].colores.size(); i++)
        if ((int)i != excepto && ps[0].colores[i].nombre == n) return true;
    return false;
}
std::string W3dPaletaColorNombreLibre(const std::string& base, int excepto) {
    return W3dNombreUnico(base, "Color", PalColorExiste, &excepto);
}

// INVARIANTE 1: el color nuevo va a TODAS las paletas (mismo nombre y valor)
int W3dPaletaAgregarColor(const std::string& nombre, const float* rgba) {
    std::vector<Paleta>& ps = W3dPaletas();
    if (ps.empty()) return -1;                      // sin paletas no hay donde
    if (ps[0].colores.size() >= 32) return -1;      // tope del reserve (punteros estables)
    const std::string unico = W3dPaletaColorNombreLibre(nombre, -1);
    for (size_t i = 0; i < ps.size(); i++) {
        PaletaColor pc;
        pc.nombre = unico;
        for (int q = 0; q < 4; q++) pc.rgba[q] = rgba ? rgba[q] : 1.0f;
        ps[i].colores.push_back(pc);
    }
    return (int)ps[0].colores.size() - 1;
}

// corrige UNA referencia pal* contra el borrado del indice 'idx' (regla 2):
// mayores se corren -1; la que apuntaba a idx pasa a color PROPIO con el
// valor EFECTIVO horneado (nada queda apuntando a memoria basura). Se llama
// ANTES de borrar el color de las paletas (el valor a hornear aun existe).
static void PalFixRef(Object* o, int* pal, float* propio, int idx) {
    if (*pal < 0) return;
    if (*pal == idx) {
        std::vector<PaletaColor>* cs = W3dColoresEfectivos(o);
        if (cs && idx < (int)cs->size())
            for (int q = 0; q < 4; q++) propio[q] = (*cs)[idx].rgba[q];
        *pal = -1;
    } else if (*pal > idx) {
        (*pal)--;
    }
}

// recorre TODO el arbol (todas las escenas del proyecto) corrigiendo los
// campos pal* de cada tipo de elemento 2D
static void PalFixRefsRec(Object* o, int idx) {
    if (!o) return;
    ObjectType t = o->getType();
    if (t == ObjectType::texto2d) {
        Texto2D* tx = (Texto2D*)o;
        PalFixRef(o, &tx->palColor, tx->color, idx);
    } else if (t == ObjectType::rect2d) {
        Rect2D* r = (Rect2D*)o;
        PalFixRef(o, &r->palColor, r->color, idx);
    } else if (t == ObjectType::imagen2d) {
        Imagen2D* im = (Imagen2D*)o;
        PalFixRef(o, &im->palTinte, im->color, idx);
    } else if (t == ObjectType::slice9) {
        Slice9* s9 = (Slice9*)o;
        PalFixRef(o, &s9->palTinte, s9->color, idx);
    } else if (t == ObjectType::boton2d) {
        Boton2D* b = (Boton2D*)o;
        PalFixRef(o, &b->palFondo, b->colorFondo, idx);
        PalFixRef(o, &b->palTexto, b->colorTexto, idx);
        PalFixRef(o, &b->palBorde, b->colorBorde, idx);
        PalFixRef(o, &b->palHover, b->colorHover, idx);
    }
    for (size_t i = 0; i < o->Childrens.size(); i++)
        PalFixRefsRec(o->Childrens[i], idx);
}

void W3dPaletaBorrarColor(int idx) {
    std::vector<Paleta>& ps = W3dPaletas();
    if (ps.empty() || idx < 0 || idx >= (int)ps[0].colores.size()) return;
    // regla 2, paso 1: PRIMERO corregir las referencias del proyecto entero
    // (el valor horneado se lee de la paleta efectiva ANTES de borrar)
    PalFixRefsRec(SceneCollection, idx);
    // paso 2: borrar el MISMO indice en TODAS las paletas (invariante 1)
    for (size_t i = 0; i < ps.size(); i++)
        if (idx < (int)ps[i].colores.size())
            ps[i].colores.erase(ps[i].colores.begin() + idx);
    // regla 3: sin colores -> DESELECCIONAR las paletas activas legacy de las
    // raices UI (todo ya quedo en color propio por el paso 1)
    if (!ps.empty() && ps[0].colores.empty() && SceneCollection)
        for (size_t i = 0; i < SceneCollection->Childrens.size(); i++) {
            Object* o = SceneCollection->Childrens[i];
            if (o && o->getType() == ObjectType::ui) ((UI*)o)->paletaActiva = -1;
        }
}

// nombre libre de PALETA (espacio GLOBAL de proyecto; el nombre es la CLAVE que usan
// Object::paleta y W3dPaletaEfectiva). 'excepto' = indice que conserva el suyo.
static bool PalNombreExiste(const std::string& n, void* ctx) {
    int excepto = *(int*)ctx;
    std::vector<Paleta>& ps = W3dPaletas();
    for (size_t i = 0; i < ps.size(); i++)
        if ((int)i != excepto && ps[i].nombre == n) return true;
    return false;
}
std::string W3dPaletaNombreLibre(const std::string& base, int excepto) {
    return W3dNombreUnico(base, "Paleta", PalNombreExiste, &excepto);
}

int W3dPaletaNueva(const std::string& nombre, int baseIdx) {
    std::vector<Paleta>& ps = W3dPaletas();
    if (ps.size() >= 8) return -1;
    Paleta pa;
    // antes RECHAZABA el nombre tomado (return -1). Ahora SUFIJA, como todo el resto
    // del editor (pedido del dueno: "ponele el .001 si queres repetirlo").
    pa.nombre = W3dPaletaNombreLibre(nombre, -1);
    ps.push_back(pa);
    ps.back().colores.reserve(32);                  // punteros estables (ver arriba)
    // COPIA de la paleta base (invariante 1: mismos largos y nombres)
    if (baseIdx < 0 || baseIdx >= (int)ps.size() - 1) baseIdx = 0;
    if ((int)ps.size() > 1)
        for (size_t c = 0; c < ps[baseIdx].colores.size(); c++)
            ps.back().colores.push_back(ps[baseIdx].colores[c]);
    return (int)ps.size() - 1;
}

// los objetos que ELEGIAN la paleta borrada vuelven a heredar ("")
static void PalLimpiarSelRec(Object* o, const std::string& nombre) {
    if (!o) return;
    if (o->paleta == nombre) o->paleta.clear();
    for (size_t i = 0; i < o->Childrens.size(); i++)
        PalLimpiarSelRec(o->Childrens[i], nombre);
}

void W3dPaletaBorrarPaleta(int idx) {
    std::vector<Paleta>& ps = W3dPaletas();
    if (idx < 0 || idx >= (int)ps.size()) return;
    std::string nombre = ps[idx].nombre;
    ps.erase(ps.begin() + idx);
    PalLimpiarSelRec(SceneCollection, nombre);
}

static void PalRenombrarSelRec(Object* o, const std::string& de, const std::string& a) {
    if (!o) return;
    if (o->paleta == de) o->paleta = a;
    for (size_t i = 0; i < o->Childrens.size(); i++)
        PalRenombrarSelRec(o->Childrens[i], de, a);
}

void W3dPaletaRenombrar(int idx, const std::string& nuevo) {
    std::vector<Paleta>& ps = W3dPaletas();
    if (idx < 0 || idx >= (int)ps.size()) return;
    // antes DESCARTABA el rename si el nombre estaba tomado (se perdia en silencio);
    // ahora sufija con LA regla comun. El vinculo objeto->paleta (Object::paleta, por
    // NOMBRE) se arrastra igual que siempre con PalRenombrarSelRec.
    const std::string unico = W3dPaletaNombreLibre(nuevo, idx);
    if (ps[idx].nombre == unico) return;
    std::string viejo = ps[idx].nombre;
    ps[idx].nombre = unico;
    PalRenombrarSelRec(SceneCollection, viejo, unico);
}

// ---------------------------------------------------------------------------
//  Lua bind: changeScene(name[, restart]).
// ---------------------------------------------------------------------------
static int LChangeScene(lua_State* L) {
    const char* n = lua_tostring(L, 1);
    bool re = lua_toboolean(L, 2) != 0;
    if (n) W3dEscenaPedirCambio(n, re);
    return 0;
}

static int LChangeSceneAsync(lua_State* L) {
    const char* name = lua_tostring(L, 1);
    if (!name) {
        lua_pushboolean(L, 0);
        return 1;
    }
    bool activateWhenReady = lua_toboolean(L, 2) != 0;
    bool queued = W3dEscenaPedirCambioAsync(name, activateWhenReady, L, 3);
    lua_pushboolean(L, queued ? 1 : 0);
    return 1;
}

static int LSceneAsyncReadyLoad(lua_State* L) {
    const char* name = lua_tostring(L, 1);
    lua_pushboolean(L, W3dEscenaAsyncReadyLoad(name ? name : "") ? 1 : 0);
    return 1;
}

void W3dEscenaRegistrarBind(void* Lv) {
    lua_State* L = (lua_State*)Lv;
    lua_pushcfunction(L, LChangeScene); lua_setglobal(L, "changeScene");
    // Keep existing projects working while scripts migrate to the English name.
    lua_pushcfunction(L, LChangeScene); lua_setglobal(L, "cambiarEscena");
    lua_pushcfunction(L, LChangeSceneAsync); lua_setglobal(L, "changeSceneAsync");
    lua_pushcfunction(L, LSceneAsyncReadyLoad); lua_setglobal(L, "sceneAsyncReadyLoad");
    // Accept the original misspelling as an alias for scripts using the requested name.
    lua_pushcfunction(L, LSceneAsyncReadyLoad); lua_setglobal(L, "sceneAsycReadyLoad");
}
