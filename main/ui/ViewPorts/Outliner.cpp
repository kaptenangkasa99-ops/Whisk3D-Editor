#include "w3dGraphics.h" // Graphics abstraction (OpenGL independence).
#ifdef _WIN32
    #include <windows.h>
#endif

#include "Outliner.h"
#include "W3dLang.h"
#include "ViewPorts/Properties.h"
#include "WhiskUI/draw/glesdraw.h"
#include "render/OpcionesRender.h" // g_redraw (CenterSelection requests a redraw).
#include "objects/ObjectMode.h" // Reparenting for drag and drop.
#include "objects/Instance.h"   // ObjectIcon: array, mirror, or instance depending on the mode.
#include "objects/Mesh.h"       // Mesh::armatures2d (one virtual blue row per 2D rig beneath its mesh).
#include "objects/UI.h"
#include "objects/LegacyUIScene.h"
#include "objects/SceneData.h"
#include "W3dEscena.h"
#include "ViewPorts/LayoutInput.h"
#include "io/GuardarW3D.h"
#include "io/UI2DFormato.h"
#include "io/W3dContenedor.h"
#include "script/W3dScript.h"
#include "w3dFilesystem.h"
#include "ViewPorts/PopUp/FileBrowser.h"
#include "variables.h"
#include "ViewPorts/PopUp/ConfirmarPopup.h" // AbrirConfirmarBorrado (confirm before deleting).
#include "ViewPorts/Notificaciones.h"       // Notificar (keyboard-move rename notifications).
#include "Undo.h"                           // UndoCapturarRenames (one step when confirming a move).
#include <cstdio>                           // sprintf (notification counter).
#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
    extern int W3dPantallaAlto;  // Screen height (Y flip; glesdraw.cpp).
    extern int ShiftCount;
    extern bool middleMouseDown;
    extern bool MouseWheel;
#else
    #include <GL/gl.h>
#endif

static LegacyUIScene* gEscenaAccionOutliner = NULL;
static Object* gSceneSaveTarget = NULL;
static std::string gSceneSavePath;
static bool gSceneSaveUnloadAfterSave = false;
static SceneData* gSceneDataActionTarget = NULL;
static SceneData* gSceneLoadPlaceholder = NULL;
static Object* gSceneRemoveTarget = NULL;

static bool IsSceneRoot(Object* object) {
    return SceneCollection && object && object->Parent == SceneCollection &&
           (object->getType() == ObjectType::scene || object->getType() == ObjectType::ui);
}

static void SaveSceneOverwrite();
static void PromptSceneSaveConflict();

static void SaveSceneRenameComplete() {
    Object* scene = gSceneSaveTarget;
    if (!scene || gSceneSavePath.empty()) return;
    const std::string folder = w3dFileSystem::ParentPath(gSceneSavePath);
    gSceneSavePath = W3dRutaEnCarpeta(folder, W3dSlugEntrada(scene->name), ".w3dscene");
    if (w3dFileSystem::FileExists(gSceneSavePath)) {
        PromptSceneSaveConflict();
        return;
    }
    SaveSceneOverwrite();
}

static void SaveSceneRename() {
    if (gSceneSaveTarget)
        PropsIniciarRenombreEscena(gSceneSaveTarget, SaveSceneRenameComplete);
}

static void SaveSceneOverwrite() {
    Object* scene = gSceneSaveTarget;
    const std::string path = gSceneSavePath;
    const bool unloadAfterSave = gSceneSaveUnloadAfterSave;
    gSceneSaveTarget = NULL;
    gSceneSavePath.clear();
    gSceneSaveUnloadAfterSave = false;
    if (!scene || path.empty()) return;
    if (!GuardarW3DEscena(path, scene)) {
        Notificar(T("The scene could not be saved; it remains loaded."), true);
        return;
    }
    SceneData* data = dynamic_cast<SceneData*>(scene);
    if (data) data->archivoEscena = path;
    if (unloadAfterSave && data) W3dSceneDataUnload(data);
}

static void PromptSceneSaveConflict() {
    Object* scene = gSceneSaveTarget;
    if (!scene || gSceneSavePath.empty()) return;
    if (!confirmarPopup) confirmarPopup = new ConfirmarPopup();
    confirmarPopup->AbrirTresOpciones(
        "A scene file named \"" + W3dSlugEntrada(scene->name) +
            "\" already exists. Rename this scene, overwrite the file, or cancel.",
        T("Rename"), SaveSceneRename, T("Overwrite"), SaveSceneOverwrite);
}

static void SaveSceneChosen(const std::string& chosen) {
    Object* scene = gSceneSaveTarget;
    if (!scene || !IsSceneRoot(scene)) {
        gSceneSaveTarget = NULL;
        return;
    }
    gSceneSavePath = W3dRutaDeSalida(chosen, W3dSlugEntrada(scene->name), ".w3dscene");
    if (w3dFileSystem::FileExists(gSceneSavePath)) {
        PromptSceneSaveConflict();
        return;
    }
    SaveSceneOverwrite();
}

static void StartSaveScene(Object* scene, bool unloadAfterSave = false) {
    if (!IsSceneRoot(scene)) return;
    gSceneSaveTarget = scene;
    gSceneSavePath.clear();
    gSceneSaveUnloadAfterSave = unloadAfterSave;
    AbrirFileBrowser(T("Save Scene"), T("Save"), ".w3dscene", SaveSceneChosen,
                     true, W3dCarpetaBaseSalida());
}

static bool FindSavedScenePath(LegacyUIScene* scene, std::string& path, bool& isContainerEntry) {
    if (!scene || scene->archivoW3dui.empty()) return false;
    const std::string& ref = scene->archivoW3dui;
    const bool absoluta = ref[0] == '/' || ref[0] == '\\' ||
                          (ref.size() > 1 && ref[1] == ':');
    if (!absoluta && !g_w3dDirProyecto.empty()) {
        std::string enProyecto = g_w3dDirProyecto + "/" + ref;
        if (w3dFileSystem::FileExists(enProyecto)) {
            path = enProyecto;
            isContainerEntry = false;
            return true;
        }
    }
    if (w3dFileSystem::FileExists(ref)) {
        path = ref;
        isContainerEntry = false;
        return true;
    }
    path = ref;
    isContainerEntry = !absoluta;
    return isContainerEntry && w3dFileSystem::FileExists(ref);
}

static void DestroySceneChildren(Object* parent) {
    while (parent && !parent->Childrens.empty()) {
        Object* child = parent->Childrens.back();
        DestroySceneChildren(child);
        delete child;
        parent->Childrens.pop_back();
    }
}

bool W3dLegacySceneUnloadData(LegacyUIScene* scene) {
    if (!scene || !scene->cargada) return false;
    UndoLimpiar();
    DeseleccionarTodo(true);
    DestroySceneChildren(scene);
    delete scene->scriptDatos;
    scene->scriptDatos = NULL;
    scene->cargada = false;
    W3dEscenaRegistrarTodas();
    return true;
}

static void SaveAndUnloadScene() {
    LegacyUIScene* scene = gEscenaAccionOutliner;
    gEscenaAccionOutliner = NULL;
    if (!scene || w3dPath.empty()) {
        Notificar(T("Save the project before unloading this scene."), true);
        return;
    }
    if (!GuardarW3D(w3dPath)) {
        Notificar(T("The project could not be saved; the scene remains loaded."), true);
        return;
    }
    W3dLegacySceneUnloadData(scene);
}

static void DiscardAndUnloadScene() {
    LegacyUIScene* scene = gEscenaAccionOutliner;
    gEscenaAccionOutliner = NULL;
    std::string path;
    bool isContainerEntry = false;
    if (!scene || !FindSavedScenePath(scene, path, isContainerEntry)) {
        Notificar(T("Save the project before unloading this new scene."), true);
        return;
    }
    (void)isContainerEntry;
    W3dLegacySceneUnloadData(scene);
}

bool W3dLegacySceneLoadData(LegacyUIScene* placeholder) {
    if (!placeholder || placeholder->cargada || !SceneCollection) return false;
    std::string path;
    bool isContainerEntry = false;
    if (!FindSavedScenePath(placeholder, path, isContainerEntry)) {
        Notificar(T("The saved scene file could not be found."), true);
        return false;
    }
    const bool refsAntes = g_w3dRefsEntradas;
    g_w3dRefsEntradas = isContainerEntry;
    UI* loadedDocument = UI2DCargar(path, true);
    g_w3dRefsEntradas = refsAntes;
    LegacyUIScene* loadedScene = dynamic_cast<LegacyUIScene*>(loadedDocument);
    if (!loadedScene) {
        if (loadedDocument) {
            Object* parent = loadedDocument->Parent;
            if (parent) {
                for (size_t i = parent->Childrens.size(); i > 0; --i)
                    if (parent->Childrens[i - 1] == loadedDocument)
                        parent->Childrens.erase(parent->Childrens.begin() + i - 1);
            } else if (SceneCollection) {
                for (size_t i = SceneCollection->Childrens.size(); i > 0; --i)
                    if (SceneCollection->Childrens[i - 1] == loadedDocument)
                        SceneCollection->Childrens.erase(SceneCollection->Childrens.begin() + i - 1);
            }
            DestroySceneChildren(loadedDocument);
            loadedDocument->Parent = NULL;
            delete loadedDocument;
        }
        Notificar(T("The saved scene could not be loaded."), true);
        return false;
    }

    Object* loadedParent = loadedScene->Parent;
    if (loadedParent) {
        for (size_t i = loadedParent->Childrens.size(); i > 0; --i)
            if (loadedParent->Childrens[i - 1] == loadedScene)
                loadedParent->Childrens.erase(loadedParent->Childrens.begin() + i - 1);
    }
    if (SceneCollection && SceneCollection != loadedParent) {
        for (size_t i = SceneCollection->Childrens.size(); i > 0; --i)
            if (SceneCollection->Childrens[i - 1] == loadedScene)
                SceneCollection->Childrens.erase(SceneCollection->Childrens.begin() + i - 1);
    }

    size_t indice = SceneCollection->Childrens.size();
    for (size_t i = 0; i < SceneCollection->Childrens.size(); i++)
        if (SceneCollection->Childrens[i] == placeholder) { indice = i; break; }
    if (indice == SceneCollection->Childrens.size()) {
        DestroySceneChildren(loadedScene);
        loadedScene->Parent = NULL;
        delete loadedScene;
        Notificar(T("The scene placeholder is no longer in the project."), true);
        return false;
    }

    loadedScene->Parent = SceneCollection;
    loadedScene->SetNameCrudo(placeholder->name);
    loadedScene->visible = placeholder->visible;
    loadedScene->renderizable = placeholder->renderizable;
    loadedScene->desplegado = placeholder->desplegado;
    loadedScene->archivoW3dui = placeholder->archivoW3dui;
    SceneCollection->Childrens[indice] = loadedScene;
    placeholder->Parent = NULL;
    delete placeholder;
    UndoLimpiar();
    W3dEscenaRegistrarTodas();
    return true;
}

bool W3dSceneDataUnload(SceneData* scene) {
    if (!scene || !scene->cargada || !IsSceneRoot(scene)) return false;
    if (W3dEscenaActiva()) {
        Notificar(T("Stop play mode before unloading a scene."), true);
        return false;
    }
    UndoLimpiar();
    DeseleccionarTodo(true);
    DestroySceneChildren(scene);
    delete scene->scriptDatos;
    scene->scriptDatos = NULL;
    scene->cargada = false;
    W3dEscenaRegistrarTodas();
    scene->Seleccionar();
    g_redraw = true;
    return true;
}

bool W3dSceneDataLoad(SceneData* placeholder) {
    if (!placeholder || placeholder->cargada || !IsSceneRoot(placeholder)) return false;
    if (W3dEscenaActiva()) {
        Notificar(T("Stop play mode before loading a scene."), true);
        return false;
    }
    const std::string path = placeholder->archivoEscena;
    if (path.empty() || !w3dFileSystem::FileExists(path)) {
        Notificar(T("The saved scene file could not be found."), true);
        return false;
    }
    size_t index = 0;
    while (index < SceneCollection->Childrens.size() &&
           SceneCollection->Childrens[index] != placeholder) ++index;
    if (index == SceneCollection->Childrens.size()) return false;

    SceneCollection->Childrens.erase(SceneCollection->Childrens.begin() + index);
    placeholder->Parent = NULL;
    W3dEscenaRegistrarTodas();
    const size_t rootsBefore = SceneCollection->Childrens.size();
    if (!W3dSceneLoadDataAdditive(path) ||
        SceneCollection->Childrens.size() != rootsBefore + 1) {
        placeholder->Parent = SceneCollection;
        SceneCollection->Childrens.insert(SceneCollection->Childrens.begin() + index,
                                           placeholder);
        W3dEscenaRegistrarTodas();
        return false;
    }

    Object* loaded = SceneCollection->Childrens.back();
    SceneData* loadedData = dynamic_cast<SceneData*>(loaded);
    if (!loadedData) {
        SceneCollection->Childrens.pop_back();
        placeholder->Parent = SceneCollection;
        SceneCollection->Childrens.insert(SceneCollection->Childrens.begin() + index,
                                           placeholder);
        W3dEscenaRegistrarTodas();
        if (loaded) {
            loaded->Parent = NULL;
            W3dLiberarSubarbol(loaded);
        }
        Notificar(T("The saved file did not contain a valid scene."), true);
        return false;
    }

    SceneCollection->Childrens.pop_back();
    SceneCollection->Childrens.insert(SceneCollection->Childrens.begin() + index, loaded);
    loadedData->archivoEscena = path;
    loadedData->Parent = SceneCollection;
    W3dLiberarSubarbol(placeholder);
    W3dEscenaRegistrarTodas();
    g_redraw = true;
    return true;
}

static void ScenePlaceholderLoadChosen(const std::string& path) {
    SceneData* placeholder = gSceneLoadPlaceholder;
    gSceneLoadPlaceholder = NULL;
    if (!placeholder || placeholder->cargada) return;
    size_t dot = path.find_last_of('.');
    std::string extension = dot == std::string::npos ? std::string() : path.substr(dot);
    for (size_t i = 0; i < extension.size(); ++i)
        if (extension[i] >= 'A' && extension[i] <= 'Z')
            extension[i] = (char)(extension[i] - 'A' + 'a');
    if (extension != ".w3dscene") {
        Notificar(T("Choose a .w3dscene file to reload this scene."), true);
        return;
    }
    const std::string oldPath = placeholder->archivoEscena;
    placeholder->archivoEscena = path;
    if (!W3dSceneDataLoad(placeholder)) placeholder->archivoEscena = oldPath;
}

static void StartScenePlaceholderLoad(SceneData* placeholder) {
    if (!placeholder || placeholder->cargada) return;
    gSceneLoadPlaceholder = placeholder;
    AbrirFileBrowser(T("Load Scene"), T("Open"), ".w3dscene",
                     ScenePlaceholderLoadChosen);
}

static void SceneDataUnloadDiscard() {
    SceneData* scene = gSceneDataActionTarget;
    gSceneDataActionTarget = NULL;
    if (!scene) return;
    if (scene->archivoEscena.empty() ||
        !w3dFileSystem::FileExists(scene->archivoEscena)) {
        Notificar(T("This scene has no saved .w3dscene file to reload. Save it before unloading."), true);
        return;
    }
    W3dSceneDataUnload(scene);
}

static void SceneDataUnloadSave() {
    SceneData* scene = gSceneDataActionTarget;
    gSceneDataActionTarget = NULL;
    if (scene) StartSaveScene(scene, true);
}

static void ToggleSceneDataLoadedState(SceneData* scene) {
    if (!scene) return;
    if (!scene->cargada) {
        if (scene->archivoEscena.empty() ||
            !w3dFileSystem::FileExists(scene->archivoEscena))
            StartScenePlaceholderLoad(scene);
        else W3dSceneDataLoad(scene);
        return;
    }
    if (W3dEscenaActiva()) {
        Notificar(T("Stop play mode before unloading a scene."), true);
        return;
    }
    gSceneDataActionTarget = scene;
    if (!confirmarPopup) confirmarPopup = new ConfirmarPopup();
    confirmarPopup->AbrirTresOpciones(
        std::string(T("Unload scene")) + " \"" + scene->name + "\"? " +
            T("Save this scene, discard its unsaved edits, or cancel."),
        T("Save"), SceneDataUnloadSave, T("Discard"), SceneDataUnloadDiscard);
}

static void RemoveSceneConfirmed() {
    Object* scene = gSceneRemoveTarget;
    gSceneRemoveTarget = NULL;
    W3dSceneRemoveRoot(scene);
}

bool W3dSceneRemoveRoot(Object* scene) {
    if (!IsSceneRoot(scene)) return false;
    if (W3dEscenaActiva()) {
        Notificar(T("Stop play mode before removing a scene."), true);
        return false;
    }
    size_t index = 0;
    while (index < SceneCollection->Childrens.size() &&
           SceneCollection->Childrens[index] != scene) ++index;
    if (index == SceneCollection->Childrens.size()) return false;
    DeseleccionarTodo(true);
    UndoLimpiar();
    SceneCollection->Childrens.erase(SceneCollection->Childrens.begin() + index);
    scene->Parent = NULL;
    W3dLiberarSubarbol(scene);
    W3dEscenaRegistrarTodas();
    g_redraw = true;
    return true;
}

static void ConfirmRemoveScene(Object* scene) {
    if (!IsSceneRoot(scene)) return;
    gSceneRemoveTarget = scene;
    if (!confirmarPopup) confirmarPopup = new ConfirmarPopup();
    confirmarPopup->Abrir(
        std::string(T("Remove scene")) + " \"" + scene->name +
            "\" from this project? This cannot be undone.",
        RemoveSceneConfirmed);
}

static void ToggleSceneLoadedState(LegacyUIScene* scene) {
    if (!scene) return;
    if (!scene->cargada) {
        W3dLegacySceneLoadData(scene);
        return;
    }
    if (W3dEscenaActiva()) {
        Notificar(T("Stop play mode before unloading a scene."), true);
        return;
    }
    gEscenaAccionOutliner = scene;
    if (!confirmarPopup) confirmarPopup = new ConfirmarPopup();
    confirmarPopup->AbrirTresOpciones(
        std::string(T("Unload scene")) + " \"" + scene->name + "\"? " +
            T("Save the project, discard this scene's unsaved edits, or cancel."),
        T("Discard"), DiscardAndUnloadScene, T("Save"), SaveAndUnloadScene);
}

// Derive the object icon from its type (Phase D: the core no longer stores UI icons).
// Map object types to IconType; the icon catalog belongs to the UI layer.
size_t ObjectIcon(Object* o) {
    switch (o->getType().v) {
        case ObjectType::scene:      return (size_t)IconType::archive;
        case ObjectType::mesh:       return (size_t)IconType::mesh;
        case ObjectType::light:      return (size_t)IconType::light;
        case ObjectType::camera:     return (size_t)IconType::camera;
        case ObjectType::collection: return (size_t)IconType::archive;
        case ObjectType::empty:      return (size_t)IconType::empty;
        case ObjectType::armature:   return (size_t)IconType::armature;
        case ObjectType::curve:      return (size_t)IconType::curve;
        case ObjectType::mirror:     return (size_t)IconType::mirror;
        case ObjectType::script:     return (size_t)IconType::gamepad;
        case ObjectType::instance: { // Array, mirror, or instance depending on the mode.
            Instance* in = (Instance*)o;
            if (in->mirror)    return (size_t)IconType::mirror;
            if (in->count > 1) return (size_t)IconType::array;
            return (size_t)IconType::instance;
        }
        case ObjectType::lod:        return (size_t)IconType::array;     // Levels of detail.
        case ObjectType::culling:    return (size_t)IconType::visible;   // Culling: visible or clipped.
        case ObjectType::particulas: return (size_t)IconType::circle;    // Particle emitter.
        case ObjectType::ui:         return (size_t)IconType::archive;   // 2D scene/interface root.
        case ObjectType::imagen2d:   return (size_t)IconType::foto;      // 2D image element.
        case ObjectType::rect2d:     return (size_t)IconType::plane;     // 2D rectangle element.
        case ObjectType::cont2d:     return (size_t)IconType::carpeta;   // 2D container that orders children.
        case ObjectType::slice9:     return (size_t)IconType::cuadricula; // Image with fixed borders.
        case ObjectType::boton2d:    return (size_t)IconType::object;    // UI button.
        case ObjectType::expandir2d: return (size_t)IconType::arrowRight; // Layout spring.
        case ObjectType::video2d:    return (size_t)IconType::camera;    // Video (no audio).
        default:                     return (size_t)IconType::archive;
    }
}

// ===================================================================================================
// Virtual "Armature 2D" rows: 2D bones are not scene objects; they live inside the mesh
// (Mesh::armatures2d). The Outliner shows one informational row per rig beneath the mesh,
// which may contain multiple independent 2D rigs. The blue color matches the UV editor's bones
// and clarifies that the rig belongs to the mesh. This row is inert: it cannot be selected,
// dragged, used as a drop target, unparented, or expanded. Bones are listed and edited in the
// Properties panel's "Armature 2D" tab and in the UV editor, where they are drawn.
// Every traversal (names, toggles, row-to-object lookup, and content height) uses the same
// Armature2DCount(obj) count, avoiding duplicated row-counting logic.
// ===================================================================================================
static const float kArm2DAzul[3] = { 0.28f, 0.55f, 1.00f }; // Same blue as the bones in the UV editor.

// Return this object's mesh with 2D armatures (NULL if not applicable).
static Mesh* Armature2DOnMesh(Object* o){
    if (!o || o->getType() != ObjectType::mesh) return NULL;
    Mesh* m = (Mesh*)o;
    return m->TieneArm2D() ? m : NULL;
}
// Number of virtual rows contributed by this object (its mesh's 2D armature count; 0 = none).
static int Armature2DCount(Object* o){ Mesh* m = Armature2DOnMesh(o); return m ? (int)m->armatures2d.size() : 0; }
// Does this object have anything to expand (object children or virtual 2D armature rows)?
static bool OutlinerHasChildren(Object* o){ return (o->Childrens.size() >= 1) || (Armature2DCount(o) > 0); }

// Result of resolving a tree row: a real object or a virtual 2D armature row.
struct OutFila {
    Object* obj;    // Real object (NULL for a virtual row).
    Mesh*   arm2d;  // Non-NULL for a virtual "Armature 2D" row of this mesh.
    int     arm2dIdx; // Armature index for this row (only when arm2d != NULL).
    int     prof;   // Indentation depth.
    OutFila() : obj(NULL), arm2d(NULL), arm2dIdx(-1), prof(0) {}
};

// Constructor.
static Object* ObjectAtRow(int row, int* depthOut = 0);
static bool FindRowInTree(int row, OutFila& out);
static int RowForObject(Object* target);

Outliner::Outliner() : ViewportBase() {
    Renglon = new Rec2D();
    CantidadRenglones = 5;
    lastContentRows = -1;
    hoverFila = -1;
    dragObjeto = NULL;
    dragging = false;
    dragY0 = 0;
    dropFila = -1;
    dropZona = -2;
    dropProf = 0;
    moviendo = false;
    moverObj = NULL;
    moverPadreOrig = NULL;
    moverAnteriorOrig = NULL;
    BarCrear();
}

// This calculation must be recursive.
void Outliner::CalculateRowLayout(Object* obj, int* MaxPosXtemp, int* MaxPosYtemp){
    int rowWidth = marginGS + IconSizeGS + gapGS + IconSizeGS + gapGS +
                   IconSizeGS + gapGS + IconSizeGS + marginGS;
    *MaxPosYtemp -= RenglonHeightGS;
    int textWidth = obj->name.size() * LetterWidthGS;
    rowWidth += textWidth + gapGS;

    // Store the maximum width.
    if (rowWidth > *MaxPosXtemp) *MaxPosXtemp = rowWidth;

    // Skip the remaining loops if there are no children or the object is collapsed.
    if (!OutlinerHasChildren(obj) || !obj->desplegado) return;
    *MaxPosYtemp -= RenglonHeightGS * Armature2DCount(obj); // One virtual row per 2D armature.

    //std::cout << "textWidth: " << textWidth << " rowWidth: " << rowWidth << std::endl;
    for (size_t o = 0; o < obj->Childrens.size(); o++) {
        CalculateRowLayout(obj->Childrens[o], MaxPosXtemp, MaxPosYtemp);
        /*int rowWidthObj = marginGS + IconSizeGS + gapGS + IconSizeGS + gapGS + IconSizeGS + gapGS + IconSizeGS + marginGS;
        *MaxPosYtemp -= RenglonHeightGS;

        // Object text.
        int textWidthObj = reinterpret_cast<Text*>(obj->Childrens[o]->name->data)->letters.size() * LetterWidthGS;
        rowWidthObj += textWidthObj + gapGS;

        if (rowWidthObj > *MaxPosXtemp) *MaxPosXtemp = rowWidthObj;
        //std::cout << "caracteres obj: " << rowWidthObj << std::endl;*/
    }
}

void Outliner::Resize(int newW, int newH){
    ViewportBase::Resize(newW, newH);
    ResizeBorder(newW, newH);

    // The row-strip width is adjusted later, once we know whether the vertical scrollbar exists.

    // Calculate how many rows fit in the viewport height.
    // Use a portable ceil calculation because std::ceil is ambiguous in RVCT.
    CantidadRenglones = (size_t)((height + RenglonHeightGS - 1) / RenglonHeightGS);

    int MaxPosXtemp = 0;
    int MaxPosYtemp = 0;

    if (!SceneCollection) {
        ResizeScrollbar(newW, newH, 0, 0, BarTopOffset());
        Renglon->SetSize(0, 0, (GLshort)width, RenglonHeightGS);
        return;
    }
    for (size_t c = 0; c < SceneCollection->Childrens.size(); c++) {
        CalculateRowLayout(SceneCollection->Childrens[c], &MaxPosXtemp, &MaxPosYtemp);
    }
    // Gap for the horizontal scrollbar below.
    MaxPosYtemp -= marginGS;
    //std::cout << "MaxPosXtemp: " << MaxPosXtemp << " width: " << width << std::endl;
    //std::cout << "MaxPosYtemp: "<< MaxPosYtemp << std::endl;
    //std::cout << "Ancho: " << newW << " Alto: "<< newH << std::endl;
    ResizeScrollbar(newW, newH, MaxPosXtemp, MaxPosYtemp, BarTopOffset());

    // Reserve the scrollbar area only when the vertical bar is present;
    // otherwise, do not waste the space.
    int reservaV = scrollY ? (borderGS + GlobalScale * 9 + 2) : 0;
    Renglon->SetSize(0, 0, (GLshort)(width - reservaV), RenglonHeightGS);
}

void Outliner::Render(){
    // Refresh the scrollbar when the number of visible rows changes (import/add/delete/expand).
    // Previously it was recalculated only on viewport resize, leaving it stale after imports
    // until the user manually resized a viewport.
    if (SceneCollection){
        struct C { static int rec(Object* o){ int n = 1;
            if (OutlinerHasChildren(o) && o->desplegado){
                n += Armature2DCount(o);                        // One virtual row per 2D armature.
                for (size_t i = 0; i < o->Childrens.size(); i++) n += rec(o->Childrens[i]);
            }
            return n; } };
        int filas = 0;
        for (size_t c = 0; c < SceneCollection->Childrens.size(); c++) filas += C::rec(SceneCollection->Childrens[c]);
        if (filas != lastContentRows){ lastContentRows = filas; Resize(width, height); }
    }
    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();

    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();

    // The tree origin is top-left (all four OSes); GL expects bottom-left.
    const int glY = W3dPantallaAlto - y - height;
    if (!SceneCollection) return;

    // Clear the viewport.
    w3dEngine::Enable(w3dEngine::ScissorTest);
    w3dEngine::Scissor(x, glY, width, height); // Match the viewport bounds.
    w3dEngine::ClearColor(ListaColores[static_cast<int>(ColorID::background)][0],
        ListaColores[static_cast<int>(ColorID::background)][1],
        ListaColores[static_cast<int>(ColorID::background)][2],
        ListaColores[static_cast<int>(ColorID::background)][3]);
    w3dEngine::Clear(w3dEngine::ColorBuffer | w3dEngine::DepthBuffer);
    w3dEngine::Disable(w3dEngine::ScissorTest);

    w3dEngine::Viewport(x, glY, width, height); // x, y, width, height.
    w3dEngine::Ortho(0, width, height, 0, -1, 1);

    w3dEngine::Disable(w3dEngine::Fog);
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::Disable(w3dEngine::Blend);
    w3dEngine::Enable(w3dEngine::ColorMaterial);

    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    w3dEngine::DisableArray(w3dEngine::NormalArray);

    // The rendering below continues as before.
    w3dEngine::PushMatrix();
    size_t RenglonesY = 0;
    w3dEngine::Translatef(0, PosY + borderGS + BarTopOffset(), 0);
    for (size_t i = 0; i < CantidadRenglones; i++) {
        w3dEngine::PushMatrix();
        w3dEngine::Translatef(0, RenglonesY, 0);
        RenglonesY += RenglonHeightGS;
        // Selected row.
        if (dragging && dropZona == 1 && (int)i == dropFila) {
            // Preview: this would become the drop's parent.
            w3dEngine::Color4ub(ListaColoresUbyte[static_cast<int>(ColorID::accentDark)][0],
                       ListaColoresUbyte[static_cast<int>(ColorID::accentDark)][1],
                       ListaColoresUbyte[static_cast<int>(ColorID::accentDark)][2], 255);
        }
        else if ((int)i == hoverFila) {
            // Hover feedback before clicking.
            w3dEngine::Color4ub(ListaColoresUbyte[static_cast<int>(ColorID::headerColor)][0], ListaColoresUbyte[static_cast<int>(ColorID::headerColor)][1], ListaColoresUbyte[static_cast<int>(ColorID::headerColor)][2], 255);
        }
        else if (i % 2 == 0) {
            w3dEngine::Color4ub(ListaColoresUbyte[static_cast<int>(ColorID::gris)][0], ListaColoresUbyte[static_cast<int>(ColorID::gris)][1], ListaColoresUbyte[static_cast<int>(ColorID::gris)][2], 255);
        }
        else {
            // Odd row.
            w3dEngine::Color4ub(ListaColoresUbyte[static_cast<int>(ColorID::background)][0], ListaColoresUbyte[static_cast<int>(ColorID::background)][1], ListaColoresUbyte[static_cast<int>(ColorID::background)][2], 255);
        }
        //RenderObject2D(*Renglon);
        Renglon->Render(false);
        w3dEngine::PopMatrix();
    }
    w3dEngine::PopMatrix();

    w3dEngine::BindTexture(Textures[0]->iID);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::BlendAlpha();
#ifndef W3D_SYMBIAN
    w3dEngine::TexFilter(false);
    w3dEngine::TexFilter(false);
#endif
    SetColorID(ColorID::grisUI);

    // Clip the text so it does not draw over the toggles on the right.
    int anchoNombres = width - 5*IconSizeGS - 4*gapGS - marginGS - borderGS -
                       gapGS - (scrollY ? (GlobalScale*9 + gapGS) : 0);
    if (anchoNombres < 0) anchoNombres = 0;
    w3dEngine::Enable(w3dEngine::ScissorTest);
    if (scrollX){
        w3dEngine::Scissor(x, glY + marginGS, anchoNombres, height - marginGS); // Exclude scene actions, visibility, camera, and scrollbar columns.
    }
    else {
        w3dEngine::Scissor(x, glY, anchoNombres, height); // Exclude scene actions, visibility, camera, and scrollbar columns.
    }

    RenglonesY = 0;
    cullBaseY = PosY + borderGS + BarTopOffset(); filaDFS = 0; // Culling: Y of the first row in the name traversal.
    w3dEngine::PushMatrix();
    w3dEngine::Translatef(marginGS + PosX, PosY + borderGS + BarTopOffset(), 0);
    for (size_t c = 0; c < SceneCollection->Childrens.size(); c++){
        DrawRow(SceneCollection->Childrens[c], !SceneCollection->Childrens[c]->visible);
        w3dEngine::Translatef(0, RenglonHeightGS, 0);
    }
    w3dEngine::PopMatrix();

    SetColorID(ColorID::grisUI);
    RenglonesY = 0;

    w3dEngine::PushMatrix();
    // Do not use PosX: the toggles stay at a fixed X position at the right edge.
    int reservaBarra = scrollY ? (GlobalScale * 9 + gapGS) : 0;   // Only the scrollbar width plus a small gap.
    w3dEngine::Translatef(width - 5*IconSizeGS - 4*gapGS - marginGS - borderGS - reservaBarra,
                          GlobalScale + PosY + borderGS + BarTopOffset(), 0);

    if (scrollX){
        w3dEngine::Scissor(x, glY + marginGS, width - marginGS - borderGS, height - marginGS); // Viewport bounds, excluding the toggles.
    }
    else {
        w3dEngine::Scissor(x, glY, width - marginGS - borderGS, height); // Viewport bounds, excluding the toggles.
    }

    cullBaseY = GlobalScale + PosY + borderGS + BarTopOffset(); filaDFS = 0; // Culling: Y of the first row in the toggle traversal.
    for (size_t c = 0; c < SceneCollection->Childrens.size(); c++) {
        DrawSceneToggles(SceneCollection->Childrens[c], !SceneCollection->Childrens[c]->visible,
                    !SceneCollection->Childrens[c]->renderizable);
    }
    w3dEngine::PopMatrix();
    w3dEngine::Disable(w3dEngine::ScissorTest);

    // Drag preview: green line showing where the row will be inserted.
    if (dragging && (dropZona == 0 || dropZona == 2)) {
        static Rec2D* linea = NULL;
        if (!linea) linea = new Rec2D();
        int lineY = borderGS + PosY + BarTopOffset()
                    + (dropFila + (dropZona == 2 ? 1 : 0)) * RenglonHeightGS;
        w3dEngine::Disable(w3dEngine::Texture2D);
        SetColorID(ColorID::accent);
        // Indent the line to the destination depth: parented rows start farther right and
        // use a shorter line; a root-level drop (depth 0) spans the full width.
        int indent = dropProf * (IconSizeGS + gapGS);
        linea->SetSize((GLshort)(borderGS + indent), (GLshort)(lineY - GlobalScale),
                       (GLshort)(width - bordersGS - indent), (GLshort)(2 * GlobalScale));
        linea->RenderObject(false);
        w3dEngine::Enable(w3dEngine::Texture2D);
    }

    RenderBar();
    DibujarBordes(this);
    DibujarScrollbar(this);
#ifdef W3D_SYMBIAN
    w3dEngine::EnableArray(w3dEngine::NormalArray); // Baseline expected by the scene.
#endif
}

void Outliner::DrawRow(Object* obj, bool hidden){
    // Draw the row only when it intersects the visible area. The child traversal still advances
    // the matrix so visible rows stay aligned. Keep one extra row above and below to avoid clipping edges.
    int myY = cullBaseY + (int)filaDFS * (int)RenglonHeightGS; filaDFS++;
    bool filaVisible = (myY + (int)RenglonHeightGS * 2 > 0) && (myY < (int)height + (int)RenglonHeightGS);
    if (filaVisible) {
    w3dEngine::PushMatrix();
    GLfloat opacityRow = hidden ? 0.5f : 1.0f;

    if (moviendo && obj == moverObj){
        // Highlight the object being moved so it remains clear which row is being reordered
        // on devices such as the N95 that have no mouse.
        SetColorID(ColorID::accent, opacityRow);
    }
    else if (dragging){
        // While dragging, dim the Outliner selection and highlight only the dragged object.
        // Restore the normal selection display when the drag ends.
        if (obj == dragObjeto){
            SetColorID(ColorID::accent, opacityRow);
        } else {
            SetColorID(ColorID::grisUI, opacityRow);
        }
    }
    else if (obj == ObjActivo){
        //std::cout << "Active object in the Outliner: " << reinterpret_cast<Text*>(SceneCollection->Childrens[c]->name->data)->value << "\n";
        if (obj->select){
            SetColorID(ColorID::accent, opacityRow);
        }
        else {
            SetColorID(ColorID::blanco, opacityRow);
        }
    }
    else if (obj->select){
        SetColorID(ColorID::accentDark, opacityRow);
    }
    else {
        SetColorID(ColorID::grisUI, opacityRow);
    }

    // Expand icon (right arrow when there are no children). A virtual 2D armature row
    // also counts as a child, so a mesh with bones can be expanded.
    if (!OutlinerHasChildren(obj) || !obj->desplegado){
        W3dDrawStrip4(IconMesh, IconsUV[static_cast<size_t>(IconType::arrowRight)]->uvs);
    }
    else {
        W3dDrawStrip4(IconMesh, IconsUV[static_cast<size_t>(IconType::arrow)]->uvs);
    }

    // Collection icon.
    w3dEngine::Translatef(IconSizeGS + gapGS, 0, 0);
    W3dDrawStrip4(IconMesh, IconsUV[ObjectIcon(obj)]->uvs);

    // Render text, or the active inline scene-name editor.
    w3dEngine::Translatef(IconSizeGS + gapGS, 0, 0);
    TextField* renameField = PropsCampoRenombreEscena(obj);
    if (renameField && g_textFieldActivo == renameField) {
        std::string shown = renameField->text;
        if (!renameField->selectAll && renameField->caret >= 0 &&
            renameField->caret <= (int)shown.size())
            shown.insert((size_t)renameField->caret, "|");
        if (renameField->selectAll)
            w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::accent)]);
        RenderBitmapText(shown);
        if (renameField->selectAll)
            w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::blanco)]);
    } else {
        RenderBitmapText(obj->name);
    }

    w3dEngine::PopMatrix();
    } // End of row drawing (culling); the child traversal below always runs.

    // Skip the remaining loops if there are no children or the object is collapsed.
    if (!OutlinerHasChildren(obj) || !obj->desplegado) return;
    Mesh* virt = Armature2DOnMesh(obj); // NULL means the mesh has no 2D armatures and adds no rows.
    const int nArm = Armature2DCount(obj);

    // Tree line.
    w3dEngine::PushMatrix();
    for (int k = 0; k < nArm; k++){
        w3dEngine::Translatef(0, RenglonHeightGS, 0);
        W3dDrawStrip4(IconLineMesh, IconsUV[static_cast<size_t>(IconType::line)]->uvs);
    }
    for (size_t o = 0; o < obj->Childrens.size(); o++){
        w3dEngine::Translatef(0, RenglonHeightGS, 0);
        W3dDrawStrip4(IconLineMesh, IconsUV[static_cast<size_t>(IconType::line)]->uvs);
    }
    w3dEngine::PopMatrix();

    // Arrows.
    w3dEngine::PushMatrix();
    DrawExpandedTreeLines(obj);
    w3dEngine::PopMatrix();

    // Regular row.
    w3dEngine::Translatef(IconSizeGS + gapGS, 0, 0);
    for (int k = 0; k < nArm; k++){       // 2D armatures come first; they are mesh data.
        w3dEngine::Translatef(0, RenglonHeightGS, 0);
        DrawArmature2DRow(virt, k, hidden);
    }
    for (size_t o = 0; o < obj->Childrens.size(); o++){
        w3dEngine::Translatef(0, RenglonHeightGS, 0);
        DrawRow(obj->Childrens[o],
            hidden ? true : !obj->Childrens[o]->visible);
    }
    w3dEngine::Translatef(-IconSizeGS - gapGS, 0, 0);
}

// Draw one virtual row for a mesh's 2D armature: an informational row with its icon and name,
// colored blue to indicate mesh data rather than a scene object. It has no expand arrow; bones
// are listed in the Properties panel's "Armature 2D" tab. The row occupies one line without
// moving the matrix. The active armature is bright blue; the others are dimmed, as in the UV editor.
void Outliner::DrawArmature2DRow(Mesh* m, int idx, bool hidden){
    if (!m || idx < 0 || idx >= (int)m->armatures2d.size()) return;
    const GLfloat op = (hidden || idx != m->armature2dActivo) ? 0.5f : 1.0f; // The active rig is fully opaque.
    int myY = cullBaseY + (int)filaDFS * (int)RenglonHeightGS; filaDFS++;
    if (!((myY + (int)RenglonHeightGS * 2 > 0) && (myY < (int)height + (int)RenglonHeightGS))) return;
    w3dEngine::PushMatrix();
    w3dEngine::Color4f(kArm2DAzul[0], kArm2DAzul[1], kArm2DAzul[2], op);
    // Leave the expand-arrow column empty; skip it so the icon and text align with object rows.
    w3dEngine::Translatef(IconSizeGS + gapGS, 0, 0);
    W3dDrawStrip4(IconMesh, IconsUV[static_cast<size_t>(IconType::armature)]->uvs);
    w3dEngine::Translatef(IconSizeGS + gapGS, 0, 0);
    RenderBitmapText(m->armatures2d[idx] ? m->armatures2d[idx]->nombre : std::string("Armature 2D"));
    w3dEngine::PopMatrix();
}

void Outliner::DrawExpandedTreeLines(Object* obj){
    for (int k = 0, n = Armature2DCount(obj); k < n; k++){ // Each virtual row has a tree line.
        w3dEngine::Translatef(0, RenglonHeightGS, 0);
        W3dDrawStrip4(IconLineMesh, IconsUV[static_cast<size_t>(IconType::line)]->uvs);
    }
    for (size_t o = 0; o < obj->Childrens.size(); o++){
        w3dEngine::Translatef(0, RenglonHeightGS, 0);
        W3dDrawStrip4(IconLineMesh, IconsUV[static_cast<size_t>(IconType::line)]->uvs);
        DrawExpandedTreeLines(obj->Childrens[o]);
    }
}

void Outliner::DrawSceneToggles(Object* obj, bool hidden, bool noRender){
    // Use the same culling as DrawRow; the translation below always advances to position children.
    int myY = cullBaseY + (int)filaDFS * (int)RenglonHeightGS; filaDFS++;
    if ((myY + (int)RenglonHeightGS * 2 > 0) && (myY < (int)height + (int)RenglonHeightGS)) {
        w3dEngine::PushMatrix();
        LegacyUIScene* escena = obj->getType() == ObjectType::ui &&
                               obj->Parent == SceneCollection
                                   ? dynamic_cast<LegacyUIScene*>(obj) : NULL;
        SceneData* dataScene = obj->getType() == ObjectType::scene &&
                               obj->Parent == SceneCollection
                                   ? dynamic_cast<SceneData*>(obj) : NULL;
        if (IsSceneRoot(obj)) {
            SetColorID(ColorID::grisUI);
            W3dDrawStrip4(IconMesh, IconsUV[static_cast<size_t>(IconType::guardar)]->uvs);
        }
        w3dEngine::Translatef((GLfloat)(IconSizeGS + gapGS), 0, 0);
        if (escena || dataScene) {
            const bool cargada = escena ? escena->cargada : dataScene->cargada;
            SetColorID(ColorID::grisUI, cargada ? 1.0f : 0.5f);
            W3dDrawStrip4(IconMesh, IconsUV[static_cast<size_t>(
                cargada ? IconType::archive : IconType::carpeta)]->uvs);
        }
        w3dEngine::Translatef((GLfloat)(IconSizeGS + gapGS), 0, 0);
        if (IsSceneRoot(obj)) {
            SetColorID(ColorID::grisUI);
            W3dDrawStrip4(IconMesh, IconsUV[static_cast<size_t>(IconType::borrar)]->uvs);
        }
        w3dEngine::Translatef((GLfloat)(IconSizeGS + gapGS), 0, 0);
        // Eye (visibility): dim when hidden directly or by a hidden parent.
        SetColorID(ColorID::grisUI, hidden ? 0.5f : 1.0f);
        W3dDrawStrip4(IconMesh, IconsUV[static_cast<size_t>(obj->visible ? IconType::visible : IconType::hidden)]->uvs);
        // Camera (render) toggle to the right of the eye: use camera_off when disabled;
        // if enabled but a parent is disabled, show it semi-transparent like the eye.
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(IconSizeGS + gapGS), 0, 0);
        SetColorID(ColorID::grisUI, noRender ? 0.5f : 1.0f);
        W3dDrawStrip4(IconMesh, IconsUV[static_cast<size_t>(obj->renderizable ? IconType::camera : IconType::camera_off)]->uvs);
        w3dEngine::PopMatrix();
        w3dEngine::PopMatrix();
    }
    w3dEngine::Translatef(0, RenglonHeightGS, 0);

    // Skip the remaining loops if there are no children or the object is collapsed.
    if (!OutlinerHasChildren(obj) || !obj->desplegado) return;

    // Virtual 2D armature rows have no eye or camera toggles, but still occupy a row.
    // Advance the culling index and matrix to keep the toggles aligned.
    for (int k = 0, n = Armature2DCount(obj); k < n; k++){ filaDFS++; w3dEngine::Translatef(0, RenglonHeightGS, 0); }

    for (size_t o = 0; o < obj->Childrens.size(); o++){
        DrawSceneToggles(obj->Childrens[o],
                    hidden   ? true : !obj->Childrens[o]->visible,
                    noRender ? true : !obj->Childrens[o]->renderizable);
    }
}

void Outliner::button_left(){
    if (mouseOverScrollY){
        mouseOverScrollYpress = true;
    }
}

#ifndef W3D_SYMBIAN
void Outliner::mouse_button_up(int boton){
    ViewPortClickDown = false;
    if (boton == W3dMB_IZQ) {
        mouseOverScrollYpress = false;
        mouseOverScrollXpress = false;
    }
    //else if (button == W3dMB_MEDIO) {
    //    middleMouseDown = false;
    //}
    FindMouseOver(lastMouseX,lastMouseY);
}
#endif

#ifndef W3D_SYMBIAN
void Outliner::event_mouse_wheel(float dy, int mx, int my){
    {
      if (BarScrollHorizontal(mx, my, (int)(dy * 40))) return; } // Over the scrollbar: horizontal scrolling.
    MouseWheel = true;
    ScrollY(dy*6*GlobalScale);
    MouseWheel = false;
}
#endif

void Outliner::FindMouseOver(int mx, int my){
    // LayoutInput calculates scrollbar hover from the grab area;
    // the old ScrollMouseOver call overwrote that state.
}

// Touch: drag one finger over the content to scroll vertically or horizontally.
// The scrollbar is handled by the locked gesture.
bool Outliner::event_finger_scroll(int px, int py, int dx, int dy){
    ScrollByTouch(dx, dy);
    return true;
}

void Outliner::event_mouse_motion(int mx, int my) {
    // Hover feedback for the row under the mouse.
    hoverFila = (my - y - borderGS - PosY - BarTopOffset()) / RenglonHeightGS;
    // The scrollbar area is reserved, so rows do not hover there.
    if (mouseOverScrollY || mouseOverScrollX) {
        hoverFila = -1;
    }
    if (leftMouseDown && dragObjeto) {
        int d = my - dragY0;
        if (d < 0) d = -d;
        if (!dragging && d > RenglonHeightGS / 2) dragging = true;
        if (dragging) {
            // Drop preview: insertion line or prospective parent.
            dropZona = -2;
            if (Contains(mx, my)) {
                int rel = my - y - borderGS - PosY - BarTopOffset();
                if (rel >= 0) {
                    dropFila = rel / RenglonHeightGS;
                    int resto = rel % RenglonHeightGS;
                    int f = dropFila;
                    OutFila hitF;
                    bool hay = FindRowInTree(f, hitF);
                    // A virtual 2D armature row is not a drop target: it is not an object and
                    // cannot be reparented from the Outliner.
                    if (hay && hitF.arm2d) { dropZona = -2; return; }
                    Object* destino = hay ? hitF.obj : NULL;
                    dropProf = hay ? hitF.prof : 0; // Destination depth; indent the line accordingly.
                    if (!destino) dropZona = -1; // Empty space means the root.
                    else if (destino == dragObjeto) dropZona = -2;
                    else if (resto < RenglonHeightGS / 4) dropZona = 0;
                    else if (resto > (RenglonHeightGS * 3) / 4) dropZona = 2;
                    else dropZona = 1;
                }
            }
            return; // Do not scroll while dragging.
        }
    }
    if (middleMouseDown || leftMouseDown) {
        ViewPortClickDown = true;

        ScrollX(dx);
        ScrollY(dy);
        return;
    }
    // If no button is pressed, check whether the mouse is over a row.
    else if (scrollY){
        FindMouseOver(mx, my);
    }
}

#ifndef W3D_SYMBIAN
void Outliner::event_key_down(int tecla, bool repeticion){
    const int key = tecla;
    if (repeticion == 0) {
        // Move mode: arrows reorder/reparent instead of navigating; OK confirms; C/Backspace/Esc cancels.
        if (moviendo) {
            switch (key) {
                case W3dK_UP:    StepKeyboardMove(0); return;
                case W3dK_DOWN:  StepKeyboardMove(1); return;
                case W3dK_LEFT:  StepKeyboardMove(2); return; // Left = move out (unparent).
                case W3dK_RIGHT: StepKeyboardMove(3); return; // Right = move in (parent under the previous sibling).
                case W3dK_RETURN: case W3dK_KP_ENTER: ConfirmKeyboardMove(); return;
                case W3dK_ESCAPE: case W3dK_BACKSPACE: case W3dK_C: CancelKeyboardMove(); return;
                default: return; // Ignore other keys while moving.
            }
        }
        switch (key) {
            case W3dK_G: // G enters move mode to reorder or reparent the active object.
                BeginKeyboardMove();
                break;
            case W3dK_A:
                SeleccionarTodo(true);
                break;
            case W3dK_H:
                ChangeVisibilityObj();
                break;
            case W3dK_R:
                if (IsSceneRoot(ObjActivo)) PropsIniciarRenombreEscena(ObjActivo);
                break;
            case W3dK_S:
                if (IsSceneRoot(ObjActivo)) StartSaveScene(ObjActivo);
                break;
            case W3dK_X:
            case W3dK_DELETE:   // Delete behaves like X.
                if (estado == editNavegacion){
                    AbrirConfirmarBorrado(true); // Confirmation popup (including collections); Yes deletes with undo.
                }
                break;
            case W3dK_LEFT:
                SetDesplegado(false);
                break;
            case W3dK_RIGHT:
                SetDesplegado(true);
                break;
            case W3dK_UP:
                changeSelect(SelectMode::PrevSingle, true);
                EnsureVisible();
                break;
            case W3dK_DOWN:
                changeSelect(SelectMode::NextSingle, true);
                EnsureVisible();
                break;
            case W3dK_KP_PERIOD: // Numpad "." centers the selection; keyboard input routes by mouse hover.
                CenterSelection(); // The router sends viewport-specific keys to the viewport under the mouse.
                break;
        };
    }
}
#endif

#ifndef W3D_SYMBIAN
void Outliner::event_key_up(int tecla){
    const int key = tecla;
    switch (key) {
        case W3dK_LSHIFT:
            if (ShiftCount < 20){
                changeSelect(SelectMode::NextSingle, true);
                EnsureVisible();
            }
            ShiftCount = 0;
            LShiftPressed = false;
            break;
        case W3dK_LALT:
            LAltPressed = false;
            break;
    }
}
#endif

// Resolve visible row N (respecting expansion) to a real object or virtual 2D armature row.
// Return true if the row is in this subtree and populate out.
static bool FindVisibleRowRecursive(Object* obj, int& fila, int prof, OutFila& out) {
    if (fila == 0) { out.obj = obj; out.arm2d = NULL; out.prof = prof; return true; }
    fila--;
    if (obj->desplegado) {
        Mesh* am = Armature2DOnMesh(obj);
        for (int k = 0, n = Armature2DCount(obj); k < n; k++) { // One row per 2D armature; no children.
            if (fila == 0) { out.obj = NULL; out.arm2d = am; out.arm2dIdx = k; out.prof = prof + 1; return true; }
            fila--;
        }
        for (size_t o = 0; o < obj->Childrens.size(); o++)
            if (FindVisibleRowRecursive(obj->Childrens[o], fila, prof + 1, out)) return true;
    }
    return false;
}

// Resolve row N in the full tree to a real object or virtual row. Return false when the row
// is in empty space (below the last row); dropping there unparents the object to the root.
static bool FindRowInTree(int fila, OutFila& out) {
    if (!SceneCollection) return false;
    for (size_t c = 0; c < SceneCollection->Childrens.size(); c++)
        if (FindVisibleRowRecursive(SceneCollection->Childrens[c], fila, 0, out)) return true;
    return false;
}

// Return the object in visible row N and its depth in profOut. NULL means empty space or a virtual
// row (a 2D armature row is not an object, so it cannot be selected or used as a drop target).
static Object* ObjectAtRow(int fila, int* profOut) {
    OutFila f;
    if (!FindRowInTree(fila, f)) return NULL;
    if (profOut) *profOut = f.prof;
    return f.obj;
}

// Return an object's visible row number, or -1 if it is not visible.
static bool FindObjectRowRecursive(Object* obj, Object* objetivo, int* fila) {
    if (obj == objetivo) return true;
    (*fila)++;
    if (obj->desplegado) {
        (*fila) += Armature2DCount(obj);    // Virtual 2D armature rows are included in row numbering.
        for (size_t o = 0; o < obj->Childrens.size(); o++) {
            if (FindObjectRowRecursive(obj->Childrens[o], objetivo, fila)) return true;
        }
    }
    return false;
}

static int RowForObject(Object* objetivo) {
    if (!objetivo || !SceneCollection) return -1;
    int fila = 0;
    for (size_t c = 0; c < SceneCollection->Childrens.size(); c++) {
        if (FindObjectRowRecursive(SceneCollection->Childrens[c], objetivo, &fila)) {
            return fila;
        }
    }
    return -1;
}

// Numpad "." centers the selection in the view (the equivalent of Blender's Frame Selected).
// Scroll PosY to place the target row in the middle of the content area. With multiple selected
// objects, center the average of their visible rows; objects inside collapsed parents do not
// contribute. If none are visible, use the active object. Only vertical scrolling changes.
void Outliner::CenterSelection() {
    if (!SceneCollection) return;
    long suma = 0; int n = 0;
    for (size_t i = 0; i < ObjSelects.size(); i++) {
        int f = RowForObject(ObjSelects[i]);
        if (f >= 0) { suma += f; n++; }
    }
    if (n == 0 && ObjActivo) {
        int f = RowForObject(ObjActivo);
        if (f >= 0) { suma = f; n = 1; }
    }
    if (n == 0) return;                     // Nothing visible is selected, so there is nothing to frame.
    int fila = (int)(suma / n);
    // Row geometry (the same calculation used by Render and SelectRowAt):
    //   y_local(row) = borderGS + PosY + BarTopOffset() + row * RenglonHeightGS
    // Center the row by setting y_local(row) + RenglonHeightGS/2 to the content area's center.
    int top = BarTopOffset();
    int centro = top + (height - top) / 2;
    int nuevo = centro - (int)RenglonHeightGS / 2 - borderGS - top - fila * (int)RenglonHeightGS;
    if (nuevo > 0) nuevo = 0;               // Do not scroll past the beginning.
    if (nuevo < MaxPosY) nuevo = MaxPosY;   // Do not scroll past the end (MaxPosY is <= 0).
    if (nuevo != PosY) { PosY = nuevo; g_redraw = true; }
}

// Keyboard-navigation auto-scroll: apply the minimum scroll needed to show the active object's
// full row in the content area. Do nothing if it is already visible; recentering on every arrow
// key would be disorienting in large scenes. Previously, only Numpad "." framed the view, so
// navigating down eventually moved the highlight out of sight.
void Outliner::EnsureVisible() {
    if (!SceneCollection || !ObjActivo) return;
    int fila = RowForObject(ObjActivo);
    if (fila < 0) return;                   // An object inside a collapsed branch has no visible row.
    const int top = BarTopOffset();
    const int alto = (int)RenglonHeightGS;
    // y_local(row) = borderGS + PosY + top + row * height (same calculation as Render).
    const int yFila = borderGS + PosY + top + fila * alto;
    int nuevo = PosY;
    if (yFila < top)                  nuevo = PosY + (top - yFila);              // It moved above the viewport.
    else if (yFila + alto > height)   nuevo = PosY - (yFila + alto - height);    // It moved below the viewport.
    if (nuevo > 0) nuevo = 0;               // Do not scroll past the beginning.
    if (nuevo < MaxPosY) nuevo = MaxPosY;   // Do not scroll past the end (MaxPosY is <= 0).
    if (nuevo != PosY) { PosY = nuevo; g_redraw = true; }
}

// Finish a row drag: reorder at the target row's edges, parent at its center while preserving
// the transform, or move to the root when dropped in empty space.
void Outliner::FinishRowDrag(int mx, int my) {
    Object* obj = dragObjeto;
    bool estaba = dragging;
    dragObjeto = NULL;
    dragging = false;
    dropZona = -2;
    if (!obj || !estaba) return;
    if (!Contains(mx, my)) return;
    int rel = my - y - borderGS - PosY - BarTopOffset();
    if (rel < 0) return;
    int fila = rel / RenglonHeightGS;
    int resto = rel % RenglonHeightGS;
    int f = fila;
    OutFila hitF;
    bool hay = FindRowInTree(f, hitF);
    if (hay && hitF.arm2d) return;   // Virtual 2D armature rows do not accept drops.
    Object* destino = hay ? hitF.obj : NULL;
    if (!destino) {
        // Empty space: unparent to the root without changing the position.
        ReparentKeepTransform(obj, SceneCollection);
    } else if (destino != obj) {
        if (resto < RenglonHeightGS / 4) {
            MoverJuntoA(obj, destino, false); // Reorder before the target.
        } else if (resto > (RenglonHeightGS * 3) / 4) {
            MoverJuntoA(obj, destino, true); // Reorder after the target.
        } else {
            // Center: make it a child of the target while preserving its transform.
            ReparentKeepTransform(obj, destino);
            destino->desplegado = true;
        }
    }
    Resize(width, height);
}

// ---- Keyboard move mode (reorder or reparent without a mouse) ----
// Names: moving an object into another scene (direction 3) changes its namespace,
// and the reparenting path may rename it (see W3dAdjuntarA in ObjectMode.cpp).
//
// The modal defers renaming until confirmation. Otherwise, each cross-scene step renames the
// subtree and adds its own MultiRenameUndo. Cancelling would restore names manually, but those
// undo entries would remain as no-ops, with no supported way to remove them from the stack.
// Instead, raise W3dNombresCargando during the modal (the same guard used by loaders) so
// W3dAdjuntarA neither renames nor captures undo. Make names unique once on confirmation and
// create one undo step. Cancelling then leaves the history untouched; the name snapshot remains
// as a safeguard. Names may temporarily repeat during the modal, just as the object's position
// is temporary until the move is confirmed.
static std::vector<std::pair<Object*, std::string> > gMoverNombres;
static void CollectObjectNames(Object* o) {
    if (!o) return;
    gMoverNombres.push_back(std::make_pair(o, o->name));
    for (size_t i = 0; i < o->Childrens.size(); i++) CollectObjectNames(o->Childrens[i]);
}
// Pre-order traversal (the same order used by name lookup and W3dAdjuntarA),
// so the same object keeps the unsuffixed name in both paths.
static void CollectSubtreeObjects(Object* o, std::vector<Object*>& out) {
    if (!o) return;
    out.push_back(o);
    for (size_t i = 0; i < o->Childrens.size(); i++) CollectSubtreeObjects(o->Childrens[i], out);
}
// On confirmation, make names unique within the moved subtree's final scope in one undo step.
// This mirrors W3dAdjuntarA, which the guard bypasses during the modal.
static void MakeSubtreeNamesUnique(Object* raiz) {
    if (!raiz) return;
    std::vector<Object*> sub;
    CollectSubtreeObjects(raiz, sub);
    bool hayCambio = false;
    for (size_t i = 0; i < sub.size() && !hayCambio; i++)
        if (sub[i]->NombreLibre(sub[i]->name) != sub[i]->name) hayCambio = true;
    if (!hayCambio) return;
    // Object::name is a valid direct destination (see W3dRenameDest in Undo.h).
    std::vector<W3dRenameDest> puntas;
    for (size_t i = 0; i < sub.size(); i++) puntas.push_back(W3dDestNombre(&sub[i]->name));
    UndoCapturarRenames(puntas);   // Capture all name destinations in one command.
    int n = 0; std::string primeros;
    for (size_t i = 0; i < sub.size(); i++) {
        const std::string viejo = sub[i]->name;
        sub[i]->SetNameObj(viejo);
        if (sub[i]->name == viejo) continue;
        n++;
        if (n <= 3) { if (!primeros.empty()) primeros += ", "; primeros += viejo + " -> " + sub[i]->name; }
    }
    if (n > 0) {
        char cant[32]; sprintf(cant, "%d", n);
        Notificar(std::string(cant) + " nombre(s) renumerados al cambiar de escena (" +
                  primeros + (n > 3 ? ", ...)" : ")") + ". Revisa tus scripts lua.", true);
    }
}

// Move the active object and save its original position so the operation can be cancelled.
void Outliner::BeginKeyboardMove() {
    if (moviendo) return;
    if (!ObjActivo || !SceneCollection) return;
    moverObj = ObjActivo;
    gMoverNombres.clear();
    CollectObjectNames(moverObj);
    // Undo: steps inside the modal do not capture undo because W3dNombresCargando is set
    // (see StepKeyboardMove). Capture the original state once here and push it on confirmation,
    // leaving exactly one undo step for the move and renaming, or none if cancelled.
    UndoReparentIniciar(moverObj);
    moverPadreOrig = moverObj->Parent;   // NULL means the root.
    moverAnteriorOrig = NULL;
    Object* padre = moverObj->Parent ? moverObj->Parent : SceneCollection;
    for (size_t i = 0; i < padre->Childrens.size(); i++)
        if (padre->Childrens[i] == moverObj) { if (i > 0) moverAnteriorOrig = padre->Childrens[i-1]; break; }
    moviendo = true;
}

// dir: 0=up, 1=down (reorder siblings), 2=out (unparent), 3=in (parent under the previous sibling).
void Outliner::StepKeyboardMove(int dir) {
    if (!moviendo || !moverObj || !SceneCollection) return;
    Object* padre = moverObj->Parent ? moverObj->Parent : SceneCollection;
    int idx = -1;
    for (size_t i = 0; i < padre->Childrens.size(); i++)
        if (padre->Childrens[i] == moverObj) { idx = (int)i; break; }
    if (idx < 0) return;
    // Defer renaming and its undo entry until ConfirmKeyboardMove; see the explanation above.
    struct MoverGuard { bool prev; MoverGuard(){ prev = W3dNombresCargando; W3dNombresCargando = true; }
                        ~MoverGuard(){ W3dNombresCargando = prev; } } moverGuard;
    if (dir == 0) {                         // Up: before the previous sibling.
        if (idx > 0) { Object* ref = padre->Childrens[idx-1]; MoverJuntoA(moverObj, ref, false); }
    } else if (dir == 1) {                  // Down: after the next sibling.
        if (idx + 1 < (int)padre->Childrens.size()) { Object* ref = padre->Childrens[idx+1]; MoverJuntoA(moverObj, ref, true); }
    } else if (dir == 2) {                  // Out (unparent): to the grandparent, just after the parent.
        if (padre != SceneCollection) {
            Object* viejoPadre = padre;
            Object* abuelo = padre->Parent ? padre->Parent : SceneCollection;
            ReparentKeepTransform(moverObj, abuelo);
            MoverJuntoA(moverObj, viejoPadre, true);
        }
    } else if (dir == 3) {                  // In (parent): become a child of the previous sibling.
        if (idx > 0) {
            Object* nuevoPadre = padre->Childrens[idx-1];
            ReparentKeepTransform(moverObj, nuevoPadre);
            nuevoPadre->desplegado = true;
        }
    }
    Resize(width, height); // Recalculate scrolling.
}

void Outliner::ConfirmKeyboardMove() {
    if (moviendo && moverObj) {
        UndoGrupoIniciar();          // Group the move and renames into a single undo step.
        UndoReparentConfirmar();     // Add the tree change captured in BeginKeyboardMove first.
        MakeSubtreeNamesUnique(moverObj); // Rename only now, adding its step to the group.
        UndoGrupoFin();
    } else {
        UndoReparentCancelar();
    }
    moviendo = false; moverObj = NULL; gMoverNombres.clear();
}

void Outliner::CancelKeyboardMove() {
    UndoReparentCancelar();   // Restore the object; the modal leaves no undo step (test 'nombresmover').
    if (moviendo && moverObj && SceneCollection) {
        // Restoring the original position must not rename objects or add undo entries either.
        struct MoverGuard { bool prev; MoverGuard(){ prev = W3dNombresCargando; W3dNombresCargando = true; }
                            ~MoverGuard(){ W3dNombresCargando = prev; } } moverGuard;
        Object* padreOrig = moverPadreOrig ? moverPadreOrig : SceneCollection;
        ReparentKeepTransform(moverObj, padreOrig); // Restore the parent and local transform; world transform is preserved.
        if (moverAnteriorOrig) {
            MoverJuntoA(moverObj, moverAnteriorOrig, true); // Place it immediately after its previous sibling.
        } else {
            for (size_t i = 0; i < padreOrig->Childrens.size(); i++) // It was first: move it to the front.
                if (padreOrig->Childrens[i] != moverObj) { MoverJuntoA(moverObj, padreOrig->Childrens[i], false); break; }
        }
        // Restore names too; reparenting may have changed them across scenes.
        for (size_t i = 0; i < gMoverNombres.size(); i++)
            if (gMoverNombres[i].first) gMoverNombres[i].first->SetNameObj(gMoverNombres[i].second);
        Resize(width, height);
    }
    moviendo = false; moverObj = NULL; gMoverNombres.clear();
}

void Outliner::SelectRowAt(int mx, int my) {
    if (!SceneCollection) return;
    int rel = my - y - borderGS - PosY - BarTopOffset();
    if (rel < 0) return;   // Ignore the border; dividing a small negative value would yield row 0.
    int fila = rel / RenglonHeightGS;
    int filaClick = fila; // The traversal below consumes "fila".
    // Right-side columns: scene save/load/remove, visibility, and camera.
    int reservaBarra = scrollY ? (GlobalScale * 9 + gapGS) : 0;
    bool enCamara = (mx - x) >= (width - IconSizeGS - marginGS - borderGS - reservaBarra) &&
                    (mx - x) <  (width - marginGS - borderGS - reservaBarra);
    bool enGuardar = (mx - x) >= (width - 5*IconSizeGS - 4*gapGS - marginGS - borderGS - reservaBarra) &&
                     (mx - x) <  (width - 4*IconSizeGS - 4*gapGS - marginGS - borderGS - reservaBarra);
    bool enOjo = !enCamara && (mx - x) >= (width - 2*IconSizeGS - gapGS - marginGS - borderGS - reservaBarra) &&
                 (mx - x) < (width - IconSizeGS - gapGS - marginGS - borderGS - reservaBarra);
    bool enCarga = !enCamara && !enOjo &&
                   (mx - x) >= (width - 4*IconSizeGS - 3*gapGS - marginGS - borderGS - reservaBarra) &&
                   (mx - x) < (width - 3*IconSizeGS - 3*gapGS - marginGS - borderGS - reservaBarra);
    bool enEliminar = !enCamara && !enOjo &&
                      (mx - x) >= (width - 3*IconSizeGS - 2*gapGS - marginGS - borderGS - reservaBarra) &&
                      (mx - x) < (width - 2*IconSizeGS - 2*gapGS - marginGS - borderGS - reservaBarra);
    // ---- Virtual 2D armature row: inert. It cannot be selected, dragged, parented,
    // toggled, or expanded. Its bones are listed in the Properties panel's "Armature 2D" tab.
    // Consume the click here so it does not fall through to the mesh.
    {
        int fv = fila; OutFila hv;
        if (FindRowInTree(fv, hv) && hv.arm2d) return;
    }
    for (size_t c = 0; c < SceneCollection->Childrens.size(); c++) {
        int prof = 0;
        OutFila hf;
        Object* hit = FindVisibleRowRecursive(SceneCollection->Childrens[c], fila, 0, hf) ? hf.obj : NULL;
        prof = hf.prof;
        if (hit) {
            // Expand arrow (the first icon at this depth).
            int xFlecha = x + marginGS + PosX + prof * (IconSizeGS + gapGS);
            if (!enOjo && !enCamara && !enCarga && !enEliminar &&
                mx >= xFlecha && mx < xFlecha + IconSizeGS &&
                !hit->Childrens.empty()) {
                hit->desplegado = !hit->desplegado;
                Resize(width, height); // Recalculate scrolling.
                return;
            }
            LegacyUIScene* legacyScene = hit->getType() == ObjectType::ui &&
                                         hit->Parent == SceneCollection
                                             ? dynamic_cast<LegacyUIScene*>(hit) : NULL;
            if (enGuardar) {
                if (IsSceneRoot(hit)) StartSaveScene(hit);
                return;
            }
            if (enCarga) {
                if (legacyScene) ToggleSceneLoadedState(legacyScene);
                else if (hit->getType() == ObjectType::scene)
                    ToggleSceneDataLoadedState(dynamic_cast<SceneData*>(hit));
                Resize(width, height);
                return;
            }
            if (enEliminar) {
                if (IsSceneRoot(hit)) ConfirmRemoveScene(hit);
                return;
            }
            if (!enOjo && !enCamara && !enCarga && !enEliminar) {
                // This click may become a drag to reorder or parent; confirm it
                // once the pointer moves while the button is held.
                dragObjeto = hit;
                dragging = false;
                dragY0 = my;
            }
            if (enCamara) {
                hit->renderizable = !hit->renderizable;
            } else if (enOjo) {
                hit->visible = !hit->visible;
            } else if (LShiftPressed) {
                // Shift-click adds the range from the active row through the clicked row
                // (for example, rows 1 and 4 select rows 1 through 4).
                int desde = RowForObject(ObjActivo);
                if (desde < 0) {
                    hit->Seleccionar();
                } else {
                    int a = desde < filaClick ? desde : filaClick;
                    int b = desde < filaClick ? filaClick : desde;
                    for (int f = a; f <= b; f++) {
                        Object* o = ObjectAtRow(f);
                        // Keep the range in ObjSelects as well as setting select=true:
                        // operations such as Join build their list from ObjSelects but delete
                        // by the select flag. Otherwise, selected objects outside ObjSelects
                        // would be deleted without being joined. Keep both representations consistent.
                        if (o && !o->select) { o->select = true; ObjSelects.push_back(o); }
                    }
                    hit->Seleccionar(); // Keep the clicked object active.
                }
            } else if (LCtrlPressed) {
                // Ctrl-click toggles one object. Deseleccionar also removes it from ObjSelects;
                // setting select=false alone would leave it there for operations such as Join.
                if (hit->select) { hit->Deseleccionar(); if (ObjActivo == hit) ObjActivo = NULL; }
                else { hit->Seleccionar(); }
            } else if (!enOjo && !enCamara && !enCarga && !enEliminar) {
                DeseleccionarTodo();
                hit->Seleccionar();
            }
            return;
        }
    }
}

void Outliner::key_down_return(){
}

Outliner::~Outliner() {
    delete Renglon;
}