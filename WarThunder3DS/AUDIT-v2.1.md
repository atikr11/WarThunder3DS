# v2.1 audit and bug fixes

The most important defect was in the HUD render pass:
`C3D_DepthTest(false, GPU_ALWAYS, 0)` disables color writes as well as depth
writes. It is now `GPU_WRITE_COLOR`, which allows the menu and HUD to reach
both screen buffers. The desktop test renderer now validates this mask for
every draw call; previously it ignored the setting and missed this defect.
The menu now identifies this version as V2.1 BUGFIX BUILD.

Other corrections:

- Building collision includes a conservative bounding box for the roof.
- Weapons check the path from breech to muzzle, preventing wall bypass.
- Bots stop firing at destroyed targets while awaiting their next decision.
- Contested capture zones no longer drain tickets.
- Stuck tanks reverse briefly rather than pushing forward indefinitely.
- Vehicle heading values wrap each simulation step to avoid long-session drift.
- The kill message correctly says the enemy loses tickets.
- Combat sounds stop on pause/menu transitions; unchanged engine settings
  are no longer sent to DSP every frame.

Performance work:

- Building traces reject objects using cheap segment bounds before exact tests.
- Box meshes compute six rotation coefficients once, rather than per corner.
- Ballistic elevation no longer calls a general power function to square a value.
- Existing visibility culling, detail reduction, fixed memory pools and reduced
  bot decision frequency are retained.

Verification:

- devkitPro C/shader build and .3dsx packaging succeeded.
- Tests cover UI color-write state for menu, tank, aircraft, pause, results,
  death, and a full-projectile/full-effects stress scene.
- Camera and control regression tests pass.
- 2,000 randomized segment/radius tests agree with the full collision scan.
- Thin-wall firing, contested scoring, and full simulated bot matches pass.

These checks do not prove that every console bug is fixed. Actual visuals,
controls, sound and frame rate still need testing on the Old 3DS XL.
The roof collision is an approximate box, and bot navigation is still simple.

QR download update:

Replace WarThunder3DS/WarThunder3DS.3dsx in your public GitHub repository with
this folder's newly built WarThunder3DS.3dsx. The existing raw-file QR code
will then address the updated file. The updated binary is committed with these fixes for QR downloads.
If the game still lacks the menu, confirm that the repository file was replaced
and the downloader overwrote the previous local copy on the console.
