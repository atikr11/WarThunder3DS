# WarThunder3DS — Valley Front (v2)

Original offline arcade combat for Old Nintendo 3DS / 3DS XL.
Not affiliated with War Thunder or Gaijin; no assets from those games.

## Build in devkitPro MSYS

```sh
cd "/c/Users/atikr/OneDrive/Documents/ChatGPT/3DS Games/WarThunder3DS"
make -j2
```

The updated `WarThunder3DS.3dsx` is already built in this folder.
Copy it and `WarThunder3DS.smdh` into `sd:/3ds/WarThunder3DS/`, replacing the
previous version, then launch using your existing Homebrew Launcher.

If your MSYS environment does not already set these variables:

```sh
export DEVKITPRO=/c/devkitPro
export DEVKITARM="$DEVKITPRO/devkitARM"
export PATH="$DEVKITPRO/tools/bin:$DEVKITARM/bin:$PATH"
make -j2
```

Dependencies: devkitARM, libctru, Citro3D, picasso, bin2s, smdhtool, 3dsxtool.
The Makefile supports this project's path containing spaces.
`make clean` removes generated game build outputs; `make -j2` rebuilds them.
Keep all files under `source/` together; main.c now uses separate simulation,
graphics, and audio modules for easier development.

## Changes in v2

- Fixed the tank joystick/aiming bug. libctru's generic directional keys also
  include the Circle Pad. Turret and cannon now use D-pad-only key bits.
- Tank camera has a fixed pitch, independent of cannon elevation and driving.
  Horizontal camera following remains smooth. Hull steering carries the turret,
  with independent D-pad aim. Camera collision shortens the camera boom.
- Vehicle selection chooses the entire battle type. Tank matches contain only
  tanks; aircraft matches contain only aircraft. No mixed-mode spawning or
  mid-match vehicle switching.
- Fully graphical UI on both screens: mission briefing, deployment menu,
  team ticket bars, objective states, health/reload bars, speed, altitude,
  target markers, hit feedback, capture progress, and tactical map.
- Tank map is 1,200 x 1,200 units, compared with 420 x 420 in v1: nearly three
  times the width and over eight times the area. Air map is 2,000 x 2,000.
- 360-degree mountain range, snow-capped peaks, three settlements, roofs,
  windows, doors, roads, crop fields, 220 trees, rock clusters, drifting clouds,
  moving flags, low-health smoke, muzzle flashes, dust, and explosions.
- Better vehicle detail nearby, with simpler models at a distance.
- Static scenery is generated once per map, stored in chunks, and culled by
  distance and view. Actors, shots, effects, and memory budgets remain bounded.
- Bots update tactical decisions at a reduced frequency, lead moving targets,
  avoid obstacles, and choose capture objectives. Exact segment collision
  checks prevent fast projectiles tunneling through buildings and vehicles.
- Tank shells have gravity and reduced damage against frontal armor.
- Optional synthesized engine, firing, impact, and explosion sounds, with
  distance attenuation and stereo panning. Audio is skipped if DSP cannot start;
  an existing `sd:/3ds/dspfirm.cdc` may be required for sound on your setup.
  There are no external sound assets to copy, and lack of audio does not block play.
- Fixed 60 Hz simulation steps, independent of rendering frame rate within the
  supported update budget. Long suspend stalls are discarded.
- Aircraft get a turn-back warning at the map edge instead of immediate death
  at an invisible wall. Ground and building collisions remain lethal.

## Modes

**Armored Front:** 6 vs 6 tanks. Capture A, B, and C by entering the marked zone
without an enemy present. Captured zones drain enemy tickets. Contest a zone to
stop further capture progress; enter it alone to neutralize and take it.
A fully owned zone drains tickets until neutralized.

**Air Superiority:** 5 vs 5 aircraft. Destroy opposing aircraft to drain tickets.
There are no ground vehicles or capture requirements.

Both modes start at 200 tickets per team. Each destroyed vehicle costs its team
8 tickets. Respawn takes six seconds. A match ends when a team runs out of tickets,
or after 12 minutes, with the higher ticket total winning.

## Controls

| Input | Tank | Aircraft |
| --- | --- | --- |
| Circle Pad up/down | Accelerate forward/reverse | Climb/dive |
| Circle Pad left/right | Hull steering | Turn |
| D-pad left/right | Rotate turret | Unused |
| D-pad up/down | Raise/lower cannon | Unused |
| R (hold) | Fire cannon | Fire machine guns |
| L (hold) | Center turret and gun elevation | Unused |
| A (hold) | Drive boost | Increase throttle |
| B (hold) | Unused | Airbrake/decrease throttle |
| Y | Toggle close/wide chase camera | Same |
| START | Pause/resume | Same |
| SELECT | Return to deployment menu | Same |

Menu: left/right selects battle type; A deploys; START exits. Touching the
bottom-screen DEPLOY button also starts a match. On the results screen A
starts a rematch. Change battle type by returning to the menu first.

Blue markers represent allies; red markers represent enemies; white represents
your vehicle. Gold objectives are neutral. Radar is north-up. Enemy radar blips
appear only inside the local detection radius; enemy labels are blocked by
buildings. The tank reticle includes shell drop at a reference distance of 300
units; it is a sight aid rather than a guaranteed impact/penetration indicator.

## Verification and limits

The C sources and PICA200 vertex shader compile with the installed devkitPro
installation and package successfully into a .3dsx. The C compiler reports no
source warnings; the installed toolchain emits a linker warning about its own
crtn.o stack metadata.

Local checks run the actual simulation code and verify:

- Circle Pad forward does not change gun elevation; D-pad elevation still works.
- Every actor matches the selected battle type.
- Building and ground collision, capture, damage/respawns, and completed bot matches.
- Constant tank camera pitch while driving and changing gun elevation.
- Scene/UI vertex limits for the menu, tank, air, pause, results, and destroyed screens.

Desktop UI previews are generated from the real geometry through a test renderer.
They check layout only: they do not emulate PICA200 clipping or verify console
performance. Actual graphics, audio, steering feel, and frame rate need another
test on your Old 3DS XL. No measured console FPS is claimed.

This remains an arcade homebrew game with simple physics and AI. The playable
terrain is mostly flat; scenic mountains are outside the arena. Trees and rock
clusters are decorative; buildings block movement and shots. There is no save
system, progression, realistic penetration model, or online multiplayer.

## Project layout

- `source/main.c` — initialization, input/menu loop, fixed-step simulation timing.
- `source/game.c` / `game.h` — modes, world generation, controls, AI, combat, scoring.
- `source/graphics.c` / `graphics.h` — procedural scenery, camera, graphical HUD.
- `source/audio.c` — optional synthesized sounds and engine loop.
- `source/vshader.v.pica` — position/color vertex shader. Fragment stages use
  the 3DS fixed-function pipeline configured in graphics.c.
- `tests/` — host simulation checks, camera/budget checks, and layout preview helper.
- `backup-v1/` — previous source, build instructions, Makefile, and playable .3dsx.
