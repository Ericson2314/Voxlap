@rem compile game
nmake game.c

@rem compile simple (must do this after game.c)
nmake simple.c

if exist winmain.obj del winmain.obj

@rem compile voxed
nmake voxed.c

@rem compile kwalk
nmake kwalk.c
