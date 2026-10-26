#include "w3drun.h"

#include <SDL2/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>
#include <set>
#include <vector>

#include "W3dEscena.h"
#include "W3dAviso.h"
#include "animation/Animation.h"
#include "base/w3dlog.h"
#include "gfx/w3dGraphics.h"
#include "gfx/w3dTexture.h"
#include "audio/W3dVolumen.h"
#include "importers/import_obj.h"
#include "importers/import_w3d.h"
#include "io/w3dFilesystem.h"
#include "io/Video2DCache.h"
#include "objects/Camera.h"
#include "objects/Mesh.h"
#include "objects/Materials.h"
#include "objects/Objects.h"
#include "objects/RuntimeRenderPackets.h"
#include "render/EscenaRender.h"
#include "render/OpcionesRender.h"
#include "render/UIOverlay.h"
#include "script/BindsJuego.h"
#include "script/SimJuego.h"
#include "script/W3dScript.h"
#include "variables.h"

namespace gfx = w3dEngine;

float g_gameRenderW = 640.0f;
float g_gameRenderH = 480.0f;
std::string w3dPath;
Cursor3D cursor3D;
bool gSimCacheOn = false;

static std::vector<Object*> g_scripts;
static std::string g_capturePath;
static int g_captureFrame = 0;
static int g_frame = 0;
static int g_width = 1;
static int g_height = 1;
static bool g_sceneLoaded = false;
static bool g_runtimeReady = false;
static bool g_inputInjected = false;
static bool g_inputReleased = false;
static bool g_playerMoved = false;
static bool g_packetTestsPassed = false;
static bool g_transparencyTestRun = false;
static bool g_visibilityPassed = false;
static bool g_materialGroupsPassed = false;
static bool g_pvsParityPassed = false;
static bool g_materialGroupImageMatch = false;
static bool g_opaqueImageMatch = false;
static bool g_mirroredImageMatch = false;
static bool g_transparencyPassed = false;
static int g_sceneLitPixels = 0;
static Vector3 g_playerStart;
static Object* g_player = NULL;
static std::string g_fontPath;
static int g_safeArea[4] = {0, 0, 0, 0};

static bool RenderFrame(int width, int height, std::vector<UI2DPos>* uiPositions);

struct SmokeMaterialState {
    Material* material;
    bool transparent;
    bool depthWrite;
    int pass;
    float alpha;
};

static void CollectMeshMaterials(Object* object, std::vector<SmokeMaterialState>* states,
                                 std::set<Material*>* seen) {
    if (!object) return;
    if (object->getType() == ObjectType::mesh) {
        Mesh* mesh = (Mesh*)object;
        for (size_t i = 0; i < mesh->materialsGroup.size(); ++i) {
            Material* material = mesh->materialsGroup[i].material;
            if (!material) material = MaterialDefecto;
            if (!material || !seen->insert(material).second) continue;
            SmokeMaterialState state;
            state.material = material;
            state.transparent = material->transparent;
            state.depthWrite = material->depth_write;
            state.pass = material->orden_pasada;
            state.alpha = material->diffuse[3];
            states->push_back(state);
        }
    }
    for (size_t i = 0; i < object->Childrens.size(); ++i)
        CollectMeshMaterials(object->Childrens[i], states, seen);
}

static bool CheckPackets(bool translucent, bool checkTransforms) {
    size_t count = 0;
    const W3dRuntimeRenderPacketInfo* packets = W3dRuntimeRenderPacketsLast(&count);
    if (!packets || count != 2) return false;
    Object* ground = FindObjectByName(SceneCollection, "Ground");
    Object* player = FindObjectByName(SceneCollection, "Player");
    bool sawGround = false;
    bool sawPlayer = false;
    for (size_t i = 0; i < count; ++i) {
        Mesh* mesh = packets[i].mesh;
        if (!mesh || !mesh->vertex || !mesh->faces || mesh->vertexSize <= 0 ||
            mesh->facesSize <= 0 || mesh->materialsGroup.empty())
            return false;
        if (!mesh->materialsGroup[0].material && !MaterialDefecto) return false;
        if (packets[i].translucent != translucent) return false;
        if (i > 0 && packets[i - 1].sortKey > packets[i].sortKey) return false;
        if (!std::isfinite(packets[i].depth)) return false;
        if (mesh == ground) sawGround = true;
        if (mesh == player) sawPlayer = true;
        if (checkTransforms) {
            Matrix4 expected;
            mesh->GetWorldMatrix(expected);
            if (std::fabs(expected.m[12] - packets[i].world.m[12]) > 0.001f ||
                std::fabs(expected.m[13] - packets[i].world.m[13]) > 0.001f ||
                std::fabs(expected.m[14] - packets[i].world.m[14]) > 0.001f)
                return false;
        }
    }
    if (!sawGround || !sawPlayer) return false;
    if (translucent && packets[0].depth < packets[1].depth) return false;
    return true;
}

static bool CheckVisibilityExtraction(int width, int height,
                                     std::vector<UI2DPos>* uiPositions) {
    Object* player = FindObjectByName(SceneCollection, "Player");
    Object* ground = FindObjectByName(SceneCollection, "Ground");
    if (!player || !ground) return false;
    const bool visible = player->visible;
    player->visible = false;
    bool rendered = RenderFrame(width, height, uiPositions);
    size_t count = 0;
    const W3dRuntimeRenderPacketInfo* packets = W3dRuntimeRenderPacketsLast(&count);
    bool omitted = rendered && packets && count == 1 && packets[0].mesh == ground;
    player->visible = visible;
    rendered = RenderFrame(width, height, uiPositions);
    return omitted && rendered && CheckPackets(false, true);
}

static bool CheckTransparencyPass(int width, int height,
                                 const std::vector<unsigned char>& opaquePixels,
                                 std::vector<UI2DPos>* uiPositions) {
    std::vector<SmokeMaterialState> states;
    std::set<Material*> seen;
    CollectMeshMaterials(SceneCollection, &states, &seen);
    if (states.empty()) return false;
    for (size_t i = 0; i < states.size(); ++i) {
        states[i].material->transparent = true;
        states[i].material->depth_write = false;
        states[i].material->orden_pasada = 2;
        states[i].material->diffuse[3] = 0.45f;
    }
    bool rendered = RenderFrame(width, height, uiPositions);
    bool packetOrder = rendered && CheckPackets(true, true);
    std::vector<unsigned char> translucentPixels((size_t)width * height * 4);
    gfx::ReadPixelsRGBA(0, 0, width, height, &translucentPixels[0]);
    bool imageChanged = false;
    for (size_t i = 0; i < opaquePixels.size(); i += 4) {
        if (opaquePixels[i] != translucentPixels[i] ||
            opaquePixels[i + 1] != translucentPixels[i + 1] ||
            opaquePixels[i + 2] != translucentPixels[i + 2]) {
            imageChanged = true;
            break;
        }
    }
    for (size_t i = 0; i < states.size(); ++i) {
        states[i].material->transparent = states[i].transparent;
        states[i].material->depth_write = states[i].depthWrite;
        states[i].material->orden_pasada = states[i].pass;
        states[i].material->diffuse[3] = states[i].alpha;
    }
    return packetOrder && imageChanged;
}

static bool CheckMaterialGroupExtraction(int width, int height,
                                         std::vector<UI2DPos>* uiPositions) {
    Mesh* ground = (Mesh*)FindObjectByName(SceneCollection, "Ground");
    Mesh* player = (Mesh*)FindObjectByName(SceneCollection, "Player");
    if (!ground || !player || ground->pvsFaces || ground->materialsGroup.empty() ||
        player->materialsGroup.empty() ||
        ground->getType() != ObjectType::mesh ||
        player->getType() != ObjectType::mesh)
        return false;

    const std::vector<MaterialGroup> originalGroups = ground->materialsGroup;
    const MaterialGroup& original = originalGroups[0];
    const int split = (original.indicesDrawnCount / 6) * 3;
    if (split < 3 || split >= original.indicesDrawnCount) return false;

    Material translucentMaterial("packet group test");
    translucentMaterial.transparent = true;
    translucentMaterial.depth_write = false;
    translucentMaterial.orden_pasada = 2;

    MaterialGroup opaqueGroup = original;
    MaterialGroup translucentGroup = original;
    Material* opaqueMaterial = original.material ? original.material : MaterialDefecto;
    opaqueGroup.count = split / 3;
    opaqueGroup.indicesDrawnCount = split;
    translucentGroup.start += split / 3;
    translucentGroup.startDrawn += split;
    translucentGroup.count -= split / 3;
    translucentGroup.indicesDrawnCount -= split;
    translucentGroup.material = &translucentMaterial;
    ground->materialsGroup.clear();
    ground->materialsGroup.push_back(opaqueGroup);
    ground->materialsGroup.push_back(translucentGroup);

    const bool packetEnabled = W3dRuntimeRenderPacketsSetEnabled(false);
    bool legacyRendered = RenderFrame(width, height, uiPositions);
    std::vector<unsigned char> legacyPixels((size_t)width * height * 4);
    if (legacyRendered)
        gfx::ReadPixelsRGBA(0, 0, width, height, &legacyPixels[0]);
    W3dRuntimeRenderPacketsSetEnabled(packetEnabled);

    const bool rendered = RenderFrame(width, height, uiPositions);
    std::vector<unsigned char> packetPixels((size_t)width * height * 4);
    if (rendered)
        gfx::ReadPixelsRGBA(0, 0, width, height, &packetPixels[0]);
    const bool imageMatch = legacyRendered && rendered &&
                            legacyPixels == packetPixels;
    g_materialGroupImageMatch = imageMatch;

    size_t count = 0;
    const W3dRuntimeRenderPacketInfo* packets = W3dRuntimeRenderPacketsLast(&count);
    bool sawOpaqueGroup = false;
    bool sawTranslucentGroup = false;
    bool valid = rendered && packets && count == 3;
    if (valid) {
        for (size_t i = 0; i < count; ++i) {
            if (i > 0 && packets[i - 1].sortKey > packets[i].sortKey)
                valid = false;
            if (packets[i].mesh != ground) continue;
            if (packets[i].materialGroup == 0 &&
                packets[i].material == opaqueMaterial &&
                !packets[i].translucent &&
                packets[i].indexStart == opaqueGroup.startDrawn &&
                packets[i].indexCount == opaqueGroup.indicesDrawnCount)
                sawOpaqueGroup = true;
            if (packets[i].materialGroup == 1 &&
                packets[i].material == &translucentMaterial &&
                packets[i].translucent &&
                packets[i].indexStart == translucentGroup.startDrawn &&
                packets[i].indexCount == translucentGroup.indicesDrawnCount)
                sawTranslucentGroup = true;
        }
    }
    if (valid && sawOpaqueGroup && sawTranslucentGroup) {
        size_t opaqueIndex = count;
        size_t translucentIndex = count;
        for (size_t i = 0; i < count; ++i) {
            if (packets[i].mesh != ground) continue;
            if (packets[i].materialGroup == 0) opaqueIndex = i;
            if (packets[i].materialGroup == 1) translucentIndex = i;
        }
        valid = opaqueIndex < translucentIndex;
    }
    ground->materialsGroup = originalGroups;
    return valid && imageMatch && sawOpaqueGroup && sawTranslucentGroup;
}

static bool CheckOrderedPvsPacketFallback(int width, int height,
                                          std::vector<UI2DPos>* uiPositions) {
    Mesh* ground = (Mesh*)FindObjectByName(SceneCollection, "Ground");
    if (!ground || ground->getType() != ObjectType::mesh ||
        ground->facesSize < 6 || ground->materialsGroup.empty() ||
        ground->pvsFaces)
        return false;

    const std::vector<MaterialGroup> originalGroups = ground->materialsGroup;
    const std::vector<MaterialGroup> originalPvsGroups = ground->pvsGroups;
    const std::vector<Mesh::PvsRun> originalPvsRuns = ground->pvsRuns;
    MeshIndex* originalPvsFaces = ground->pvsFaces;
    const int originalPvsFacesSize = ground->pvsFacesSize;
    const bool originalPvsOrdered = ground->pvsOrdenado;
    const MaterialGroup& original = originalGroups[0];
    const int split = (original.indicesDrawnCount / 6) * 3;
    if (split < 3 || split >= original.indicesDrawnCount) return false;

    Material translucentMaterial("ordered PVS packet test");
    translucentMaterial.transparent = true;
    translucentMaterial.depth_write = false;
    translucentMaterial.orden_pasada = 2;

    MaterialGroup opaqueGroup = original;
    MaterialGroup translucentGroup = original;
    opaqueGroup.count = split / 3;
    opaqueGroup.startDrawn = 0;
    opaqueGroup.indicesDrawnCount = split;
    translucentGroup.start += split / 3;
    translucentGroup.startDrawn = split;
    translucentGroup.count -= split / 3;
    translucentGroup.indicesDrawnCount -= split;
    translucentGroup.material = &translucentMaterial;
    ground->materialsGroup.clear();
    ground->materialsGroup.push_back(opaqueGroup);
    ground->materialsGroup.push_back(translucentGroup);

    std::vector<MeshIndex> pvsFaces(6);
    for (int i = 0; i < 6; ++i) pvsFaces[(size_t)i] = ground->faces[i];
    ground->pvsFaces = &pvsFaces[0];
    ground->pvsFacesSize = (int)pvsFaces.size();
    ground->pvsGroups = ground->materialsGroup;
    ground->pvsGroups[0].startDrawn = 0;
    ground->pvsGroups[0].indicesDrawnCount = 3;
    ground->pvsGroups[1].startDrawn = 0;
    ground->pvsGroups[1].indicesDrawnCount = 0;
    ground->pvsOrdenado = true;
    ground->pvsRuns.clear();
    Mesh::PvsRun run;
    run.grupo = 1;
    run.start = 3;
    run.count = 3;
    ground->pvsRuns.push_back(run);

    const bool packetEnabled = W3dRuntimeRenderPacketsSetEnabled(false);
    bool legacyRendered = RenderFrame(width, height, uiPositions);
    std::vector<unsigned char> legacyPixels((size_t)width * height * 4);
    if (legacyRendered)
        gfx::ReadPixelsRGBA(0, 0, width, height, &legacyPixels[0]);
    W3dRuntimeRenderPacketsSetEnabled(packetEnabled);

    const bool rendered = RenderFrame(width, height, uiPositions);
    std::vector<unsigned char> packetPixels((size_t)width * height * 4);
    if (rendered)
        gfx::ReadPixelsRGBA(0, 0, width, height, &packetPixels[0]);

    size_t packetCount = 0;
    const W3dRuntimeRenderPacketInfo* packets =
        W3dRuntimeRenderPacketsLast(&packetCount);
    bool retainedPvsPacket = false;
    if (packets && packetCount == 2) {
        for (size_t i = 0; i < packetCount; ++i) {
            if (packets[i].mesh == ground &&
                packets[i].materialGroup == (size_t)-1 &&
                packets[i].indexStart == 0 &&
                packets[i].indexCount == (int)pvsFaces.size() &&
                packets[i].translucent)
                retainedPvsPacket = true;
        }
    }

    ground->materialsGroup = originalGroups;
    ground->pvsGroups = originalPvsGroups;
    ground->pvsRuns = originalPvsRuns;
    ground->pvsFaces = originalPvsFaces;
    ground->pvsFacesSize = originalPvsFacesSize;
    ground->pvsOrdenado = originalPvsOrdered;
    return legacyRendered && rendered && legacyPixels == packetPixels &&
           retainedPvsPacket;
}

static bool CheckMirroredPacketParity(int width, int height,
                                      std::vector<UI2DPos>* uiPositions) {
    Mesh* player = (Mesh*)FindObjectByName(SceneCollection, "Player");
    if (!player || player->getType() != ObjectType::mesh) return false;
    const Vector3 originalScale = player->scale;
    player->scale.x = originalScale.x == 0.0f ? -1.0f : -std::fabs(originalScale.x);

    const bool packetEnabled = W3dRuntimeRenderPacketsSetEnabled(false);
    const bool legacyRendered = RenderFrame(width, height, uiPositions);
    std::vector<unsigned char> legacyPixels((size_t)width * height * 4);
    if (legacyRendered)
        gfx::ReadPixelsRGBA(0, 0, width, height, &legacyPixels[0]);
    W3dRuntimeRenderPacketsSetEnabled(packetEnabled);

    const bool packetRendered = RenderFrame(width, height, uiPositions);
    std::vector<unsigned char> packetPixels((size_t)width * height * 4);
    if (packetRendered)
        gfx::ReadPixelsRGBA(0, 0, width, height, &packetPixels[0]);
    bool mirroredTransform = false;
    size_t packetCount = 0;
    const W3dRuntimeRenderPacketInfo* packets =
        W3dRuntimeRenderPacketsLast(&packetCount);
    for (size_t i = 0; packets && i < packetCount; ++i) {
        if (packets[i].mesh != player) continue;
        const Matrix4& m = packets[i].world;
        const float determinant =
            m.m[0] * (m.m[5] * m.m[10] - m.m[9] * m.m[6]) -
            m.m[4] * (m.m[1] * m.m[10] - m.m[9] * m.m[2]) +
            m.m[8] * (m.m[1] * m.m[6] - m.m[5] * m.m[2]);
        mirroredTransform = determinant < 0.0f;
    }
    const bool sameImage = legacyRendered && packetRendered &&
                           legacyPixels == packetPixels && mirroredTransform &&
                           CheckPackets(false, true);
    g_mirroredImageMatch = sameImage;
    player->scale = originalScale;
    if (!RenderFrame(width, height, uiPositions)) return false;
    return sameImage;
}

void ProgresoIniciar(const std::string&) {}
void ProgresoActualizar(float) {}
void ProgresoFin() {}

const char* W3dGameFontPng() {
    return g_fontPath.c_str();
}

const VideoPreview* Video2DPreview(const std::string&) {
    return NULL;
}

unsigned W3dMsAhora() {
    return SDL_GetTicks();
}

bool ImportModeloPorExtension(const std::string& path) {
    return ImportOBJ(path, false);
}

namespace w3dEngine {
void VolumenAplicarProyecto(float) {}
}

void Notificar(const std::string& message, bool error) {
    (error ? std::fprintf(stderr, "%s\n", message.c_str())
           : std::fprintf(stdout, "%s\n", message.c_str()));
}

static void GatherScripts(Object* object) {
    if (!object) return;
    if (object->scriptDatos && !object->scriptDatos->scripts.empty())
        g_scripts.push_back(object);
    for (size_t i = 0; i < object->Childrens.size(); ++i)
        GatherScripts(object->Childrens[i]);
}

static void StartSubtree(Object* object, bool restart) {
    if (!object) return;
    if (object->scriptDatos && !object->scriptDatos->scripts.empty()) {
        if (restart) W3dScriptDescargarDe(object);
        if (W3dScriptCargar(object)) {
            for (size_t scriptIndex = 0;
                 scriptIndex < object->scriptDatos->scripts.size(); ++scriptIndex) {
                const W3dScriptEntrada& entry = object->scriptDatos->scripts[scriptIndex];
                for (size_t refIndex = 0; refIndex < entry.refs.size(); ++refIndex) {
                    const std::string& prop = entry.refs[refIndex].first;
                    const std::string& value = entry.refs[refIndex].second;
                    Object* scope = W3dEscenaRaizDe(object);
                    Object* target = scope ? FindObjectByName(scope, value) : NULL;
                    if (target)
                        W3dScriptResolverRef(object, (int)scriptIndex, prop, target);
                    W3dScriptResolverOpcion(object, (int)scriptIndex, prop, value);
                    W3dScriptResolverValor(object, (int)scriptIndex, prop, value);
                }
            }
            W3dScriptInicio(object);
        }
    }
    for (size_t i = 0; i < object->Childrens.size(); ++i)
        StartSubtree(object->Childrens[i], restart);
}

static void StartSceneScripts(class UI* scene, bool restart) {
    StartSubtree((Object*)scene, restart);
}

static void PixelLayout(int width, int height, float* x, float* y,
                        float* drawWidth, float* drawHeight, float* scale) {
    float canvasWidth = 0.0f;
    float canvasHeight = 0.0f;
    UI2D_TamanoLienzo(&canvasWidth, &canvasHeight);
    if (canvasWidth < 1.0f) canvasWidth = (float)width;
    if (canvasHeight < 1.0f) canvasHeight = (float)height;
    *scale = (float)width / canvasWidth;
    if ((float)height / canvasHeight < *scale)
        *scale = (float)height / canvasHeight;
    if (*scale <= 0.0f) *scale = 1.0f;
    *drawWidth = canvasWidth * *scale;
    *drawHeight = canvasHeight * *scale;
    *x = ((float)width - *drawWidth) * 0.5f;
    *y = ((float)height - *drawHeight) * 0.5f;
}

static bool RenderFrame(int width, int height, std::vector<UI2DPos>* uiPositions) {
    if (!g_sceneLoaded || !SceneCollection) return false;
    gfx::Viewport(0, 0, width, height);
    gfx::ClearColor(g_renderBg[0], g_renderBg[1], g_renderBg[2], 1.0f);
    gfx::Clear(gfx::ColorBuffer | gfx::DepthBuffer | gfx::StencilBuffer);
    gfx::Enable(gfx::DepthTest);
    gfx::Disable(gfx::CullFace);

    if (CameraActive) {
        CameraActive->UpdatePosition();
        CameraActive->UpdateLookAt();
        W3dVista3D view3d;
        view3d.cam.pos = CameraActive->GetGlobalPosition();
        view3d.cam.rot = CameraActive->Rot();
        view3d.fov = CameraActive->fov;
        view3d.nearC = CameraActive->nearClip;
        view3d.farC = CameraActive->farClip;
        view3d.orto = CameraActive->orthographic;
        view3d.ortoSize = CameraActive->fov;
        view3d.aspectoVista = (float)width / (float)height;
        view3d.aspectoImagen = CameraActive->AspectoDeclarado();
        if (view3d.aspectoImagen <= 0.01f)
            view3d.aspectoImagen = view3d.aspectoVista;
        W3dEncuadreMarco(view3d.aspectoVista, view3d.aspectoImagen, 1.0f,
                         &view3d.marcoNX, &view3d.marcoNY);
        W3dEscena3DModo((int)RenderType::MaterialPreview);
        W3dEscena3DProyeccion(view3d);
        W3dEscena3DLuces((int)RenderType::MaterialPreview);
        W3dEscena3DCamara(view3d, view3d.cam.pos, Vector3(1.0f, 1.0f, 1.0f));
        W3dEscena3DPasada();
        std::vector<unsigned char> scenePixels((size_t)width * height * 4);
        gfx::ReadPixelsRGBA(0, 0, width, height, &scenePixels[0]);
        g_sceneLitPixels = 0;
        for (size_t i = 0; i + 3 < scenePixels.size(); i += 4) {
            int dr = (int)scenePixels[i] - (int)(g_renderBg[0] * 255.0f);
            int dg = (int)scenePixels[i + 1] - (int)(g_renderBg[1] * 255.0f);
            int db = (int)scenePixels[i + 2] - (int)(g_renderBg[2] * 255.0f);
            if (dr * dr + dg * dg + db * db > 900)
                ++g_sceneLitPixels;
        }
    }

    float x = 0.0f, y = 0.0f, drawWidth = 0.0f, drawHeight = 0.0f, scale = 1.0f;
    PixelLayout(width, height, &x, &y, &drawWidth, &drawHeight, &scale);
    gfx::Viewport(0, 0, width, height);
    gfx::MatrixMode(gfx::Projection);
    gfx::LoadIdentity();
    gfx::Ortho(0.0f, (float)width, (float)height, 0.0f, -1.0f, 1.0f);
    gfx::MatrixMode(gfx::ModelView);
    gfx::LoadIdentity();
    gfx::Disable(gfx::DepthTest);
    UI2D_BaseRecorte(0, 0, width, height);
    UI2D_DibujarOverlay(x, y, drawWidth, drawHeight, scale, uiPositions);
    BindsJuegoRender(scale, x, y, (float)g_safeArea[0], (float)g_safeArea[1],
                     (float)g_safeArea[2], (float)g_safeArea[3], uiPositions);
    return true;
}

void W3dGameFontPngSet(const char* path) {
    g_fontPath = path ? path : "";
}

bool W3dGameCargarEscena3D(const char* path) {
    std::vector<unsigned char> data;
    if (!path || !w3dFileSystem::ReadFileBytes(path, data) || data.empty()) {
        w3dLogfE("[runtime-smoke] cannot read project JSON");
        return false;
    }
    w3dPath = path;
    g_sceneLoaded = W3dProyectoCargarEscena3D(&data[0], data.size());
    return g_sceneLoaded;
}

void W3dGameInicio() {
    W3dScriptSetBindExtra(BindsJuegoRegistrar);
    W3dEscenaSetInit(StartSceneScripts);
    W3dEscenaRegistrarTodas();
    GatherScripts(SceneCollection);
    bool hasScenes = W3dEscenaHayEscenas();
    for (size_t i = 0; i < g_scripts.size(); ++i) {
        Object* scriptObject = g_scripts[i];
        Object* root = W3dEscenaRaizDe(scriptObject);
        if (hasScenes && root && root->getType() == ObjectType::ui) continue;
            if (scriptObject->scriptDatos && W3dScriptCargar(scriptObject)) {
                for (size_t scriptIndex = 0;
                     scriptIndex < scriptObject->scriptDatos->scripts.size(); ++scriptIndex) {
                    const W3dScriptEntrada& entry = scriptObject->scriptDatos->scripts[scriptIndex];
                    for (size_t refIndex = 0; refIndex < entry.refs.size(); ++refIndex) {
                        const std::string& prop = entry.refs[refIndex].first;
                        const std::string& value = entry.refs[refIndex].second;
                        Object* scope = W3dEscenaRaizDe(scriptObject);
                        Object* target = scope ? FindObjectByName(scope, value) : NULL;
                        if (target)
                            W3dScriptResolverRef(scriptObject, (int)scriptIndex, prop, target);
                        W3dScriptResolverOpcion(scriptObject, (int)scriptIndex, prop, value);
                        W3dScriptResolverValor(scriptObject, (int)scriptIndex, prop, value);
                    }
                }
                W3dScriptInicio(scriptObject);
            }
    }
    if (hasScenes) W3dEscenaArrancar();
    g_player = SceneCollection ? FindObjectByName(SceneCollection, "Player") : NULL;
    if (g_player) g_playerStart = g_player->pos;
    g_runtimeReady = g_sceneLoaded && !g_scripts.empty();
}

void W3dGameActualizar(float dt) {
    ++g_frame;
    if (!g_inputInjected) {
        W3dGameTecla(SDLK_RIGHT, true);
        g_inputInjected = true;
    }
    if (g_frame == 20 && !g_inputReleased) {
        W3dGameTecla(SDLK_RIGHT, false);
        g_inputReleased = true;
    }
    for (size_t i = 0; i < g_scripts.size(); ++i) {
        Object* object = g_scripts[i];
        if (W3dEscenaEsDeActiva(object))
            W3dScriptActualizar(object, dt);
    }
    W3dEscenaAplicarPendiente();
    BindsJuegoSnapshotPunteros();
    if (g_player && (g_player->pos - g_playerStart).LengthSq() > 0.0001f)
        g_playerMoved = true;
}

void W3dGameRender(int width, int height) {
    g_width = width;
    g_height = height;
    g_gameRenderW = (float)width;
    g_gameRenderH = (float)height;
    std::vector<UI2DPos> uiPositions;
    if (!RenderFrame(width, height, &uiPositions)) {
        BindsJuegoPedirSalir();
        return;
    }
    g_packetTestsPassed = CheckPackets(false, true);
    if (g_capturePath.empty() || g_frame < g_captureFrame) return;

    if (!g_transparencyTestRun) {
        g_visibilityPassed = CheckVisibilityExtraction(width, height, &uiPositions);
        const bool packetEnabled = W3dRuntimeRenderPacketsSetEnabled(false);
        bool comparisonPassed = RenderFrame(width, height, &uiPositions);
        std::vector<unsigned char> legacyPixels((size_t)width * height * 4);
        if (comparisonPassed)
            gfx::ReadPixelsRGBA(0, 0, width, height, &legacyPixels[0]);
        W3dRuntimeRenderPacketsSetEnabled(packetEnabled);
        comparisonPassed = comparisonPassed && RenderFrame(width, height, &uiPositions);
        std::vector<unsigned char> packetPixels((size_t)width * height * 4);
        if (comparisonPassed)
            gfx::ReadPixelsRGBA(0, 0, width, height, &packetPixels[0]);
        bool identical = comparisonPassed && legacyPixels == packetPixels;
        g_opaqueImageMatch = identical;
        g_packetTestsPassed = g_packetTestsPassed && g_visibilityPassed && identical &&
                              CheckPackets(false, true);
        g_transparencyPassed = CheckTransparencyPass(width, height, packetPixels, &uiPositions);
        g_packetTestsPassed = g_packetTestsPassed && g_transparencyPassed;
        g_materialGroupsPassed =
            CheckMaterialGroupExtraction(width, height, &uiPositions);
        g_packetTestsPassed = g_packetTestsPassed && g_materialGroupsPassed;
        g_pvsParityPassed =
            CheckOrderedPvsPacketFallback(width, height, &uiPositions);
        g_packetTestsPassed = g_packetTestsPassed && g_pvsParityPassed;
        g_packetTestsPassed = g_packetTestsPassed &&
                              CheckMirroredPacketParity(width, height, &uiPositions);
        g_transparencyTestRun = true;
        if (!RenderFrame(width, height, &uiPositions)) g_packetTestsPassed = false;
        g_packetTestsPassed = g_packetTestsPassed && CheckPackets(false, true);
    }
    std::vector<unsigned char> pixels((size_t)width * height * 4);
    gfx::ReadPixelsRGBA(0, 0, width, height, &pixels[0]);
    int litPixels = 0;
    for (size_t i = 0; i + 3 < pixels.size(); i += 4)
        if (pixels[i] > 8 || pixels[i + 1] > 8 || pixels[i + 2] > 8)
            ++litPixels;
    bool saved = gfx::SavePNG(g_capturePath.c_str(), &pixels[0], width, height, true);
    bool pass = g_runtimeReady && g_inputReleased && g_playerMoved && g_packetTestsPassed &&
                g_sceneLitPixels > width * height / 100 &&
                litPixels > width * height / 100 && saved;
    std::printf("[runtime-smoke] loaded=%d scripts=%u playerMoved=%d packets=%d visibility=%d materialGroups=%d materialImageMatch=%d pvsParity=%d mirroredMatch=%d opaqueMatch=%d transparency=%d scenePixels=%d pixels=%d saved=%d\n",
                g_sceneLoaded ? 1 : 0, (unsigned)g_scripts.size(), g_playerMoved ? 1 : 0,
                g_packetTestsPassed ? 1 : 0, g_visibilityPassed ? 1 : 0,
                g_materialGroupsPassed ? 1 : 0,
                g_materialGroupImageMatch ? 1 : 0, g_pvsParityPassed ? 1 : 0,
                g_mirroredImageMatch ? 1 : 0,
                g_opaqueImageMatch ? 1 : 0, g_transparencyPassed ? 1 : 0,
                g_sceneLitPixels, litPixels, saved ? 1 : 0);
    if (pass)
        std::printf("[RUNTIME SMOKE PASS] generated template runtime loaded, updated, and rendered\n");
    else
        std::printf("[RUNTIME SMOKE FAIL] generated template runtime did not satisfy checks\n");
    BindsJuegoPedirSalir();
}

void W3dGameCaptura(const char* path, int frame) {
    g_capturePath = path ? path : "";
    g_captureFrame = frame > 0 ? frame : 1;
}

bool W3dGameQuierePararse() {
    return BindsJuegoQuierePararse();
}

void W3dGameTecla(int key, bool down) {
    const char* name = NULL;
    if (key >= SDLK_a && key <= SDLK_z) {
        static char letter[2] = {0, 0};
        letter[0] = (char)key;
        name = letter;
    } else if (key == SDLK_RIGHT) name = "right";
    else if (key == SDLK_LEFT) name = "left";
    else if (key == SDLK_UP) name = "up";
    else if (key == SDLK_DOWN) name = "down";
    else if (key == SDLK_SPACE) name = "space";
    if (name) W3dScriptTecla(name, down);
}

static void Pointer(int slot, int x, int y, bool active) {
    float canvasWidth = 0.0f, canvasHeight = 0.0f;
    UI2D_TamanoLienzo(&canvasWidth, &canvasHeight);
    if (canvasWidth < 1.0f) canvasWidth = (float)g_width;
    if (canvasHeight < 1.0f) canvasHeight = (float)g_height;
    float scale = (float)g_width / canvasWidth;
    if ((float)g_height / canvasHeight < scale)
        scale = (float)g_height / canvasHeight;
    if (scale <= 0.0f) scale = 1.0f;
    float px = ((float)x - (float)g_width * 0.5f) / scale;
    float py = ((float)y - (float)g_height * 0.5f) / scale;
    BindsJuegoPunto(slot, px, py, active);
    W3dScriptDedo(slot, px, py, active);
    if (slot == 0) W3dScriptToque(px, py, active);
}

void W3dGameToque(int x, int y, bool active) {
    Pointer(0, x, y, active);
}

void W3dGameRaton(int x, int y, bool present) {
    float canvasWidth = 0.0f, canvasHeight = 0.0f;
    UI2D_TamanoLienzo(&canvasWidth, &canvasHeight);
    if (canvasWidth < 1.0f) canvasWidth = (float)g_width;
    if (canvasHeight < 1.0f) canvasHeight = (float)g_height;
    float scale = (float)g_width / canvasWidth;
    if ((float)g_height / canvasHeight < scale)
        scale = (float)g_height / canvasHeight;
    if (scale <= 0.0f) scale = 1.0f;
    BindsJuegoRaton(((float)x - (float)g_width * 0.5f) / scale,
                    ((float)y - (float)g_height * 0.5f) / scale, present);
}

void W3dGameDedo(int slot, int x, int y, bool active) {
    if (slot >= 0 && slot < 4) Pointer(slot, x, y, active);
}

void W3dGameStick(int slot, float x, float y) {
    W3dScriptStick(slot, x, y);
}

void W3dGameBoton(const char* name, bool down) {
    W3dScriptBotonPad(name, down);
}

void W3dGameSafeArea(int x, int y, int width, int height) {
    g_safeArea[0] = x;
    g_safeArea[1] = y;
    g_safeArea[2] = width;
    g_safeArea[3] = height;
}
