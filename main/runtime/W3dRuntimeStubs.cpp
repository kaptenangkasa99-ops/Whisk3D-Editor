#include "undo/Undo.h"
#include "importers/import_gltf.h"
#include "W3dAviso.h"
#include "w3dlog.h"
#include <string>
#include <vector>

// Runtime project loading shares editor sources but has no editor UI or undo stack.
bool gLoopCutEsPunto = false;

bool ImportModeloPorExtension(const std::string& path) {
    return ImportGLTF(path);
}

void Notificar(const std::string& message, bool error) {
    if (error) w3dLogfE("%s", message.c_str());
    else w3dLogf("%s", message.c_str());
}

void ProgresoIniciar(const std::string&) {}
void ProgresoActualizar(float) {}
void ProgresoFin() {}

W3dRenameDest W3dDestHueso3D(Armature*, int) { return W3dRenameDest(); }
W3dRenameDest W3dDestHueso2D(Mesh*, Armature2D*, int) { return W3dRenameDest(); }
W3dRenameDest W3dDestCapaMalla(Mesh*, int, int) { return W3dRenameDest(); }
std::string* W3dDestResolver(const W3dRenameDest&) { return 0; }

void UndoCapturarRenames(const std::vector<W3dRenameDest>&) {}
void UndoCapturarMallaGeo(Mesh*) {}
void UndoFundirUltimos(int) {}
bool UndoMoverCapaMalla(Mesh*, int, int, int) { return false; }
void UndoModQuitar(Mesh*) {}
void UndoModVaciar(Mesh*) {}
