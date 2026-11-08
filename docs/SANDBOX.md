# Extending the sandbox

The falling-sand engine is `core/include/arcade/sandbox.h` + `core/src/sandbox.c`. It is used by the hub background and by `games/sandbox`.

## Adding an element (4 edits)
1. `ElementId` in `sandbox.h`: add a value before `EL_COUNT`.
2. `k_elements[]` in `sandbox.c`: name, RGB colour, density, `MoveKind`.
3. If the behaviour is new, add a `MoveKind` and a branch in `sandbox_step()`.
4. A test in `tests/test_sandbox.c` (e.g. "oil floats on water"). The palette and the number key (`handle_keys` in `games/sandbox/sandbox_logic.c`) pick it up automatically up to key 9.

## Ready-made ideas
| Element | Rule sketch |
|---------|-------------|
| Oil | liquid, density 1 (floats on water), flammable |
| Fire | short-lived: each tick moves up randomly, ignites oil/plant neighbours, dies after N ticks (add a per-cell age array) |
| Lava | slow liquid (moves only every 3rd tick); turns water neighbours into stone |
| Plant | static; grows into adjacent empty cells above it when water is nearby |
| Acid | liquid that deletes one stone/sand neighbour and itself with some probability |
| Gas / steam | rises instead of falls (`MOVE_GAS`: the mirror image of liquid), condenses back to water |
| Emitter | a fixed cell that spawns an element every k ticks (`struct` array in `sbx_State`) |

Keep the engine pure and deterministic (use `sb->rng`), and keep `sandbox_step` fast: it runs for every cell every frame.
