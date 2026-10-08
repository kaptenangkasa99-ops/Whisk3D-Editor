#ifndef GUARDARW3D_H
#define GUARDARW3D_H
#include <string>

// ============================================================================
//  GuardarW3D — guarda el PROYECTO como .w3d.
//
//  FORMATO v5: el .w3d es un JSON legible y los assets se guardan en carpetas
//  vecinas al lado del archivo (escenas/, scripts/, texturas/, mallas/, etc.).
//  Al guardar, el JSON se reemplaza atomicamente despues de escribir los assets.
//  Los formatos anteriores (JSON plano, ZIP y texto Whisk3D{}) siguen abriendo
//  y se migran al layout v5 en el siguiente guardado.
//
//  REFERENCIA INTERNA vs EXTERNA: interna = ruta relativa ("texturas/pausa.png");
//  externa = prefijo "ext:". Los
//  externos NO se copian: se guarda la ruta, se listan en EXTERNOS.txt y si
//  falta alguno se avisa CLARO. Una referencia rota se CONSERVA (un pendrive
//  sacado no puede convertirse en perdida de configuracion).
//
//  LA GEOMETRIA VA EN mallas/<slug>.w3dm (formato propio, W3dMalla.h): poligonos
//  nativos, capas UV y de color, costuras, bordes marcados, normales del usuario,
//  geometria suelta, mesh parts y los pesos de los grupos, TODO por indice. El GLB
//  quedo solo para importar y para el "Export to..." del usuario. Los materiales,
//  que viajaban adentro del GLB, ahora son el bloque raiz "materiales" del JSON y
//  cada .w3dm los referencia POR NOMBRE.
//
//  Cubre: mallas, materiales, vertex anims, armatures + clips, modArmature,
//  armature 2D, el STACK de modificadores (Mirror/Screw/SubSurf/...),
//  espejo/instancia/curva (con target por nombre), el stack de CONSTRAINTS de
//  cualquier objeto (con la fuente por nombre), riel de camara,
//  paletas, layout, compilar e icono. Lo UNICO que queda afuera a proposito es
//  una Curve sin archivo de origen (no hay editor que la produzca); se avisa por
//  notificacion al guardar, sin bloquear.
//
//  QUEDA AFUERA A PROPOSITO (y se marca como "ext:", a la vista): el .obj/.wobj
//  del que se importo una malla y el .txt de una Curve. El del .obj YA NO ES UNA
//  PERDIDA: la geometria se hornea en el .w3dm y el archivo original queda solo
//  como "de donde salio" (si no esta, se avisa y la malla carga igual). El .txt
//  de la Curve sigue siendo una dependencia real: Curve::LoadFromFile usa
//  std::ifstream y no pasa por el VFS.
// ============================================================================
//
//  ESCRITURA ATOMICA: cada asset se escribe a un temporal y se mueve a su ruta;
//  el JSON se arma en "<destino>.w3dtmp" y se reemplaza atomicamente al final.
//  Un error al escribir el JSON conserva la version anterior del proyecto.
//
//  VERIFICACION ANTES DE COMMIT: se exige que TODA referencia interna del JSON y
//  de cada .w3dui tenga su archivo escrito. Si falta uno, el guardado
//  ABORTA sin renombrar. Sin esto el proyecto abriria "sin la textura" y no
//  fallaria nada, que es el fallo mas caro que puede tener este diseno.
//
//  SALIDA REPRODUCIBLE: los nombres de assets y el JSON se emiten en orden
//  determinista. Guardar dos veces sin cambios da el mismo contenido.
//
//  EL CICLO DEL ARCHIVO ABIERTO: un proyecto v4 montado se desmonta antes de
//  reemplazar su archivo y despues se conmuta el resolvedor al layout v5.
// ============================================================================
bool GuardarW3D(const std::string& ruta);

// Ctrl+S: guarda a w3dPath, o abre el explorador en modo guardar si no hay
void GuardarProyecto();
// "Guardar como": siempre pide destino (si el .w3d elegido ya existe, pide
// confirmacion antes de pisarlo, igual que Render/Export)
void GuardarProyectoComo();

// TEST del harness (--script, comando 'saveatom'): fuerza un fallo de escritura
// del .w3d DESPUES de abrir el archivo destino (simula el disco lleno). Sirve
// para verificar que un guardado a medias NO destruye la version anterior.
// Siempre false en el editor real.
extern bool g_w3dFallarEscritura;

// ICONO del juego (tarjeta Juego): un PNG con alpha en su maxima definicion.
// Se guarda en el .w3d como RUTA EXTERNA relativa al .w3d (nunca embebido);
// "Compilar juego" genera de ahi los tamanos chicos (.desktop/hicolor del .deb,
// mipmaps del APK, SDL_SetWindowIcon del main). Vacio = sin icono.
extern std::string g_proyIcono;

// ============================================================================
//  CONFIG de la tarjeta Juego (Compilar juego): viaja con el PROYECTO.
//  Antes eran statics del editor (Properties.cpp) que se reseteaban en cada
//  arranque: el "Modo ventana" elegido se perdia y el juego compilaba siempre
//  con los defaults (pantalla completa). Ahora la config vive aca, se escribe
//  en el .w3d v3 como objeto raiz "compilar" (strings legibles, editable a
//  mano) y se carga al ABRIR el proyecto (import_w3d). La tarjeta Juego edita
//  estos campos directo y GuardarW3D escribe los valores vigentes al guardar
//  (mismo patron que g_proyIcono).
// ============================================================================
struct W3dCompilarCfg {
    int  modoVentana;   // 0 Ventana, 1 Pantalla completa, 2 Sin bordes
    int  orientacion;   // 0 Todas, 1 Solo vertical, 2 Solo horizontal
    int  assetsModo;    // 0 Sueltos (editables), 1 Empaquetados (protegidos)
    int  plataforma;    // 0 Linux .deb, 1 Linux AppImage, 2 WebGL, 3 Android
    bool usarFisica;    // false = W3D_SIN_FISICA (binds stub no-op)
    bool usarSonido;    // false = sin W3D_ENABLE_AUDIO (beep() mudo)
    bool modoDebug;     // true = W3D_DEV_LOG=1 (log + ring + depurar())
    unsigned uid;       // UID3 de Symbian del juego (app propia). 0 = sin asignar. Rango self-signed: 0xE0000000-0xEFFFFFFF
    int  volumen;       // 0..100 volumen del gameplay (audio del juego). 100 = sin atenuar.
};
extern W3dCompilarCfg g_proyCompilar;

// vuelve a los DEFAULTS del editor (pantalla completa, todas las orientaciones,
// assets sueltos, Linux .deb, fisica y sonido si, debug no). Se llama al abrir
// un proyecto: un .w3d viejo sin el bloque "compilar" queda con los defaults.
void W3dCompilarReset();

// int <-> string legible del JSON (bloque "compilar"). Un string desconocido o
// vacio devuelve el default del campo (un typo editando a mano no rompe nada).
const char* W3dCompilarModoVentanaStr(int m);   // "ventana"|"pantallaCompleta"|"sinBordes"
int         W3dCompilarModoVentanaInt(const std::string& s);
const char* W3dCompilarOrientacionStr(int o);   // "todas"|"vertical"|"horizontal"
int         W3dCompilarOrientacionInt(const std::string& s);
const char* W3dCompilarAssetsStr(int a);        // "sueltos"|"empaquetados"
int         W3dCompilarAssetsInt(const std::string& s);
const char* W3dCompilarPlataformaStr(int p);    // "linux-deb"|"appimage"|"web"|"android"
int         W3dCompilarPlataformaInt(const std::string& s);

#endif // GUARDARW3D_H
