#pragma once

extern kv6data *getkv6 (const char *);
extern kfatype *getkfa (const char *);
extern void freekv6 (kv6data *kv6);
extern void savekv6 (const char *, kv6data *);
extern void getspr (vx5sprite *, const char *);
extern kv6data *genmipkv6 (kv6data *);
extern char *getkfilname (long);
extern void animsprite (vx5sprite *, long);
extern void drawsprite (vx5sprite *);
extern long meltsphere (vx5sprite *, lpoint3d *, long);
extern long meltspans (vx5sprite *, vspans *, long, lpoint3d *);