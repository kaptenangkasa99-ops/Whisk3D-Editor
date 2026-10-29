# History of Decisions

Why the rules in [decisiones.md](decisiones.md) are the way they are. This **does not go into the code**: it is context for when someone wants to change a rule and needs to know what broke before.

---

## Undo: Eight Rounds of the Same Bug

The bug was always the same with different clothing: **an undo step saves a position, the list shifts, and Ctrl+Z applies to the adjacent element**. It appeared eight times across different commands because it was handled one by one through intuition rather than through systematic classification.

**Rounds 1 to 5.** It was first discovered in *rename* steps, and the criteria were written down only for those. However, steps that store positions are not limited to renames: a *reorder* step ("swap i and j") also breaks if an element is deleted in between by a non-undoable delete operation—it does not corrupt names, but it leaves an ordering that never existed. That gave rise to those commands also implementing `RemapLista`, tracking both of their ends and self-terminating (no-op) if one of them was deleted.

**Round 6.** A defense by construction was attempted: making `RemapLista` pure virtual so that the compiler enforces its implementation. That closes one loophole ("I forgot to write it"), but **not** the other: "I misclassified a member." `KeyframesUndo` already implemented `RemapLista` and was already in the dispatch table, yet its `snap` was listed as (b) when it was actually (c): a positional parallel vector against a list that was not even stored, because it was re-resolved from globals upon execution. The compiler cannot catch that. That yielded the mechanical rules R1/R2 in `decisiones.md`.

**Rounds 7 and 8.** The order of key segments turned out not to be cosmetic. With the number at the front (`arm:k<clip>/<rig>`, as left in round 7), the prefix of the remap notification cannot include the rig, and deleting or reordering a clip from **one** armature shifts the clip indices for **all** of them. This is locked down by the `dopeclip3d` test.

**What Round 8 left behind, which is the most useful lesson:** all three failures were *justified as covered* in a comment within the fix itself ("harmless because the resolver checks the rig name", "(a) pointer revalidated against `SceneAnimations`"). In other words, the comment was not merely wrong: **it shut down subsequent code reviews**, because whoever came next read it and moved on. Hence the authoring rule: if a comment declares something harmless, it must cite the test that proves it.

**The other symptom from the same family:** a guard like "if sizes do not match, touch nothing" (which was the case for `BonesUndo` / `Bones2DUndo`). It looks defensive, but it is just the bug operating in silence.

Tests that lock all of this down: `reparentundo`, `dopeclip3d`, `modtargetuaf`, `capaorden`, `capaindices`, `cliporden`, `nombresundo`.