#include "w3drun.h"

#include <SDL2/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>
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
static std::string g_fontPath;
static int g_safeArea[4] = {0, 0, 0, 0};

static bool RenderFrame(int width, int height, std::vector<UI2DPos>* uiPositions);

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

static void StartSceneScripts(Object* scene, bool restart) {
    StartSubtree(scene, restart);
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

const char* W3dGameFontPng() {
    return g_fontPath.c_str();
}

bool W3dGameCargarEscena3D(const char* path) {
    std::vector<unsigned char> data;
    if (!path || !w3dFileSystem::ReadFileBytes(path, data) || data.empty()) {
        w3dLogfE("[game-runtime] cannot read project JSON");
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
        if (hasScenes && root &&
            (root->getType() == ObjectType::ui || root->getType() == ObjectType::scene)) continue;
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
}

void W3dGameActualizar(float dt) {
    ++g_frame;
    for (size_t i = 0; i < g_scripts.size(); ++i) {
        Object* object = g_scripts[i];
        if (W3dEscenaEsDeActiva(object))
            W3dScriptActualizar(object, dt);
    }
    W3dEscenaAplicarPendiente();
    BindsJuegoSnapshotPunteros();
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
    if (!g_capturePath.empty() && g_frame >= g_captureFrame) {
        std::vector<unsigned char> pixels((size_t)width * height * 4);
        gfx::ReadPixelsRGBA(0, 0, width, height, &pixels[0]);
        if (!gfx::SavePNG(g_capturePath.c_str(), &pixels[0], width, height, true))
            w3dLogfE("[game-runtime] could not save capture: %s", g_capturePath.c_str());
        BindsJuegoPedirSalir();
    }
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
