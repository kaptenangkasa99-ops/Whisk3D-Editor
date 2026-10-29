#ifndef W3D_SCENE_DATA_H
#define W3D_SCENE_DATA_H

#include "objects/Objects.h"
#include <string>

// Logical project scene. World objects and independent UI canvas roots are
// children of this node; UI retains only canvas and layout properties.
class SceneData : public Object {
public:
    std::string archivoEscena;
    bool cargada;

    SceneData(Object* parent = NULL, Vector3 pos = Vector3(0, 0, 0))
        : Object(parent, "Scene", pos), cargada(true) {
        if (!parent && SceneCollection)
            Parent = SceneCollection;
    }

    ObjectType getType() W3D_OVERRIDE { return ObjectType::scene; }
    void RenderObject() W3D_OVERRIDE {}
};

#endif
