#ifndef OUTLINER_H
#define OUTLINER_H

#include <vector>
#ifndef W3D_SYMBIAN
    #include <SDL2/SDL.h>
    #include "variables.h"
    #include "sdl_key_compat.h"
#endif
#include "ViewPorts.h"
#include "ScrollBar.h"
#include "WithBorder.h"
#include "objects/Objects.h"
#include "objects/ObjectMode.h"
#include "objects/Textures.h"
#include "WhiskUI/draw/rectangle.h"
#include "WhiskUI/text/bitmapText.h"

class Mesh; // Virtual "Armature 2D" row; its 2D bones belong to the mesh, not the scene.
class LegacyUIScene;

bool W3dLegacySceneUnloadData(LegacyUIScene* scene);
bool W3dLegacySceneLoadData(LegacyUIScene* placeholder);

class Outliner : public ViewportBase, public WithBorder, public Scrollable {
    public:
        Scrollable* AsScrollable() { return this; }
        size_t CantidadRenglones;
        int lastContentRows;   // Visible rows from the last render; changes trigger scrollbar recalculation.
                               // Previously this only happened on resize, leaving imported content unscrollable.
        Rec2D* Renglon;

        // Vertical culling: DrawRow/DrawSceneToggles traverse the full hierarchy and advance the matrix,
        // but skip drawing icons and text for rows outside the visible area. Without this, scenes with many
        // objects issue draw calls for rows that Scissor later discards. filaDFS is the DFS row index
        // (mapped 1:1 to the matrix Y); cullBaseY is the screen Y of the first row in the current traversal.
        int cullBaseY;
        unsigned filaDFS;

        Outliner();
        ~Outliner() W3D_OVERRIDE;

        void CalculateRowLayout(Object* obj, int* MaxPosXtemp, int* MaxPosYtemp);
        void Resize(int newW, int newH) W3D_OVERRIDE;
        void Render() W3D_OVERRIDE;
        void DrawRow(Object* obj, bool hidden);
        // Virtual "Armature 2D" row for a mesh: one informational blue row beneath the mesh,
        // clarifying that the 2D rig is part of the mesh, not a scene object. It cannot be dragged,
        // accept drops, unparented, or expanded; the bones are listed in the Properties panel's
        // "Armature 2D" tab.
        void DrawArmature2DRow(Mesh* m, int idx, bool hidden);
        void DrawExpandedTreeLines(Object* obj);
        void DrawSceneToggles(Object* obj, bool hidden, bool noRender);

        void button_left() W3D_OVERRIDE;
        void FindMouseOver(int mx, int my);
        void event_mouse_motion(int mx, int my) W3D_OVERRIDE;
        bool event_finger_scroll(int px, int py, int dx, int dy) W3D_OVERRIDE; // Touch drag scrolls vertically or horizontally.
#ifndef W3D_SYMBIAN
        void mouse_button_up(int boton) W3D_OVERRIDE;
        void event_mouse_wheel(float dy, int mx, int my) W3D_OVERRIDE;
        void event_key_down(int tecla, bool repeticion) W3D_OVERRIDE;
        void event_key_up(int tecla) W3D_OVERRIDE;
#endif
        // Shared row selection handler. The N95 HID mouse router calls this;
        // on PC it can be wired to mouse_button_up.
        void SelectRowAt(int mx, int my);

        // Numpad "." (like Blender's Frame Selected): scroll the Outliner to center
        // the selection, using the average of the selected objects' visible rows.
        void CenterSelection();
        // Apply the minimum scroll needed to show the active object's full row. Unlike
        // CenterSelection, this does nothing if the row is already visible; recentering
        // on every arrow key is disorienting in large scenes. Used by arrow-key navigation.
        void EnsureVisible();

        // Row under the mouse, for hover feedback before clicking.
        int hoverFila;
        // Row dragging (reorder or parent by dropping onto another row).
        Object* dragObjeto;
        bool dragging;
        int dragY0;
        // Drop preview: -2 none, -1 empty space (root),
        // 0 before the row, 1 as its child, 2 after it.
        int dropFila;
        int dropZona;
        int dropProf; // Destination depth; the insertion line is indented to this level.
        void FinishRowDrag(int mx, int my);

        // Keyboard move mode (no mouse; useful on N95 to order lights before other objects):
        // G (PC) / 1 (Symbian) enters; arrows reorder (up/down) or reparent (left=out/right=in);
        // OK/Enter confirms; C/Backspace cancels and restores the original tree position.
        bool moviendo;
        Object* moverObj;          // Object being moved (the active object when the mode starts).
        Object* moverPadreOrig;    // Original parent (NULL = root), used when cancelling.
        Object* moverAnteriorOrig; // Previous sibling (NULL = was first), used when cancelling.
        void BeginKeyboardMove();       // Enter move mode with the active object.
        void StepKeyboardMove(int dir); // 0=up, 1=down, 2=out (unparent), 3=in (parent).
        void ConfirmKeyboardMove();     // Exit and keep the new position.
        void CancelKeyboardMove();      // Restore the original position and exit.
        bool IsKeyboardMoveActive() const { return moviendo; }
        int ViewportKind() const { return 2; } // Viewport type menu.
        void ClearHover() { hoverFila = -1; } // Mouse left the viewport.

        void key_down_return();
};

#endif