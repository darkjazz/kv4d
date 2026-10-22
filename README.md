# kv4d

Experimental 4D cellular automata visualiser, a spin-off of kv. Built with
[Cinder](https://libcinder.org/) 0.9.2 and OpenGL 3.2 / GLSL 150.

The automaton runs on a 4D torus (up to 64⁴), stepped entirely on the GPU, with an 80-cell Moore
or 8-cell von Neumann neighbourhood. It is drawn through a 4D rotation and one of four views. It is
controlled from the keyboard, over OSC, or from SuperCollider via `sc/Kv4d.sc`.

## Build

```sh
cmake -S . -B build            # -DCINDER_PATH=... if Cinder isn't at ~/libdev/cinder_0.9.2
cmake --build build -j8
open build/kv4d.app            # or: build/kv4d.app/Contents/MacOS/kv4d -size 24
```

`cmake -G Xcode -S . -B xcode` generates an Xcode project instead. The prebuilt `libcinder.a` is
x86_64 only, so the build targets x86_64. The app is compiled as C++14 because Cinder 0.9.2's boost
headers break under C++17. The Cinder-free core (`src/sim/Rule4D`, `src/sim/Grid4D`) uses C++17.

## Run

```
kv4d [-size N] [-screenx W] [-screeny H] [-fps F] [-full 0|1] [-wmode 0|1]
     [-inport 7000] [-outport 57121] [-selftest] [-capture DIR]
```

| Flag | Meaning | Default |
|---|---|---|
| `-size` | grid edge, N⁴ cells | 20 |
| `-wmode 1` | open on the second display | 0 |
| `-inport` / `-outport` | OSC receive / reply ports | 7000 / 57121 |
| `-selftest` | step the GPU sim against the CPU reference, print PASS/FAIL, exit 1 on failure | |
| `-capture DIR` | save a PNG of each view mode to DIR, then quit | |

kv also listens on 7000. To run both at once, start kv4d with `-inport 7100`.

### Keys

| Key | Action |
|---|---|
| `1` `2` `3` `4` | project / slice / stack / grid (1 s crossfade) |
| `space` `n` | pause, single step |
| `r` `s` `t` | reseed (same seed type), symmetric seed, wire-tesseract seed |
| `[` `]` | previous / next rule preset |
| `,` `.` | move the slice along w |
| `-` `=` | slower / faster generations |
| `w` | toggle rotation through the w planes |
| `g` `b` | cubes ↔ points, additive ↔ solid |
| `e` `l` | trails, glow |
| `a` | audio input on/off |
| `h` `f` | HUD, fullscreen |
| mouse | orbit / zoom camera |

## How it works

**Simulation** (`src/sim`). The 4D grid is packed into a 2D atlas texture, `u = x + X·z`, `v = y + Y·w`,
so 32⁴ fits in 1024². `sim_step.frag` computes one generation per fullscreen pass, swapping between
two RG32F framebuffers. `r` holds the state and `g` the previous state, which the renderer uses to
interpolate between generations. `Grid4D` is a plain C++ implementation of the same rules in the same
layout. It is used for seeding and as the reference that `-selftest` checks the GPU against
(Generations must match exactly; continuous within 1e-4).

**Rules** (`Rule4D`):
- **Generations:** birth/survival counts plus a number of states. Life is the 2-state case.
  Specs look like `B12-17/S11-20/C2/NM`: B = birth counts, S = survival counts, C = states,
  N = neighbourhood (M = Moore, V = von Neumann).
- **Continuous:** `fract(mean(neighbours) + add)`, as in kv.

The presets come from `tools/rulesearch`, a multithreaded random search on the CPU reference. It
keeps rules that are still changing after 60 generations at 16⁴ without dying out or filling the grid:

```sh
./build/rulesearch 600 16 60     # candidates, size, steps
```

| preset | rule | seed density |
|---|---|---|
| coral | B12-17/S11-20/C2/NM | 0.16 |
| sparse | B6-11/S13-25/C2/NM | 0.32 |
| soup | B21-27/S12-29/C2/NM | 0.35 |
| brain | B10-12/S11-12/C3/NM | 0.26 |
| decay | B6-10/S4-14/C4/NM | 0.19 |
| fronts | B8-10/S38-60/C4/NM | 0.33 |
| lattice | B9-14/S38-39/C3/NM | 0.14 |
| pulse | B6-7/S31-37/C3/NM | 0.51 |

**Rendering** (`src/view`). One instanced draw with N⁴ instances. `cells.vert` derives each cell's
coordinates from `gl_InstanceID` and reads its state straight from the atlas, so nothing is read back
to the CPU. It rotates the cell in 4D (6 planes: xy xz xw yz yw zw), then places it for two view modes
at once and blends between them, which is how mode crossfades work:

| mode | what you see |
|---|---|
| project | 4D perspective: `xyz · d / (d − w)`. Cells nearer in w are larger. |
| slice | the 3D hyperplane at `w = slice`. Rotating through xw/yw/zw gives oblique slices. |
| stack | all w layers overlaid, coloured by w |
| grid | one 3D cube per w layer, laid out as small multiples |

Colour runs from `colorA` to `colorB` along w. Project and stack switch to additive blending, with
alpha scaled by 1.5/W so overlapping layers don't saturate. Slice and grid use solid lit cubes. A
projected tesseract wireframe gives orientation. Post-processing is feedback trails followed by glow
(the glow shader is adapted from kv).

**Audio** (`src/io/Audio`). Low/mid/high bands, using kv's analysis. Mappings:
- `rotate`: low band speeds up rotation in the w planes
- `slice`: mid band moves the slice
- `pulse`: amplitude scales cell size
- `inject`: low band above a threshold drops a random hyperball of live cells

## OSC (`/kv4d/...`, port 7000)

Number arguments may be int or float.

| address | args |
|---|---|
| `/world/init` | x [y z w] |
| `/world/rule` | spec string |
| `/world/preset` | index or name |
| `/world/continuous` | add |
| `/world/births`, `/world/survivals` | lo hi [lo hi ...] |
| `/world/states` | n |
| `/world/nhood` | 0 Moore, 1 von Neumann |
| `/world/add` | add |
| `/world/seed` | type (0 box, 1 symmetric, 2 tesseract, 3 clear), extent, density |
| `/world/reseed` | |
| `/world/inject` | x y z w (0..1), radius (fraction of grid), density |
| `/world/rate` | generations/s |
| `/world/pause` | 0/1 |
| `/world/step` | |
| `/world/interpolate` | 0/1 |
| `/world/query/alive` | 0/1. Replies `/kv4d/world/alive count generation` to 57121 after each generation. |
| `/view/mode` | 0-3, fade seconds |
| `/view/rotate` | plane (name or 0-5), speed rad/s |
| `/view/angle` | plane, radians |
| `/view/reset` | |
| `/view/project` | eye distance in w |
| `/view/slice` | w (-0.5..0.5) [width] [sweep Hz] |
| `/view/grid` | spacing |
| `/view/extent` | world size |
| `/view/geometry` | 0 cubes, 1 points |
| `/view/cellscale`, `/view/pointsize`, `/view/alpha`, `/view/threshold` | value |
| `/view/color` | r g b [r g b] |
| `/view/blend` | 1 additive, 0 solid |
| `/view/tesseract` | 0/1 |
| `/view/background` | r g b |
| `/efx/trails` | decay (0 = off) |
| `/efx/glow` | threshold intensity radius |
| `/audio/enable` | 0/1 [device name] |
| `/audio/gain` | gain |
| `/audio/map` | rotate\|slice\|pulse\|inject amount |
| `/framerate` | fps |
| `/quit` | |

`tools/osc_smoke.py [port] [address args...]` sends single messages, or with no address runs a
round-trip test. `sc/Kv4d.sc` wraps all of the above for SuperCollider.

## Layout

```
src/
  kv4dApp.cpp        window, keys, HUD, -selftest, -capture
  Session.*          shared control surface (sim timing, seeding, audio mappings)
  sim/Rule4D.*       rule model + spec parser + presets          (no Cinder)
  sim/Grid4D.*       CPU reference + seeding                       (no Cinder)
  sim/World4D.*      GPU simulation
  view/Renderer4D.*  4D rotation, view modes, tesseract wireframe
  view/Efx.*         trails + glow
  io/Audio.*, io/Osc4D.*
assets/shaders/      sim_step, sim_inject, cells, trails, glow
sc/Kv4d.sc           SuperCollider client
tools/               rulesearch.cpp, osc_smoke.py
```

## Ideas not done yet

- 4D Lenia (continuous kernels), larger neighbourhood radii
- mapping onto S³ (Hopf fibration), following kv's spherical patterns
- sonifying a slice: send the states along a line or plane back over OSC
- a reduction pass for the alive count instead of a full readback
