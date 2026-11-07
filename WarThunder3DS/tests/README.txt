Local validation helpers
========================

simulation_runner.c compiles the actual game.c with HOST_TEST to check controls,
collision, battle mode separation, capture, and completed bot matches.
render_runner.c runs the actual graphics.c geometry/UI generation with lightweight
Citro3D substitutes, tests camera pitch stability, and checks memory budgets.
These do not emulate Nintendo hardware or assert a hardware frame rate.

run-simulation.cmd and run-render.cmd use the local MSVC compiler path. This
machine has compiler binaries but no complete Windows SDK/CRT headers, so
hostcrt/ supplies minimal declarations and import libraries for the system CRT.
This is for host testing only; it is not part of the 3DS build or game runtime.
Change the TOOLS path in those .cmd files if testing elsewhere.

Run both scripts from the project directory. After run-render.cmd, run preview.py
with Python and Pillow to generate approximate screen layout previews.
Generated executables, geometry dumps, and images go into tests/output/.
Normal MSYS make only builds source/*.c and does not require these host tools.
