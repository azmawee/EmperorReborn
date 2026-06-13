#pragma once

// Optional 4:3 pillarbox for the Bink cutscene videos. In widescreen the movies otherwise stretch to
// fill 16:9 (faces and objects look fat). This is a separate 2D path from the W3D camera widescreen,
// so it needs its own hook.
//
// This first cut is DIAGNOSTIC ONLY: it identifies the movie blit function at [0x5d02e0] and confirms
// the Bink decode path (BinkDoFrame, IAT 0x5d03ac) by logging to emperor.txt while a cutscene plays.
// The actual 4:3 rewrite is wired in once the blit is confirmed from a real run. Calling this never
// changes what is drawn, so it is safe to ship disabled.
void initMovieFix(bool cutscene43);
