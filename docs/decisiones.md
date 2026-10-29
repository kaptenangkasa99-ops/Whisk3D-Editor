# Whisk3D Design Decisions

The rules that must be respected when modifying the editor. Short and to the point: if something requires a lengthy explanation, it goes in [decisiones-historia.md](decisiones-historia.md), not here and **never** as a giant comment in the code.

Criteria for code comments: they state **what it does** and, if not obvious, **why**. Two or three lines. The story of how that decision was reached goes in the history document.

---

## Undo: Steps Storing an Index

An undo step may store the position of an item within a list (layer 3 of a mesh, clip 2 of an armature). If that list **shifts** between when it is saved and when it is applied—because an element was deleted or reordered—Ctrl+Z hits the wrong element. There is no crash or warning: it corrupts silently.

**The rule, for any new list:** if a list has rename targets by index (any `W3dRenameDest` other than `Directo`), any operation that shifts its indices—deleting, reordering, inserting in the middle—must do one of two things:

1. **snapshot the entire list** in the same undo step, or
2. **notify** via `UndoListaBorrada` / `UndoListaMovida` / `UndoMoverCapaMalla` / `UndoMoverClipArm`.

And if the undo step stores indices of that list, it must implement `RemapLista`.
When adding the list, also include it in `W3dNombresJuntarEspacios` (`ObjectMode.cpp`).

**How to classify each member of an undo command:**

| Type | What it is | Safe? |
|---|---|---|
| (a) | Pointer revalidated upon execution | Yes (if dead, no-op) |
| (b) | Full snapshot of the list | Yes, **only if the list is saved** |
| (c) | Position, or parallel positional vector | **Dangerous**: specify which list it indexes against |
| (d) | Own value | Yes |

Three mechanical rules to prevent misclassification:

- **R1.** A snapshot is not (b) if the list is not saved. If applying it requires fetching the list again (via a global, "the active item", or `FindTarget`), it is (c) against "whatever list is selected at that time," which is the worst case: it shifts without anything being deleted or reordered.
- **R2.** Identity consists of **two parts**: the element and the list. The second is the one that is always forgotten.
- **R3.** In keys with an owner, the owner goes **before** the number (`arm:<rig>/k<clip>`): the remap notification truncates the key right before the number, so with the number at the front, deleting a clip from an armature shifts the indices for all of them.

**A guard like "if sizes do not match, touch nothing" is not a solution**: it is the silent version of the bug (Ctrl+Z restores nothing and gives no warning).

**Authoring Rule:** A comment declaring something harmless must be accompanied by the test that proves it, cited by name. Without that, it is not documentation; it is an unrun hypothesis.