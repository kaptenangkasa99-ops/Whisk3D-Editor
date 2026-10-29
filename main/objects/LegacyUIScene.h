#ifndef W3D_LEGACY_UI_SCENE_H
#define W3D_LEGACY_UI_SCENE_H

#include "objects/UI.h"

// Compatibility wrapper for projects where a top-level UI canvas represented
// an entire runtime scene. New canvases use UI directly under SceneData.
class LegacyUIScene : public UI {
public:
    bool cargada;

    LegacyUIScene(Object* parent = NULL, Vector3 pos = Vector3(0, 0, 0))
        : UI(parent, pos), cargada(true) {
        if (!parent && SceneCollection)
            Parent = SceneCollection;
    }
};

#endif
