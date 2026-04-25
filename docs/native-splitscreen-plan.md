# Native Split Screen Plan

This fork starts from the `EDuke32-CSRefactor` upstream on top of `netduke32_main`, using branch `feature/native-splitscreen`.

## Current baseline

The upstream tree already contains old split screen remnants:

- `source/duke3d/src/cmdline.cpp`: fake local multiplayer via `-qN`
- `source/duke3d/src/game.cpp`: render path hooks guarded by `SPLITSCREEN_MOD_HACKS`
- `source/duke3d/src/screens.cpp`: HUD/status bar hacks for a second local player
- `source/duke3d/src/player.cpp`: local input plumbing for the fake multiplayer path
- `package/sdk/samples/splitscr.con`: historical 2-player split screen sample

This repository now exposes those hooks in the GNU build with:

```sh
make LEGACY_SPLITSCREEN_HACKS=1
```

That does not guarantee the old sample still works end to end. It does give us a concrete baseline to compile and inspect instead of rebuilding the feature from nothing.

## Immediate goal

Create a native split screen implementation in one process and one window:

- milestone 1: 2 local players
- milestone 2: 4 local players in a 2x2 layout
- milestone 3: menu, HUD, pause, respawn and audio cleanup

## Technical strategy

### 1. Stabilize the legacy local-multiplayer path

Use the existing fake multiplayer path as the bootstrap layer:

- `g_fakeMultiMode`
- `ud.multimode`
- per-player `g_player[]`
- existing `P_ProcessInput()` and movement simulation

Short-term objective: make local split screen ride on those existing player systems instead of trying to invent a separate local-only player stack.

### 2. Replace ad-hoc 2P hacks with a viewport system

Current code assumes a single active view driven by:

- `screenpeek`
- `myconnectindex`
- global renderer state such as aspect, palette, crosshair and HUD placement

We need a small split screen layer that owns:

- active local player count
- viewport rectangles
- per-viewport render context
- per-viewport HUD and crosshair placement

Initial split screen module:

- `source/duke3d/src/splitscreen.h`
- `source/duke3d/src/splitscreen.cpp`

Initial responsibilities:

- return the active local player indices
- return viewport rectangles for 1P, 2P and 4P
- wrap temporary swaps of `screenpeek`
- centralize split-screen state instead of scattering `g_fakeMultiMode==2`

### 3. Lift input from one local player to N local players

Current frame code only pulls one local input:

- `drawframe_do()` in `source/duke3d/src/game.cpp`
- `P_GetInput(myconnectindex)` in `source/duke3d/src/player.cpp`

Plan:

- keep player 1 on the current input path first
- add a second input path for gamepad-only local players
- map one device per local player
- write directly into each local player's `input_t`

Do not solve multi-keyboard or multi-mouse in v1.

### 4. Render each local player sequentially per frame

Current draw path:

- `G_DrawRooms(screenpeek, smoothratio)`
- `G_DisplayRest(smoothratio)`

Plan:

- split the frame into viewport iterations
- set viewport rectangle
- set the active local player for that viewport
- call 3D render plus HUD pass for that local player
- restore global render state at the end of each viewport

This will touch at minimum:

- `source/duke3d/src/game.cpp`
- `source/duke3d/src/screens.cpp`
- `source/duke3d/src/sbar.cpp`
- `source/duke3d/src/player.cpp`
- `source/duke3d/src/sounds.cpp`

### 5. Generalize from 2P to 4P

Do not jump straight to 4P. The safe order is:

1. compile the legacy hooks
2. boot a 2P local baseline
3. replace hard-coded player-2 logic with indexed local-player logic
4. add 3P and 4P viewport layouts
5. scale HUD and crosshair rules per viewport

## First implementation tasks

1. Verify a local build with `LEGACY_SPLITSCREEN_HACKS=1`.
2. Confirm whether `package/sdk/samples/splitscr.con` still boots a 2-player baseline.
3. Add a dedicated split screen state module and move `g_fakeMultiMode` access behind helpers.
4. Change `drawframe_do()` to iterate local viewports instead of rendering only `screenpeek`.
5. Add per-player local input collection for pads 2-4.

## User requirements outside the repo

You still need the original Duke Nukem 3D game data, typically `DUKE3D.GRP`, to run the game after compilation.
