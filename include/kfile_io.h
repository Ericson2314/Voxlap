#pragma once


static char *sxlbuf = 0;
static long sxlparspos, sxlparslen;

extern long loadsxl (const char *, char **, char **, char **);
extern char *parspr (vx5sprite *, char **);
extern void loadnul (dpoint3d *, dpoint3d *, dpoint3d *, dpoint3d *);
extern long loaddta (const char *, dpoint3d *, dpoint3d *, dpoint3d *, dpoint3d *);
extern long loadpng (const char *, dpoint3d *, dpoint3d *, dpoint3d *, dpoint3d *);
extern void loadbsp (const char *, dpoint3d *, dpoint3d *, dpoint3d *, dpoint3d *);
extern long loadvxl (const char *, dpoint3d *, dpoint3d *, dpoint3d *, dpoint3d *);
extern long savevxl (const char *, dpoint3d *, dpoint3d *, dpoint3d *, dpoint3d *);
extern long loadsky (const char *);
