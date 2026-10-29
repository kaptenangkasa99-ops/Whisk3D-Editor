#ifndef W3DESCENA_H
#define W3DESCENA_H
#include <string>
#include <vector>

// ============================================================================
//  W3dEscena — el motor MULTI-ESCENA, COMPARTIDO por el editor (Play, via
//  SimJuego) y el runtime compilado (game/w3drun), para que un juego con varias
//  pantallas corra IGUAL en los dos lados.
//
//  A logical scene is either a new ObjectType::scene root (world objects plus
//  separate UI canvases) or a legacy top-level UI root from an older project.
//  SceneCollection remains the project-wide container.
//
//  El cambio de escena se pide con changeScene(name) desde Lua; se APLICA
//  al FINAL del frame (no en el callback, para no mutar la iteracion de actualizar).
//  Al aplicarse: oculta la activa anterior, muestra la nueva, y hace init PEREZOSO
//  (si la nueva nunca corrio inicio() se le cargan+resuelven+inicio() sus scripts;
//  si ya corrio, se reanuda sin re-inicio: su estado lua se preserva).
//
//  Compatibility: legacy .w3dui roots remain valid scenes. With no registered
//  logical scenes (for example a plain 3D project), scripts retain visibility-based
//  execution.
//
//  C++03 puro (mismo codigo para Android/Symbian). Comentarios sin acentos.
// ============================================================================

class Object;
class UI;

// Callback invoked when a logical scene starts. Legacy UI-root scenes remain
// valid scene roots; new scenes are ObjectType::scene objects.
typedef void (*W3dEscenaInitFn)(Object* escena, bool reiniciar);
void W3dEscenaSetInit(W3dEscenaInitFn fn);

// (Re)builds the name-to-scene index from SceneCollection children.
// No corre ningun inicio(); resetea activa/inited/pendiente (NO la escena inicial).
void W3dEscenaRegistrarTodas();
// Is at least one logical scene registered?
bool W3dEscenaHayEscenas();
// Root of the active logical scene (new scene root or legacy UI root).
Object* W3dEscenaActivaObjeto();
// Primary canvas of the active scene, retained for canvas-size callers.
UI* W3dEscenaActiva();
// busca una escena por nombre (NULL si no existe)
Object* W3dEscenaBuscar(const std::string& nombre);
void W3dEscenaNombreCambiado(Object* escena, const std::string& nombreAnterior,
                             const std::string& nombreNuevo);

// nombre de la escena INICIAL (la que arranca). Lo fija el importador del .w3d o el
// runtime; PERSISTE (NO lo borran RegistrarTodas / Limpiar: es un ajuste del proyecto).
void W3dEscenaSetInicial(const std::string& nombre);
const std::string& W3dEscenaInicial();
// modo multi-escena declarado por el proyecto (informativo por ahora)
void W3dEscenaSetModo(bool multi);
bool W3dEscenaModo();

// ACTIVA la escena inicial (o la primera registrada si no hay una fijada): la muestra,
// oculta el resto y corre su init (via el callback). Las demas quedan sin inicir (perezoso).
void W3dEscenaArrancar();

// pide un cambio de escena; se aplica al FINAL del frame (W3dEscenaAplicarPendiente),
// NO en el acto (para no mutar la iteracion de actualizar). reiniciar=true fuerza el
// re-inicio de la escena destino. Un nombre inexistente se ignora (no rompe el juego).
void W3dEscenaPedirCambio(const std::string& nombre, bool reiniciar = false);
// Incremental, main-thread preparation for an already resident scene. The
// optional Lua callback receives (name, fraction, status).
bool W3dEscenaPedirCambioAsync(const std::string& nombre, bool activarAlTerminar,
                               void* luaState, int callbackIndex);
// Activate a prepared async request. An empty name selects the current request.
bool W3dEscenaAsyncReadyLoad(const std::string& nombre);
// aplica el cambio pendiente (si hay): oculta la activa, muestra la nueva, init perezoso
// de la nueva si nunca corrio. Llamar UNA vez al final de cada frame.
void W3dEscenaAplicarPendiente();

// True if the object belongs to the active scene and is visible. Objects outside
// logical scene roots retain the legacy visibility-based behavior.
bool W3dEscenaEsDeActiva(Object* o);
// Logical scene root containing the object, or SceneCollection for project-global objects.
Object* W3dEscenaRaizDe(Object* o);
// Collect UI canvas roots owned by a scene. Legacy UI-root scenes include
// themselves; new scene roots include their descendant UI canvases.
void W3dEscenaUIEn(Object* raiz, std::vector<UI*>& lienzos);

// limpia TODO (mapa/activa/inited/pendiente); NO borra la escena inicial. Lo llama el STOP.
void W3dEscenaLimpiar();

// registra el bind Lua changeScene(name[, restart]). Lo llama el punto de binds
// COMPARTIDO (BindsJuegoRegistrar), asi el editor y el runtime lo exponen identico.
void W3dEscenaRegistrarBind(void* L);

#endif // W3DESCENA_H
