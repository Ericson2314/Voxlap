#if 0
game.exe: game.obj voxlap5.obj v5.obj kplib.obj winmain.obj game.c; link game voxlap5 v5 kplib winmain ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib /opt:nowin98
game.obj: game.c voxlap5.h sysmain.h; cl /c /J /TP game.c      /Ox /Ob2 /G6Fy /Gs /MD /QIfist
voxlap5.obj: voxlap5.c voxlap5.h;     cl /c /J /TP voxlap5.c   /Ox /Ob2 /G6Fy /Gs /MD
v5.obj: v5.asm; ml /c /coff v5.asm
kplib.obj: kplib.c;                   cl /c /J /TP kplib.c     /Ox /Ob2 /G6Fy /Gs /MD
winmain.obj: winmain.cpp sysmain.h;   cl /c /J /TP winmain.cpp /Ox /Ob2 /G6Fy /Gs /MD /DUSEKZ /DZOOM_TEST
!if 0
#endif

//VOXLAP engine by Ken Silverman (http://advsys.net/ken)

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "sysmain.h"
#include "voxlap5.h"

	//NUMSECRETS:actual num,1:1,2:1,4:2,8:5,16:8,32:14,64:26,128:49,256:87
	//512:168,1024:293,2048:480,4096:711,8192:931
#define NUMSECRETS 8

#define HITWALL "wav/quikdrop.wav"
#define BLOWUP "wav/blowup.wav"
#define DEBRIS "wav/debris.wav"
#define DEBRIS2 "wav/plop.wav"
#define BOUNCE "wav/bounce.wav"
#define DIVEBORD "wav/divebord.wav"
#define PICKUP "wav/pickup.wav"
#define DOOROPEN "wav/dooropen.wav"
#define SHOOTBUL "wav/shootgun.wav"
#define MACHINEGUN "wav/shoot.wav"
#define PLAYERHURT "wav/hit.wav"
#define PLAYERDIE "wav/no!.wav"
#define MONSHOOT "wav/monshoot.wav"
#define MONHURT "wav/gothit.wav"
#define MONHURT2 "wav/hurt.wav"
#define MONDIE "wav/mondie.wav"
#define TALK "wav/pop2.wav"
#define WOODRUB "wav/woodrub.wav"
static long volpercent = 100;

	//KV6 objects that are used in game (don't need to include decorations):
enum {
CACO,WORM,KGRENADE,SNAKE,JOYSTICK,TNTBUNDL,DOOR,BARREL,DESKLAMP,EXPLODE,\
NUM0,NUM1,NUM2,NUM3,NUM4,NUM5,NUM6,NUM7,NUM8,NUM9,WOODBORD,\
NUMKV6 };
const char kv6list[] = {"\
CACO,WORM,KGRENADE,SNAKE,JOYSTICK,TNTBUNDL,DOOR,BARREL,DESKLAMP,EXPLODE,\
NUM0,NUM1,NUM2,NUM3,NUM4,NUM5,NUM6,NUM7,NUM8,NUM9,WOODBORD,\
"};

kv6data *kv6[NUMKV6]; //list of pointers to each enum'd kv6 object

char cursxlnam[MAX_PATH+1];
long capturecount = 0, disablemonsts = 1;

char *sxlbuf = 0;
long sxleng = 0;

	//Player position variables:
#define CLIPRAD 5
dpoint3d ipos, istr, ihei, ifor, ivel;

	//Debris variables:
#define MAXDBRI 2048
typedef struct { point3d p, v; long tim, col; } dbritype;
dbritype dbri[MAXDBRI];
long dbrihead = 0, dbritail = 0;

	//Tile variables: (info telling where status bars & menus are stored)
typedef struct { long f, p, x, y; } tiletype;
#define FONTXDIM 9
#define FONTYDIM 12
tiletype numb[11], asci[128+18]; //Note: 0-31 of asci unused (512 wasted bytes)
tiletype target;
long showtarget = 1;

	//User message:
char message[256] = {0}, typemessage[256] = {0};
long messagetimeout = 0, typemode = 0, quitmessagetimeout = 0x80000000;

#define MAXSPRITES 1024 //NOTE: this shouldn't be static!
#ifdef __cplusplus
struct spritetype : vx5sprite //Note: C++!
{
	point3d v, r;  //other attributes (not used by voxlap engine)
	long owner, tim, tag;
};
#else
typedef struct
{
	point3d p; long flags;
	static union { point3d s, x; }; static union { kv6data *voxnum; kfatype *kfaptr; };
	static union { point3d h, y; }; long kfatim;
	static union { point3d f, z; }; long okfatim;
//----------------------------------------------------
	point3d v, r;  //other attributes (not used by voxlap engine)
	long owner, tim, tag;
} spritetype;
#endif

spritetype spr[MAXSPRITES], tempspr, woodspr, ospr2goal, spr2goal;
long numsprites, spr2goaltim = 0;
//long sortorder[MAXSPRITES];

long curvystp = (~1); //<0=don't draw, 0=draw normal, 1=draw curvy&use this step size
kv6data *curvykv6 = 0;
vx5sprite curvyspr;
long lockanginc = 0;

	//Mouse button state global variables:
long obstatus = 0, bstatus = 0;

	//Timer global variables:
double odtotclk, dtotclk;
float fsynctics;
long totclk;

	//FPS counter
#define FPSSIZ 64
long fpsometer[FPSSIZ], fpsind[FPSSIZ], numframes = 0, showfps = 0;

	//LUT: falling code delay (in ms) between each drop by 1
	//unitfalldelay[i] = (long)(sqrt((i+1)*2/gravity)*1000)
	//                  -(long)(sqrt( i   *2/gravity)*1000);
	//  where: gravity = 128 units/sec^2
char unitfalldelay[255] =
{
  125,51,40,34,29,27,24,23,22,20,19,19,17,17,17,16,
	15,15,14,15,13,14,13,13,13,12,12,12,12,11,11,12,
	11,10,11,11,10,10,10,10,10,10,9,10,9,9,9,10,9,8,9,9,9,8,9,8,8,8,9,8,8,8,8,8,
	7,8,8,7,8,7,8,7,8,7,7,7,7,7,8,7,7,6,7,7,7,7,6,7,7,6,7,6,7,6,7,6,
	7,6,6,7,6,6,6,6,6,6,7,6,6,6,5,6,6,6,6,6,6,5,6,6,6,5,6,5,6,6,5,6,
	5,6,5,6,5,5,6,5,6,5,5,6,5,5,5,6,5,5,5,5,5,5,6,5,5,5,5,5,5,5,5,5,
	5,4,5,5,5,5,5,5,5,4,5,5,5,4,5,5,5,4,5,5,4,5,4,5,5,4,5,4,5,5,4,5,
	4,5,4,5,4,4,5,4,5,4,4,5,4,5,4,4,5,4,4,5,4,4,4,5,4,4,4,5,4,4,4,4,
	5,4,4,4,4,4,4,4,5,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
};
point3d lastbulpos, lastbulvel; //hack to remember last bullet exploded

long rubble[] =
{
0x5111111,0x5121111,0x5131111,0x6111111,0x6121111,0x6131111,0x7111111,0x7121011,0x8111010,0x8121010,0x9111010,0x9121010,0xc101212,0xc111113,0xc121112,0xd101314,0xd111215,0xd121214,0xd131213,0xe101517,0xe111317,0xe121317,0xe131315,0xf0f1719,0xf10151a,0xf11141a,0xf121319,0xf131317,0xf141414,0x100e181c,0x100f171c,0x1010151c,0x1011131c,0x1012131b,0x10131319,0x10141315,0x10151314,0x110c1b1c,0x110d191c,0x110e181c,0x110f171c,0x1110151c,0x1111121c,0x11120a0a,0x1112121c,0x1113090b,0x1113111c,0x1114080b,0x11141216,0x1115080a,0x11151214,0x11160809,0x11161213,0x11171313,0x120b1b1c,0x120c1a1c,0x120d191c,0x120e181c,0x120f080c,0x120f161c,0x1210070e,0x1210151c,0x1211070f,0x1211141c,0x12120610,0x1212131c,0x1213060e,0x1213141c,0x1214060d,0x12141415,0x1214191b,0x1215060c,0x1216070b,0x1217080a,0x12171212,0x12181112,0x12191212,0x130b1b1c,0x130c1a1c,0x130d0a0b,0x130d181c,0x130e090c,0x130e171c,0x130f070d,0x130f151c,0x1310070e,0x1310141c,0x1311060e,0x1311141c,0x1312060e,0x1312141d,0x1313050e,0x1313161d,0x1314050d,0x1314191d,0x1315060c,0x1316060b,0x1317070b,0x13180809,0x140a1b1b,0x140b1a1c,0x140c191c,0x140d0a0b,0x140d171c,0x140e080c,0x140e161c,0x140f070d,0x140f141c,0x1410060e,0x1410131d,0x1411060e,0x1411141d,0x1412050e,0x1412151d,0x1413050d,0x1413161d,0x1414050d,0x1414181d,0x1415050c,0x1416060b,0x1417060b,0x1418080a,0x150a1a1b,0x150b191c,0x150c171c,0x150d090b,0x150d151c,0x150e080c,0x150e141c,0x150f070d,0x150f131c,0x1510060d,0x1510121d,0x1511050e,0x1511131d,0x1512050d,0x1512141d,0x1513050d,0x1513161d,0x1514050c,0x1514181d,0x1515050c,0x15151c1d,0x1516060b,0x1517060b,0x1518070a,0x15190909,0x16091a1b,0x160a191b,0x160b171c,0x160c151c,0x160d090a,0x160d141c,0x160e080c,0x160e131c,0x160f070d,0x160f121d,0x1610060d,0x1610111d,0x1611050e,0x1611121d,0x1612050d,0x1612131d,0x1613050d,0x1613151d,0x1614050c,0x1614171d,0x1615050c,0x16151b1d,0x1616060b,0x1617060b,0x1618070a,0x1709191b,0x170a171b,0x170b161c,0x170c141c,0x170d090a,0x170d131c,0x170e080c,0x170e111c,0x170f070d,0x170f101d,0x1710061d,0x1711060e,0x1711101d,0x1712050d,0x1712121d,0x1713050d,0x1713141d,0x1714050c,0x1714171d,0x1715050b,0x17151a1d,0x1716060b,0x1717060a,0x1718080a,0x1808191a,0x1809171b,0x180a161b,0x180b141c,0x180c131c,0x180d0a0a,0x180d111c,0x180e090c,0x180e101c,0x180f081c,0x1810071d,0x1811061d,0x1812060e,0x1812111d,0x1813050d,0x1813131d,0x1814060c,0x1814151d,0x1815060b,0x18151a1d,0x1816060b,0x18161d1d,0x1817070a,0x1818080a,0x19071919,0x1908181a,0x1909161b,0x190a141b,0x190b131b,0x190c111c,0x190d0a0a,0x190d101c,0x190e0a1c,0x190f081c,0x1910071c,0x1911071d,0x1912061d,0x1913060d,0x1913111d,0x1914060c,0x1914131d,0x1915060b,0x1915191d,0x1916070b,0x19161c1d,0x1917080a,0x1a071819,0x1a08161a,0x1a09151a,0x1a0a131b,0x1a0b111b,0x1a0c101b,0x1a0d0b0b,0x1a0d0e1c,0x1a0e0b1c,0x1a0f0a1c,0x1a10091c,0x1a11081c,0x1a12071c,0x1a13071d,0x1a14070d,0x1a14121d,0x1a15080c,0x1a15171c,0x1a16080b,0x1a161b1c,0x1b071718,0x1b081519,0x1b09131a,0x1b0a121a,0x1b0b101b,0x1b0c0e1b,0x1b0d0c1b,0x1b0e0c1b,0x1b0f0c1b,0x1b100b1b,0x1b110a1c,0x1b12091c,0x1b13091c,0x1b14091c,0x1b150a0c,0x1b15111c,0x1b16191c,0x1c071518,0x1c081419,0x1c091219,0x1c0a101a,0x1c0b0f1a,0x1c0c0d1a,0x1c0d0d1a,0x1c0e0d1a,0x1c0f0e1b,0x1c100e1b,0x1c110e1b,0x1c120e1b,0x1c130f1b,0x1c14101b,0x1c15101b,0x1c16111b,0x1c171a1a,0x1d071416,0x1d081217,0x1d091118,0x1d0a0f19,0x1d0b0e19,0x1d0c0e19,0x1d0d0f19,0x1d0e0f19,0x1d0f0f19,0x1d10101a,0x1d11101a,0x1d12111a,0x1d13111a,0x1d14121a,0x1d15131a,0x1d161319,0x1d171518,0x1e081115,0x1e091016,0x1e0a1017,0x1e0b1018,0x1e0c1018,0x1e0d1118,0x1e0e1118,0x1e0f1218,0x1e101218,0x1e111318,0x1e121418,0x1e131418,0x1e141517,
0x50f1414,0x5101216,0x5111216,0x5121216,0x5131216,0x5141215,0x5151314,0x60e1216,0x60f1217,0x6101117,0x6111217,0x6121217,0x6131217,0x6141217,0x6151216,0x6161215,0x70a1618,0x70b1518,0x70c1319,0x70d1319,0x70e1218,0x70f1118,0x7101118,0x7111218,0x7121218,0x7131118,0x7141118,0x7151117,0x7161216,0x7171215,0x8081619,0x809141a,0x80a141a,0x80b141a,0x80c131a,0x80d131a,0x80e121a,0x80f1119,0x8101119,0x8111119,0x8121119,0x8131119,0x8141118,0x8151118,0x8161117,0x8171216,0x9071519,0x908151a,0x909141b,0x90a141b,0x90b141b,0x90c131b,0x90d131b,0x90e121b,0x90f111a,0x910111a,0x911111a,0x9121119,0x9131019,0x9141119,0x9151118,0x9161118,0x9171217,0x9181315,0xa061618,0xa07151a,0xa08151b,0xa09141c,0xa0a141c,0xa0b141c,0xa0c141c,0xa0d131c,0xa0e121b,0xa0f111b,0xa10111b,0xa11111a,0xa12101a,0xa13101a,0xa141119,0xa151119,0xa161118,0xa171217,0xa181315,0xb061519,0xb07151b,0xb08151c,0xb09141c,0xb0a141c,0xb0b151c,0xb0c161c,0xb0d151c,0xb0e131c,0xb0f121b,0xb10111b,0xb11111b,0xb12111a,0xb13111a,0xb141119,0xb151119,0xb161218,0xb171217,0xb181316,0xc051717,0xc06151a,0xc07151b,0xc08151c,0xc09141c,0xc0a141d,0xc0b171d,0xc0c171d,0xc0d171c,0xc0e161c,0xc0f151c,0xc10131b,0xc11141b,0xc12131a,0xc13111a,0xc14111a,0xc151219,0xc161219,0xc171318,0xc181416,0xd051618,0xd06161a,0xd07151b,0xd08151c,0xd09151c,0xd0a171d,0xd0b181d,0xd0c181d,0xd0d181d,0xd0e171c,0xd0f161c,0xd10151b,0xd11161b,0xd12151b,0xd13141a,0xd14131a,0xd151319,0xd161319,0xd171418,0xd181417,0xd191415,0xe051618,0xe06161a,0xe07151b,0xe08151c,0xe09151c,0xe0a191d,0xe0b191d,0xe0c191d,0xe0d181d,0xe0e181c,0xe0f171c,0xe10181c,0xe11181b,0xe12181b,0xe13161b,0xe14141a,0xe15141a,0xe161419,0xe171419,0xe181418,0xe191416,0xf051617,0xf06161a,0xf07161b,0xf08151c,0xf091a1c,0xf0a1a1c,0xf0b1a1d,0xf0c191d,0xf0d191c,0xf0e181c,0xf0f1a1c,0xf101b1c,0xf111b1b,0xf121a1b,0xf13181b,0xf14151b,0xf15141a,0xf16141a,0xf171419,0xf181419,0xf191418,0xf1a1416,0x10061619,0x10071a1a,0x10081b1b,0x10091b1c,0x100a1b1c,0x100b1b1c,0x100c1a1c,0x100d1a1c,0x10131a1b,0x1014161b,0x1015151b,0x1016141b,0x1017141a,0x1018141a,0x10191419,0x101a1418,0x101b1415,0x110a1c1c,0x110b1b1c,0x1114171c,0x1115151c,0x1116141b,0x1117141b,0x1118131a,0x1119131a,0x111a1319,0x111b1317,0x12141618,0x12141c1c,0x1215141c,0x1216131c,0x1217131b,0x1218131b,0x1219131a,0x121a1319,0x121b1318,0x121c1316,0x13141618,0x1315151c,0x1316141c,0x1317131c,0x1318131b,0x1319131b,0x131a121a,0x131b1219,0x131c1217,0x14141717,0x1415151d,0x1416141d,0x1417141c,0x1418131c,0x1419131b,0x141a131a,0x141b1219,0x141c1217,0x15141717,0x1515161b,0x1516151d,0x1517141d,0x1518141c,0x1519131b,0x151a131b,0x151b1219,0x151c1218,0x16141616,0x1615151a,0x1616151d,0x1617141d,0x1618141c,0x1619131c,0x161a131b,0x161b1319,0x161c1217,0x17141516,0x17151519,0x1716141d,0x1717141d,0x1718141c,0x1719131b,0x171a131b,0x171b1319,0x171c1317,0x18151419,0x1816141c,0x1817141d,0x1818141c,0x1819131b,0x181a131a,0x181b1319,0x181c1516,0x19151418,0x1916141b,0x1917141c,0x1918131c,0x1919131b,0x191a131a,0x191b1318,0x1a151316,0x1a16131a,0x1a17131c,0x1a18131b,0x1a19131a,0x1a1a1319,0x1b161318,0x1b17131b,0x1b18131a,0x1b191319,0x1b1a1517,0x1c171319,0x1c181319,0x1c191517,
0x6141111,0xb0b1414,0xb0c1315,0xb0d1314,0xc0b1416,0xc0c1316,0xc0d1316,0xc0e1315,0xc0f1314,0xd0a1416,0xd0b1417,0xd0c1317,0xd0d1317,0xd0e1416,0xd0f1415,0xe0a1418,0xe0b1418,0xe0c1318,0xe0d1317,0xe0e1417,0xe0f1416,0xe101414,0xf091519,0xf0a1419,0xf0b1319,0xf0c1318,0xf0d1318,0xf0e1317,0xf0f1416,0xf101414,0x10071619,0x1008151a,0x1009141a,0x100a131a,0x100b121a,0x100c1219,0x100d1219,0x100e1317,0x100f1316,0x10101414,0x11061519,0x1107151a,0x11080e0f,0x1108131b,0x11090e1b,0x110a0e1b,0x110b0f1a,0x110c0f1a,0x110d1018,0x110e1117,0x110f1216,0x11101214,0x12051115,0x12060f18,0x12070e1a,0x12080d1a,0x12090d1b,0x120a0d1b,0x120b0c1a,0x120c0c19,0x120d0d18,0x120e0e17,0x120f0f15,0x12100f14,0x12111013,0x12121112,0x12130f13,0x12140e13,0x12150d13,0x12160c12,0x12170b11,0x12180a0e,0x13050f16,0x13060e18,0x13070d19,0x13080c1a,0x13090c1b,0x130a0c1b,0x130b0c1a,0x130c0b19,0x130d0c17,0x130e0d16,0x130f0e14,0x13100f13,0x13110f13,0x13120f13,0x13130f15,0x13140e15,0x13150d14,0x13160c13,0x13170c12,0x13180a12,0x13190c12,0x131a0f11,0x14040f14,0x14050e16,0x14060d18,0x14070c19,0x14080c1a,0x14090c1b,0x140a0b1a,0x140b0b19,0x140c0a18,0x140d0c16,0x140e0d15,0x140f0e13,0x14100f12,0x14110f13,0x14120f14,0x14130e15,0x14140e16,0x14150d14,0x14160c13,0x14170c13,0x14180b12,0x14190a12,0x141a0e12,0x141b1011,0x15040e15,0x15050d17,0x15060c18,0x15070c19,0x15080b1a,0x15090b1b,0x150a0b19,0x150b0b18,0x150c0a16,0x150d0c14,0x150e0d13,0x150f0e12,0x15100e11,0x15110f12,0x15120e13,0x15130e15,0x15140d16,0x15150d15,0x15160c14,0x15170c13,0x15180b13,0x15190a12,0x151a0e12,0x151b1011,0x16031012,0x16040e15,0x16050d17,0x16060c19,0x16070c1a,0x16080b1a,0x16090b19,0x160a0b18,0x160b0a16,0x160c0a14,0x160d0b13,0x160e0d12,0x160f0e11,0x16100e10,0x16110f11,0x16120e12,0x16130e14,0x16140d15,0x16150d14,0x16160c14,0x16170c13,0x16180b13,0x16190a12,0x161a0f12,0x161b1012,0x17031013,0x17040e15,0x17050d17,0x17060c19,0x17070b1a,0x17080b1a,0x17090b18,0x170a0b16,0x170b0a15,0x170c0a13,0x170d0b12,0x170e0d10,0x170f0e0f,0x17110f0f,0x17120e11,0x17130e13,0x17140d14,0x17150c14,0x17160c13,0x17170b13,0x17180b13,0x17190a12,0x171a0f12,0x171b1112,0x18031013,0x18040e15,0x18050d17,0x18060c19,0x18070b1a,0x18080b18,0x18090b16,0x180a0b15,0x180b0b13,0x180c0a12,0x180d0b10,0x180e0d0f,0x18120f10,0x18130e12,0x18140d14,0x18150c13,0x18160c13,0x18170b13,0x18180b13,0x18190e12,0x181a1012,0x181b1212,0x19031111,0x19040e15,0x19050d17,0x19060c18,0x19070c18,0x19080b17,0x19090b15,0x190a0b13,0x190b0b12,0x190c0b10,0x190d0b0f,0x19130e10,0x19140d12,0x19150c13,0x19160c13,0x19170b13,0x19180b12,0x19190f12,0x191a1112,0x1a040f14,0x1a050d16,0x1a060d18,0x1a070c17,0x1a080c15,0x1a090b14,0x1a0a0b12,0x1a0b0b10,0x1a0c0b0f,0x1a0d0c0d,0x1a140e11,0x1a150d12,0x1a160c12,0x1a170b12,0x1a180f12,0x1a191112,0x1a1a1212,0x1b041012,0x1b050e15,0x1b060d17,0x1b070d16,0x1b080c14,0x1b090c12,0x1b0a0c11,0x1b0b0c0f,0x1b0c0c0d,0x1b150d10,0x1b160f12,0x1b171012,0x1b181112,0x1b191212,0x1c051013,0x1c060e16,0x1c070e14,0x1c080d13,0x1c090d11,0x1c0a0d0f,0x1c0b0d0e,0x1c171212,0x1d061113,0x1d070f13,0x1d080f11,0x1d090e10,0x1d0a0e0e,
0x60f1111,0x6101010,0x6111010,0x6121010,0x6131010,0x6141010,0x6151111,0x70d1212,0x70e1111,0x70f1010,0x7101010,0x7110f10,0x7120f0f,0x7130f10,0x7140f10,0x7151010,0x7161111,0x80b1313,0x80c1212,0x80d1112,0x80e1011,0x80f0f10,0x8100f10,0x8110f0f,0x8120f0f,0x8130f10,0x8140f10,0x8150f10,0x8161010,0x8171111,0x9081414,0x9091313,0x90a1313,0x90b1213,0x90c1112,0x90d1012,0x90e1011,0x90f0f10,0x9100f10,0x9110e0f,0x9120e0f,0x9130e0f,0x9140e10,0x9150f10,0x9160f10,0x9171111,0xa071414,0xa081314,0xa091213,0xa0a1213,0xa0b1113,0xa0c1113,0xa0d1012,0xa0e0f11,0xa0f0f10,0xa100e10,0xa110e10,0xa120e0f,0xa130e0f,0xa140e10,0xa150f10,0xa160f10,0xa171011,0xa181212,0xb071314,0xb081314,0xb091213,0xb0a1113,0xb0b1113,0xb0c1012,0xb0d0f12,0xb0e0f12,0xb0f0e11,0xb100e10,0xb110e10,0xb120e10,0xb130e10,0xb140e10,0xb150e10,0xb160f11,0xb171011,0xb181112,0xc061414,0xc071314,0xc081214,0xc091113,0xc0a1113,0xc0b1013,0xc0c1012,0xc0d0f12,0xc0e0f12,0xc0f0e12,0xc100e11,0xc110e10,0xc120e10,0xc130e10,0xc140e10,0xc150e11,0xc160f11,0xc171012,0xc181113,0xd061415,0xd071314,0xd081214,0xd091114,0xd0a1013,0xd0b1013,0xd0c0f12,0xd0d0f12,0xd0e0e13,0xd0f0e13,0xd100e12,0xd110d11,0xd120d11,0xd130d11,0xd140e12,0xd150e12,0xd160f12,0xd170f13,0xd181113,0xd191313,0xe061415,0xe071214,0xe081114,0xe091114,0xe0a1013,0xe0b0f13,0xe0c0f12,0xe0d0e12,0xe0e0e13,0xe0f0e13,0xe100d13,0xe110d12,0xe120d12,0xe130d12,0xe140d13,0xe150d13,0xe160e13,0xe170f13,0xe181013,0xe191213,0xf061315,0xf071215,0xf081114,0xf091014,0xf0a0f13,0xf0b0f12,0xf0c0e12,0xf0d0e12,0xf0e0d12,0xf0f0d13,0xf100c13,0xf110c13,0xf120b12,0xf130b12,0xf140b13,0xf150c13,0xf160d13,0xf170e13,0xf180f13,0xf191113,0xf1a1213,0x10061215,0x10071115,0x10081014,0x10090f13,0x100a0e12,0x100b0e11,0x100c0e11,0x100d0d11,0x100e0c12,0x100f0b12,0x10100a13,0x10110912,0x10120912,0x10130812,0x10140912,0x10150912,0x10160a13,0x10170c13,0x10180e13,0x10191013,0x101a1113,0x101b1313,0x11061114,0x11070f14,0x11081012,0x110a0d0d,0x110b0d0e,0x110c0d0e,0x110d0c0f,0x110e0b10,0x110f0a11,0x11100811,0x11110811,0x11120709,0x11120b11,0x11130708,0x11130c10,0x11140707,0x11140c11,0x11150707,0x11150b11,0x11160a11,0x11170912,0x11180d12,0x11190f12,0x111a1012,0x111b1212,0x120d0b0c,0x120e0a0d,0x120f0d0e,0x12180f10,0x12190e11,0x121a0f12,0x121b1112,0x131b1011,
};
long rubblestart[4] = {0,326,623,899,};
long rubblenum[4] = {326,297,276,217,};

	//This is used to control the rate of rapid fire
#define MAXWEAP 4
long curweap = 1, lastshottime[MAXWEAP] = {0,0,0,0};
long myhealth = 100, showhealth = 1;
long numdynamite = 0, numjoystick = 0;

	//See AIMGRAV.BAS for derivation
	//     Given: (dx,dy,dz): difference vector (target - source)
	//                  grav: acceleration of gravity
	//                     v: velocity
	//returns: (*xi,*yi,*zi): shooting velocity
	//
	//Assumes that velocity&gravity are processed (approximately) like this:
	//   x += xi; y += yi; z += zi; zi += grav;
long aimgrav (float dx, float dy, float dz, float *xi, float *yi, float *zi, float grav, float v)
{
	double dd2, dd, Zc, vv, dz2, k, Ya, Yb, zii, di;

	dd2 = dx*dx + dy*dy;
	if (dd2 == 0)
		{ (*xi) = 0; (*yi) = 0; if (dz < 0) (*zi) = -v; else (*zi) = v; return(0); }
	dd = sqrt(dd2); Zc = grav*dd2*.5;
	vv = v*v; dz2 = dz*dz; k = dz2*vv - Zc*dz*2;
	Ya = (dz2+dd2)*2; Yb = Ya*vv*.5 + k;
	zii = Yb*Yb - (k*vv + Zc*Zc)*Ya*2; if (zii < 0) return(-1);
	zii = (Yb-sqrt(zii)) / Ya; //fast (low) trajectory
	if (zii < 0) (*zi) = 0; else (*zi) = sqrt(zii);
	if (dz*vv < Zc) (*zi) = -(*zi);
	if (vv < zii) { (*xi) = (*yi) = 0; return(0); }
	di = sqrt(vv-zii)/dd; (*xi) = di*dx; (*yi) = di*dy; return(0);
}

void vecrand (float sc, point3d *a)
{
	float f;

		//UNIFORM spherical randomization (see spherand.c)
	a->z = ((double)(rand()&32767))/16383.5-1.0;
	f = (((double)(rand()&32767))/16383.5-1.0)*PI; a->x = cos(f); a->y = sin(f);
	f = sqrt(1.0 - a->z*a->z)*sc; a->x *= f; a->y *= f; a->z *= sc;
}

void matrand (float sc, point3d *a, point3d *b, point3d *c)
{
	float f;

	vecrand(sc,a);
	vecrand(sc,c);
	b->x = a->y*c->z - a->z*c->y;
	b->y = a->z*c->x - a->x*c->z;
	b->z = a->x*c->y - a->y*c->x;
	f = sc/sqrt(b->x*b->x + b->y*b->y + b->z*b->z);
	b->x *= f; b->y *= f; b->z *= f;

	c->x = a->y*b->z - a->z*b->y;
	c->y = a->z*b->x - a->x*b->z;
	c->z = a->x*b->y - a->y*b->x;

	a->x *= sc; a->y *= sc; a->z *= sc;
}

void findrandomspot (long *x, long *y, long *z)
{
	long cnt;

	cnt = 64;
	do
	{
		(*x) = (rand()&(VSID-1));
		(*y) = (rand()&(VSID-1));
		for((*z)=255;(*z)>=0;(*z)--)
			if (!isvoxelsolid(*x,*y,*z)) break;
		cnt--;
	} while (((*z) < 0) && (cnt > 0));
}

long isboxempty (long x0, long y0, long z0, long x1, long y1, long z1)
{
	long x, y;
	for(y=y0;y<y1;y++)
		for(x=x0;x<x1;x++)
			if (anyvoxelsolid(x,y,z0,z1)) return(1);
	return(0);
}

	//Returns 0 if any voxel inside (vx-x)ý+(vy-y)ý+(vz-z)ý < rý is solid
	//   NOTE: code uses really cool & optimized algorithm!
long issphereempty (long x, long y, long z, long r)
{
	long xx, yy, zz, k, r2, nr, z2;

	if (r <= 0) return(1);
	if (anyvoxelsolid(x,y,z-r+1,z+r)) return(0);
	r2 = r*r; nr = r2;
	for(yy=1;yy<r;yy++)
	{
		nr += 1-(yy<<1); k = nr; zz = r; z2 = r2;
		while (z2 >= k) { z2 += 1-(zz<<1); zz--; }
		if (anyvoxelsolid(x-yy,y,z-zz,z+zz+1)) return(0);
		if (anyvoxelsolid(x+yy,y,z-zz,z+zz+1)) return(0);
		if (anyvoxelsolid(x,y-yy,z-zz,z+zz+1)) return(0);
		if (anyvoxelsolid(x,y+yy,z-zz,z+zz+1)) return(0);
		for(xx=1;xx<yy;xx++)
		{
			k += 1-(xx<<1); if (k <= 0) break;
			while (z2 >= k) { z2 += 1-(zz<<1); zz--; }
			if (zz < 0) break;
			if (anyvoxelsolid(x-xx,y-yy,z-zz,z+zz+1)) return(0);
			if (anyvoxelsolid(x+xx,y-yy,z-zz,z+zz+1)) return(0);
			if (anyvoxelsolid(x-yy,y-xx,z-zz,z+zz+1)) return(0);
			if (anyvoxelsolid(x+yy,y-xx,z-zz,z+zz+1)) return(0);
			if (anyvoxelsolid(x-yy,y+xx,z-zz,z+zz+1)) return(0);
			if (anyvoxelsolid(x+yy,y+xx,z-zz,z+zz+1)) return(0);
			if (anyvoxelsolid(x-xx,y+yy,z-zz,z+zz+1)) return(0);
			if (anyvoxelsolid(x+xx,y+yy,z-zz,z+zz+1)) return(0);
		}
		k += 1-(xx<<1);
		if (k > 0)
		{
			while (z2 >= k) { z2 += 1-(zz<<1); zz--; }
			if (anyvoxelsolid(x-yy,y-yy,z-zz,z+zz+1)) return(0);
			if (anyvoxelsolid(x-yy,y+yy,z-zz,z+zz+1)) return(0);
			if (anyvoxelsolid(x+yy,y-yy,z-zz,z+zz+1)) return(0);
			if (anyvoxelsolid(x+yy,y+yy,z-zz,z+zz+1)) return(0);
		}
	}
	return(1);
}

	//Returns 0 if any voxel inside (vx-x)ý+(vy-y)ý+(vz-z)ý < rý is empty
	//   NOTE: code uses really cool & optimized algorithm!
long isspheresolid (long x, long y, long z, long r)
{
	long xx, yy, zz, k, r2, nr, z2;

	if (r <= 0) return(0);
	if (anyvoxelempty(x,y,z-r+1,z+r)) return(0);
	r2 = r*r; nr = r2;
	for(yy=1;yy<r;yy++)
	{
		nr += 1-(yy<<1); k = nr; zz = r; z2 = r2;
		while (z2 >= k) { z2 += 1-(zz<<1); zz--; }
		if (anyvoxelempty(x-yy,y,z-zz,z+zz+1)) return(0);
		if (anyvoxelempty(x+yy,y,z-zz,z+zz+1)) return(0);
		if (anyvoxelempty(x,y-yy,z-zz,z+zz+1)) return(0);
		if (anyvoxelempty(x,y+yy,z-zz,z+zz+1)) return(0);
		for(xx=1;xx<yy;xx++)
		{
			k += 1-(xx<<1); if (k <= 0) break;
			while (z2 >= k) { z2 += 1-(zz<<1); zz--; }
			if (zz < 0) break;
			if (anyvoxelempty(x-xx,y-yy,z-zz,z+zz+1)) return(0);
			if (anyvoxelempty(x+xx,y-yy,z-zz,z+zz+1)) return(0);
			if (anyvoxelempty(x-yy,y-xx,z-zz,z+zz+1)) return(0);
			if (anyvoxelempty(x+yy,y-xx,z-zz,z+zz+1)) return(0);
			if (anyvoxelempty(x-yy,y+xx,z-zz,z+zz+1)) return(0);
			if (anyvoxelempty(x+yy,y+xx,z-zz,z+zz+1)) return(0);
			if (anyvoxelempty(x-xx,y+yy,z-zz,z+zz+1)) return(0);
			if (anyvoxelempty(x+xx,y+yy,z-zz,z+zz+1)) return(0);
		}
		k += 1-(xx<<1);
		if (k > 0)
		{
			while (z2 >= k) { z2 += 1-(zz<<1); zz--; }
			if (anyvoxelempty(x-yy,y-yy,z-zz,z+zz+1)) return(0);
			if (anyvoxelempty(x-yy,y+yy,z-zz,z+zz+1)) return(0);
			if (anyvoxelempty(x+yy,y-yy,z-zz,z+zz+1)) return(0);
			if (anyvoxelempty(x+yy,y+yy,z-zz,z+zz+1)) return(0);
		}
	}
	return(1);
}

long groucol[9] = {0x506050,0x605848,0x705040,0x804838,0x704030,0x603828,
	0x503020,0x402818,0x302010,};
long mycolfunc (lpoint3d *p)
{
	long i, j;
	j = groucol[(p->z>>5)+1]; i = groucol[p->z>>5];
	i = ((((((j&0xff00ff)-(i&0xff00ff))*(p->z&31))>>5)+i)&0xff00ff) +
		 ((((((j&0x00ff00)-(i&0x00ff00))*(p->z&31))>>5)+i)&0x00ff00);
	i += (labs((p->x&31)-16)<<16)+(labs((p->y&31)-16)<<8)+labs((p->z&31)-16);
	j = rand(); i += (j&7) + ((j&0x70)<<4) + ((j&0x700)<<8);
	return(i+0x80000000);
}

	//Heightmap for bottom
unsigned char bothei[32*32], botheimin;
long botcol[32*32];
long botcolfunc (lpoint3d *p) { return(botcol[(p->y&(32-1))*32+(p->x&(32-1))]); }
void botinit ()
{
	float f, g;
	long i, j, x, y, c;
	float fsc[4+14] = {.4,.45,.55,.45,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
	long col[4+14] = {0x80453010,0x80403510,0x80403015,0x80423112,
							0x80403013,0x80433010,0x80403313,0x80403310,0x80433013,
							0x80403310,0x80403310,0x80433013,0x80433010,0x80403310,
							0x80433010,0x80403013,0x80433310,0x80403013,
							};
	long xpos[4+14] = {0,16, 3,20,  8,24, 0, 7,25,19,12,26,10, 3,29,11,18,25};
	long ypos[4+14] = {0, 5,16,22,  1, 3, 8, 8, 9,13,15,16,12,24,24,28,29,29};
	long rad[4+14] = {68,51,78,67, 20,21,25,19,22,26,18,24,25,23,25,17,23,25};

		//Initialize solid heightmap at bottom of map
	botheimin = 255;
	for(y=0;y<32;y++)
		for(x=0;x<32;x++)
		{
			f = 254.0; c = 0x80403010;
			for(j=0;j<4+14;j++)
			{
				i = (((xpos[j]-x)&31)-16)*(((xpos[j]-x)&31)-16)+
					 (((ypos[j]-y)&31)-16)*(((ypos[j]-y)&31)-16);
				if (i >= rad[j]) continue;
				g = 255.0-sqrt((float)(rad[j]-i))*fsc[j];
				if (g < f) { f = g; c = col[j]; }
			}
			i = y*32+x; botcol[i] = (((rand()<<15)+rand())&0x30303)+c;
			bothei[i] = (unsigned char)f;
			if (bothei[i] < botheimin) botheimin = bothei[i];
		}
}

static _inline void fcossin (float a, float *c, float *s)
{
	_asm
	{
		fld a
		fsincos
		mov eax, c
		fstp dword ptr [eax]
		mov eax, s
		fstp dword ptr [eax]
	}
}

#define EXTRASLICECOVER 1

void drawspritebendx (vx5sprite *spr, float bendang, long stepsiz)
{
	vx5sprite tempspr;
	float f, g, h, c, s;
	long i, omin, omax;

	omin = vx5.xplanemin; omax = vx5.xplanemax; tempspr = *spr;
	h = 1.f / ((float)(tempspr.voxnum->xsiz)); bendang *= h;
	g = (float)stepsiz*.5 - tempspr.voxnum->xpiv;
	h *= (float)(tempspr.voxnum->xsiz - tempspr.voxnum->xpiv)*.5;
	for(i=0;i<tempspr.voxnum->xsiz;i+=stepsiz)
	{
		vx5.xplanemin = i; vx5.xplanemax = i+stepsiz+EXTRASLICECOVER;
		f = (float)i + g;
		fcossin(f*bendang,&c,&s);
		tempspr.s.x = spr->s.x*c - spr->h.x*s;
		tempspr.s.y = spr->s.y*c - spr->h.y*s;
		tempspr.s.z = spr->s.z*c - spr->h.z*s;
		tempspr.h.x = spr->s.x*s + spr->h.x*c;
		tempspr.h.y = spr->s.y*s + spr->h.y*c;
		tempspr.h.z = spr->s.z*s + spr->h.z*c; f *= h;
		tempspr.p.x = (spr->s.x-tempspr.s.x)*f + spr->p.x;
		tempspr.p.y = (spr->s.y-tempspr.s.y)*f + spr->p.y;
		tempspr.p.z = (spr->s.z-tempspr.s.z)*f + spr->p.z;
		drawsprite(&tempspr);
	}
	vx5.xplanemin = omin; vx5.xplanemax = omax;
}

void drawspritebendy (vx5sprite *spr, float bendang, long stepsiz)
{
	vx5sprite tempspr;
	float f, g, h, c, s;
	long i, omin, omax;

	omin = vx5.xplanemin; omax = vx5.xplanemax; tempspr = *spr;
	h = 1.f / ((float)(tempspr.voxnum->xsiz)); bendang *= h;
	g = (float)stepsiz*.5 - tempspr.voxnum->xpiv;
	h *= (float)(tempspr.voxnum->xsiz - tempspr.voxnum->xpiv)*.5;
	for(i=0;i<tempspr.voxnum->xsiz;i+=stepsiz)
	{
		vx5.xplanemin = i; vx5.xplanemax = i+stepsiz+EXTRASLICECOVER;
		f = (float)i + g;
		fcossin(f*bendang,&c,&s);
		tempspr.s.x = spr->s.x*c - spr->f.x*s;
		tempspr.s.y = spr->s.y*c - spr->f.y*s;
		tempspr.s.z = spr->s.z*c - spr->f.z*s;
		tempspr.f.x = spr->s.x*s + spr->f.x*c;
		tempspr.f.y = spr->s.y*s + spr->f.y*c;
		tempspr.f.z = spr->s.z*s + spr->f.z*c; f *= h;
		tempspr.p.x = (spr->s.x-tempspr.s.x)*f + spr->p.x;
		tempspr.p.y = (spr->s.y-tempspr.s.y)*f + spr->p.y;
		tempspr.p.z = (spr->s.z-tempspr.s.z)*f + spr->p.z;
		drawsprite(&tempspr);
	}
	vx5.xplanemin = omin; vx5.xplanemax = omax;
}

void drawspritetwist (vx5sprite *spr, float twistang, long stepsiz)
{
	vx5sprite tempspr;
	float g, h, c, s;
	long i, omin, omax;

	omin = vx5.xplanemin; omax = vx5.xplanemax; tempspr = *spr;

	h = 1.f / ((float)(tempspr.voxnum->xsiz)); twistang *= h;
	g = (float)stepsiz*.5 - tempspr.voxnum->xpiv;
	for(i=0;i<tempspr.voxnum->xsiz;i+=stepsiz)
	{
		vx5.xplanemin = i; vx5.xplanemax = i+stepsiz+EXTRASLICECOVER;
		fcossin(((float)i + g)*twistang,&c,&s);
		tempspr.h.x = spr->h.x*c - spr->f.x*s;
		tempspr.h.y = spr->h.y*c - spr->f.y*s;
		tempspr.h.z = spr->h.z*c - spr->f.z*s;
		tempspr.f.x = spr->h.x*s + spr->f.x*c;
		tempspr.f.y = spr->h.y*s + spr->f.y*c;
		tempspr.f.z = spr->h.z*s + spr->f.z*c;
		drawsprite(&tempspr);
	}

	vx5.xplanemin = omin; vx5.xplanemax = omax;
}

long initmap ()
{
	kv6data *tempkv6;
	lpoint3d lp;
	float f, g;
	long i, j, k;
	char tempnam[MAX_PATH], *vxlnam, *skynam, *kv6nam, *userst;

		//Load KV6 objects from hard-coded list
	j = 0; strcpy(tempnam,"kv6\\");
	for(i=0;i<NUMKV6;i++)
	{
		k = 4; while (kv6list[j] != ',') tempnam[k++] = kv6list[j++];
		strcpy(&tempnam[k],".kv6"); j++;
		kv6[i] = getkv6(tempnam);

			//Generate all lower mip-maps here:
		for(tempkv6=kv6[i];tempkv6=genmipkv6(tempkv6););
	}

		//Initialize solid height&color map at bottom of world
	botinit();

	i = strlen(cursxlnam); numsprites = 0;
	if ((i >= 4) && ((cursxlnam[i-3] == 'S') || (cursxlnam[i-3] == 's')))
	{
		if (loadsxl(cursxlnam,&vxlnam,&skynam,&userst))
		{  //.SXL valid so load sprite info out of .SXL

			if (!loadvxl(vxlnam,&ipos,&istr,&ihei,&ifor))
				loadnul(&ipos,&istr,&ihei,&ifor);
			loadsky(skynam);

			//parse (global) userst here

			for(numsprites=0;kv6nam=parspr(&spr[numsprites],&userst);numsprites++)
			{
				if (numsprites >= MAXSPRITES) continue; //OOPS! Just to be safe

					//KLIGHT is dummy sprite for light sources - don't load it!
				if (!stricmp(kv6nam,"KV6\\KLIGHT.KV6"))
				{
					//Copy light position info here!
					continue;
				}

				getspr(&spr[numsprites],kv6nam);

#if 1
					//Generate all lower mip-maps here:
				if (!(spr[numsprites].flags&2)) //generate mips for KV6 in SXL
				{
					tempkv6 = spr[numsprites].voxnum;
					while (tempkv6 = genmipkv6(tempkv6));
				}
				else //generate mips for KFA in SXL
				{
					for(i=(spr[numsprites].kfaptr->numspr)-1;i>=0;i--)
					{
						tempkv6 = spr[numsprites].kfaptr->spr[i].voxnum;
						while (tempkv6 = genmipkv6(tempkv6));
					}
				}
#endif

				if ((spr[numsprites].voxnum == kv6[CACO]) || (spr[numsprites].voxnum == kv6[WORM]))
					{ spr[numsprites].owner = 100; continue; }
				else if (spr[numsprites].voxnum == kv6[DOOR])
					{ sscanf(userst,"%d %d",&spr[numsprites].tag,&spr[numsprites].owner); continue; }
			}

		} else loadnul(&ipos,&istr,&ihei,&ifor);
	}
	else if ((i >= 4) && ((cursxlnam[i-3] == 'V') || (cursxlnam[i-3] == 'v')))
	{
		if (!loadvxl(cursxlnam,&ipos,&istr,&ihei,&ifor))
			loadnul(&ipos,&istr,&ihei,&ifor);
	}
	else loadnul(&ipos,&istr,&ihei,&ifor);

	if (!numsprites) //.SXL invalid, so generate random sprite positions
		for(i=0;i<NUMKV6;i++)
		{
			if (((i >= NUM0) && (i <= NUM9)) || (i == CACO) || (i == WORM)) continue;

			memset(&spr[numsprites],0,sizeof(spritetype));
			spr[numsprites].voxnum = kv6[i];
			findrandomspot(&lp.x,&lp.y,&lp.z);

			f = 0.5;
			spr[numsprites].p.x = lp.x; spr[numsprites].p.y = lp.y;
			spr[numsprites].p.z = lp.z-((float)spr[numsprites].voxnum->zsiz-spr[numsprites].voxnum->zpiv)*f;
			spr[numsprites].s.x = spr[numsprites].h.y = spr[numsprites].f.z = f;
			spr[numsprites].flags = 0;

			if ((spr[numsprites].voxnum == kv6[CACO]) || (spr[numsprites].voxnum == kv6[WORM]))
				spr[numsprites].owner = 100;
			if (spr[numsprites].voxnum == kv6[DOOR])
				{ spr[numsprites].tag = 90; spr[numsprites].owner = 1000; }

			numsprites++;
		}

	for(i=0;i<NUMSECRETS;i++) //Put some random secrets underground
	{
		lp.x = (rand()&(VSID-1));
		lp.y = (rand()&(VSID-1));
		lp.z = (rand()&255);
		if (!isspheresolid(lp.x,lp.y,lp.z,20)) continue;

		vx5.colfunc = manycolfunc; setsphere(&lp,16,-1);

		memset(&spr[numsprites],0,sizeof(spritetype));
		spr[numsprites].voxnum = kv6[JOYSTICK];
		spr[numsprites].p.x = lp.x; spr[numsprites].p.y = lp.y; spr[numsprites].p.z = lp.z;
		spr[numsprites].s.x = spr[numsprites].h.y = spr[numsprites].f.z = 0.5;
		spr[numsprites].flags = 0;
		numsprites++;

			//Put a hint on the surface
		for(lp.z-=20;lp.z>0;lp.z--)
			if (!isvoxelsolid(lp.x,lp.y,lp.z)) break;
		if ((k = getcube(lp.x,lp.y,lp.z+1))&~1) *(long *)k = 0x8000ffff;
	}

	vx5.kv6mipfactor = 128;

	vx5.lightmode = 1;
	vx5.vxlmipuse = 9; vx5.mipscandist = 192;
	vx5.fallcheck = 1;
	updatevxl();

	vx5.maxscandist = (long)(VSID*1.42);

	//vx5.fogcol = 0x6f6f7f; vx5.maxscandist = 768; //TEMP HACK!!!

	ivel.x = ivel.y = ivel.z = 0;

	woodspr.owner = -1;

	return(0);
}

long initapp (long argc, char **argv)
{
	long i, j, k, z, argfilindex, cpuoption = -1;

	prognam = "\"Ken-VOXLAP\" test game";
	xres = 640; yres = 480; colbits = 32; fullscreen = 0; argfilindex = -1;
	for(i=argc-1;i>0;i--)
	{
		if ((argv[i][0] != '/') && (argv[i][0] != '-')) { argfilindex = i; continue; }
		if (!stricmp(&argv[i][1],"win")) { fullscreen = 0; continue; }
		if (!stricmp(&argv[i][1],"3dn")) { cpuoption = 0; continue; }
		if (!stricmp(&argv[i][1],"sse")) { cpuoption = 1; continue; }
		if (!stricmp(&argv[i][1],"sse2")) { cpuoption = 2; continue; }
		//if (!stricmp(&argv[i][1],"?")) { showinfo(); return(-1); }
		if ((argv[i][1] >= '0') && (argv[i][1] <= '9'))
		{
			k = 0; z = 0;
			for(j=1;;j++)
			{
				if ((argv[i][j] >= '0') && (argv[i][j] <= '9'))
					{ k = (k*10+argv[i][j]-48); continue; }
				switch (z)
				{
					case 0: xres = k; break;
					case 1: yres = k; break;
					//case 2: colbits = k; break;
				}
				if (!argv[i][j]) break;
				z++; if (z > 2) break;
				k = 0;
			}
		}
	}
	if (xres > MAXXDIM) xres = MAXXDIM;
	if (yres > MAXYDIM) yres = MAXYDIM;

	if (initvoxlap() < 0) return(-1);

	setsideshades(0,4,1,3,2,2);

		//AthlonXP 2000+: SSE:26.76ms, 3DN:27.34ms, SSE2:28.93ms
	extern long cputype;
	switch(cpuoption)
	{
		case 0: cputype &= ~((1<<25)|(1<<26)); cputype |= ((1<<30)|(1<<31)); break;
		case 1: cputype |= (1<<25); cputype &= ~(1<<26); cputype &= ~((1<<30)|(1<<31)); break;
		case 2: cputype |= ((1<<25)|(1<<26)); cputype &= ~((1<<30)|(1<<31)); break;
		default:;
	}

	kzaddstack("voxdata.zip");

		//Load numbers 0-9 and - into memory
	kpzload("png/knumb.png",(int *)&numb[0].f,(int *)&numb[0].p,(int *)&numb[0].x,(int *)&numb[0].y);
	for(i=1;i<11;i++)
	{
		numb[i].f = numb[i-1].p*32 + numb[i-1].f;
		numb[i].p = numb[0].p; numb[i].x = 24; numb[i].y = 32;
	}
	numb[0].x = 24; numb[0].y = 32;

		//Load asci (32-127+18) into memory
	kpzload("png/kasci9x12.png",(int *)&asci[32].f,(int *)&asci[32].p,(int *)&asci[32].x,(int *)&asci[32].y);
	for(i=33;i<128+18;i++)
	{
		asci[i].f = asci[i-1].p*FONTYDIM + asci[i-1].f;
		asci[i].p = asci[32].p; asci[i].x = FONTXDIM; asci[i].y = FONTYDIM;
	}
	asci[32].x = FONTXDIM; asci[32].y = FONTYDIM;
	for(i=0;i<32;i++) asci[i] = asci[32]; //Fill these in just in case...

		//Load target into memory
	kpzload("png/target.png",(int *)&target.f,(int *)&target.p,(int *)&target.x,(int *)&target.y);
	for(k=0;k<target.y;k++) //Reduce alpha...
		for(j=0;j<target.x;j++)
		{
			i = target.f + k*target.p + (j<<2) + 3;
			*(char *)i = (((*(char *)i)*48)>>8);
		}

	if (argfilindex >= 0) strcpy(cursxlnam,argv[argfilindex]);
						  else strcpy(cursxlnam,"vxl/default.sxl");
	if (initmap() < 0) return(-1);

		//Init klock
	readklock(&dtotclk);
	totclk = (long)(dtotclk*1000.0);

	memset(fpsometer,0x7f,sizeof(fpsometer)); for(i=0;i<FPSSIZ;i++) fpsind[i] = i; numframes = 0;
	fsynctics = 1.f;

	return(0);
}

void uninitapp ()
{
	long i;
	if (asci[32].f) { free((void *)asci[32].f); asci[32].f = 0; }
	if (numb[0].f) { free((void *)numb[0].f); numb[0].f = 0; }
	kzuninit();
	uninitvoxlap();
}

	//liquid==0 means it bounces
	//liquid!=0 means no bounce and freezes into floor (useful for blood)
void spawndebris (point3d *p, float vel, long col, long num, long liquid)
{
	point3d fp;

	for(;num>=0;num--)
	{
		dbri[dbrihead].p.x = p->x;
		dbri[dbrihead].p.y = p->y;
		dbri[dbrihead].p.z = p->z;
		do  //Uniform distribution on solid sphere
		{
			fp.x = (((float)rand())-16383.5)/16383.5;
			fp.y = (((float)rand())-16383.5)/16383.5;
			fp.z = (((float)rand())-16383.5)/16383.5;
		} while (fp.x*fp.x + fp.y*fp.y + fp.z*fp.z > 1.0);
		dbri[dbrihead].v.x = fp.x*vel;
		dbri[dbrihead].v.y = fp.y*vel;
		dbri[dbrihead].v.z = fp.z*vel;
		dbri[dbrihead].tim = totclk;
		dbri[dbrihead].col = (col&0xffffff);
		if (liquid) dbri[dbrihead].col |= 0x80000000;
		dbrihead = ((dbrihead+1)&(MAXDBRI-1));
		if (dbrihead == dbritail) dbritail = ((dbritail+1)&(MAXDBRI-1));
	}
}

void movedebris ()
{
	long i, j;
	float f;
	point3d ofp, fp;
	lpoint3d lp;

	for(i=dbritail;i!=dbrihead;i=((i+1)&(MAXDBRI-1)))
	{
		if (totclk-dbri[i].tim >= 2000)
		{
			if (i != dbritail) dbri[i] = dbri[dbritail];
			dbritail = ((dbritail+1)&(MAXDBRI-1)); continue;
		}
		ofp = dbri[i].p;
		dbri[i].v.z += fsynctics*2.0;
		dbri[i].p.x += dbri[i].v.x * fsynctics*64;
		dbri[i].p.y += dbri[i].v.y * fsynctics*64;
		dbri[i].p.z += dbri[i].v.z * fsynctics*64;
		if (dbri[i].col >= 0) //bounce and disappear
		{
			if (isvoxelsolid((long)dbri[i].p.x,(long)dbri[i].p.y,(long)dbri[i].p.z))
			{
				//if (!(rand()&3))
				//{
					dbri[i] = dbri[dbritail];
					dbritail = ((dbritail+1)&(MAXDBRI-1));
				//}
				//else //TOO SLOW TO BE USEFUL :/
				//{
				//   estnorm((long)dbri[i].p.x,(long)dbri[i].p.y,(long)dbri[i].p.z,&fp);
				//   f = (dbri[i].v.x*fp.x + dbri[i].v.y*fp.y + dbri[i].v.z*fp.z)*2.f;
				//   dbri[i].v.x = (dbri[i].v.x - fp.x*f)*.5f;
				//   dbri[i].v.y = (dbri[i].v.y - fp.y*f)*.5f;
				//   dbri[i].v.z = (dbri[i].v.z - fp.z*f)*.5f;
				//}
			}
		}
		else //no bounce; freeze color into .VXL map
		{
			if (!cansee(&ofp,&dbri[i].p,&lp))
			{
				j = getcube(lp.x,lp.y,lp.z);
				if ((unsigned long)j >= 2)
				{
					long r, g, b, r2, g2, b2;
					r = (long)((char *)j)[2];
					g = (long)((char *)j)[1];
					b = (long)((char *)j)[0];
					r2 = ((char *)&dbri[i].col)[2];
					g2 = ((char *)&dbri[i].col)[1];
					b2 = ((char *)&dbri[i].col)[0];
					*(long *)j = ((*(long *)j)&0xff000000) +
									 ((((r2-r)>>2) + r)<<16) +
									 ((((g2-g)>>2) + g)<<8) +
									 ((((b2-b)>>2) + b));
					//*(long *)j = dbri[i].col;
				}

				dbri[i] = dbri[dbritail];
				dbritail = ((dbritail+1)&(MAXDBRI-1));
			}
		}
	}
}

void explodesprite (vx5sprite *spr, float vel, long liquid, long stepsize)
{
	point3d fp, fp2, fp3, fp4;
	kv6voxtype *v, *ve;
	kv6data *kv;
	long i, x, y, z;

		//WARNING: This code will change if I re-design the KV6 format!
	kv = spr->voxnum; if (!kv) return;
	v = kv->vox;
	i = -(rand()%stepsize); //i = stepstart;
	stepsize--;
	for(x=0;x<kv->xsiz;x++)
	{
		fp.x = x - kv->xpiv;
		fp2.x = spr->s.x*fp.x + spr->p.x;
		fp2.y = spr->s.y*fp.x + spr->p.y;
		fp2.z = spr->s.z*fp.x + spr->p.z;
		for(y=0;y<kv->ysiz;y++)
		{
			fp.y = y - kv->ypiv;
			fp3.x = spr->h.x*fp.y + fp2.x;
			fp3.y = spr->h.y*fp.y + fp2.y;
			fp3.z = spr->h.z*fp.y + fp2.z;
			for(ve=&v[kv->ylen[x*kv->ysiz+y]];v<ve;v++)
			{
#if 1          //Surface voxels only
				i--; if (i >= 0) continue; i = stepsize;
				fp.z = v->z - kv->zpiv;
				fp4.x = spr->f.x*fp.z + fp3.x;
				fp4.y = spr->f.y*fp.z + fp3.y;
				fp4.z = spr->f.z*fp.z + fp3.z;
				spawndebris(&fp4,vel,v->col,1,liquid);
#else          //All voxels in volume. WARNING: use only for very small KV6!
				if (v->vis&16) z = v->z;
				for(;z<=v->z;z++)
				{
					i++; if (i < 0) continue; i = stepsize;
					fp.z = z - kv->zpiv;
					fp4.x = spr->f.x*fp.z + fp3.x;
					fp4.y = spr->f.y*fp.z + fp3.y;
					fp4.z = spr->f.z*fp.z + fp3.z;
					spawndebris(&fp4,vel,v->col,1,liquid);
				}
#endif
			}
		}
	}
}

void deletesprite (long index)
{
	numsprites--;
	playsoundupdate(&spr[index].p,(point3d *)0);
	playsoundupdate(&spr[numsprites].p,&spr[index].p);
	spr[index] = spr[numsprites];
}

static point3d dummysnd[256];
static long dummysndhead = 0, dummysndtail = 0;

void doframe ()
{
	dpoint3d dp, dp2, dp3, dpos;
	point3d fp, fp2, fp3, fpos;
	lpoint3d lp, lp2;
	double d;
	float f, fmousx, fmousy;
	long i, j, k, l, m, *hind, hdir;
	char tempbuf[260];

	if (!startdirectdraw(&i,&j,&k,&l)) goto skipalldraw;
	voxsetframebuffer(i,j,k,l);
	setcamera(&ipos,&istr,&ihei,&ifor,xres*.5,yres*.5,xres*.5);
	setears3d(ipos.x,ipos.y,ipos.z,ifor.x,ifor.y,ifor.z,ihei.x,ihei.y,ihei.z);
	opticast();

	//for(i=0;i<numsprites;i++) sortorder[i] = i;
	//for(j=1;j<numsprites;j++)  //SLOW & UGLY SPRITE SORTING... TEMP HACK!!!
	//   for(i=0;i<j;i++)
	//      if (spr[sortorder[i]].p.x*ifor.x + spr[sortorder[i]].p.y*ifor.y + spr[sortorder[i]].p.z*ifor.z <
	//          spr[sortorder[j]].p.x*ifor.x + spr[sortorder[j]].p.y*ifor.y + spr[sortorder[j]].p.z*ifor.z)
	//         { k = sortorder[i]; sortorder[i] = sortorder[j]; sortorder[j] = k; }
	//for(l=numsprites-1;l>=0;l--) //NOTE: backwards means front to back order!
	//{
	//   i = sortorder[l];
	for(i=numsprites-1;i>=0;i--)
	{
			//Draw the worm's health above its head
		if ((spr[i].voxnum == kv6[WORM]) && (spr[i].owner > 0))
		{
			memcpy(&tempspr,&spr[i],sizeof(spritetype));
			f = 0.5;
			tempspr.s.x *= f; tempspr.s.y *= f; tempspr.s.z *= f;
			tempspr.h.x *= f; tempspr.h.y *= f; tempspr.h.z *= f;
			tempspr.f.x *= f; tempspr.f.y *= f; tempspr.f.z *= f;
			f = (float)spr[i].voxnum->zsiz*.6;

			j = spr[i].owner;

			if (j >= 100) k = -10; else if (j >= 10) k = -5; else k = 0;
			tempspr.p.x = spr[i].p.x - spr[i].f.x*f + spr[i].h.x*10 + spr[i].s.x*k;
			tempspr.p.y = spr[i].p.y - spr[i].f.y*f + spr[i].h.y*10 + spr[i].s.y*k;
			tempspr.p.z = spr[i].p.z - spr[i].f.z*f + spr[i].h.z*10 + spr[i].s.z*k;

			if (j >= 100)
			{
				tempspr.voxnum = kv6[NUM0+(j/100)]; drawsprite(&tempspr);
				tempspr.p.x += spr[i].s.x*10;
				tempspr.p.y += spr[i].s.y*10;
				tempspr.p.z += spr[i].s.z*10;
			}
			if (j >= 10)
			{
				tempspr.voxnum = kv6[NUM0+((j/10)%10)]; drawsprite(&tempspr);
				tempspr.p.x += spr[i].s.x*10;
				tempspr.p.y += spr[i].s.y*10;
				tempspr.p.z += spr[i].s.z*10;
			}
			tempspr.voxnum = kv6[NUM0+(j%10)]; drawsprite(&tempspr);
		}
		else if (spr[i].voxnum == kv6[CACO])
		{
			for(j=-1;j<=1;j+=2) //Draw oscillating legs under CACO :)
			{
				memcpy(&tempspr,&spr[i],sizeof(spritetype));
				f = 2.0;
				tempspr.s.x *= f; tempspr.s.y *= f; tempspr.s.z *= f;
				tempspr.h.x *= f; tempspr.h.y *= f; tempspr.h.z *= f;
				tempspr.f.x *= f; tempspr.f.y *= f; tempspr.f.z *= f;

				if (spr[i].v.x*spr[i].v.x + spr[i].v.y*spr[i].v.y + spr[i].v.z*spr[i].v.z > 1*1)
					f = sin(dtotclk*6)*(float)j*.5;
				else
					f = 0;

				orthorotate(PI*1.5,f,0,&tempspr.s,&tempspr.h,&tempspr.f);
				tempspr.p.x = spr[i].p.x + spr[i].s.x*10*(float)j + spr[i].f.x*24;
				tempspr.p.y = spr[i].p.y + spr[i].s.y*10*(float)j + spr[i].f.y*24;
				tempspr.p.z = spr[i].p.z + spr[i].s.z*10*(float)j + spr[i].f.z*24;
				tempspr.voxnum = kv6[NUM1]; drawsprite(&tempspr);
			}
		}

		if (spr[i].flags&2)
		{
			if (keystatus[0x29]) //` (Change ANASPLIT animation...)
			{
				keystatus[0x29] = 0;
				spr[i].okfatim = spr[i].kfatim;
				if (spr[i].kfatim < 2000) spr[i].kfatim = 2000;
											else spr[i].kfatim = 0;
			}
			animsprite(&spr[i],fsynctics*1000.0);
		}
		drawsprite(&spr[i]);
	}

	for(i=dbritail;i!=dbrihead;i=((i+1)&(MAXDBRI-1)))
		drawspherefill(dbri[i].p.x,dbri[i].p.y,dbri[i].p.z,(float)(totclk-dbri[i].tim)*.0004-1,dbri[i].col);

	if (woodspr.owner >= 0)
		drawsprite(&woodspr);

	if (curvystp >= 0)
	{
		if (!curvystp) drawsprite(&curvyspr);
		else
		{
			switch ((totclk>>11)%3)
			{
				case 0: drawspritebendx(&curvyspr,sin((double)(totclk&2047)*PI/1024.0)*1.0,curvystp); break;
				case 1: drawspritebendy(&curvyspr,sin((double)(totclk&2047)*PI/1024.0)*1.0,curvystp); break;
				case 2: drawspritetwist(&curvyspr,sin((double)(totclk&2047)*PI/1024.0)*2.0,curvystp); break;
			}
		}
	}

		//Show target in middle of screen
	if (showtarget)
		drawtile(target.f,target.p,target.x,target.y,target.x<<15,target.y<<15,
			xres<<15,yres<<15,65536,65536,0,-1);

		//Show health on bottom of screen
	if (showhealth)
	{
		sprintf(tempbuf,"%d",myhealth); j = strlen(tempbuf);
		if (xres >= 512) l = 16; else l = 15;
		for(i=0;i<j;i++)
		{
			if ((tempbuf[i] >= '0') && (tempbuf[i] <= '9')) k = tempbuf[i]-'0'; else k = 10;
			drawtile(numb[k].f,numb[k].p,numb[k].x,numb[k].y,0,numb[k].y<<16,
				((i*24-j*12)<<l)+(xres<<15),(yres-6)<<16,1<<l,1<<l,0,(min(max(100-myhealth,0),100)*0x010202)^-1);
		}
	}

	if (numdynamite > 0)
	{
		dpos.x = 0; dpos.y = 0; dpos.z = -2;
		  dp.x = 1;   dp.y = 0;   dp.z = 0;
		 dp2.x = 0;  dp2.y = 1;  dp2.z = 0;
		 dp3.x = 0;  dp3.y = 0;  dp3.z = 1;
		setcamera(&dpos,&dp,&dp2,&dp3,xres*.08,yres*.92,xres*.5);
		tempspr.p.x = 0; tempspr.p.y = 0; tempspr.p.z = 0; f = .0135;
		tempspr.s.x = f; tempspr.s.y = 0; tempspr.s.z = 0;
		tempspr.h.x = 0; tempspr.h.y = 0; tempspr.h.z = -f;
		tempspr.f.x = 0; tempspr.f.y = f; tempspr.f.z = 0;
		orthorotate(dtotclk*2,0,0,&tempspr.s,&tempspr.h,&tempspr.f);
		tempspr.flags = 0;
		tempspr.voxnum = kv6[TNTBUNDL];
		drawsprite(&tempspr);
	}
	if (numjoystick > 0)
	{
		dpos.x = 0; dpos.y = 0; dpos.z = -2;
		  dp.x = 1;   dp.y = 0;   dp.z = 0;
		 dp2.x = 0;  dp2.y = 1;  dp2.z = 0;
		 dp3.x = 0;  dp3.y = 0;  dp3.z = 1;
		setcamera(&dpos,&dp,&dp2,&dp3,xres*.16,yres*.92,xres*.5);
		tempspr.p.x = 0; tempspr.p.y = 0; tempspr.p.z = 0; f = .011;
		tempspr.s.x = f; tempspr.s.y = 0; tempspr.s.z = 0;
		tempspr.h.x = 0; tempspr.h.y = 0; tempspr.h.z = -f;
		tempspr.f.x = 0; tempspr.f.y = f; tempspr.f.z = 0;
		orthorotate(dtotclk*2,0,0,&tempspr.s,&tempspr.h,&tempspr.f);
		tempspr.flags = 0;
		tempspr.voxnum = kv6[JOYSTICK]; drawsprite(&tempspr);
	}

		//Show last message
	if (totclk < messagetimeout)
	{
		j = strlen(message); l = 0; lp.x = xres/FONTXDIM; lp.y = 0;
		while (l < j)
		{
			lp.z = min(l+lp.x,j);
			if (!l) m = ((-l)*FONTXDIM-(((lp.z-l)*FONTXDIM)>>1));
				else m = ((-l)*FONTXDIM-((lp.x*FONTXDIM)>>1));
			for(i=l;i<lp.z;i++)
			{
				k = message[i]; if (k == '\n') { l = i+1-lp.x; break; }
				drawtile(asci[k].f,asci[k].p,asci[k].x,asci[k].y,
							0,-(*(char *)asci[k].f)<<16,
							((i*FONTXDIM+m)<<16)+(xres<<15),lp.y,
							65536,65536,0,0xffdfdf7f);
			}
			l += lp.x; lp.y += ((FONTYDIM+2)<<16);
		}
	}

		//Show typemessage if in typemode
	if (typemode)
	{
		j = strlen(typemessage);
		lp.x = xres/FONTXDIM; lp.y = yres-32-6-((j-1)/lp.x)*(FONTYDIM+2);
		for(l=0;l<j;l+=lp.x)
		{
			lp.z = min(l+lp.x,j);
			if (!l) m = ((-l)*FONTXDIM-(((lp.z-l)*FONTXDIM)>>1));
				else m = ((-l)*FONTXDIM-((lp.x*FONTXDIM)>>1));
			for(i=l;i<lp.z;i++)
			{
				k = typemessage[i];
				drawtile(asci[k].f,asci[k].p,asci[k].x,asci[k].y,
							0,(asci[k].y-(*(char *)asci[k].f))<<16,
							((i*FONTXDIM+m)<<16)+(xres<<15),
							((l/lp.x)*(FONTYDIM+2)+lp.y)<<16,
							65536,65536,0,-1);
			}
		}
	}

	//print4x6(0,0,0xffffff,0,"%s",dabuf);
	print6x8((xres-(56*6))>>1,yres-8,0xc0c0c0,-1,"\"Ken-VOXLAP\" test game by Ken Silverman (advsys.net/ken)");

	if (keystatus[0x3b]) //F1 (start a looping sound)
	{
		keystatus[0x3b] = 0;
		i = ((dummysndhead+1)%(sizeof(dummysnd)/sizeof(dummysnd[0])));
		if (i != dummysndtail)
		{
			dummysnd[dummysndhead].x = ipos.x;
			dummysnd[dummysndhead].y = ipos.y;
			dummysnd[dummysndhead].z = ipos.z;
			playsound("wav/airshoot.wav",100,1.0,&dummysnd[dummysndhead],KSND_3D|KSND_LOOP);
			dummysndhead = i;
		}
	}

		//Use lopass filter if you can't 'see' sound.
	fp.x = ipos.x; fp.y = ipos.y; fp.z = ipos.z;
	for(i=dummysndtail;i!=dummysndhead;i=((i+1)%(sizeof(dummysnd)/sizeof(dummysnd[0]))))
	{
		if (!cansee(&dummysnd[i],&fp,&lp)) playsoundupdate(&dummysnd[i],(point3d *)-2);
												else playsoundupdate(&dummysnd[i],(point3d *)-3);
	}

	if (keystatus[0x3c]) //F2 (stop least recently started looping sound)
	{
		keystatus[0x3c] = 0;
		if (dummysndhead != dummysndtail)
		{
			playsoundupdate(&dummysnd[dummysndtail],(point3d *)-1);
			dummysndtail = ((dummysndtail+1)%(sizeof(dummysnd)/sizeof(dummysnd[0])));
		}
	}

	if (keystatus[0xb7])
	{
		FILE *fil;

		keystatus[0xb7] = 0;
		strcpy(tempbuf,"KVX50000.PNG");
		while (1)
		{
			tempbuf[4] = ((capturecount/1000)%10)+48;
			tempbuf[5] = ((capturecount/100)%10)+48;
			tempbuf[6] = ((capturecount/10)%10)+48;
			tempbuf[7] = (capturecount%10)+48;
			if (!(fil = fopen(tempbuf,"rb"))) break;
			fclose(fil);
			capturecount++;
		}
		if (keystatus[0x1d]|keystatus[0x9d])
		{
			if (!surroundcapture32bit(&ipos,tempbuf,512))
			{
				capturecount++;
				sprintf(message,"Surround capture: %s",tempbuf);
				messagetimeout = totclk+4000;
			}
		}
		else
		{
			if (!screencapture32bit(tempbuf))
			{
				capturecount++;
				sprintf(message,"Screen capture: %s",tempbuf);
				messagetimeout = totclk+4000;
			}
		}
	}

		//FPS counter
	fpsometer[numframes&(FPSSIZ-1)] = (long)(fsynctics*100000); numframes++;
	if (showfps)
	{
			//Fast sort when already sorted... otherwise slow!
		j = min(numframes,FPSSIZ)-1;
		for(k=0;k<j;k++)
			if (fpsometer[fpsind[k]] > fpsometer[fpsind[k+1]])
			{
				m = fpsind[k+1];
				for(l=k;l>=0;l--)
				{
					fpsind[l+1] = fpsind[l];
					if (fpsometer[fpsind[l]] <= fpsometer[m]) break;
				}
				fpsind[l+1] = m;
			}
		i = ((fpsometer[fpsind[j>>1]]+fpsometer[fpsind[(j+1)>>1]])>>1); //Median

		drawline2d(0,i>>4,FPSSIZ,i>>4,0xe06060);
		for(k=0;k<FPSSIZ;k++) drawpoint2d(k,min(fpsometer[(numframes+k)&(FPSSIZ-1)]>>4,yres-1),0xc0c0c0);
		print4x6(0,0,0xc0c0c0,-1,"%d.%02dms %.2ffps",i/100,i%100,100000.0/(float)i);
	}

	stopdirectdraw();
	nextpage();
skipalldraw:;

		//Read keyboard, mouse, and timer
	readkeyboard();
	obstatus = bstatus; readmouse(&fmousx,&fmousy,&bstatus);
	odtotclk = dtotclk; readklock(&dtotclk);
	totclk = (long)(dtotclk*1000.0); fsynctics = (float)(dtotclk-odtotclk);

		//Rotate player's view
	dp.x = istr.z*.1; dp.y = fmousy*-.008; dp.z = fmousx*.008;
	dorthorotate(dp.x,dp.y,dp.z,&istr,&ihei,&ifor);

		//Draw less when turning fast to increase the frame rate
	if (!lockanginc)
	{
		i = 1; if (xres*yres >= 640*350) i++;
		f = dp.x*dp.x + dp.y*dp.y + dp.z*dp.z;
		if (f >= .01*.01)
		{
			i++;
			if (f >= .08*.08)
			{
				i++; if (f >= .64*.64) i++;
			}
		}
		vx5.anginc = (float)i;
	}

		//Move player and perform simple physics (gravity,momentum,friction)
	f = fsynctics*12.0;
	if (keystatus[0x2a]) f *= .0625;
	if (keystatus[0x36]) f *= 16.0;
	if (keystatus[0xcb]) { ivel.x -= istr.x*f; ivel.y -= istr.y*f; ivel.z -= istr.z*f; }
	if (keystatus[0xcd]) { ivel.x += istr.x*f; ivel.y += istr.y*f; ivel.z += istr.z*f; }
	if (keystatus[0xc8]) { ivel.x += ifor.x*f; ivel.y += ifor.y*f; ivel.z += ifor.z*f; }
	if (keystatus[0xd0]) { ivel.x -= ifor.x*f; ivel.y -= ifor.y*f; ivel.z -= ifor.z*f; }
	if (keystatus[0x9d]) { ivel.x -= ihei.x*f; ivel.y -= ihei.y*f; ivel.z -= ihei.z*f; } //Rt.Ctrl
	if (keystatus[0x52]) { ivel.x += ihei.x*f; ivel.y += ihei.y*f; ivel.z += ihei.z*f; } //KP0
	//ivel.z += fsynctics*2.0; //Gravity (used to be *4.0)
	f = fsynctics*64.0;
	dp.x = ivel.x*f;
	dp.y = ivel.y*f;
	dp.z = ivel.z*f;
	dp2 = ipos; clipmove(&ipos,&dp,CLIPRAD);
	if (f != 0)
	{
		f = .9/f; //Friction
		ivel.x = (ipos.x-dp2.x)*f;
		ivel.y = (ipos.y-dp2.y)*f;
		ivel.z = (ipos.z-dp2.z)*f;
	}

	if (vx5.clipmaxcr < CLIPRAD*.9)
	{
		if (vx5.clipmaxcr >= CLIPRAD*.1) //Try to push player to safety
		{
			if (vx5.cliphitnum > 0)
			{
				switch (vx5.cliphitnum)
				{
					case 1: dpos.x = dp2.x-vx5.cliphit[0].x;
							  dpos.y = dp2.y-vx5.cliphit[0].y;
							  dpos.z = dp2.z-vx5.cliphit[0].z;
							  break;
					case 2: dpos.x = dp2.x-(vx5.cliphit[0].x+vx5.cliphit[1].x)*.5;
							  dpos.y = dp2.y-(vx5.cliphit[0].y+vx5.cliphit[1].y)*.5;
							  dpos.z = dp2.z-(vx5.cliphit[0].z+vx5.cliphit[1].z)*.5;
							  break;
					case 3:
						 dp.x = (vx5.cliphit[1].x-vx5.cliphit[0].x);
						 dp.y = (vx5.cliphit[1].y-vx5.cliphit[0].y);
						 dp.z = (vx5.cliphit[1].z-vx5.cliphit[0].z);
						dp3.x = (vx5.cliphit[2].x-vx5.cliphit[0].x);
						dp3.y = (vx5.cliphit[2].y-vx5.cliphit[0].y);
						dp3.z = (vx5.cliphit[2].z-vx5.cliphit[0].z);
						dpos.x = dp.y*dp3.z - dp.z*dp3.y;
						dpos.y = dp.z*dp3.x - dp.x*dp3.z;
						dpos.z = dp.x*dp3.y - dp.y*dp3.x;

						  //Ugly hack making sure cross product points in right direction
						if ((dp2.x*3-(vx5.cliphit[0].x+vx5.cliphit[1].x+vx5.cliphit[2].x))*dpos.x +
							 (dp2.y*3-(vx5.cliphit[0].y+vx5.cliphit[1].y+vx5.cliphit[2].y))*dpos.y +
							 (dp2.z*3-(vx5.cliphit[0].z+vx5.cliphit[1].z+vx5.cliphit[2].z))*dpos.z < 0)
							{ dpos.x = -dpos.x; dpos.y = -dpos.y; dpos.z = -dpos.z; }

						break;
				}
				d = dpos.x*dpos.x + dpos.y*dpos.y + dpos.z*dpos.z;
				if (d > 0)
				{
					d = 1.0/sqrt(d);
					ivel.x += dpos.x*d;
					ivel.y += dpos.y*d;
					ivel.z += dpos.z*d;
				}
			}
		}
		else //Out of room... squish player!
		{
			playsound(PLAYERDIE,100,1.0,0,0);
			strcpy(message,"You got squished!"); messagetimeout = totclk+4000;
			do
			{
				findrandomspot(&lp.x,&lp.y,&lp.z);
				ipos.x = lp.x; ipos.y = lp.y; ipos.z = lp.z-CLIPRAD;
			} while (findmaxcr(ipos.x,ipos.y,ipos.z,CLIPRAD) < CLIPRAD*.9);
			ivel.x = ivel.y = ivel.z = 0;
		}
	}
	f = ivel.x*ivel.x + ivel.y*ivel.y + ivel.z*ivel.z; //Limit maximum velocity
	if (f > 8.0*8.0) { f = 8.0/sqrt(f); ivel.x *= f; ivel.y *= f; ivel.z *= f; }

	if (!typemode)
	{
		while (i = keyread()) //Detect 'T' (for typing mode)
			if (((i&255) == 'T') || ((i&255) == 't'))
				{ typemode = 1; typemessage[0] = '_'; typemessage[1] = 0; break; }
	}
	if (typemode)
	{
		while (i = keyread())
		{
			if (!(i&255)) continue;
			i &= 255;
			j = strlen(typemessage);
			if (i == 8) //Backspace
			{
				if (j > 1) { typemessage[j-2] = '_'; typemessage[j-1] = 0; }
			}
			else if (i == 13) //Enter
			{
				if (j > 1)
				{
					playsound(TALK,100,1.0,0,0);
					typemessage[j-1] = 0;
					if (typemessage[0] == '/')
					{
						if (!stricmp(&typemessage[1],"fps")) { showfps ^= 1; typemessage[0] = 0; }
						if (!stricmp(&typemessage[1],"fallcheck")) { vx5.fallcheck ^= 1; typemessage[0] = 0; }
						if (!stricmp(&typemessage[1],"sideshademode"))
						{
							if (!vx5.sideshademode) setsideshades(0,28,8,24,12,12); else setsideshades(0,4,1,3,2,2); //setsideshades(0,0,0,0,0,0);
							typemessage[0] = 0;
						}
						if (!_memicmp(&typemessage[1],"faceshade=",10))
						{
							char *cptr;
							tempbuf[0] = tempbuf[1] = tempbuf[2] = tempbuf[3] = tempbuf[4] = tempbuf[5] = 0;
							cptr = strtok(&typemessage[11],",");
							if (cptr)
							{
								tempbuf[0] = strtol(cptr,0,0);
								for(k=1;k<6;k++)
								{
									cptr = strtok(0,","); if (!cptr) break;
									tempbuf[k] = strtol(cptr,0,0);
								}
							}
							setsideshades(tempbuf[0],tempbuf[1],tempbuf[2],tempbuf[3],tempbuf[4],tempbuf[5]);
							typemessage[0] = 0;
						}
						if (!stricmp(&typemessage[1],"curvy")) { if (curvystp >= 0) curvystp = ~curvystp; typemessage[0] = 0; }
						if (!stricmp(&typemessage[1],"monst")) { disablemonsts ^= 1; typemessage[0] = 0; }
						if (!stricmp(&typemessage[1],"light"))
						{
							if (vx5.numlights < MAXLIGHTS)
							{
								i = 128; //radius to use
								vx5.lightsrc[vx5.numlights].p.x = ipos.x;
								vx5.lightsrc[vx5.numlights].p.y = ipos.y;
								vx5.lightsrc[vx5.numlights].p.z = ipos.z;
								vx5.lightsrc[vx5.numlights].r2 = (float)(i*i);
								vx5.lightsrc[vx5.numlights].sc = 262144.0; //4096.0;
								vx5.numlights++;

								updatebbox((long)ipos.x-i,(long)ipos.y-i,(long)ipos.z-i,(long)ipos.x+i,(long)ipos.y+i,(long)ipos.z+i,0);
							}
							typemessage[0] = 0;
						}
						if (!stricmp(&typemessage[1],"lightclear"))
						{
							i = 128; //radius to use
							for(j=vx5.numlights-1;j>=0;j--)
								updatebbox((long)vx5.lightsrc[j].p.x-i,(long)vx5.lightsrc[j].p.y-i,(long)vx5.lightsrc[j].p.z-i,
											  (long)vx5.lightsrc[j].p.x+i,(long)vx5.lightsrc[j].p.y+i,(long)vx5.lightsrc[j].p.z+i,0);
							vx5.numlights = 0; typemessage[0] = 0;
						}
						if (!_memicmp(&typemessage[1],"lightmode=",10))
						{
							 vx5.lightmode = min(max(atoi(&typemessage[11]),0),2);
							 updatebbox(0,0,0,VSID,VSID,MAXZDIM,0);
							 typemessage[0] = 0;
						}
						if (!_memicmp(&typemessage[1],"curvy=",6))
						{
							if (curvystp < 0) curvystp = ~curvystp;

							strcpy(tempbuf,"kv6\\");
							if (!typemessage[7]) strcat(tempbuf,"desklamp"); else strcat(tempbuf,&typemessage[7]);
							strcat(tempbuf,".kv6");

							curvykv6 = getkv6(tempbuf);
							if (curvykv6)
							{
								for(kv6data *tempkv6=curvykv6;tempkv6=genmipkv6(tempkv6);); //Generate all lower mip-maps here:
								curvyspr.p.x = ipos.x + ifor.x*32;
								curvyspr.p.y = ipos.y + ifor.y*32;
								curvyspr.p.z = ipos.z + ifor.z*32;
								curvyspr.s.x = istr.x*+.25; curvyspr.s.y = istr.y*+.25; curvyspr.s.z = istr.z*+.25;
								curvyspr.h.x = ifor.x*-.25; curvyspr.h.y = ifor.y*-.25; curvyspr.h.z = ifor.z*-.25;
								curvyspr.f.x = ihei.x*+.25; curvyspr.f.y = ihei.y*+.25; curvyspr.f.z = ihei.z*+.25;

								curvyspr.flags = 0; curvyspr.voxnum = curvykv6;
							} else curvystp = ~curvystp;
							typemessage[0] = 0;
						}

						if (!_memicmp(&typemessage[1],"curvystp=",9)) { curvystp = atoi(&typemessage[10]); typemessage[0] = 0; }
						if (!_memicmp(&typemessage[1],"scandist=",9)) { vx5.maxscandist = atoi(&typemessage[10]); typemessage[0] = 0; }
						if (!_memicmp(&typemessage[1],"fogcol=",7)) { vx5.fogcol = strtol(&typemessage[8],0,0); typemessage[0] = 0; }
						if (!_memicmp(&typemessage[1],"kv6col=",7)) { vx5.kv6col = strtol(&typemessage[8],0,0); typemessage[0] = 0; }
						if (!_memicmp(&typemessage[1],"anginc=",7)) { vx5.anginc = atoi(&typemessage[8]); if (vx5.anginc > 0) lockanginc = 1; else { lockanginc = 0; vx5.anginc = 1; } typemessage[0] = 0; }
						if (!_memicmp(&typemessage[1],"vxlmip=",7)) { vx5.mipscandist = atoi(&typemessage[8]); typemessage[0] = 0; }
						if (!_memicmp(&typemessage[1],"kv6mip=",7)) { vx5.kv6mipfactor = atoi(&typemessage[8]); typemessage[0] = 0; }
						if (!_memicmp(&typemessage[1],"health=",7)) { myhealth = atoi(&typemessage[8]); typemessage[0] = 0; }
						if (!_memicmp(&typemessage[1],"numdynamite=",12)) { numdynamite = atoi(&typemessage[13]); typemessage[0] = 0; }
						if (!_memicmp(&typemessage[1],"numjoystick=",12)) { numjoystick = atoi(&typemessage[13]); typemessage[0] = 0; }
						if (!_memicmp(&typemessage[1],"showtarget=",11)) { showtarget = atoi(&typemessage[12]); typemessage[0] = 0; }
						if (!_memicmp(&typemessage[1],"showhealth=",11)) { showhealth = atoi(&typemessage[12]); typemessage[0] = 0; }
					}
					if (typemessage[0])
					{
						//net_send(typemessage); ???
						strcpy(message,typemessage); messagetimeout = totclk+4000;
						typemessage[0] = '_'; typemessage[1] = 0;
					}
					typemode = 0;
				}
			}
			else
			{
				if (j+1 < sizeof(typemessage))
				{
					typemessage[j-1] = (char)i;
					typemessage[j] = '_';
					typemessage[j+1] = 0;
				}
			}
		}
	}

	for(i=0;i<MAXWEAP;i++)
		if (keystatus[i+2]) curweap = i;
	if (keystatus[0x1d]|(bstatus&1))
	{
		switch (curweap)
		{
			case 0: //Shoot instant bullet
				if (totclk-lastshottime[curweap] >= 50)
				{
					lastshottime[curweap] = totclk;
					hitscan(&ipos,&ifor,&lp2,&hind,&hdir);
					if (hind)
					{
						fp.x = lp2.x; fp.y = lp2.y; fp.z = lp2.z;
						playsound(MACHINEGUN,100,(float)(rand()-16384)*.000005+1.0,&fp,KSND_3D);
						i = (*hind)|0xff000000; //Maximum brightness
						vx5.colfunc = mycolfunc; vx5.curcol = 0x80704030;
						setsphere(&lp2,4,-1);

						if (vx5.maxz >= botheimin)
						{
							vx5.colfunc = botcolfunc;
							setheightmap(bothei,32,32,32,vx5.minx,vx5.miny,vx5.maxx,vx5.maxy);
						}

						lastbulpos.x = lp2.x; lastbulpos.y = lp2.y; lastbulpos.z = lp2.z;
						lastbulvel.x = ifor.x; lastbulvel.y = ifor.y; lastbulvel.z = ifor.z;
						spawndebris(&fp,0.5,i,4,0);
					}
				}
				break;
			case 1: //Shoot travelling bullet
				if (totclk-lastshottime[curweap] >= 180)
				{
					lastshottime[curweap] = totclk;
					if (numsprites < MAXSPRITES)
					{
						i = numsprites++; //Allocate sprite

						spr[i].voxnum = kv6[SNAKE];
						spr[i].owner = -1; spr[i].tim = totclk; spr[i].tag = 0;

						spr[i].p.x = ipos.x+ihei.x*4;
						spr[i].p.y = ipos.y+ihei.y*4;
						spr[i].p.z = ipos.z+ihei.z*4;

						f = 0.2;
						spr[i].s.x = istr.x*f; spr[i].s.y = istr.y*f; spr[i].s.z = istr.z*f;
						spr[i].h.x = ihei.x*f; spr[i].h.y = ihei.y*f; spr[i].h.z = ihei.z*f;
						spr[i].f.x = ifor.x*f; spr[i].f.y = ifor.y*f; spr[i].f.z = ifor.z*f;
						spr[i].flags = 0;
						spr[i].v.x = ifor.x*256; spr[i].v.y = ifor.y*256; spr[i].v.z = ifor.z*256;

						playsound(SHOOTBUL,100,(float)(rand()-16384)*.000005+1.0,&spr[i].p,KSND_MOVE);
					}
				}
				break;
			case 2:
				if (woodspr.owner < 0)
				{
					fp.x = ipos.x; fp2.x = fp.x + ifor.x*48;
					fp.y = ipos.y; fp2.y = fp.y + ifor.y*48;
					fp.z = ipos.z; fp2.z = fp.z + ifor.z*48;
					if (!cansee(&fp,&fp2,&lp))
					{
						playsound(WOODRUB,100,(float)(rand()-16384)*.000005+1.6,0,0);

						woodspr.voxnum = kv6[WOODBORD]; woodspr.flags = 0;
						woodspr.v.x = (float)lp.x;
						woodspr.v.y = (float)lp.y;
						woodspr.v.z = (float)lp.z;
						woodspr.owner = 0;
					}
				}
				if (woodspr.owner >= 0)
				{
					fp.x = ipos.x+ifor.x*48;
					fp.y = ipos.y+ifor.y*48;
					fp.z = ipos.z+ifor.z*48;

					//woodspr.p.x = (woodspr.v.x+fp.x)*0.5;
					//woodspr.p.y = (woodspr.v.y+fp.y)*0.5;
					//woodspr.p.z = (woodspr.v.z+fp.z)*0.5;

					woodspr.s.x = woodspr.v.x-fp.x;
					woodspr.s.y = woodspr.v.y-fp.y;
					woodspr.s.z = woodspr.v.z-fp.z;
					f = 1.0 / sqrt(woodspr.s.x*woodspr.s.x + woodspr.s.y*woodspr.s.y + woodspr.s.z*woodspr.s.z);
					woodspr.s.x *= f; woodspr.s.y *= f; woodspr.s.z *= f;

					f = kv6[WOODBORD]->xsiz*-.5;
					woodspr.p.x = woodspr.s.x*f + woodspr.v.x;
					woodspr.p.y = woodspr.s.y*f + woodspr.v.y;
					woodspr.p.z = woodspr.s.z*f + woodspr.v.z;

					woodspr.f.x = woodspr.s.y*ifor.z - woodspr.s.z*ifor.y;
					woodspr.f.y = woodspr.s.z*ifor.x - woodspr.s.x*ifor.z;
					woodspr.f.z = woodspr.s.x*ifor.y - woodspr.s.y*ifor.x;
					f = 1.0 / sqrt(woodspr.f.x*woodspr.f.x + woodspr.f.y*woodspr.f.y + woodspr.f.z*woodspr.f.z);
					woodspr.f.x *= f; woodspr.f.y *= f; woodspr.f.z *= f;

					woodspr.h.x = woodspr.f.y*woodspr.s.z - woodspr.f.z*woodspr.s.y;
					woodspr.h.y = woodspr.f.z*woodspr.s.x - woodspr.f.x*woodspr.s.z;
					woodspr.h.z = woodspr.f.x*woodspr.s.y - woodspr.f.y*woodspr.s.x;
				}
				break;
			case 3:
				if (totclk-lastshottime[curweap] >= 180)
				{
					lastshottime[curweap] = totclk;
					playsound(DIVEBORD,100,(float)(rand()-16384)*.000005+0.6,0,0);
					vx5.colfunc = mycolfunc; vx5.curcol = 0x80704030;
					lp.x = ipos.x+ifor.x*16;
					lp.y = ipos.y+ifor.y*16;
					lp.z = ipos.z+ifor.z*16;
					setsphere(&lp,8,0);
				}
				break;
		}
	}

	if ((woodspr.owner >= 0) && ((!(keystatus[0x1d]|(bstatus&1))) || (curweap != 2)))
	{
		playsound(WOODRUB,100,(float)(rand()-16384)*.000005+1.0,0,0);
		vx5.colfunc = kv6colfunc; setkv6(&woodspr,0);
		woodspr.owner = -1;
	}

		//Activate object (open/close door for example)
	if (((keystatus[0x39]) && (!typemode)) || ((bstatus&2) > (obstatus&2)))
	{
		keystatus[0x39] = 0;
		for(i=numsprites-1;i>=0;i--)
		{
			if (spr[i].voxnum == kv6[DOOR])
			{
				dp.x = spr[i].p.x-ipos.x;
				dp.y = spr[i].p.y-ipos.y;
				dp.z = spr[i].p.z-ipos.z;
				if (1) //((dp.x*dp.x + dp.y*dp.y + dp.z*dp.z < 64*64) && (dp.x*ifor.x + dp.y*ifor.y + dp.z*ifor.z >= 0))
				{
					playsound(DOOROPEN,100,1.0,&spr[i].p,KSND_3D);
					spr[i].tag = -spr[i].tag; //Set Direction
					spr[i].tim = spr[i].owner-spr[i].tim; //Set stop time
				}
			}
		}
	}

	movedebris();

	for(i=numsprites-1;i>=0;i--)
	{
		if (spr[i].tag == -17) //Animate melted sprites
		{
			if ((totclk > spr[i].tim+2000) || (spr[i].owner >= 3))
			{
				//spr[i].s.x *= .97; spr[i].s.y *= .97; spr[i].s.z *= .97;
				//spr[i].h.x *= .97; spr[i].h.y *= .97; spr[i].h.z *= .97;
				//spr[i].f.x *= .97; spr[i].f.y *= .97; spr[i].f.z *= .97;
				//if (spr[i].s.x*spr[i].s.x + spr[i].s.y*spr[i].s.y + spr[i].s.z*spr[i].s.z < .1*.1)
				{
					if (spr[i].voxnum)
					{
						j = spr[i].voxnum->vox[0].col;

						playsound(DEBRIS2,70,(float)(rand()-32768)*.00002+1.0,&spr[i].p,KSND_3D);
						explodesprite(&spr[i],.125,0,3);

						//spawndebris(&spr[i].p,1,j,16,0);
						//spawndebris(&spr[i].p,0.5,j,8,0);
						//spawndebris(&spr[i].p,0.25,j,4,0);

						//setkv6(&spr[i],0);

							//Delete temporary sprite data from memory
						free(spr[i].voxnum);
					}
					deletesprite(i);
					continue;
				}
			}

			fpos = spr[i].p;

				//Do velocity & gravity
			spr[i].v.z += fsynctics*64;
			spr[i].p.x += spr[i].v.x * fsynctics;
			spr[i].p.y += spr[i].v.y * fsynctics;
			spr[i].p.z += spr[i].v.z * fsynctics;
			spr[i].v.z += fsynctics*64;

				//Do rotation
			f = min(totclk-spr[i].tim,250)*fsynctics*.01;
			axisrotate(&spr[i].s,&spr[i].r,f);
			axisrotate(&spr[i].h,&spr[i].r,f);
			axisrotate(&spr[i].f,&spr[i].r,f);

				//Make it bounce
			if (!cansee(&fpos,&spr[i].p,&lp))  //Wake up immediately if it hit a wall
			{
				spr[i].p = fpos;
				estnorm(lp.x,lp.y,lp.z,&fp);
				f = (spr[i].v.x*fp.x + spr[i].v.y*fp.y + spr[i].v.z*fp.z)*2.f;
				spr[i].v.x = (spr[i].v.x - fp.x*f)*.75f;
				spr[i].v.y = (spr[i].v.y - fp.y*f)*.75f;
				spr[i].v.z = (spr[i].v.z - fp.z*f)*.75f;
				vecrand(1.0,&spr[i].r);
				//spr[i].r.x = ((float)rand()/16383.5f)-1.f;
				//spr[i].r.y = ((float)rand()/16383.5f)-1.f;
				//spr[i].r.z = ((float)rand()/16383.5f)-1.f;

				if (f > 96)
				{
						//Make it shatter immediately if it hits ground too quickly
					if (spr[i].voxnum)
					{
						j = spr[i].voxnum->vox[0].col;

						playsound(DEBRIS2,70,(float)(rand()-32768)*.00002+1.0,&spr[i].p,KSND_3D);
						explodesprite(&spr[i],.125,0,3);

						//spawndebris(&spr[i].p,f*.5  /96,j,16,0);
						//spawndebris(&spr[i].p,f*.25 /96,j,8,0);
						//spawndebris(&spr[i].p,f*.125/96,j,4,0);

							//Delete temporary sprite data from memory
						free(spr[i].voxnum);
					}
					deletesprite(i);
					continue;
				}
				else
				{
					playsound(DIVEBORD,25,(float)(rand()-16384)*.00001+1.0,&spr[i].p,KSND_3D);
					spr[i].owner++;
				}
			}
		}

			//move monster(s)
		if (((spr[i].voxnum == kv6[CACO]) || (spr[i].voxnum == kv6[WORM])) && (!disablemonsts))
		{
			if ((spr[i].owner < 0) && (spr[i].voxnum == kv6[CACO])) //Caco hurt
			{
				if (totclk > spr[i].tim+250) //Wake up after 0.25 second
					{ spr[i].owner += 65536; continue; }

				fpos = spr[i].p;

					//Do velocity & gravity
				spr[i].v.z += fsynctics*64;
				spr[i].p.x += spr[i].v.x * fsynctics;
				spr[i].p.y += spr[i].v.y * fsynctics;
				spr[i].p.z += spr[i].v.z * fsynctics;
				spr[i].v.z += fsynctics*64;

					//Wake up immediately if it hit something
				//if (isboxempty(spr[i].p.x-16,spr[i].p.y-16,spr[i].p.z-16,
				//               spr[i].p.x+16,spr[i].p.y+16,spr[i].p.z+16))
				if (!issphereempty(spr[i].p.x,spr[i].p.y,spr[i].p.z,16))
					{ spr[i].p = fpos; spr[i].owner += 65536; continue; }

					//Wake up immediately if it hit something
				//if (!cansee(&fpos,&spr[i].p,&lp))
				//   { spr[i].p = fpos; spr[i].owner += 65536; continue; }

				f = (totclk-spr[i].tim)*fsynctics*.01;
				axisrotate(&spr[i].s,&spr[i].r,f);
				axisrotate(&spr[i].h,&spr[i].r,f);
				axisrotate(&spr[i].f,&spr[i].r,f);

				//orthorotate(0,(totclk-spr[i].tim)*fsynctics*.01,0,&spr[i].s,&spr[i].h,&spr[i].f);
				continue;
			}

				//In middle of Worm death animation
			if ((spr[i].owner < 0) && (spr[i].voxnum == kv6[WORM]))
			{
				if (totclk > spr[i].tim+4000)
				{
					spr[i].owner = 100; spr[i].flags &= ~4; //Make sprite visible again
					findrandomspot(&lp.x,&lp.y,&lp.z);
					playsoundupdate(&spr[i].p,(point3d *)0);
					spr[i].p.x = lp.x; spr[i].p.y = lp.y; spr[i].p.z = lp.z-((float)spr[i].voxnum->zsiz-spr[i].voxnum->zpiv)*.5;
					continue;
				}

				fp.x = spr[i].p.x-spr[i].v.x*.1;
				fp.y = spr[i].p.y-spr[i].v.y*.1;
				fp.z = spr[i].p.z-spr[i].v.z*.1;
				spawndebris(&fp,0.5,(rand()&0x3f3f)+0xc03020,1,1);

				if (spr[i].owner == -1) //In death animation, otherwise dead on floor
				{
					fpos = spr[i].p;

						//Do velocity & gravity
					spr[i].v.z += fsynctics*64;
					spr[i].p.x += spr[i].v.x * fsynctics;
					spr[i].p.y += spr[i].v.y * fsynctics;
					spr[i].p.z += spr[i].v.z * fsynctics;
					spr[i].v.z += fsynctics*64;

					if (!cansee(&fpos,&spr[i].p,&lp))
					{
						spr[i].p.x = (float)lp.x;
						spr[i].p.y = (float)lp.y;
						spr[i].p.z = (float)lp.z;
						spr[i].owner = -2;

							//New flatten code idea:
						estnorm(lp.x,lp.y,lp.z,&fp);
							//Suck fp vector out of <spr[i].s.x,spr[i].s.y,spr[i].s.z>
						f = (spr[i].s.x*fp.x + spr[i].s.y*fp.y + spr[i].s.z*fp.z)*.4;
						spr[i].s.x -= fp.x*f; spr[i].s.y -= fp.y*f; spr[i].s.z -= fp.z*f;
						f = (spr[i].h.x*fp.x + spr[i].h.y*fp.y + spr[i].h.z*fp.z)*.4;
						spr[i].h.x -= fp.x*f; spr[i].h.y -= fp.y*f; spr[i].h.z -= fp.z*f;
						f = (spr[i].f.x*fp.x + spr[i].f.y*fp.y + spr[i].f.z*fp.z)*.4;
						spr[i].f.x -= fp.x*f; spr[i].f.y -= fp.y*f; spr[i].f.z -= fp.z*f;

						playsound(DEBRIS2,100,1.0,&spr[i].p,KSND_3D);

							//"freeze" sprite to the map
						vx5.colfunc = kv6colfunc; setkv6(&spr[i],0);
						spr[i].flags |= 4; //Make sprite invisible temporarily
					}
					orthorotate(0,(totclk-spr[i].tim)*fsynctics*.01,0,&spr[i].s,&spr[i].h,&spr[i].f);
				}
				continue;
			}

			fp.x = ipos.x-spr[i].p.x;
			fp.y = ipos.y-spr[i].p.y;
			fp.z = ipos.z-spr[i].p.z;
			f = fp.x*fp.x + fp.y*fp.y + fp.z*fp.z;
			if (f > 80*80)
			{
				if (spr[i].voxnum == kv6[CACO]) f = 52; else f = 96;
				spr[i].v.x = spr[i].h.x*f;
				spr[i].v.y = spr[i].h.y*f;
				spr[i].v.z = spr[i].h.z*f;
				spr[i].p.x += spr[i].v.x*fsynctics;
				spr[i].p.y += spr[i].v.y*fsynctics;
				spr[i].p.z += spr[i].v.z*fsynctics;
			}
			else
				{ spr[i].v.x = 0; spr[i].v.y = 0; spr[i].v.z = 0; }
			fp2.x = 0; fp2.y = 0; fp2.z = 1;
			orthonormalize(&fp,&fp2,&fp3);
			orthonormalize(&spr[i].s,&spr[i].h,&spr[i].f);
			slerp(&spr[i].s,&spr[i].h,&spr[i].f,&fp3,&fp,&fp2,
					&spr[i].s,&spr[i].h,&spr[i].f,fsynctics);
			spr[i].s.x *= .5; spr[i].s.y *= .5; spr[i].s.z *= .5;
			spr[i].h.x *= .5; spr[i].h.y *= .5; spr[i].h.z *= .5;
			spr[i].f.x *= .5; spr[i].f.y *= .5; spr[i].f.z *= .5;

				//Monsters shoot bullets at random times...
			if ((totclk > spr[i].tim) && (numsprites < MAXSPRITES) && (!(rand()&63)))
			{
				fp.x = ipos.x-spr[i].p.x;
				fp.y = ipos.y-spr[i].p.y;
				fp.z = ipos.z-spr[i].p.z;

				j = numsprites; //Allocate sprite
				if (spr[i].voxnum == kv6[CACO]) f = 256; else f = 128; //Caco shoots faster
				if (!aimgrav(fp.x,fp.y,fp.z,&spr[j].v.x,&spr[j].v.y,&spr[j].v.z,128,f))
				{
					numsprites++;

					spr[j].flags = 0;
					spr[j].voxnum = kv6[KGRENADE];
					spr[j].owner = i; spr[j].tim = totclk; spr[j].tag = 0;

					spr[j].p = spr[i].p;
					spr[j].s = spr[i].s; spr[j].h = spr[i].h; spr[j].f = spr[i].f;

						//Make sure monster doesn't shoot too often
					if (spr[i].voxnum == kv6[CACO])
						{ spr[i].tim = totclk+1000; f = .5; }
					else
						{ spr[i].tim = totclk+250; f = .125; }

					spr[j].s.x *= f; spr[j].s.y *= f; spr[j].s.z *= f;
					spr[j].h.x *= f; spr[j].h.y *= f; spr[j].h.z *= f;
					spr[j].f.x *= f; spr[j].f.y *= f; spr[j].f.z *= f;

					playsound(MONSHOOT,100,1.0,&spr[j].p,KSND_MOVE);
				}
			}
			continue;
		}
			//move travelling bullets
		if ((spr[i].voxnum == kv6[KGRENADE]) || (spr[i].voxnum == kv6[SNAKE]))
		{
			fpos = spr[i].p;

				//Do velocity & gravity
			spr[i].v.z += fsynctics*64;
			spr[i].p.x += spr[i].v.x * fsynctics;
			spr[i].p.y += spr[i].v.y * fsynctics;
			spr[i].p.z += spr[i].v.z * fsynctics;
			spr[i].v.z += fsynctics*64;

			orthorotate(0,(1.0-(totclk-spr[i].tim)*.00025)*.3,0,&spr[i].s,&spr[i].h,&spr[i].f);

				//Check if monster's bullet hits player
			if (spr[i].voxnum == kv6[KGRENADE])
			{
				f = (spr[i].p.x-ipos.x)*(spr[i].p.x-ipos.x) +
					 (spr[i].p.y-ipos.y)*(spr[i].p.y-ipos.y) +
					 (spr[i].p.z-ipos.z)*(spr[i].p.z-ipos.z);
				if (f < 16*16)
				{
					k = ((rand()&15)+8);
					fp.x = ipos.x; fp.y = ipos.y; fp.z = ipos.z;
					spawndebris(&fp,0.5,(rand()&0x3f3f)+0xc03020,k,1);
					myhealth -= k;
					if (myhealth < 0)
					{
						playsound(PLAYERDIE,100,1.0,0,0);
						strcpy(message,"You died..."); messagetimeout = totclk+4000;
#if 0                //You die so often that repawning every time is annoying
						do
						{
							findrandomspot(&lp.x,&lp.y,&lp.z);
							ipos.x = lp.x; ipos.y = lp.y; ipos.z = lp.z-CLIPRAD;
						} while (findmaxcr(ipos.x,ipos.y,ipos.z,CLIPRAD) < CLIPRAD*.99);
#endif
						myhealth = 100;
					}
					else playsound(PLAYERHURT,30,(float)(rand()-16384)*.00001+1.0,0,0);
					goto travelbul_blowup;
				}
			}

			for(j=numsprites-1;j>=0;j--)
			{
				f = (spr[i].p.x-spr[j].p.x)*(spr[i].p.x-spr[j].p.x) +
					 (spr[i].p.y-spr[j].p.y)*(spr[i].p.y-spr[j].p.y) +
					 (spr[i].p.z-spr[j].p.z)*(spr[i].p.z-spr[j].p.z);
				if (spr[j].voxnum == kv6[BARREL])
				{
					if (f < 16*16)
					{
						playsound(BLOWUP,100,0.5,&spr[j].p,KSND_3D);
						lp2.x = spr[j].p.x;
						lp2.y = spr[j].p.y;
						lp2.z = spr[j].p.z;
						vx5.colfunc = mycolfunc; /*sphcolfunc;*/ vx5.curcol = 0x80704030;
						setsphere(&lp2,32,-1);
						if (vx5.maxz >= botheimin)
						{
							vx5.colfunc = botcolfunc;
							setheightmap(bothei,32,32,32,vx5.minx,vx5.miny,vx5.maxx,vx5.maxy);
						}

						lastbulpos.x = lp2.x; lastbulpos.y = lp2.y; lastbulpos.z = lp2.z;
						lastbulvel.x = ifor.x; lastbulvel.y = ifor.y; lastbulvel.z = ifor.z;
						//spawndebris(&spr[j].p,2,0xc08060,64,0);
						//spawndebris(&spr[j].p,1.0,0xc08060,32,0);
						//spawndebris(&spr[j].p,0.5,0xc08060,16,0);
						explodesprite(&spr[j],1,0,3);
						findrandomspot(&lp.x,&lp.y,&lp.z);
						spr[j].p.x = lp.x; spr[j].p.y = lp.y; spr[j].p.z = lp.z-((float)spr[i].voxnum->zsiz-spr[i].voxnum->zpiv)*.5;

						spr[i].owner = 0; //Hack so it only does 1 setsphere
						goto travelbul_blowup;
					}
				}
				else if (spr[j].voxnum == kv6[CACO])
				{
					if ((spr[i].owner != j) && (spr[j].owner >= 0) && (f < 20*20))
					{
						if (disablemonsts)
						{
							disablemonsts = 0;
							strcpy(message,"Type \"/monst\" to disable monsters"); messagetimeout = totclk+4000;
						}
						playsound(MONHURT,100,1.0,&spr[j].p,KSND_MOVE);
						spr[j].owner -= 65536; //Put into hurt state
						spr[j].v.x = spr[i].v.x*.5;
						spr[j].v.y = spr[i].v.y*.5;
						spr[j].v.z = spr[i].v.z*.5;
						spr[j].tim = totclk;

						//do
						//{
						//   dp.x = rand()-16384; dp.y = rand()-16384; dp.z = rand()-16384;
						//   f = dp.x*dp.x + dp.y*dp.y + dp.z*dp.z;
						//} while (f < 1.0);
						//f = 1.0/sqrt(f);
						//spr[j].r.x = dp.x*f; spr[j].r.y = dp.y*f; spr[j].r.z = dp.z*f;
						vecrand(1.0,&spr[j].r);

							//Flatten
						//f = sqrt(spr[j].f.x*spr[j].f.x + spr[j].f.y*spr[j].f.y + spr[j].f.z*spr[j].f.z);
						//if (f > 0) { f = .01/f; spr[j].f.x *= f; spr[j].f.y *= f; spr[j].f.z *= f; }

						goto travelbul_blowup;
					}
				}
				else if (spr[j].voxnum == kv6[WORM]) //Worm
				{
					if ((spr[i].owner != j) && (f < 12*12))
					{
							//Decrease worm's health
						k = ((rand()&31)+1);
						//spawndebris(&spr[j].p,0.5,(rand()&0x3f3f)+0xc03020,k,1);
						spr[j].owner -= k;
						if (spr[j].owner <= 0) //Start death animation
						{
							if (spr[i].owner == -1)
							{
								strcpy(message,"You killed the worm."); messagetimeout = totclk+4000;
							}
							else
							{
								strcpy(message,"The monster killed the worm."); messagetimeout = totclk+4000;
							}

							playsound(MONDIE,100,1.0,&spr[j].p,KSND_MOVE);
							if (1) //rand()&1)
							{
								spr[j].v.x *= .5;
								spr[j].v.y *= .5;
								spr[j].v.z *= .5;
								spr[j].owner = -1; spr[j].tim = totclk;
							}
							else
							{
								playsound(DEBRIS2,70,1.0,&spr[j].p,KSND_3D);
								explodesprite(&spr[j],0.125,1,5);
								spr[i].owner = 100; spr[i].flags &= ~4; //Make sprite visible again
								findrandomspot(&lp.x,&lp.y,&lp.z);
								spr[j].p.x = lp.x; spr[j].p.y = lp.y; spr[j].p.z = lp.z-((float)spr[j].voxnum->zsiz-spr[j].voxnum->zpiv)*.5;
							}
						}
						else
						{
							if (disablemonsts)
							{
								disablemonsts = 0;
								strcpy(message,"Type \"/monst\" to disable monsters"); messagetimeout = totclk+4000;
							}
							playsound(MONHURT2,spr[j].owner,1.0,&spr[j].p,KSND_MOVE);
							explodesprite(&spr[j],0.5,0,153);
						}
						goto travelbul_blowup;
					}
				}
			}

			if (totclk > spr[i].tim+4000) //Bullet timed out
			{
				deletesprite(i);
			}
			else if (!cansee(&fpos,&spr[i].p,&lp)) //Bullet hit something
			{
	travelbul_blowup:;
				if (spr[i].owner < 0) //Player's bullet
				{
					lastbulpos = spr[i].p;
					lastbulvel = spr[i].v;

					k = 12;
					if (numdynamite > 0) { numdynamite--; k = 32; }
					if ((k == 12) && (numjoystick > 0)) { numjoystick--; k = 32; }
					if (k == 12)
					{
						fp = spr[i].v;
						playsound(HITWALL,100,1.0,&spr[i].p,KSND_3D);
						deletesprite(i);
						lp2.x = lp.x-16; lp2.y = lp.y-16; lp2.z = lp.z-16;
						for(i=0;i<4;i++)
						{
							if ((numsprites < MAXSPRITES) && (j = meltspans(&spr[numsprites],(vspans *)&rubble[rubblestart[i]],rubblenum[i],&lp2)))
							{
								k = numsprites++;
								spr[k].tag = -17; spr[k].tim = totclk; spr[k].owner = 0;

								for(j=4;j;j--)
								{
									vecrand(48.0,&spr[k].v);
									if ((!isvoxelsolid((long)(spr[k].p.x+spr[k].v.x),
															 (long)(spr[k].p.y+spr[k].v.y),
															 (long)(spr[k].p.z+spr[k].v.z))) &&
										 (!isvoxelsolid((long)(spr[k].p.x+spr[k].v.x*.25),
															 (long)(spr[k].p.y+spr[k].v.y*.25),
															 (long)(spr[k].p.z+spr[k].v.z*.25))))
										break;
								}
								//spr[k].v.x = (((float)rand()/16383.5f)-1.f)*64.0;
								//spr[k].v.y = (((float)rand()/16383.5f)-1.f)*64.0;
								//spr[k].v.z = (((float)rand()/16383.5f)-1.f)*64.0;
								vecrand(1.0,&spr[k].r);
								//spr[k].r.x = ((float)rand()/16383.5f)-1.f;
								//spr[k].r.y = ((float)rand()/16383.5f)-1.f;
								//spr[k].r.z = ((float)rand()/16383.5f)-1.f;
							}
							vx5.colfunc = mycolfunc;
							setspans((vspans *)&rubble[rubblestart[i]],rubblenum[i],&lp2,-1);
						}
					}
					else
					{
						if (j = meltsphere(&spr[i],&lp,k))
						{
							if (k == 12) playsound(HITWALL,100,1.0,&spr[i].p,KSND_3D);
									  else playsound(BLOWUP,100,1.0,&spr[i].p,KSND_3D);

							spr[i].tag = -17; spr[i].tim = totclk; spr[i].owner = 0;
							f = min(256.0/(float)j,1.0);
							spr[i].v.x *= f; spr[i].v.y *= f; spr[i].v.z *= f;
							vecrand(1.0,&spr[i].r);
							//spr[i].r.x = ((float)rand()/16383.5f)-1.f;
							//spr[i].r.y = ((float)rand()/16383.5f)-1.f;
							//spr[i].r.z = ((float)rand()/16383.5f)-1.f;
						}
						else
						{
							deletesprite(i);
						}

						vx5.colfunc = mycolfunc; vx5.curcol = 0x80704030;
	#if 0
						setsphere(&lp,k,-1);
	#else
						tempspr.p.x = lp.x; tempspr.p.y = lp.y; tempspr.p.z = lp.z;
						matrand((float)k*.036,&tempspr.s,&tempspr.h,&tempspr.f);
						tempspr.flags = 0; tempspr.voxnum = kv6[EXPLODE];
						setkv6(&tempspr,-1);
	#endif
						if (vx5.maxz >= botheimin)
						{
							vx5.colfunc = botcolfunc;
							setheightmap(bothei,32,32,32,vx5.minx,vx5.miny,vx5.maxx,vx5.maxy);
						}
					}
				}
				else //Monster's bullet
				{
					deletesprite(i);
				}
			}
			continue;
		}

		if ((spr[i].voxnum == kv6[JOYSTICK]) || (spr[i].voxnum == kv6[TNTBUNDL]))
		{
			orthorotate(.1,0,0,&spr[i].s,&spr[i].h,&spr[i].f);

				//Check if touching power-ups...
			fp.x = ipos.x-spr[i].p.x;
			fp.y = ipos.y-spr[i].p.y;
			fp.z = ipos.z-spr[i].p.z;
			if (fp.x*fp.x + fp.y*fp.y + fp.z*fp.z >= 16*16) continue;

			if (spr[i].voxnum == kv6[JOYSTICK])
			{
				playsound(PICKUP,100,1.0,0,0); numjoystick++;
				strcpy(message,"You found the joystick!"); messagetimeout = totclk+4000;
			}
			if (spr[i].voxnum == kv6[TNTBUNDL])
			{
				playsound(PICKUP,100,1.0,0,0); numdynamite++;
				strcpy(message,"You picked up some dynamite."); messagetimeout = totclk+4000;
			}

			findrandomspot(&lp.x,&lp.y,&lp.z);
			spr[i].p.x = lp.x;
			spr[i].p.y = lp.y;
			spr[i].p.z = lp.z-((float)spr[i].voxnum->zsiz-spr[i].voxnum->zpiv)*0.5;
			continue;
		}
		if ((spr[i].voxnum == kv6[DOOR]) && (spr[i].tim > 0))
		{
			f = ((float)spr[i].tag*PI/180.0);
			if (spr[i].owner) f /= (float)spr[i].owner;
			j = min((long)(fsynctics*1000.0),spr[i].tim);
			orthorotate(j*f,0,0,&spr[i].s,&spr[i].h,&spr[i].f);
			spr[i].tim -= j;
			continue;
		}
		if (spr[i].voxnum == kv6[DESKLAMP])
		{
			if (totclk >= spr2goaltim+1000)
			{
				ospr2goal = spr2goal;
				vecrand(1.0,&spr2goal.s);
				vecrand(1.0,&spr2goal.h);
				//spr2goal.s.x = rand()-16384; spr2goal.s.y = rand()-16384; spr2goal.s.z = rand()-16384;
				//spr2goal.h.x = rand()-16384; spr2goal.h.y = rand()-16384; spr2goal.h.z = rand()-16384;
				orthonormalize(&spr2goal.s,&spr2goal.h,&spr2goal.f);
				spr2goaltim = totclk;
			}
			slerp(&ospr2goal.s,&ospr2goal.h,&ospr2goal.f,
					&spr2goal.s,&spr2goal.h,&spr2goal.f,
					&spr[i].s,&spr[i].h,&spr[i].f,((float)(totclk-spr2goaltim))*.001);
			f = .5;
			spr[i].s.x *= f; spr[i].s.y *= f; spr[i].s.z *= f;
			spr[i].h.x *= f; spr[i].h.y *= f; spr[i].h.z *= f;
			spr[i].f.x *= f; spr[i].f.y *= f; spr[i].f.z *= f;
			continue;
		}

		for(j=NUM0;j<=NUM9;j++)
			if (spr[i].voxnum == kv6[j]) break;
		if (j <= NUM9)
		{
			orthorotate(.05,.05,.05,&spr[i].s,&spr[i].h,&spr[i].f);
			continue;
		}
	}

	updatevxl();
	do
	{
		j = 0;
		startfalls();
		for(i=vx5.flstnum-1;i>=0;i--)
		{
			if (vx5.flstcnt[i].userval == -1) //New piece
			{
				vx5.flstcnt[i].userval2 = totclk+unitfalldelay[0];
				vx5.flstcnt[i].userval = 1;

					//New piece!
				//fp = vx5.flstcnt[i].centroid;
				if ((numsprites < MAXSPRITES) && (meltfall(&spr[numsprites],i,1)))
				{
					k = numsprites++;
					playsound(DEBRIS,vx5.flstcnt[i].mass>>4,(float)(rand()-16384)*.00001+0.5,&spr[k].p,KSND_MOVE);

					spr[k].tag = -17; spr[k].tim = totclk; spr[k].owner = 0;
					spr[k].v.x = 0;
					spr[k].v.y = 0;
					spr[k].v.z = 0;

					spr[k].r.x = (lastbulpos.y-fp.y)*lastbulvel.z - (lastbulpos.z-fp.z)*lastbulvel.y;
					spr[k].r.y = (lastbulpos.z-fp.z)*lastbulvel.x - (lastbulpos.x-fp.x)*lastbulvel.z;
					spr[k].r.z = (lastbulpos.x-fp.x)*lastbulvel.y - (lastbulpos.y-fp.y)*lastbulvel.x;

					//spr[k].r.x = ((float)rand()/16383.5f)-1.f;
					//spr[k].r.y = ((float)rand()/16383.5f)-1.f;
					//spr[k].r.z = ((float)rand()/16383.5f)-1.f;
					continue;
				}
			}
			if (totclk >= vx5.flstcnt[i].userval2) //Timeout: drop piece #i by 1 unit
			{
				//dofall(i);
				vx5.flstcnt[i].userval2 += unitfalldelay[vx5.flstcnt[i].userval++];
				if (totclk >= vx5.flstcnt[i].userval2) j = 1;
			}
		}
		finishfalls();
	} while (j);

	if (keystatus[0x4a]) //KP-
	{
		keystatus[0x4a] = 0;
		volpercent = max(volpercent-10,0);
		sprintf(message,"Volume: %d%%",volpercent);
		quitmessagetimeout = messagetimeout = totclk+4000;
		setvolume(volpercent);
	}
	if (keystatus[0x4e]) //KP-
	{
		keystatus[0x4e] = 0;
		volpercent = min(volpercent+10,100);
		sprintf(message,"Volume: %d%%",volpercent);
		quitmessagetimeout = messagetimeout = totclk+4000;
		setvolume(volpercent);
	}

	if (keystatus[0x9c]) //KP Enter
	{
		keystatus[0x9c] = 0;
		static long macq = 1;
		macq ^= 1; setacquire(macq,1);
	}

	if (keystatus[1]) //ESC
	{
		keystatus[1] = 0;
		if (typemode)
			typemode = 0;
		else
		{
			strcpy(message,"Press Y to quit!");
			quitmessagetimeout = messagetimeout = totclk+4000;
		}
	}

	if (keystatus[0x42]) //F8: change video to random setting!
	{
		validmodetype *validmodelist;
		long validmodecnt;

		keystatus[0x42] = 0;
		validmodecnt = getvalidmodelist(&validmodelist);
		do
		{
			i = (rand()%validmodecnt);
		} while ((validmodelist[i].x > 640) || (validmodelist[i].y > 480) || (validmodelist[i].c != 32));
		changeres(validmodelist[i].x,validmodelist[i].y,validmodelist[i].c,rand()&1);
		sprintf(message,"%d x %d x %d (",xres,yres,colbits);
		if (!fullscreen) strcat(message,"windowed)");
						else strcat(message,"fullscreen)");
		messagetimeout = totclk+4000;
	}

	if ((keystatus[0x15]) && (totclk < quitmessagetimeout) &&
		(!strcmp(message,"Press Y to quit!"))) quitloop(); //'Y'
}

#if 0
//Example of how to use drawpicinquad (this code makes screen fall over)
	static long pic[256*192];
	static float ang = 0;
	clearscreen(0);
	startdirectdraw(&frameptr,&pitch,&xdim,&ydim);
	voxsetframebuffer((long)pic,256*4,256,192);
	setcamera(&ipos,&istr,&ihei,&ifor,256*.5,192*.5,256*.5);
	opticast();
	float rx = 256*.5;
	float ry = 192*.5 - 192*cos(ang);
	float rz = 256*.5 + 192*sin(ang);
	drawpicinquad((long)pic,256*4,256,192,frameptr,pitch,xdim,ydim,
		xdim*.5-rx*xdim*.5/rz,ydim*.5+ry*xdim*.5/rz,
		xdim*.5+rx*xdim*.5/rz,ydim*.5+ry*xdim*.5/rz,xdim,ydim,0,ydim);
	//ang -= .02; if (ang < -0.75) ang = 0;
	ang += .02; if (ang >= 2.35) ang = -.75;
#endif

#if 0
!endif
#endif
