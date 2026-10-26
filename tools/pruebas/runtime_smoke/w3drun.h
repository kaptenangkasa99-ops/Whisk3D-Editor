#ifndef W3D_TEST_RUNTIME_H
#define W3D_TEST_RUNTIME_H

#include <string>
#include <vector>

class Object;

void W3dGameFontPngSet(const char* path);
bool W3dGameCargarEscena3D(const char* path);
void W3dGameInicio();
void W3dGameActualizar(float dt);
void W3dGameRender(int width, int height);
void W3dGameCaptura(const char* path, int frame);
bool W3dGameQuierePararse();
void W3dGameTecla(int key, bool down);
void W3dGameToque(int x, int y, bool active);
void W3dGameRaton(int x, int y, bool present);
void W3dGameDedo(int slot, int x, int y, bool active);
void W3dGameStick(int slot, float x, float y);
void W3dGameBoton(const char* name, bool down);
void W3dGameSafeArea(int x, int y, int width, int height);

#endif
