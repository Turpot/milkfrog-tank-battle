# Reference Credits

Gameplay and first three stage layouts:
https://github.com/newagebegins/BattleCity
Reference commit: a1b1741782dcf23fc814e9a7abc5f383900b2cda
MIT license reproduced in BATTLECITY_MIT_LICENSE.txt.

The C implementation, shared reverse BFS and keyboard inspection view are in src.
The three converted text maps and built-in data are supplied under assets/maps and src/maps_builtin.h.
The course enemy schedule is adjusted: 8 per stage, at most 4 alive. Stage 1 has only ordinary enemies; stage 2 has no heavy enemies; stage 3 has no ordinary enemies. Counts by type (ordinary, fast, rapid, heavy): (8,0,0,0), (3,3,2,0), (0,3,2,3).

Frog sprites are adapted from the user's existing project atlas. The player uses the existing yellow frog; enemies use recolored versions. Two tread frames are generated locally. No reference repository artwork is bundled.

The runtime uses C11 and raylib 6.2, statically linked. raylib license is in lib/RAYLIB_LICENSE.txt.
The game uses the Windows SimHei font when installed; no Windows font files are redistributed. The bundled fallback Noto Sans SC subset is distributed under OFL 1.1, reproduced beside the subset font.
