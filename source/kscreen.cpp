#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "voxlap5.h"
#include "kglobals.h"
#include "cpu_detect.h"
#include "porthacks.h"
#include "kfonts.h"
#include "ksnippits.h"

#ifdef __cplusplus
extern "C" {
#endif
	short qbplbpp[4];
	long kv6frameplace, kv6bytesperline;
#ifdef __cplusplus
}
#endif


//------------------------ Simple PNG OUT code begins ------------------------
FILE *pngofil;
long pngoxplc, pngoyplc, pngoxsiz, pngoysiz;
unsigned long pngocrc, pngoadcrc;

long crctab32[256];  //SEE CRC32.C
#define updatecrc32(c,crc) crc=(crctab32[(crc^c)&255]^(((unsigned)crc)>>8))
#define updateadl32(c,crc) \
{  c += (crc&0xffff); if (c   >= 65521) c   -= 65521; \
	crc = (crc>>16)+c; if (crc >= 65521) crc -= 65521; \
	crc = (crc<<16)+c; \
} \

void fputbytes (unsigned long v, long n)
	{ for(;n;v>>=8,n--) { fputc(v,pngofil); updatecrc32(v,pngocrc); } }

void pngoutopenfile (const char *fnam, long xsiz, long ysiz)
{
	long i, j, k;
	char a[40];

	pngoxsiz = xsiz; pngoysiz = ysiz; pngoxplc = pngoyplc = 0;
	for(i=255;i>=0;i--)
	{
		k = i; for(j=8;j;j--) k = ((unsigned long)k>>1)^((-(k&1))&0xedb88320);
		crctab32[i] = k;
	}
	pngofil = fopen(fnam,"wb");
	*(long *)&a[0] = 0x474e5089; *(long *)&a[4] = 0x0a1a0a0d;
	*(long *)&a[8] = 0x0d000000; *(long *)&a[12] = 0x52444849;
	*(long *)&a[16] = bswap(xsiz); *(long *)&a[20] = bswap(ysiz);
	*(long *)&a[24] = 0x00000208; *(long *)&a[28] = 0;
	for(i=12,j=-1;i<29;i++) updatecrc32(a[i],j);
	*(long *)&a[29] = bswap(j^-1);
	fwrite(a,37,1,pngofil);
	pngocrc = 0xffffffff; pngoadcrc = 1;
	fputbytes(0x54414449,4); fputbytes(0x0178,2);
}

void pngoutputpixel (long rgbcol)
{
	long a[4];

	if (!pngoxplc)
	{
		fputbytes(pngoyplc==pngoysiz-1,1);
		fputbytes(((pngoxsiz*3+1)*0x10001)^0xffff0000,4);
		fputbytes(0,1); a[0] = 0; updateadl32(a[0],pngoadcrc);
	}
	fputbytes(bswap(rgbcol<<8),3);
	a[0] = (rgbcol>>16)&255; updateadl32(a[0],pngoadcrc);
	a[0] = (rgbcol>> 8)&255; updateadl32(a[0],pngoadcrc);
	a[0] = (rgbcol    )&255; updateadl32(a[0],pngoadcrc);
	pngoxplc++; if (pngoxplc < pngoxsiz) return;
	pngoxplc = 0; pngoyplc++; if (pngoyplc < pngoysiz) return;
	fputbytes(bswap(pngoadcrc),4);
	a[0] = bswap(pngocrc^-1); a[1] = 0; a[2] = 0x444e4549; a[3] = 0x826042ae;
	fwrite(a,1,16,pngofil);
	a[0] = bswap(ftell(pngofil)-(33+8)-16);
	fseek(pngofil,33,SEEK_SET); fwrite(a,1,4,pngofil);
	fclose(pngofil);
}
//------------------------- Simple PNG OUT code ends -------------------------


/** @note by Ken Silverman: This here for game programmer only. I would never use it! */

/**
 * Draw a pixel on the screen.
 */
void drawpoint2d (long sx, long sy, long col)
{
	if ((unsigned long)sx >= (unsigned long)xres_voxlap) return;
	if ((unsigned long)sy >= (unsigned long)yres_voxlap) return;
	*(long *)(ylookup[sy]+(sx<<2)+frameplace) = col;
}

	//This here for game programmer only. I would never use it!
/**
 * Draw a pixel on the screen. (specified by VXL location) Ignores Z-buffer.
 */
void drawpoint3d (float x0, float y0, float z0, long col)
{
	float ox, oy, oz, r;
	long x, y;

	ox = x0-gipos.x; oy = y0-gipos.y; oz = z0-gipos.z;
	z0 = ox*gifor.x + oy*gifor.y + oz*gifor.z; if (z0 < SCISDIST) return;
	r = 1.0f / z0;
	x0 = (ox*gistr.x + oy*gistr.y + oz*gistr.z)*gihz;
	y0 = (ox*gihei.x + oy*gihei.y + oz*gihei.z)*gihz;

	ftol(x0*r + gihx-.5f,&x); if ((unsigned long)x >= (unsigned long)xres_voxlap) return;
	ftol(y0*r + gihy-.5f,&y); if ((unsigned long)y >= (unsigned long)yres_voxlap) return;
	*(long *)(ylookup[y]+(x<<2)+frameplace) = col;
}

/**
 * Draw a 2d line on the screen
 */
void drawline2d (float x1, float y1, float x2, float y2, long col)
{
	float dx, dy, fxresm1, fyresm1;
	long i, j, incr, ie;

	dx = x2-x1; dy = y2-y1; if ((dx == 0) && (dy == 0)) return;
	fxresm1 = (float)xres_voxlap-.5; fyresm1 = (float)yres_voxlap-.5;
	if (x1 >= fxresm1) { if (x2 >= fxresm1) return; y1 += (fxresm1-x1)*dy/dx; x1 = fxresm1; }
	else if (x1 < 0) { if (x2 < 0) return; y1 += (0-x1)*dy/dx; x1 = 0; }
	if (x2 >= fxresm1) { y2 += (fxresm1-x2)*dy/dx; x2 = fxresm1; }
	else if (x2 < 0) { y2 += (0-x2)*dy/dx; x2 = 0; }
	if (y1 >= fyresm1) { if (y2 >= fyresm1) return; x1 += (fyresm1-y1)*dx/dy; y1 = fyresm1; }
	else if (y1 < 0) { if (y2 < 0) return; x1 += (0-y1)*dx/dy; y1 = 0; }
	if (y2 >= fyresm1) { x2 += (fyresm1-y2)*dx/dy; y2 = fyresm1; }
	else if (y2 < 0) { x2 += (0-y2)*dx/dy; y2 = 0; }

	if (fabs(dx) >= fabs(dy))
	{
		if (x2 > x1) { ftol(x1,&i); ftol(x2,&ie); } else { ftol(x2,&i); ftol(x1,&ie); }
		if (i < 0) i = 0; if (ie >= xres_voxlap) ie = xres_voxlap-1;
		ftol(1048576.0*dy/dx,&incr); ftol(y1*1048576.0+((float)i+.5f-x1)*incr,&j);
		for(;i<=ie;i++,j+=incr)
			if ((unsigned long)(j>>20) < (unsigned long)yres_voxlap)
				*(long *)(ylookup[j>>20]+(i<<2)+frameplace) = col;
	}
	else
	{
		if (y2 > y1) { ftol(y1,&i); ftol(y2,&ie); } else { ftol(y2,&i); ftol(y1,&ie); }
		if (i < 0) i = 0; if (ie >= yres_voxlap) ie = yres_voxlap-1;
		ftol(1048576.0*dx/dy,&incr); ftol(x1*1048576.0+((float)i+.5f-y1)*incr,&j);
		for(;i<=ie;i++,j+=incr)
			if ((unsigned long)(j>>20) < (unsigned long)xres_voxlap)
				*(long *)(ylookup[i]+((j>>18)&~3)+frameplace) = col;
	}
}

#if (USEZBUFFER == 1)
void drawline2dclip (float x1, float y1, float x2, float y2, float rx0, float ry0, float rz0, float rx1, float ry1, float rz1, long col)
{
	float dx, dy, fxresm1, fyresm1, Za, Zb, Zc, z;
	long i, j, incr, ie, p;

	dx = x2-x1; dy = y2-y1; if ((dx == 0) && (dy == 0)) return;
	fxresm1 = (float)xres_voxlap-.5; fyresm1 = (float)yres_voxlap-.5;
	if (x1 >= fxresm1) { if (x2 >= fxresm1) return; y1 += (fxresm1-x1)*dy/dx; x1 = fxresm1; }
	else if (x1 < 0) { if (x2 < 0) return; y1 += (0-x1)*dy/dx; x1 = 0; }
	if (x2 >= fxresm1) { y2 += (fxresm1-x2)*dy/dx; x2 = fxresm1; }
	else if (x2 < 0) { y2 += (0-x2)*dy/dx; x2 = 0; }
	if (y1 >= fyresm1) { if (y2 >= fyresm1) return; x1 += (fyresm1-y1)*dx/dy; y1 = fyresm1; }
	else if (y1 < 0) { if (y2 < 0) return; x1 += (0-y1)*dx/dy; y1 = 0; }
	if (y2 >= fyresm1) { x2 += (fyresm1-y2)*dx/dy; y2 = fyresm1; }
	else if (y2 < 0) { x2 += (0-y2)*dx/dy; y2 = 0; }

	if (fabs(dx) >= fabs(dy))
	{
			//Original equation: (rz1*t+rz0) / (rx1*t+rx0) = gihz/(sx-gihx)
		Za = gihz*(rx0*rz1 - rx1*rz0); Zb = rz1; Zc = -gihx*rz1 - gihz*rx1;

		if (x2 > x1) { ftol(x1,&i); ftol(x2,&ie); } else { ftol(x2,&i); ftol(x1,&ie); }
		if (i < 0) i = 0; if (ie >= xres_voxlap) ie = xres_voxlap-1;
		ftol(1048576.0*dy/dx,&incr); ftol(y1*1048576.0+((float)i+.5f-x1)*incr,&j);
		for(;i<=ie;i++,j+=incr)
			if ((unsigned long)(j>>20) < (unsigned long)yres_voxlap)
			{
				p = ylookup[j>>20]+(i<<2)+frameplace;
				z = Za / ((float)i*Zb + Zc);
				if (*(long *)&z >= *(long *)(p+zbufoff)) continue;
				*(long *)(p+zbufoff) = *(long *)&z;
				*(long *)p = col;
			}
	}
	else
	{
		Za = gihz*(ry0*rz1 - ry1*rz0); Zb = rz1; Zc = -gihy*rz1 - gihz*ry1;

		if (y2 > y1) { ftol(y1,&i); ftol(y2,&ie); } else { ftol(y2,&i); ftol(y1,&ie); }
		if (i < 0) i = 0; if (ie >= yres_voxlap) ie = yres_voxlap-1;
		ftol(1048576.0*dx/dy,&incr); ftol(x1*1048576.0+((float)i+.5f-y1)*incr,&j);
		for(;i<=ie;i++,j+=incr)
			if ((unsigned long)(j>>20) < (unsigned long)xres_voxlap)
			{
				p = ylookup[i]+((j>>18)&~3)+frameplace;
				z = Za / ((float)i*Zb + Zc);
				if (*(long *)&z >= *(long *)(p+zbufoff)) continue;
				*(long *)(p+zbufoff) = *(long *)&z;
				*(long *)p = col;
			}
	}
}
#endif // (USEZBUFFER == 1)

/**
 * Draw a 3d line on the screen (specified by VXL location).
 * Line is automatically Z-buffered into the map.
 * Set alpha of col to non-zero to disable Z-buffering
 */
void drawline3d (float x0, float y0, float z0, float x1, float y1, float z1, long col)
{
	float ox, oy, oz, r;

	ox = x0-gipos.x; oy = y0-gipos.y; oz = z0-gipos.z;
	x0 = ox*gistr.x + oy*gistr.y + oz*gistr.z;
	y0 = ox*gihei.x + oy*gihei.y + oz*gihei.z;
	z0 = ox*gifor.x + oy*gifor.y + oz*gifor.z;

	ox = x1-gipos.x; oy = y1-gipos.y; oz = z1-gipos.z;
	x1 = ox*gistr.x + oy*gistr.y + oz*gistr.z;
	y1 = ox*gihei.x + oy*gihei.y + oz*gihei.z;
	z1 = ox*gifor.x + oy*gifor.y + oz*gifor.z;

	if (z0 < SCISDIST)
	{
		if (z1 < SCISDIST) return;
		r = (SCISDIST-z0)/(z1-z0); z0 = SCISDIST;
		x0 += (x1-x0)*r; y0 += (y1-y0)*r;
	}
	else if (z1 < SCISDIST)
	{
		r = (SCISDIST-z1)/(z1-z0); z1 = SCISDIST;
		x1 += (x1-x0)*r; y1 += (y1-y0)*r;
	}

	ox = gihz/z0;
	oy = gihz/z1;

#if (USEZBUFFER == 1)
	if (!(col&0xff000000))
		drawline2dclip(x0*ox+gihx,y0*ox+gihy,x1*oy+gihx,y1*oy+gihy,x0,y0,z0,x1-x0,y1-y0,z1-z0,col);
	else
		drawline2d(x0*ox+gihx,y0*ox+gihy,x1*oy+gihx,y1*oy+gihy,col&0xffffff);
#else
	drawline2d(x0*ox+gihx,y0*ox+gihy,x1*oy+gihx,y1*oy+gihy,col);
#endif
}

/**
 * Transform & Project a 3D point to a 2D screen coordinate. This could be
 * used for billboard sprites.
 *
 * @param x VXL location to transform & project
 * @param y VXL location to transform & project
 * @param z VXL location to transform & project
 * @param px screen coordinate returned
 * @param py screen coordinate returned
 * @param sx depth of screen coordinate
 * @return 1 if visible else 0
 */
long project2d (float x, float y, float z, float *px, float *py, float *sx)
{
	float ox, oy, oz;

	ox = x-gipos.x; oy = y-gipos.y; oz = z-gipos.z;
	z = ox*gifor.x + oy*gifor.y + oz*gifor.z; if (z < SCISDIST) return(0);

	z = gihz / z;
	*px = (ox*gistr.x + oy*gistr.y + oz*gistr.z)*z + gihx;
	*py = (ox*gihei.x + oy*gihei.y + oz*gihei.z)*z + gihy;
	*sx = z;
	return(1);
}

/**
 * Draw a solid-color filled sphere on screen (useful for particle effects)
 * @param ox center of sphere
 * @param oy center of sphere
 * @param oz center of sphere
 * @param bakrad radius of sphere. NOTE: if bakrad is negative, it uses
 *               Z-buffering with abs(radius), otherwise no Z-buffering
 * @param col 32-bit color of sphere
 */
void drawspherefill (float ox, float oy, float oz, float bakrad, long col)
{
	float a, b, c, d, e, f, g, h, t, cxcx, cycy, Za, Zb, Zc, ysq;
	float r2a, rr2a, nb, nbi, isq, isqi, isqii, cx, cy, cz, rad;
	long sx1, sy1, sx2, sy2, p, sx;

	rad = fabs(bakrad);
#if (USEZBUFFER == 0)
	bakrad = rad;
#endif

	ox -= gipos.x; oy -= gipos.y; oz -= gipos.z;
	cz = ox*gifor.x + oy*gifor.y + oz*gifor.z; if (cz < SCISDIST) return;
	cx = ox*gistr.x + oy*gistr.y + oz*gistr.z;
	cy = ox*gihei.x + oy*gihei.y + oz*gihei.z;

		//3D Sphere projection (see spherast.txt for derivation) (13 multiplies)
	cxcx = cx*cx; cycy = cy*cy; g = rad*rad - cxcx - cycy - cz*cz;
	a = g + cxcx; if (!a) return;
	b = cx*cy; b += b;
	c = g + cycy;
	f = gihx*cx + gihy*cy - gihz*cz;
	d = -cx*f - gihx*g; d += d;
	e = -cy*f - gihy*g; e += e;
	f = f*f + g*(gihx*gihx+gihy*gihy+gihz*gihz);

		//isq = (b*b-4*a*c)yý + (2*b*d-4*a*e)y + (d*d-4*a*f) = 0
	Za = b*b - a*c*4; if (!Za) return;
	Zb = b*d*2 - a*e*4;
	Zc = d*d - a*f*4;
	ysq = Zb*Zb - Za*Zc*4; if (ysq <= 0) return;
	t = sqrt(ysq); //fsqrtasm(&ysq,&t);
	h = .5f / Za;
	ftol((-Zb+t)*h,&sy1); if (sy1 < 0) sy1 = 0;
	ftol((-Zb-t)*h,&sy2); if (sy2 > yres_voxlap) sy2 = yres_voxlap;
	if (sy1 >= sy2) return;
	r2a = .5f / a; rr2a = r2a*r2a;
	nbi = -b*r2a; nb = nbi*(float)sy1-d*r2a;
	h = Za*(float)sy1; isq = ((float)sy1*(h+Zb)+Zc)*rr2a;
	isqi = (h+h+Za+Zb)*rr2a; isqii = Za*rr2a*2;

	p = ylookup[sy1]+frameplace;
	sy2 = ylookup[sy2]+frameplace;
#if (USEZBUFFER == 1)
	if ((*(long *)&bakrad) >= 0)
	{
#endif
		while (1)  //(a)xý + (b*y+d)x + (c*y*y+e*y+f) = 0
		{
			t = sqrt(isq); //fsqrtasm(&isq,&t);
			ftol(nb-t,&sx1); if (sx1 < 0) sx1 = 0;
			ftol(nb+t,&sx2);
			sx2 = MIN(sx2,xres_voxlap)-sx1;
			if (sx2 > 0) clearbuf((void *)((sx1<<2)+p),sx2,col);
			p += bytesperline; if (p >= sy2) return;
			isq += isqi; isqi += isqii; nb += nbi;
		}
#if (USEZBUFFER == 1)
	}
	else
	{     //Use Z-buffering

		if (ofogdist >= 0) //If fog enabled...
		{
			ftol(sqrt(ox*ox + oy*oy),&sx); //Use cylindrical x-y distance for fog
			if (sx > 2047) sx = 2047;
			sx = (long)(*(short *)&foglut[sx]);
			col = ((((( vx5.fogcol     &255)-( col     &255))*sx)>>15)    ) +
					((((((vx5.fogcol>> 8)&255)-((col>> 8)&255))*sx)>>15)<< 8) +
					((((((vx5.fogcol>>16)&255)-((col>>16)&255))*sx)>>15)<<16) + col;
		}

		while (1)  //(a)xý + (b*y+d)x + (c*y*y+e*y+f) = 0
		{
			t = sqrt(isq); //fsqrtasm(&isq,&t);
			ftol(nb-t,&sx1); if (sx1 < 0) sx1 = 0;
			ftol(nb+t,&sx2);
			if (sx2 > xres_voxlap) sx2 = xres_voxlap;
			for(sx=sx1;sx<sx2;sx++)
				if (*(long *)&cz < *(long *)(p+(sx<<2)+zbufoff))
				{
					*(long *)(p+(sx<<2)+zbufoff) = *(long *)&cz;
					*(long *)(p+(sx<<2)) = col;
				}
			sy1++;
			p += bytesperline; if (p >= sy2) return;
			isq += isqi; isqi += isqii; nb += nbi;
		}
	}
#endif
}

/**
 * Draw a texture-mapped quadrilateral to the screen. Drawpicinquad projects
 * the source texture with perspective into the 4 coordinates specified.
 *
 * @param rpic source pointer to top-left corner
 * @param rbpl source pitch (bytes per line)
 * @param rxsiz source dimensions of texture/frame
 * @param rysiz source dimensions of texture/frame
 *
 * @param wpic destination pointer to top-left corner
 * @param wbpl destination pitch (bytes per line)
 * @param wxsiz destination dimensions of texture/frame
 * @param wysiz destination dimensions of texture/frame
 *
 * @param x0 the 4 points of the quadrilateral in the destination texture/frame.
 *           The points must be in loop order.
 * @param x1 the 4 points of the quadrilateral in the destination texture/frame.
 *           The points must be in loop order.
 * @param x2 the 4 points of the quadrilateral in the destination texture/frame.
 *           The points must be in loop order.
 * @param x3 the 4 points of the quadrilateral in the destination texture/frame.
 *           The points must be in loop order.
 *
 * @param y0 the 4 points of the quadrilateral in the destination texture/frame.
 *           The points must be in loop order.
 * @param y1 the 4 points of the quadrilateral in the destination texture/frame.
 *           The points must be in loop order.
 * @param y2 the 4 points of the quadrilateral in the destination texture/frame.
 *           The points must be in loop order.
 * @param y3 the 4 points of the quadrilateral in the destination texture/frame.
 *           The points must be in loop order.
 */
void drawpicinquad (long rpic, long rbpl, long rxsiz, long rysiz,
						  long wpic, long wbpl, long wxsiz, long wysiz,
						  float x0, float y0, float x1, float y1,
						  float x2, float y2, float x3, float y3)
{
	float px[4], py[4], k0, k1, k2, k3, k4, k5, k6, k7, k8;
	float t, u, v, dx, dy, l0, l1, m0, m1, m2, n0, n1, n2, r;
	long i, j, k, l, imin, imax, sx, sxe, sy, sy1, dd, uu, vv, ddi, uui, vvi;
	long x, xi, *p, *pe, uvmax, iu, iv;

	#if defined(__GNUC__) && !defined(NOASM) //only for gcc inline asm
	register lpoint2d reg0 asm("mm0");
	register lpoint2d reg1 asm("mm1");
	register lpoint2d reg2 asm("mm2");
	//register lpoint2d reg3 asm("mm3");
	register lpoint2d reg4 asm("mm4");
	register lpoint2d reg5 asm("mm5");
	register lpoint2d reg6 asm("mm6");
	register lpoint2d reg7 asm("mm7");
	#endif

	px[0] = x0; px[1] = x1; px[2] = x2; px[3] = x3;
	py[0] = y0; py[1] = y1; py[2] = y2; py[3] = y3;

		//This code projects 4 point2D's into a t,u,v screen-projection matrix
		//
		//Derivation: (given 4 known (sx,sy,kt,ku,kv) pairs, solve for k0-k8)
		//   kt = k0*sx + k1*sy + k2
		//   ku = k3*sx + k4*sy + k5
		//   kv = k6*sx + k7*sy + k8
		//0 = (k3*x0 + k4*y0 + k5) / (k0*x0 + k1*y0 + k2) / rxsiz
		//0 = (k6*x0 + k7*y0 + k8) / (k0*x0 + k1*y0 + k2) / rysiz
		//1 = (k3*x1 + k4*y1 + k5) / (k0*x1 + k1*y1 + k2) / rxsiz
		//0 = (k6*x1 + k7*y1 + k8) / (k0*x1 + k1*y1 + k2) / rysiz
		//1 = (k3*x2 + k4*y2 + k5) / (k0*x2 + k1*y2 + k2) / rxsiz
		//1 = (k6*x2 + k7*y2 + k8) / (k0*x2 + k1*y2 + k2) / rysiz
		//0 = (k3*x3 + k4*y3 + k5) / (k0*x3 + k1*y3 + k2) / rxsiz
		//1 = (k6*x3 + k7*y3 + k8) / (k0*x3 + k1*y3 + k2) / rysiz
		//   40*, 28+, 1~, 30W
	k3 = y3 - y0; k4 = x0 - x3; k5 = x3*y0 - x0*y3;
	k6 = y0 - y1; k7 = x1 - x0; k8 = x0*y1 - x1*y0;
	n0 = x2*y3 - x3*y2; n1 = x3*y1 - x1*y3; n2 = x1*y2 - x2*y1;
	l0 = k6*x2 + k7*y2 + k8;
	l1 = k3*x2 + k4*y2 + k5;
	t = n0 + n1 + n2; dx = (float)rxsiz*t*l0; dy = (float)rysiz*t*l1;
	t = l0*l1;
	l0 *= (k3*x1 + k4*y1 + k5);
	l1 *= (k6*x3 + k7*y3 + k8);
	m0 = l1 - t; m1 = l0 - l1; m2 = t - l0;
	k0 = m0*y1 + m1*y2 + m2*y3;
	k1 = -(m0*x1 + m1*x2 + m2*x3);
	k2 = n0*l0 + n1*t + n2*l1;
	k3 *= dx; k4 *= dx; k5 *= dx;
	k6 *= dy; k7 *= dy; k8 *= dy;

		//Make sure k's are in good range for conversion to integers...
	t = fabs(k0);
	if (fabs(k1) > t) t = fabs(k1);
	if (fabs(k2) > t) t = fabs(k2);
	if (fabs(k3) > t) t = fabs(k3);
	if (fabs(k4) > t) t = fabs(k4);
	if (fabs(k5) > t) t = fabs(k5);
	if (fabs(k6) > t) t = fabs(k6);
	if (fabs(k7) > t) t = fabs(k7);
	if (fabs(k8) > t) t = fabs(k8);
	t = -268435456.0 / t;
	k0 *= t; k1 *= t; k2 *= t;
	k3 *= t; k4 *= t; k5 *= t;
	k6 *= t; k7 *= t; k8 *= t;
	ftol(k0,&ddi);

	imin = 0; imax = 0;
	for(i=1;i<4;i++)
	{
		if (py[i] < py[imin]) imin = i;
		if (py[i] > py[imax]) imax = i;
	}

	uvmax = (rysiz-1)*rbpl + (rxsiz<<2);

	i = imax;
	do
	{
		j = ((i+1)&3);
			//offset would normally be -.5, but need to bias by +1.0
		ftol(py[j]+.5,&sy); if (sy < 0) sy = 0;
		ftol(py[i]+.5,&sy1); if (sy1 > wysiz) sy1 = wysiz;
		if (sy1 > sy)
		{
			ftol((px[i]-px[j])*4096.0/(py[i]-py[j]),&xi);
			ftol(((float)sy-py[j])*(float)xi + px[j]*4096.0 + 4096.0,&x);
			for(;sy<sy1;sy++,x+=xi) lastx[sy] = (x>>12);
		}
		i = j;
	} while (i != imin);
	do
	{
		j = ((i+1)&3);
			//offset would normally be -.5, but need to bias by +1.0
		ftol(py[i]+.5,&sy); if (sy < 0) sy = 0;
		ftol(py[j]+.5,&sy1); if (sy1 > wysiz) sy1 = wysiz;
		if (sy1 > sy)
		{
			ftol((px[j]-px[i])*4096.0/(py[j]-py[i]),&xi);
			ftol(((float)sy-py[i])*(float)xi + px[i]*4096.0 + 4096.0,&x);
			for(;sy<sy1;sy++,x+=xi)
			{
				sx = lastx[sy]; if (sx < 0) sx = 0;
				sxe = (x>>12); if (sxe > wxsiz) sxe = wxsiz;
				if (sx >= sxe) continue;
				t = k0*(float)sx + k1*(float)sy + k2; r = 1.0 / t;
				u = k3*(float)sx + k4*(float)sy + k5; ftol(u*r-.5,&iu);
				v = k6*(float)sx + k7*(float)sy + k8; ftol(v*r-.5,&iv);
				ftol(t,&dd);
				ftol((float)iu*k0 - k3,&uui); ftol((float)iu*t - u,&uu);
				ftol((float)iv*k0 - k6,&vvi); ftol((float)iv*t - v,&vv);
				if (k3*t < u*k0) k =    -4; else { uui = -(uui+ddi); uu = -(uu+dd); k =    4; }
				if (k6*t < v*k0) l = -rbpl; else { vvi = -(vvi+ddi); vv = -(vv+dd); l = rbpl; }
				iu = iv*rbpl + (iu<<2);
				p  = (long *)(sy*wbpl+(sx<<2)+wpic);
				pe = (long *)(sy*wbpl+(sxe<<2)+wpic);
				do
				{
					if ((unsigned long)iu < uvmax) p[0] = *(long *)(rpic+iu);
					dd += ddi;
					uu += uui; while (uu < 0) { iu += k; uui -= ddi; uu -= dd; }
					vv += vvi; while (vv < 0) { iu += l; vvi -= ddi; vv -= dd; }
					p++;
				} while (p < pe);
			}
		}
		i = j;
	} while (i != imax);
}


/** Draws 4x6 font on screen (very fast!)
 *
 *  @param x x of top-left corner
 *  @param y y of top-left corner
 *  @param fcol foreground color (32-bit RGB format)
 *  @param bcol background color (32-bit RGB format) or -1 for transparent
 *  @param fmt string - same syntax as printf
 */
void print4x6 (long x, long y, long fcol, long bcol, const char *fmt, ...)
{
	va_list arglist;
	char st[280], *c;
	long i, j;

	if (!fmt) return;
	va_start(arglist,fmt);
	vsprintf(st,fmt,arglist);
	va_end(arglist);

	y = y*bytesperline+(x<<2)+frameplace;
	if (bcol < 0)
	{
		for(j=20;j>=0;y+=bytesperline,j-=4)
			for(c=st,x=y;*c;c++,x+=16)
			{
				i = (font4x6[*c]>>j);
				if (i&8) *(long *)(x   ) = fcol;
				if (i&4) *(long *)(x+ 4) = fcol;
				if (i&2) *(long *)(x+ 8) = fcol;
				if (i&1) *(long *)(x+12) = fcol;
				if ((*c) == 9) x += 32;
			}
		return;
	}
	fcol -= bcol;
	for(j=20;j>=0;y+=bytesperline,j-=4)
		for(c=st,x=y;*c;c++,x+=16)
		{
			i = (font4x6[*c]>>j);
			*(long *)(x   ) = (((i<<28)>>31)&fcol)+bcol;
			*(long *)(x+ 4) = (((i<<29)>>31)&fcol)+bcol;
			*(long *)(x+ 8) = (((i<<30)>>31)&fcol)+bcol;
			*(long *)(x+12) = (((i<<31)>>31)&fcol)+bcol;
			if ((*c) == 9) { for(i=16;i<48;i+=4) *(long *)(x+i) = bcol; x += 32; }
		}
}

/** Draws 6x8 font on screen (very fast!)
 *
 *  @param x x of top-left corner
 *  @param y y of top-left corner
 *  @param fcol foreground color (32-bit RGB format)
 *  @param bcol background color (32-bit RGB format) or -1 for transparent
 *  @param fmt string - same syntax as printf
 */
void print6x8 (long x, long y, long fcol, long bcol, const char *fmt, ...)
{
	va_list arglist;
	char st[280], *c, *v;
	long i, j;

	if (!fmt) return;
	va_start(arglist,fmt);
	vsprintf(st,fmt,arglist);
	va_end(arglist);

	y = y*bytesperline+(x<<2)+frameplace;
	if (bcol < 0)
	{
		for(j=1;j<256;y+=bytesperline,j<<=1)
			for(c=st,x=y;*c;c++,x+=24)
			{
				v = (char *)(((long)font6x8) + ((long)c[0])*6);
				if (v[0]&j) *(long *)(x   ) = fcol;
				if (v[1]&j) *(long *)(x+ 4) = fcol;
				if (v[2]&j) *(long *)(x+ 8) = fcol;
				if (v[3]&j) *(long *)(x+12) = fcol;
				if (v[4]&j) *(long *)(x+16) = fcol;
				if (v[5]&j) *(long *)(x+20) = fcol;
				if ((*c) == 9) x += ((2*6)<<2);
			}
		return;
	}
	fcol -= bcol;
	for(j=1;j<256;y+=bytesperline,j<<=1)
		for(c=st,x=y;*c;c++,x+=24)
		{
			v = (char *)(((long)font6x8) + ((long)c[0])*6);
			*(long *)(x   ) = (((-(v[0]&j))>>31)&fcol)+bcol;
			*(long *)(x+ 4) = (((-(v[1]&j))>>31)&fcol)+bcol;
			*(long *)(x+ 8) = (((-(v[2]&j))>>31)&fcol)+bcol;
			*(long *)(x+12) = (((-(v[3]&j))>>31)&fcol)+bcol;
			*(long *)(x+16) = (((-(v[4]&j))>>31)&fcol)+bcol;
			*(long *)(x+20) = (((-(v[5]&j))>>31)&fcol)+bcol;
			if ((*c) == 9) { for(i=24;i<72;i+=4) *(long *)(x+i) = bcol; x += ((2*6)<<2); }
		}
}

/**
 * Draws a 32-bit color texture from memory to the screen. This is the
 * low-level function used to draw text loaded from a PNG,JPG,TGA,GIF.
 *
 * @param tf pointer to top-left corner of SOURCE picture
 * @param tp pitch (bytes per line) of the SOURCE picture
 * @param tx dimensions of the SOURCE picture
 * @param ty dimensions of the SOURCE picture
 * @param tcx texel (<<16) at (sx,sy). Set this to (0,0) if you want (sx,sy)
 *            to be the top-left corner of the destination
 * @param tcy texel (<<16) at (sx,sy). Set this to (0,0) if you want (sx,sy)
 *            to be the top-left corner of the destination
 * @param sx screen coordinate (matches the texture at tcx,tcy)
 * @param sy screen coordinate (matches the texture at tcx,tcy)
 * @param xz x&y zoom, all (<<16). Use (65536,65536) for no zoom change
 * @param yz x&y zoom, all (<<16). Use (65536,65536) for no zoom change
 * @param black shade scale (ARGB format). For no effects, use (0,-1)
 *              NOTE: if alphas of black&white are same, then alpha channel ignored
 * @param white shade scale (ARGB format). For no effects, use (0,-1)
 *              NOTE: if alphas of black&white are same, then alpha channel ignored
 */
void drawtile (long tf, long tp, long tx, long ty, long tcx, long tcy,
					long sx, long sy, long xz, long yz, long black, long white)
{
	long sx0, sy0, sx1, sy1, x0, y0, x1, y1, x, y, u, v, ui, vi, uu, vv;
	long p, i, j, a;

	#if defined(__GNUC__) && !defined(NOASM) //only for gcc inline asm
	register lpoint2d reg0 asm("mm0");
	register lpoint2d reg1 asm("mm1");
	register lpoint2d reg2 asm("mm2");
	//register lpoint2d reg3 asm("mm3"); /** @warning Is this commented out on purpose */
	register lpoint2d reg4 asm("mm4");
	register lpoint2d reg5 asm("mm5");
	register lpoint2d reg6 asm("mm6");
	register lpoint2d reg7 asm("mm7");
	#endif

	if (!tf) return;
	sx0 = sx - mulshr16(tcx,xz); sx1 = sx0 + xz*tx;
	sy0 = sy - mulshr16(tcy,yz); sy1 = sy0 + yz*ty;
	x0 = MAX((sx0+65535)>>16,0); x1 = MIN((sx1+65535)>>16,xres_voxlap);
	y0 = MAX((sy0+65535)>>16,0); y1 = MIN((sy1+65535)>>16,yres_voxlap);
	ui = shldiv16(65536,xz); u = mulshr16(-sx0,ui);
	vi = shldiv16(65536,yz); v = mulshr16(-sy0,vi);
	if (!((black^white)&0xff000000)) //Ignore alpha
	{
		for(y=y0,vv=y*vi+v;y<y1;y++,vv+=vi)
		{
			p = ylookup[y] + frameplace; j = (vv>>16)*tp + tf;
			for(x=x0,uu=x*ui+u;x<x1;x++,uu+=ui)
			*(long *)((x<<2)+p) = *(long *)(((uu>>16)<<2) + j);
		}
	}
	else //Use alpha for masking
	{
		//Init for black/white code
		#if defined(__GNUC__) && !defined(NOASM) //gcc inline asm
		__asm__ __volatile__
		(
			"pxor	%[y7], %[y7]\n"
			"movd	%[w], %[y5]\n"
			"movd	%[b], %[y4]\n"
			"punpcklbw	%[y7], %[y5]\n"   //mm5: [00Wa00Wr00Wg00Wb]
			"punpcklbw	%[y7], %[y4]\n"   //mm4: [00Ba00Br00Bg00Bb]
			"psubw	%[y4], %[y5]\n"       //mm5: each word range: -255 to 255
			"movq	%[y5], %[y0]\n"       //if (? == -255) ? = -256;
			"movq	%[y5], %[y1]\n"       //if (? ==  255) ? =  256;
			"pcmpeqw	%c[skp], %[y0]\n" //if (mm0.w[#] == 0x00ff) mm0.w[#] = 0xffff
			"pcmpeqw	%c[skn], %[y1]\n" //if (mm1.w[#] == 0xff01) mm1.w[#] = 0xffff
			"psubw	%[y5], %[y0]\n"
			"paddw	%[y5], %[y1]\n"
			"psllw	$4, %[y5]\n"          //mm5: [-WBa-WBr-WBg-WBb]
			"movq	%c[mask], %[y6]\n"
			: [y0] "=y" (reg0), [y1] "=y" (reg1),
			  [y4] "=y" (reg4), [y5] "=y" (reg5),
			  [y6] "=y" (reg6), [y7] "=y" (reg7)
			: [w] "g" (white), [skp] "p" (&mskp255),
			  [b] "g" (black), [skn] "p" (&mskn255),
			  [mask] "p" (&rgbmask64)
			:
		);
		#elif defined(_MSC_VER) && !defined(NOASM) //msvc inline asm
		_asm
		{
			pxor	mm7, mm7
			movd	mm5, white
			movd	mm4, black
			punpcklbw	mm5, mm7 //mm5: [00Wa00Wr00Wg00Wb]
			punpcklbw	mm4, mm7 //mm4: [00Ba00Br00Bg00Bb]
			psubw	mm5, mm4     //mm5: each word range: -255 to 255
			movq	mm0, mm5     //if (? == -255) ? = -256;
			movq	mm1, mm5     //if (? ==  255) ? =  256;
			pcmpeqw	mm0, mskp255 //if (mm0.w[#] == 0x00ff) mm0.w[#] = 0xffff
			pcmpeqw	mm1, mskn255 //if (mm1.w[#] == 0xff01) mm1.w[#] = 0xffff
			psubw	mm5, mm0
			paddw	mm5, mm1
			psllw	mm5, 4       //mm5: [-WBa-WBr-WBg-WBb]
			movq	mm6, rgbmask64
		}
		#else
		#error NO C Equivalent defined
		#endif //no C equivalent here
		for(y=y0,vv=y*vi+v;y<y1;y++,vv+=vi)
		{
			p = ylookup[y] + frameplace; j = (vv>>16)*tp + tf;
			for(x=x0,uu=x*ui+u;x<x1;x++,uu+=ui)
			{
				i = *(long *)(((uu>>16)<<2) + j);

				#if defined(__GNUC__) && !defined(NOASM) //gcc inline asm
				__asm__ __volatile__
				(
					"punpcklbw	%[y7], %[y0]\n" //mm1: [00Aa00Rr00Gg00Bb]
					"psllw	$4, %[y0]\n"        //mm1: [0Aa00Rr00Gg00Bb0]
					"pmulhw	%[y5], %[y0]\n"     //mm1: [--Aa--Rr--Gg--Bb]
					"paddw	%[y4], %[y0]\n"     //mm1: [00Aa00Rr00Gg00Bb]
					"movq	%[y0], %[y1]\n"
					"packuswb	%[y0], %[y0]\n"  //mm1: [AaRrGgBbAaRrGgBb]
					: [y0] "+y" (i),    [y1] "+y" (reg1), [y7] "+y" (reg7),
					  [y4] "+y" (reg4), [y5] "+y" (reg5)
					:
					:
				);
				#elif defined(_MSC_VER) && !defined(NOASM)
				_asm
				{
					movd	mm0, i       //mm1: [00000000AaRrGgBb]
					punpcklbw	mm0, mm7 //mm1: [00Aa00Rr00Gg00Bb]
					psllw	mm0, 4       //mm1: [0Aa00Rr00Gg00Bb0]
					pmulhw	mm0, mm5     //mm1: [--Aa--Rr--Gg--Bb]
					paddw	mm0, mm4     //mm1: [00Aa00Rr00Gg00Bb]
					movq	mm1, mm0
					packuswb	mm0, mm0 //mm1: [AaRrGgBbAaRrGgBb]
					movd	i, mm0
				}
				#else // C Default
				((uint8_t *)&i)[0] = ((uint8_t *)&i)[0] * (((uint8_t *)&white)[0] - ((uint8_t *)&black)[0])/256 + ((uint8_t *)&black)[0];
				((uint8_t *)&i)[1] = ((uint8_t *)&i)[1] * (((uint8_t *)&white)[1] - ((uint8_t *)&black)[1])/256 + ((uint8_t *)&black)[1];
				((uint8_t *)&i)[2] = ((uint8_t *)&i)[2] * (((uint8_t *)&white)[2] - ((uint8_t *)&black)[2])/256 + ((uint8_t *)&black)[2];
				((uint8_t *)&i)[3] = ((uint8_t *)&i)[3] * (((uint8_t *)&white)[3] - ((uint8_t *)&black)[3])/256 + ((uint8_t *)&black)[3];
				#endif

					//a = (((unsigned long)i)>>24);
					//if (!a) continue;
					//if (a == 255) { *(long *)((x<<2)+p) = i; continue; }
				if ((unsigned long)(i+0x1000000) < 0x2000000)
				{
					if (i < 0) *(long *)((x<<2)+p) = i;
					continue;
				}
				#if defined(__GNUC__) && !defined(NOASM) //gcc inline asm
				__asm__ __volatile__
				(
					//mm0 = (mm1-mm0)*a + mm0
					"lea	(%[d],%[a],4), %[a]\n"
					"movd	(%[a]), %[y0]\n"       //mm0: [00000000AaRrGgBb]
					//"movd	mm1, i\n"                //mm1: [00000000AaRrGgBb]
					"pand	%[y6], %[y0]\n"        //zero alpha from screen pixel
					"punpcklbw	%[y7], %[y0]\n"    //mm0: [00Aa00Rr00Gg00Bb]
					//"punpcklbw	mm1, mm7\n"      //mm1: [00Aa00Rr00Gg00Bb]
					"psubw	%[y0], %[y1]\n"        //mm1: [--Aa--Rr--Gg--Bb] range:+-255
					"psllw	$4, %[y1]\n"           //mm1: [-Aa0-Rr0-Gg0-Bb0]
					"pshufw	$0xff, %[y1], %[y2]\n" //mm2: [-Aa0-Aa0-Aa0-Aa0]
					"pmulhw	%[y2], %[y1]\n"
					//"mov	edx, a\n"                //alphalookup[i] = i*0x001000100010;
					//"pmulhw	mm1, alphalookup[edx*8]\n"
					"paddw	%[y1], %[y0]\n"
					"packuswb	%[y0], %[y0]\n"
					"movd	%[y0], (%[a])\n"
					: [y0] "+y" (reg0), [y1] "+y" (reg1),
					  [y2] "+y" (reg2),
					  [y6] "+y" (reg6), [y7] "+y" (reg7)
					: [a] "r" (x), [d] "r" (p)
					:
				);
				#elif defined(_MSC_VER) && !defined(NOASM)
				_asm
				{
					mov eax, x            //mm0 = (mm1-mm0)*a + mm0
					mov edx, p
					lea eax, [eax*4+edx]
					movd mm0, [eax]       //mm0: [00000000AaRrGgBb]
					//movd mm1, i           //mm1: [00000000AaRrGgBb]
					pand mm0, mm6         //zero alpha from screen pixel
					punpcklbw mm0, mm7    //mm0: [00Aa00Rr00Gg00Bb]
					//punpcklbw mm1, mm7    //mm1: [00Aa00Rr00Gg00Bb]
					psubw mm1, mm0        //mm1: [--Aa--Rr--Gg--Bb] range:+-255
					psllw mm1, 4          //mm1: [-Aa0-Rr0-Gg0-Bb0]
					pshufw mm2, mm1, 0xff //mm2: [-Aa0-Aa0-Aa0-Aa0]
					pmulhw mm1, mm2
					//mov edx, a            //alphalookup[i] = i*0x001000100010;
					//pmulhw mm1, alphalookup[edx*8]
					paddw mm0, mm1
					packuswb mm0, mm0
					movd [eax], mm0
				}
				#else // C Default
				#error No C Default yet defined
				#endif
			}
		}
		clearMMX();
	}
}


/** Captures a screenshot of the current frame to disk.
 *  The current frame is defined by the last call to the voxsetframebuffer function.
 *  @note You <b>MUST</b> call this function while video memory is accessible. In
 *  DirectX, that means it MUST only be between a call to startdirectdraw and
 *  stopdirectdraw.
 *  @param fname filename to write to (writes uncompressed .PNG format)
 *  @return 0:always
 */
long screencapture32bit (const char *fname)
{
	long p, x, y;

	pngoutopenfile(fname,xres_voxlap,yres_voxlap);
	p = frameplace;
	for(y=0;y<yres_voxlap;y++,p+=bytesperline)
		for(x=0;x<xres_voxlap;x++)
			pngoutputpixel(*(long *)(p+(x<<2)));

	return(0);
}

/** Generates a cubic panorama (skybox) from the given position.
 *  This is an old function that is very slow, but it is pretty cool
 *  being able to view a full panorama screenshot. Unfortunately, it
 *  doesn't draw sprites or the sky.
 *
 *  @param pos VXL map position of camera
 *  @param fname filename to write to (writes uncompressed .PNG format)
 *  @param boxsiz length of side of square. I recommend using 256 or 512 for this.
 *  @return 0:always
 */
long surroundcapture32bit (dpoint3d *pos, const char *fname, long boxsiz)
{
	lpoint3d hit;
	dpoint3d d;
	long x, y, hboxsiz, *hind, hdir;
	float f;

	//Picture layout:
	//   ÛÛÛÛÛÛúúúú
	//   úúúúÛÛÛÛÛÛ

	f = 2.0 / (float)boxsiz; hboxsiz = (boxsiz>>1);
	pngoutopenfile(fname,boxsiz*5,boxsiz*2);
	for(y=-hboxsiz;y<hboxsiz;y++)
	{
		for(x=-hboxsiz;x<hboxsiz;x++) //(1,1,-1) - (-1,1,1)
		{
			d.x = -(x+.5)*f; d.y = 1; d.z = (y+.5)*f;
			hitscan(pos,&d,&hit,&hind,&hdir);
			if (hind) pngoutputpixel(lightvox(*hind)); else pngoutputpixel(0);
		}
		for(x=-hboxsiz;x<hboxsiz;x++) //(-1,1,-1) - (-1,-1,1)
		{
			d.x = -1; d.y = -(x+.5)*f; d.z = (y+.5)*f;
			hitscan(pos,&d,&hit,&hind,&hdir);
			if (hind) pngoutputpixel(lightvox(*hind)); else pngoutputpixel(0);
		}
		for(x=-hboxsiz;x<hboxsiz;x++) //(-1,-1,-1) - (1,-1,1)
		{
			d.x = (x+.5)*f; d.y = -1; d.z = (y+.5)*f;
			hitscan(pos,&d,&hit,&hind,&hdir);
			if (hind) pngoutputpixel(lightvox(*hind)); else pngoutputpixel(0);
		}
		for(x=(boxsiz<<1);x>0;x--) pngoutputpixel(0);
	}
	for(y=-hboxsiz;y<hboxsiz;y++)
	{
		for(x=(boxsiz<<1);x>0;x--) pngoutputpixel(0);
		for(x=-hboxsiz;x<hboxsiz;x++) //(-1,-1,1) - (1,1,1)
		{
			d.x = (x+.5)*f; d.y = (y+.5)*f; d.z = 1;
			hitscan(pos,&d,&hit,&hind,&hdir);
			if (hind) pngoutputpixel(lightvox(*hind)); else pngoutputpixel(0);
		}
		for(x=-hboxsiz;x<hboxsiz;x++) //(1,-1,1) - (1,1,-1)
		{
			d.x = 1; d.y = (y+.5)*f; d.z = -(x+.5)*f;
			hitscan(pos,&d,&hit,&hind,&hdir);
			if (hind) pngoutputpixel(lightvox(*hind)); else pngoutputpixel(0);
		}
		for(x=-hboxsiz;x<hboxsiz;x++) //(1,-1,-1) - (-1,1,-1)
		{
			d.x = -(x+.5)*f; d.y = (y+.5)*f; d.z = -1;
			hitscan(pos,&d,&hit,&hind,&hdir);
			if (hind) pngoutputpixel(lightvox(*hind)); else pngoutputpixel(0);
		}
	}
	return(0);
}
