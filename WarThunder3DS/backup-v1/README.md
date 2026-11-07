# WarThunder3DS

Original, low-poly, offline arcade combat prototype for the Old Nintendo 3DS XL.
Not affiliated with War Thunder or Gaijin. Contains no assets from those games.

## Build in the devkitPro MSYS terminal

```sh
cd "/c/Users/atikr/OneDrive/Documents/ChatGPT/3DS Games/WarThunder3DS"
make -j2
```

If DEVKITPRO / DEVKITARM are not set on this Windows installation:

```sh
export DEVKITPRO=/c/devkitPro
export DEVKITARM="$DEVKITPRO/devkitARM"
export PATH="$DEVKITPRO/tools/bin:$DEVKITARM/bin:$PATH"
make -j2
```

Requires devkitARM, libctru, Citro3D, picasso, bin2s, smdhtool and 3dsxtool
(all supplied through devkitPro). The Makefile uses relative project paths so
this project can be built from its current folder, which contains spaces.
Run `make clean` to remove generated files, then `make -j2` to rebuild.

## Install

Copy WarThunder3DS.3dsx and WarThunder3DS.smdh into
`sd:/3ds/WarThunder3DS/`, then launch through your existing Homebrew Launcher.
No data folders, downloads, DSP firmware, or network connection are needed.

## Included

- Third-person controllable tank or aircraft; 13 other vehicles controlled by bots.
- Two teams with tanks and aircraft sharing one battlefield.
- Tank turret elevation and rotation, shell gravity, rapid aircraft guns.
- Swept projectile collision, damage, explosions, six-second respawns.
- Capture point, 100 team tickets, four-ticket vehicle losses, ten-minute matches.
- Central flag drains enemy tickets after being captured by uncontested tanks.
- Bottom-screen status, radar, menu, pause, rematch, and frame-rate display.
- Procedural colored meshes, bounded memory pools, no textures or asset loading.
- Standard Old 3DS inputs only; no C-stick, ZL/ZR, stereoscopy, or New 3DS speedup.

## Controls

| Input | Tank | Aircraft |
| --- | --- | --- |
| Circle Pad | Forward/reverse and hull steering | Turn and pitch (up climbs) |
| D-pad left/right | Rotate turret | Unused |
| D-pad up/down | Raise/lower cannon | Unused |
| R (hold) | Fire cannon | Fire guns |
| L (hold) | Center turret and elevation | Unused |
| A (hold) | Faster movement | Increase throttle |
| B (hold) | Unused | Decrease throttle |
| X | Switch vehicle and redeploy, costs four tickets | Same |
| START | Pause/resume | Same |
| SELECT | Return to menu | Same |

In the menu use left/right to select a vehicle, A to start, START to quit.
After a match press A for a rematch. Radar: P = player, A = ally, E = enemy,
F = central flag. Blue vehicles are allies; red vehicles are enemies.
Aircraft must stay above ground and below 160 units altitude, inside the
420 by 420 unit battlefield. Colliding with buildings or boundaries destroys
the aircraft. Tanks cannot drive through buildings or arena boundaries.

## Scope and verification

This is a first playable prototype, with one tank type and one aircraft type,
simple flight and damage models, basic bot steering, and a flat battlefield.
There is currently no sound, save system, armor penetration, or progression.
Buildings can obscure the chase camera. Bots can get stuck on scenery.
It is not a realistic vehicle simulator.

The C code and PICA shader were compiled with the installed devkitPro tools
and packaged into a .3dsx. Actual visuals, controls, bot behavior, and frame
rate still need testing on an Old 3DS XL; a successful build does not verify
hardware behavior. Intended performance is conservative, but not measured.

## Files

- source/main.c: simulation, bot behavior, procedural geometry, UI, rendering.
- source/vshader.v.pica: PICA200 vertex position/color shader.
- Makefile: compiles the shader and C and produces the .3dsx and .smdh.

The 3DS uses a fixed-function fragment pipeline, configured in main.c,
so this project needs only a vertex shader source.
