#ifndef KVOXEL_MODELLING_H
#define KVOXEL_MODELLING_H
/** This is a subset of functionality that is more useful to editing than to games
 *  and hence could be left out of builds which do not require these features.
*/

// ------------------------ CONVEX 3D HULL CODE BEGINS ------------------------

#define MAXPOINTS (256 *2) //Leave the *2 here for safety!
point3d nm[MAXPOINTS*2+2];
float nmc[MAXPOINTS*2+2];
long tri[MAXPOINTS*8+8], lnk[MAXPOINTS*8+8], tricnt;
char umost[VSID*VSID], dmost[VSID*VSID];

void initetrasid (point3d *pt, long z)
{
	long i, j, k;
	float x0, y0, z0, x1, y1, z1;

	i = tri[z*4]; j = tri[z*4+1]; k = tri[z*4+2];
	x0 = pt[i].x-pt[k].x; y0 = pt[i].y-pt[k].y; z0 = pt[i].z-pt[k].z;
	x1 = pt[j].x-pt[k].x; y1 = pt[j].y-pt[k].y; z1 = pt[j].z-pt[k].z;
	nm[z].x = y0*z1 - z0*y1;
	nm[z].y = z0*x1 - x0*z1;
	nm[z].z = x0*y1 - y0*x1;
	nmc[z] = nm[z].x*pt[k].x + nm[z].y*pt[k].y + nm[z].z*pt[k].z;
}

void inithull3d (point3d *pt, long nump)
{
	float px, py, pz;
	long i, k, s, z, szz, zz, zx, snzz, nzz, zzz, otricnt;

	tri[0] = 0; tri[4] = 0; tri[8] = 0; tri[12] = 1;
	tri[1] = 1; tri[2] = 2; initetrasid(pt,0);
	if (nm[0].x*pt[3].x + nm[0].y*pt[3].y + nm[0].z*pt[3].z >= nmc[0])
	{
		tri[1] = 1; tri[2] = 2; lnk[0] = 10; lnk[1] = 14; lnk[2] = 4;
		tri[5] = 2; tri[6] = 3; lnk[4] = 2; lnk[5] = 13; lnk[6] = 8;
		tri[9] = 3; tri[10] = 1; lnk[8] = 6; lnk[9] = 12; lnk[10] = 0;
		tri[13] = 3; tri[14] = 2; lnk[12] = 9; lnk[13] = 5; lnk[14] = 1;
	}
	else
	{
		tri[1] = 2; tri[2] = 1; lnk[0] = 6; lnk[1] = 12; lnk[2] = 8;
		tri[5] = 3; tri[6] = 2; lnk[4] = 10; lnk[5] = 13; lnk[6] = 0;
		tri[9] = 1; tri[10] = 3; lnk[8] = 2; lnk[9] = 14; lnk[10] = 4;
		tri[13] = 2; tri[14] = 3; lnk[12] = 1; lnk[13] = 5; lnk[14] = 9;
	}
	tricnt = 4*4;

	for(z=0;z<4;z++) initetrasid(pt,z);

	for(z=4;z<nump;z++)
	{
		px = pt[z].x; py = pt[z].y; pz = pt[z].z;
		for(zz=tricnt-4;zz>=0;zz-=4)
		{
			i = (zz>>2);
			if (nm[i].x*px + nm[i].y*py + nm[i].z*pz >= nmc[i]) continue;

			s = 0;
			for(zx=2;zx>=0;zx--)
			{
				i = (lnk[zz+zx]>>2);
				s += (nm[i].x*px + nm[i].y*py + nm[i].z*pz < nmc[i]) + s;
			}
			if (s == 7) continue;

			nzz = ((0x4a4>>(s+s))&3); szz = zz; otricnt = tricnt;
			do
			{
				snzz = nzz;
				do
				{
					zzz = nzz+1; if (zzz >= 3) zzz = 0;

						//Insert triangle tricnt: (p0,p1,z)
					tri[tricnt+0] = tri[zz+nzz];
					tri[tricnt+1] = tri[zz+zzz];
					tri[tricnt+2] = z;
					initetrasid(pt,tricnt>>2);
					k = lnk[zz+nzz]; lnk[tricnt] = k; lnk[k] = tricnt;
					lnk[tricnt+1] = tricnt+6;
					lnk[tricnt+2] = tricnt-3;
					tricnt += 4;

						//watch out for loop inside single triangle
					if (zzz == snzz) goto endit;
					nzz = zzz;
				} while (!(s&(1<<zzz)));
				do
				{
					i = zz+nzz;
					zz = (lnk[i]&~3);
					nzz = (lnk[i]&3)+1; if (nzz == 3) nzz = 0;
					s = 0;
					for(zx=2;zx>=0;zx--)
					{
						i = (lnk[zz+zx]>>2);
						s += (nm[i].x*px + nm[i].y*py + nm[i].z*pz < nmc[i]) + s;
					}
				} while (s&(1<<nzz));
			} while (zz != szz);
endit:;  lnk[tricnt-3] = otricnt+2; lnk[otricnt+2] = tricnt-3;

			for(zz=otricnt-4;zz>=0;zz-=4)
			{
				i = (zz>>2);
				if (nm[i].x*px + nm[i].y*py + nm[i].z*pz < nmc[i])
				{
					tricnt -= 4; //Delete triangle zz%
					nm[i] = nm[tricnt>>2]; nmc[i] = nmc[tricnt>>2];
					for(i=0;i<3;i++)
					{
						tri[zz+i] = tri[tricnt+i];
						lnk[zz+i] = lnk[tricnt+i];
						lnk[lnk[zz+i]] = zz+i;
					}
				}
			}
			break;
		}
	}
	tricnt >>= 2;
}

static long incmod3[3];
void tmaphulltrisortho (point3d *pt)
{
	point3d *i0, *i1;
	float r, knmx, knmy, knmc, xinc;
	long i, k, op, p, pe, y, yi, z, zi, sy, sy1, itop, ibot, damost;

	for(k=0;k<tricnt;k++)
	{
		if (nm[k].z >= 0)
			{ damost = (long)umost; incmod3[0] = 1; incmod3[1] = 2; incmod3[2] = 0; }
		else
			{ damost = (long)dmost; incmod3[0] = 2; incmod3[1] = 0; incmod3[2] = 1; }

		itop = (pt[tri[(k<<2)+1]].y < pt[tri[k<<2]].y); ibot = 1-itop;
			  if (pt[tri[(k<<2)+2]].y < pt[tri[(k<<2)+itop]].y) itop = 2;
		else if (pt[tri[(k<<2)+2]].y > pt[tri[(k<<2)+ibot]].y) ibot = 2;

			//Pre-calculations
		if (fabs(nm[k].z) < .000001) r = 0; else r = -65536.0 / nm[k].z;
		knmx = nm[k].x*r; knmy = nm[k].y*r;
		//knmc = 65536.0-nmc[k]*r-knmx-knmy;
		//knmc = -nmc[k]*r-(knmx+knmy)*.5f;
		knmc = /*65536.0*/  -nmc[k]*r+knmx;
		ftol(knmx,&zi);

		i = ibot;
		do
		{
			i1 = &pt[tri[(k<<2)+i]]; ftol(i1->y,&sy1); i = incmod3[i];
			i0 = &pt[tri[(k<<2)+i]]; ftol(i0->y,&sy); if (sy == sy1) continue;
			xinc = (i1->x-i0->x)/(i1->y-i0->y);
			ftol((((float)sy-i0->y)*xinc+i0->x)*65536,&y); ftol(xinc*65536,&yi);
			for(;sy<sy1;sy++,y+=yi) lastx[sy] = (y>>16);
		} while (i != itop);
		do
		{
			i0 = &pt[tri[(k<<2)+i]]; ftol(i0->y,&sy); i = incmod3[i];
			i1 = &pt[tri[(k<<2)+i]]; ftol(i1->y,&sy1); if (sy == sy1) continue;
			xinc = (i1->x-i0->x)/(i1->y-i0->y);
			ftol((((float)sy-i0->y)*xinc+i0->x)*65536,&y); ftol(xinc*65536,&yi);
			op = sy*VSID+damost;
			for(;sy<sy1;sy++,y+=yi,op+=VSID)
			{
				ftol(knmx*(float)lastx[sy] + knmy*(float)sy + knmc,&z);
				pe = (y>>16)+op; p = lastx[sy]+op;
				for(;p<pe;p++,z+=zi) *(char *)p = (z>>16);
			}
		} while (i != ibot);
	}
}

void sethull3d (point3d *pt, long nump, long dacol, long bakit)
{
	void (*modslab)(long *, long, long);
	float fminx, fminy, fminz, fmaxx, fmaxy, fmaxz;
	long i, x, y, xs, ys, xe, ye, z0, z1;

	if (nump > (MAXPOINTS>>1)) nump = (MAXPOINTS>>1); //DANGER!!!

	fminx = fminy = VSID; fminz = MAXZDIM; fmaxx = fmaxy = fmaxz = 0;
	for(i=0;i<nump;i++)
	{
		pt[i].x = MIN(MAX(pt[i].x,0),VSID-1);
		pt[i].y = MIN(MAX(pt[i].y,0),VSID-1);
		pt[i].z = MIN(MAX(pt[i].z,0),MAXZDIM-1);

		if (pt[i].x < fminx) fminx = pt[i].x;
		if (pt[i].y < fminy) fminy = pt[i].y;
		if (pt[i].z < fminz) fminz = pt[i].z;
		if (pt[i].x > fmaxx) fmaxx = pt[i].x;
		if (pt[i].y > fmaxy) fmaxy = pt[i].y;
		if (pt[i].z > fmaxz) fmaxz = pt[i].z;
	}

	ftol(fminx,&xs); if (xs < 0) xs = 0;
	ftol(fminy,&ys); if (ys < 0) ys = 0;
	ftol(fmaxx,&xe); if (xe >= VSID) xe = VSID-1;
	ftol(fmaxy,&ye); if (ye >= VSID) ye = VSID-1;
	vx5.minx = xs; vx5.maxx = xe+1;
	vx5.miny = ys; vx5.maxy = ye+1;
	ftol(fminz-.5,&vx5.minz); ftol(fmaxz+.5,&vx5.maxz);
	if ((xs > xe) || (ys > ye))
		{ if (bakit) voxbackup(xs,ys,xs,ys,bakit); return; }
	if (bakit) voxbackup(xs,ys,xe,ye,bakit);

	i = ys*VSID+(xs&~3); x = ((((xe+3)&~3)-(xs&~3))>>2)+1;
	for(y=ys;y<=ye;y++,i+=VSID)
		{ clearbuf((void *)&umost[i],x,-1); clearbuf((void *)&dmost[i],x,0); }

	inithull3d(pt,nump);
	tmaphulltrisortho(pt);

	if (vx5.colfunc == jitcolfunc) vx5.amount = 0x70707;

	if (dacol == -1) modslab = delslab; else modslab = insslab;

	for(y=ys;y<=ye;y++)
		for(x=xs;x<=xe;x++)
			modslab(scum2(x,y),(long)umost[y*VSID+x],(long)dmost[y*VSID+x]);
	scum2finish();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,dacol);
}

// ------------------------- CONVEX 3D HULL CODE ENDS -------------------------

/**
 * Sticks a default VXL map into memory (puts you inside a brownish box)
 * This is useful for VOXED when you want to start a new map.
 * @param ipo default starting camera position
 * @param ist RIGHT unit vector
 * @param ihe DOWN unit vector
 * @param ifo FORWARD unit vector
 */
void loadnul (dpoint3d *ipo, dpoint3d *ist, dpoint3d *ihe, dpoint3d *ifo)
{
	lpoint3d lp0, lp1;
	long i, x, y;
	char *v;
	float f;

	if (!vbuf) { vbuf = (long *)malloc((VOXSIZ>>2)<<2); if (!vbuf) evilquit("vbuf malloc failed"); }
	if (!vbit) { vbit = (long *)malloc((VOXSIZ>>7)<<2); if (!vbit) evilquit("vbuf malloc failed"); }

	v = (char *)(&vbuf[1]); //1st dword for voxalloc compare logic optimization

		//Completely re-compile vbuf
	for(x=0;x<VSID;x++)
		for(y=0;y<VSID;y++)
		{
			sptr[y*VSID+x] = v;
			i = 0; // + (rand()&1);  //i = default height of plain
			v[0] = 0;
			v[1] = i;
			v[2] = i;
			v[3] = 0;  //z0 (Dummy filler)
			//i = ((((x+y)>>3) + ((x^y)>>4)) % 231) + 16;
			//i = (i<<16)+(i<<8)+i;
			v += 4;
			(*(long *)v) = ((x^y)&15)*0x10101+0x807c7c7c; //colorjit(i,0x70707)|0x80000000;
			v += 4;
		}

	memset(&sptr[VSID*VSID],0,sizeof(sptr)-VSID*VSID*4);
	vbiti = (((long)v-(long)vbuf)>>2); //# vbuf longs/vbit bits allocated
	clearbuf((void *)vbit,vbiti>>5,-1);
	clearbuf((void *)&vbit[vbiti>>5],(VOXSIZ>>7)-(vbiti>>5),0);
	vbit[vbiti>>5] = (1<<vbiti)-1;

		//Blow out sphere and stick you inside map
	vx5.colfunc = jitcolfunc; vx5.curcol = 0x80704030;

	lp0.x = VSID*.5-90; lp0.y = VSID*.5-90; lp0.z = MAXZDIM*.5-45;
	lp1.x = VSID*.5+90; lp1.y = VSID*.5+90; lp1.z = MAXZDIM*.5+45;
	setrect(&lp0,&lp1,-1);
	//lp.x = VSID*.5; lp.y = VSID*.5; lp.z = MAXZDIM*.5; setsphere(&lp,64,-1);

	vx5.globalmass = calcglobalmass();

	ipo->x = VSID*.5; ipo->y = VSID*.5; ipo->z = MAXZDIM*.5; //ipo->z = -16;
	f = 0.0*PI/180.0;
	ist->x = cos(f); ist->y = sin(f); ist->z = 0;
	ihe->x = 0; ihe->y = 0; ihe->z = 1;
	ifo->x = sin(f); ifo->y = -cos(f); ifo->z = 0;

	gmipnum = 1; vx5.flstnum = 0;
	updatebbox(0,0,0,VSID,VSID,MAXZDIM,0);
}

	//Old&Slow sector code, but only this one supports the 3D bumpmapping :(
static void setsectorb (point3d *p, long *point2, long n, float thick, long dacol, long bakit, long bumpmap)
{
	point3d norm, p2;
	float d, f, x0, y0, x1, y1;
	long i, j, k, got, x, y, z, xs, ys, zs, xe, ye, ze, maxis, ndacol;

	norm.x = 0; norm.y = 0; norm.z = 0;
	for(i=0;i<n;i++)
	{
		j = point2[i]; k = point2[j];
		norm.x += (p[i].y-p[j].y)*(p[k].z-p[j].z) - (p[i].z-p[j].z)*(p[k].y-p[j].y);
		norm.y += (p[i].z-p[j].z)*(p[k].x-p[j].x) - (p[i].x-p[j].x)*(p[k].z-p[j].z);
		norm.z += (p[i].x-p[j].x)*(p[k].y-p[j].y) - (p[i].y-p[j].y)*(p[k].x-p[j].x);
	}
	f = 1.0 / sqrt(norm.x*norm.x + norm.y*norm.y + norm.z*norm.z);
	norm.x *= f; norm.y *= f; norm.z *= f;

	if ((fabs(norm.z) >= fabs(norm.x)) && (fabs(norm.z) >= fabs(norm.y)))
		maxis = 2;
	else if (fabs(norm.y) > fabs(norm.x))
		maxis = 1;
	else
		maxis = 0;

	xs = xe = p[0].x;
	ys = ye = p[0].y;
	zs = ze = p[0].z;
	for(i=n-1;i;i--)
	{
		if (p[i].x < xs) xs = p[i].x;
		if (p[i].y < ys) ys = p[i].y;
		if (p[i].z < zs) zs = p[i].z;
		if (p[i].x > xe) xe = p[i].x;
		if (p[i].y > ye) ye = p[i].y;
		if (p[i].z > ze) ze = p[i].z;
	}
	xs = MAX(xs-thick-bumpmap,0); xe = MIN(xe+thick+bumpmap,VSID-1);
	ys = MAX(ys-thick-bumpmap,0); ye = MIN(ye+thick+bumpmap,VSID-1);
	zs = MAX(zs-thick-bumpmap,0); ze = MIN(ze+thick+bumpmap,MAXZDIM-1);
	vx5.minx = xs; vx5.maxx = xe+1;
	vx5.miny = ys; vx5.maxy = ye+1;
	vx5.minz = zs; vx5.maxz = ze+1;
	if ((xs > xe) || (ys > ye) || (zs > ze)) return;
	if (bakit) voxbackup(xs,ys,xe+1,ye+1,bakit);

	clearbuf((void *)&templongbuf[zs],ze-zs+1,-3);

	ndacol = (dacol==-1)-2;

	for(y=ys;y<=ye;y++)
		for(x=xs;x<=xe;x++)
		{
			got = 0;
			d = ((float)x-p[0].x)*norm.x + ((float)y-p[0].y)*norm.y + ((float)zs-p[0].z)*norm.z;
			for(z=zs;z<=ze;z++,d+=norm.z)
			{
				if (bumpmap)
				{
					if (d < -thick) continue;
					p2.x = (float)x - d*norm.x;
					p2.y = (float)y - d*norm.y;
					p2.z = (float)z - d*norm.z;
					if (d > (float)hpngcolfunc(&p2)+thick) continue;
				}
				else
				{
					if (fabs(d) > thick) continue;
					p2.x = (float)x - d*norm.x;
					p2.y = (float)y - d*norm.y;
					p2.z = (float)z - d*norm.z;
				}

				k = 0;
				for(i=n-1;i>=0;i--)
				{
					j = point2[i];
					switch(maxis)
					{
						case 0: x0 = p[i].z-p2.z; x1 = p[j].z-p2.z;
								  y0 = p[i].y-p2.y; y1 = p[j].y-p2.y; break;
						case 1: x0 = p[i].x-p2.x; x1 = p[j].x-p2.x;
								  y0 = p[i].z-p2.z; y1 = p[j].z-p2.z; break;
						case 2: x0 = p[i].x-p2.x; x1 = p[j].x-p2.x;
								  y0 = p[i].y-p2.y; y1 = p[j].y-p2.y; break;
						default: _gtfo(); //tells MSVC default can't be reached
					}
					if (((*(long *)&y0)^(*(long *)&y1)) < 0)
					{
						if (((*(long *)&x0)^(*(long *)&x1)) >= 0) k ^= (*(long *)&x0);
						else { f = (x0*y1-x1*y0); k ^= (*(long *)&f)^(*(long *)&y1); }
					}
				}
				if (k >= 0) continue;

				templongbuf[z] = ndacol; got = 1;
			}
			if (got)
			{
				scum(x,y,zs,ze+1,templongbuf);
				clearbuf((void *)&templongbuf[zs],ze-zs+1,-3);
			}
		}
	scumfinish();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,dacol);
}

	//This is for ordfillpolygon&splitpoly
typedef struct { long p, i, t; } raster;
#define MAXCURS 100 //THIS IS VERY EVIL... FIX IT!!!
static raster rst[MAXCURS];
static long slist[MAXCURS];

	//Code taken from POLYOLD\POLYSPLI.BAS:splitpoly (06/09/2001)
void splitpoly (float *px, float *py, long *point2, long *bakn,
					 float x0, float y0, float dx, float dy)
{
	long i, j, s2, n, sn, splcnt, z0, z1, z2, z3;
	float t, t1;

	n = (*bakn); if (n < 3) return;
	i = 0; s2 = sn = n; splcnt = 0;
	do
	{
		t1 = (px[i]-x0)*dy - (py[i]-y0)*dx;
		do
		{
			j = point2[i]; point2[i] |= 0x80000000;
			t = t1; t1 = (px[j]-x0)*dy - (py[j]-y0)*dx;
			if ((*(long *)&t) < 0)
				{ px[n] = px[i]; py[n] = py[i]; point2[n] = n+1; n++; }
			if (((*(long *)&t) ^ (*(long *)&t1)) < 0)
			{
				if ((*(long *)&t) < 0) slist[splcnt++] = n;
				t /= (t-t1);
				px[n] = (px[j]-px[i])*t + px[i];
				py[n] = (py[j]-py[i])*t + py[i];
				point2[n] = n+1; n++;
			}
			i = j;
		} while (point2[i] >= 0);
		if (n > s2) { point2[n-1] = s2; s2 = n; }
		for(i=sn-1;(i) && (point2[i] < 0);i--);
	} while (i > 0);

	if (fabs(dx) > fabs(dy))
	{
		for(i=1;i<splcnt;i++)
		{
			z0 = slist[i];
			for(j=0;j<i;j++)
			{
				z1 = point2[z0]; z2 = slist[j]; z3 = point2[z2];
				if (fabs(px[z0]-px[z3])+fabs(px[z2]-px[z1]) < fabs(px[z0]-px[z1])+fabs(px[z2]-px[z3]))
					{ point2[z0] = z3; point2[z2] = z1; }
			}
		}
	}
	else
	{
		for(i=1;i<splcnt;i++)
		{
			z0 = slist[i];
			for(j=0;j<i;j++)
			{
				z1 = point2[z0]; z2 = slist[j]; z3 = point2[z2];
				if (fabs(py[z0]-py[z3])+fabs(py[z2]-py[z1]) < fabs(py[z0]-py[z1])+fabs(py[z2]-py[z3]))
					{ point2[z0] = z3; point2[z2] = z1; }
			}
		}
	}

	for(i=sn;i<n;i++)
		{ px[i-sn] = px[i]; py[i-sn] = py[i]; point2[i-sn] = point2[i]-sn; }
	(*bakn) = n-sn;
}

void ordfillpolygon (float *px, float *py, long *point2, long n, long day, long xs, long xe, void (*modslab)(long *, long, long))
{
	float f;
	long k, i, z, zz, z0, z1, zx, sx0, sy0, sx1, sy1, sy, nsy, gap, numrst;
	long np, ni;

	if (n < 3) return;

	for(z=0;z<n;z++) slist[z] = z;

		//Sort points by y's
	for(gap=(n>>1);gap;gap>>=1)
		for(z=0;z<n-gap;z++)
			for(zz=z;zz>=0;zz-=gap)
			{
				if (py[point2[slist[zz]]] <= py[point2[slist[zz+gap]]]) break;
				z0 = slist[zz]; slist[zz] = slist[zz+gap]; slist[zz+gap] = z0;
			}

	ftol(py[point2[slist[0]]]+.5,&sy); if (sy < xs) sy = xs;

	numrst = 0; z = 0; n--; //Note: n is local variable!
	while (z < n)
	{
		z1 = slist[z]; z0 = point2[z1];
		for(zx=0;zx<2;zx++)
		{
			ftol(py[z0]+.5,&sy0); ftol(py[z1]+.5,&sy1);
			if (sy1 > sy0) //Insert raster (z0,z1)
			{
				f = (px[z1]-px[z0]) / (py[z1]-py[z0]);
				ftol(((sy-py[z0])*f + px[z0])*65536.0 + 65535.0,&np);
				if (sy1-sy0 >= 2) ftol(f*65536.0,&ni); else ni = 0;
				k = (np<<1)+ni;
				for(i=numrst;i>0;i--)
				{
					if ((rst[i-1].p<<1)+rst[i-1].i < k) break;
					rst[i] = rst[i-1];
				}
				rst[i].i = ni; rst[i].p = np; rst[i].t = (z0<<16)+z1;
				numrst++;
			}
			else if (sy1 < sy0) //Delete raster (z1,z0)
			{
				numrst--;
				k = (z1<<16)+z0; i = 0;
				while ((i < numrst) && (rst[i].t != k)) i++;
				while (i < numrst) { rst[i] = rst[i+1]; i++; }
			}
			z1 = point2[z0];
		}

		z++;
		ftol(py[point2[slist[z]]]+.5,&nsy); if (nsy > xe) nsy = xe;
		for(;sy<nsy;sy++)
			for(i=0;i<numrst;i+=2)
			{
				modslab(scum2(sy,day),MAX(rst[i].p>>16,0),MIN(rst[i+1].p>>16,MAXZDIM));
				rst[i].p += rst[i].i; rst[i+1].p += rst[i+1].i;
			}
	}
}

	//Draws a flat polygon
	//given: p&point2: 3D points, n: # points, thick: thickness, dacol: color
static float ppx[MAXCURS*4], ppy[MAXCURS*4];
static long npoint2[MAXCURS*4];
void setsector (point3d *p, long *point2, long n, float thick, long dacol, long bakit)
{
	void (*modslab)(long *, long, long);
	point3d norm;
	float f, rnormy, xth, zth, dax, daz, t, t1;
	long i, j, k, x, y, z, sn, s2, nn, xs, ys, zs, xe, ye, ze;

	norm.x = 0; norm.y = 0; norm.z = 0;
	for(i=0;i<n;i++)
	{
		j = point2[i]; k = point2[j];
		norm.x += (p[i].y-p[j].y)*(p[k].z-p[j].z) - (p[i].z-p[j].z)*(p[k].y-p[j].y);
		norm.y += (p[i].z-p[j].z)*(p[k].x-p[j].x) - (p[i].x-p[j].x)*(p[k].z-p[j].z);
		norm.z += (p[i].x-p[j].x)*(p[k].y-p[j].y) - (p[i].y-p[j].y)*(p[k].x-p[j].x);
	}
	f = 1.0 / sqrt(norm.x*norm.x + norm.y*norm.y + norm.z*norm.z);
	norm.x *= f; norm.y *= f; norm.z *= f;

	if (vx5.colfunc == jitcolfunc) vx5.amount = 0x70707;
	else if ((vx5.colfunc == pngcolfunc) && (vx5.pic) && (vx5.xsiz > 0) && (vx5.ysiz > 0) && (vx5.picmode == 3))
	{
			//Find biggest height offset to minimize bounding box size
		j = k = vx5.pic[0];
		for(y=vx5.ysiz-1;y>=0;y--)
		{
			i = y*(vx5.bpl>>2);
			for(x=vx5.xsiz-1;x>=0;x--)
			{
				if (vx5.pic[i+x] < j) j = vx5.pic[i+x];
				if (vx5.pic[i+x] > k) k = vx5.pic[i+x];
			}
		}
		if ((j^k)&0xff000000) //If high bytes are !=, then use bumpmapping
		{
			setsectorb(p,point2,n,thick,dacol,bakit,MAX(labs(j>>24),labs(k>>24)));
			return;
		}
	}

	xs = xe = p[0].x;
	ys = ye = p[0].y;
	zs = ze = p[0].z;
	for(i=n-1;i;i--)
	{
		if (p[i].x < xs) xs = p[i].x;
		if (p[i].y < ys) ys = p[i].y;
		if (p[i].z < zs) zs = p[i].z;
		if (p[i].x > xe) xe = p[i].x;
		if (p[i].y > ye) ye = p[i].y;
		if (p[i].z > ze) ze = p[i].z;
	}
	xs = MAX(xs-thick,0); xe = MIN(xe+thick,VSID-1);
	ys = MAX(ys-thick,0); ye = MIN(ye+thick,VSID-1);
	zs = MAX(zs-thick,0); ze = MIN(ze+thick,MAXZDIM-1);
	vx5.minx = xs; vx5.maxx = xe+1;
	vx5.miny = ys; vx5.maxy = ye+1;
	vx5.minz = zs; vx5.maxz = ze+1;
	if ((xs > xe) || (ys > ye) || (zs > ze)) return;
	if (bakit) voxbackup(xs,ys,xe+1,ye+1,bakit);

	if (dacol == -1) modslab = delslab; else modslab = insslab;

	if (fabs(norm.y) >= .001)
	{
		rnormy = 1.0 / norm.y;
		for(y=ys;y<=ye;y++)
		{
			nn = n;
			for(i=0;i<n;i++)
			{
				f = ((float)y-p[i].y) * rnormy;
				ppx[i] = norm.z*f + p[i].z;
				ppy[i] = norm.x*f + p[i].x;
				npoint2[i] = point2[i];
			}
			if (fabs(norm.x) > fabs(norm.z))
			{
				splitpoly(ppx,ppy,npoint2,&nn,p[0].z,((p[0].y-(float)y)*norm.y-thick)/norm.x+p[0].x,norm.x,-norm.z);
				splitpoly(ppx,ppy,npoint2,&nn,p[0].z,((p[0].y-(float)y)*norm.y+thick)/norm.x+p[0].x,-norm.x,norm.z);
			}
			else
			{
				splitpoly(ppx,ppy,npoint2,&nn,((p[0].y-(float)y)*norm.y-thick)/norm.z+p[0].z,p[0].x,norm.x,-norm.z);
				splitpoly(ppx,ppy,npoint2,&nn,((p[0].y-(float)y)*norm.y+thick)/norm.z+p[0].z,p[0].x,-norm.x,norm.z);
			}
			ordfillpolygon(ppx,ppy,npoint2,nn,y,xs,xe,modslab);
		}
	}
	else
	{
		xth = norm.x*thick; zth = norm.z*thick;
		for(y=ys;y<=ye;y++)
		{
			for(z=0;z<n;z++) slist[z] = 0;
			nn = 0; i = 0; sn = n;
			do
			{
				s2 = nn; t1 = p[i].y-(float)y;
				do
				{
					j = point2[i]; slist[i] = 1; t = t1; t1 = p[j].y-(float)y;
					if (((*(long *)&t) ^ (*(long *)&t1)) < 0)
					{
						k = ((*(unsigned long *)&t)>>31); t /= (t-t1);
						daz = (p[j].z-p[i].z)*t + p[i].z;
						dax = (p[j].x-p[i].x)*t + p[i].x;
						ppx[nn+k] = daz+zth; ppx[nn+1-k] = daz-zth;
						ppy[nn+k] = dax+xth; ppy[nn+1-k] = dax-xth;
						npoint2[nn] = nn+1; npoint2[nn+1] = nn+2; nn += 2;
					}
					i = j;
				} while (!slist[i]);
				if (nn > s2) { npoint2[nn-1] = s2; s2 = nn; }
				for(i=sn-1;(i) && (slist[i]);i--);
			} while (i);
			ordfillpolygon(ppx,ppy,npoint2,nn,y,xs,xe,modslab);
		}
	}
	scum2finish();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,dacol);
}

	//Given: p[>=3]: points 0,1 are the axis of rotation, others make up shape
	//      numcurs: number of points
	//        dacol: color
void setlathe (point3d *p, long numcurs, long dacol, long bakit)
{
	point3d norm, ax0, ax1, tp0, tp1;
	float d, f, x0, y0, x1, y1, px, py, pz;
	long i, j, cnt, got, x, y, z, xs, ys, zs, xe, ye, ze, maxis, ndacol;

	norm.x = (p[0].y-p[1].y)*(p[2].z-p[1].z) - (p[0].z-p[1].z)*(p[2].y-p[1].y);
	norm.y = (p[0].z-p[1].z)*(p[2].x-p[1].x) - (p[0].x-p[1].x)*(p[2].z-p[1].z);
	norm.z = (p[0].x-p[1].x)*(p[2].y-p[1].y) - (p[0].y-p[1].y)*(p[2].x-p[1].x);
	f = 1.0 / sqrt(norm.x*norm.x + norm.y*norm.y + norm.z*norm.z);
	norm.x *= f; norm.y *= f; norm.z *= f;

	ax0.x = p[1].x-p[0].x; ax0.y = p[1].y-p[0].y; ax0.z = p[1].z-p[0].z;
	f = 1.0 / sqrt(ax0.x*ax0.x + ax0.y*ax0.y + ax0.z*ax0.z);
	ax0.x *= f; ax0.y *= f; ax0.z *= f;

	ax1.x = ax0.y*norm.z - ax0.z*norm.y;
	ax1.y = ax0.z*norm.x - ax0.x*norm.z;
	ax1.z = ax0.x*norm.y - ax0.y*norm.x;

	x0 = 0; //Cylindrical thickness: Perp-dist from line (p[0],p[1])
	y0 = 0; //Cylindrical min dot product from line (p[0],p[1])
	y1 = 0; //Cylindrical max dot product from line (p[0],p[1])
	for(i=numcurs-1;i;i--)
	{
		d = (p[i].x-p[0].x)*ax0.x + (p[i].y-p[0].y)*ax0.y + (p[i].z-p[0].z)*ax0.z;
		if (d < y0) y0 = d;
		if (d > y1) y1 = d;
		px = (p[i].x-p[0].x) - d*ax0.x;
		py = (p[i].y-p[0].y) - d*ax0.y;
		pz = (p[i].z-p[0].z) - d*ax0.z;
		f = px*px + py*py + pz*pz;     //Note: f is thickness SQUARED
		if (f > x0) x0 = f;
	}
	x0 = sqrt(x0)+1.0;
	tp0.x = ax0.x*y0 + p[0].x; tp1.x = ax0.x*y1 + p[0].x;
	tp0.y = ax0.y*y0 + p[0].y; tp1.y = ax0.y*y1 + p[0].y;
	tp0.z = ax0.z*y0 + p[0].z; tp1.z = ax0.z*y1 + p[0].z;
	xs = MAX(MIN(tp0.x,tp1.x)-x0,0); xe = MIN(MAX(tp0.x,tp1.x)+x0,VSID-1);
	ys = MAX(MIN(tp0.y,tp1.y)-x0,0); ye = MIN(MAX(tp0.y,tp1.y)+x0,VSID-1);
	zs = MAX(MIN(tp0.z,tp1.z)-x0,0); ze = MIN(MAX(tp0.z,tp1.z)+x0,MAXZDIM-1);
	vx5.minx = xs; vx5.maxx = xe+1;
	vx5.miny = ys; vx5.maxy = ye+1;
	vx5.minz = zs; vx5.maxz = ze+1;
	if ((xs > xe) || (ys > ye) || (zs > ze)) return;
	if (bakit) voxbackup(xs,ys,xe,ye,bakit);

	if ((fabs(norm.z) >= fabs(norm.x)) && (fabs(norm.z) >= fabs(norm.y)))
		maxis = 2;
	else if (fabs(norm.y) > fabs(norm.x))
		maxis = 1;
	else
		maxis = 0;

	clearbuf((void *)&templongbuf[zs],ze-zs+1,-3);

	if (vx5.colfunc == jitcolfunc) vx5.amount = 0x70707;

	ndacol = (dacol==-1)-2;

	for(y=ys;y<=ye;y++)
		for(x=xs;x<=xe;x++)
		{
			got = 0;
			d = ((float)x-p[0].x)*ax0.x + ((float)y-p[0].y)*ax0.y + ((float)zs-p[0].z)*ax0.z;
			for(z=zs;z<=ze;z++,d+=ax0.z)
			{
					//Another way: p = sqrt((xyz dot ax1)^2 + (xyz dot norm)^2)
				px = ((float)x-p[0].x) - d*ax0.x;
				py = ((float)y-p[0].y) - d*ax0.y;
				pz = ((float)z-p[0].z) - d*ax0.z;
				f = sqrt(px*px + py*py + pz*pz);

				px = ax0.x*d + ax1.x*f + p[0].x;
				py = ax0.y*d + ax1.y*f + p[0].y;
				pz = ax0.z*d + ax1.z*f + p[0].z;

				cnt = j = 0;
				for(i=numcurs-1;i>=0;i--)
				{
					switch(maxis)
					{
						case 0: x0 = p[i].z-pz; x1 = p[j].z-pz;
								  y0 = p[i].y-py; y1 = p[j].y-py; break;
						case 1: x0 = p[i].x-px; x1 = p[j].x-px;
								  y0 = p[i].z-pz; y1 = p[j].z-pz; break;
						case 2: x0 = p[i].x-px; x1 = p[j].x-px;
								  y0 = p[i].y-py; y1 = p[j].y-py; break;
						default: _gtfo(); //tells MSVC default can't be reached
					}
					if (((*(long *)&y0)^(*(long *)&y1)) < 0)
					{
						if (((*(long *)&x0)^(*(long *)&x1)) >= 0) cnt ^= (*(long *)&x0);
						else { f = (x0*y1-x1*y0); cnt ^= (*(long *)&f)^(*(long *)&y1); }
					}
					j = i;
				}
				if (cnt >= 0) continue;

				templongbuf[z] = ndacol; got = 1;
			}
			if (got)
			{
				scum(x,y,zs,ze+1,templongbuf);
				clearbuf((void *)&templongbuf[zs],ze-zs+1,-3);
			}
		}
	scumfinish();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,dacol);
}

	//Given: p[>=1]: centers
	//   vx5.currad: cutoff value
	//      numcurs: number of points
	//        dacol: color
void setblobs (point3d *p, long numcurs, long dacol, long bakit)
{
	float dx, dy, dz, v, nrad;
	long i, got, x, y, z, xs, ys, zs, xe, ye, ze, ndacol;

	if (numcurs <= 0) return;

		//Boundaries are quick hacks - rewrite this code!!!
	xs = MAX(p[0].x-64,0); xe = MIN(p[0].x+64,VSID-1);
	ys = MAX(p[0].y-64,0); ye = MIN(p[0].y+64,VSID-1);
	zs = MAX(p[0].z-64,0); ze = MIN(p[0].z+64,MAXZDIM-1);
	vx5.minx = xs; vx5.maxx = xe+1;
	vx5.miny = ys; vx5.maxy = ye+1;
	vx5.minz = zs; vx5.maxz = ze+1;
	if ((xs > xe) || (ys > ye) || (zs > ze)) return;
	if (bakit) voxbackup(xs,ys,xe,ye,bakit);

	clearbuf((void *)&templongbuf[zs],ze-zs+1,-3);

	if (vx5.colfunc == jitcolfunc) vx5.amount = 0x70707;

	ndacol = (dacol==-1)-2;

	nrad = (float)numcurs / ((float)vx5.currad*(float)vx5.currad + 256.0);
	for(y=ys;y<=ye;y++)
		for(x=xs;x<=xe;x++)
		{
			got = 0;
			for(z=zs;z<=ze;z++)
			{
				v = 0;
				for(i=numcurs-1;i>=0;i--)
				{
					dx = p[i].x-(float)x;
					dy = p[i].y-(float)y;
					dz = p[i].z-(float)z;
					v += 1.0f / (dx*dx + dy*dy + dz*dz + 256.0f);
				}
				if (*(long *)&v > *(long *)&nrad) { templongbuf[z] = ndacol; got = 1; }
			}
			if (got)
			{
				scum(x,y,zs,ze+1,templongbuf);
				clearbuf((void *)&templongbuf[zs],ze-zs+1,-3);
			}
		}
	scumfinish();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,dacol);
}

//FLOODFILL3D begins --------------------------------------------------------

#define FILLBUFSIZ 16384 //Use realloc instead!
typedef struct { unsigned short x, y, z0, z1; } spoint4d; //128K
static spoint4d fbuf[FILLBUFSIZ];

long dntil0 (long x, long y, long z)
{
	char *v = sptr[y*VSID+x];
	while (1)
	{
		if (z < v[1]) break;
		if (!v[0]) return(MAXZDIM);
		v += v[0]*4;
		if (z < v[3]) return(v[3]);
	}
	return(z);
}

long dntil1 (long x, long y, long z)
{
	char *v = sptr[y*VSID+x];
	while (1)
	{
		if (z <= v[1]) return(v[1]);
		if (!v[0]) break;
		v += v[0]*4;
		if (z < v[3]) break;
	}
	return(z);
}

long uptil1 (long x, long y, long z)
{
	char *v = sptr[y*VSID+x];
	if (z < v[1]) return(0);
	while (v[0])
	{
		v += v[0]*4;
		if (z < v[3]) break;
		if (z < v[1]) return(v[3]);
	}
	return(z);
}

	//Conducts on air and writes solid
void setfloodfill3d (long x, long y, long z, long minx, long miny, long minz,
															long maxx, long maxy, long maxz)
{
	long wholemap, j, z0, z1, nz1, i0, i1, (*bakcolfunc)(lpoint3d *);
	spoint4d a;

	if (minx < 0) minx = 0;
	if (miny < 0) miny = 0;
	if (minz < 0) minz = 0;
	maxx++; maxy++; maxz++;
	if (maxx > VSID) maxx = VSID;
	if (maxy > VSID) maxy = VSID;
	if (maxz > MAXZDIM) maxz = MAXZDIM;
	vx5.minx = minx; vx5.maxx = maxx;
	vx5.miny = miny; vx5.maxy = maxy;
	vx5.minz = minz; vx5.maxz = maxz;
	if ((minx >= maxx) || (miny >= maxy) || (minz >= maxz)) return;

	if ((x < minx) || (x >= maxx) ||
		 (y < miny) || (y >= maxy) ||
		 (z < minz) || (z >= maxz)) return;

	if ((minx != 0) || (miny != 0) || (minz != 0) || (maxx != VSID) || (maxy != VSID) || (maxz != VSID))
		wholemap = 0;
	else wholemap = 1;

	if (isvoxelsolid(x,y,z)) return;

	bakcolfunc = vx5.colfunc; vx5.colfunc = curcolfunc;

	a.x = x; a.z0 = uptil1(x,y,z); if (a.z0 < minz) a.z0 = minz;
	a.y = y; a.z1 = dntil1(x,y,z+1); if (a.z1 > maxz) a.z1 = maxz;
	if (((!a.z0) && (wholemap)) || (a.z0 >= a.z1)) { vx5.colfunc = bakcolfunc; return; } //oops! broke free :/
	insslab(scum2(x,y),a.z0,a.z1); scum2finish();
	i0 = i1 = 0; goto floodfill3dskip;
	do
	{
		a = fbuf[i0]; i0 = ((i0+1)&(FILLBUFSIZ-1));
floodfill3dskip:;
		for(j=3;j>=0;j--)
		{
			if (j&1) { x = a.x+(j&2)-1; if ((x < minx) || (x >= maxx)) continue; y = a.y; }
				 else { y = a.y+(j&2)-1; if ((y < miny) || (y >= maxy)) continue; x = a.x; }

			if (isvoxelsolid(x,y,a.z0)) { z0 = dntil0(x,y,a.z0); z1 = z0; }
										  else { z0 = uptil1(x,y,a.z0); z1 = a.z0; }
			if ((!z0) && (wholemap)) { vx5.colfunc = bakcolfunc; return; } //oops! broke free :/
			while (z1 < a.z1)
			{
				z1 = dntil1(x,y,z1);

				if (z0 < minz) z0 = minz;
				nz1 = z1; if (nz1 > maxz) nz1 = maxz;
				if (z0 < nz1)
				{
					fbuf[i1].x = x; fbuf[i1].y = y;
					fbuf[i1].z0 = z0; fbuf[i1].z1 = nz1;
					i1 = ((i1+1)&(FILLBUFSIZ-1));
					//if (i0 == i1) floodfill stack overflow!
					insslab(scum2(x,y),z0,nz1); scum2finish();
				}
				z0 = dntil0(x,y,z1); z1 = z0;
			}
		}
	} while (i0 != i1);

	vx5.colfunc = bakcolfunc;

	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,0);
}

void hollowfillstart (long x, long y, long z)
{
	spoint4d a;
	char *v;
	long i, j, z0, z1, i0, i1;

	a.x = x; a.y = y;

	v = sptr[y*VSID+x]; j = ((((long)v)-(long)vbuf)>>2); a.z0 = 0;
	while (1)
	{
		a.z1 = (long)(v[1]);
		if ((a.z0 <= z) && (z < a.z1) && (!(vbit[j>>5]&(1<<j)))) break;
		if (!v[0]) return;
		v += v[0]*4; j += 2;
		a.z0 = (long)(v[3]);
	}
	vbit[j>>5] |= (1<<j); //fill a.x,a.y,a.z0<=?<a.z1

	i0 = i1 = 0; goto floodfill3dskip2;
	do
	{
		a = fbuf[i0]; i0 = ((i0+1)&(FILLBUFSIZ-1));
floodfill3dskip2:;
		for(i=3;i>=0;i--)
		{
			if (i&1) { x = a.x+(i&2)-1; if ((unsigned long)x >= VSID) continue; y = a.y; }
				 else { y = a.y+(i&2)-1; if ((unsigned long)y >= VSID) continue; x = a.x; }

			v = sptr[y*VSID+x]; j = ((((long)v)-(long)vbuf)>>2); z0 = 0;
			while (1)
			{
				z1 = (long)(v[1]);
				if ((z0 < a.z1) && (a.z0 < z1) && (!(vbit[j>>5]&(1<<j))))
				{
					fbuf[i1].x = x; fbuf[i1].y = y;
					fbuf[i1].z0 = z0; fbuf[i1].z1 = z1;
					i1 = ((i1+1)&(FILLBUFSIZ-1));
					if (i0 == i1) return; //floodfill stack overflow!
					vbit[j>>5] |= (1<<j); //fill x,y,z0<=?<z1
				}
				if (!v[0]) break;
				v += v[0]*4; j += 2;
				z0 = (long)(v[3]);
			}
		}
	} while (i0 != i1);
}

	//hollowfill
void sethollowfill ()
{
	long i, j, l, x, y, z0, z1, *lptr, (*bakcolfunc)(lpoint3d *);
	char *v;

	vx5.minx = 0; vx5.maxx = VSID;
	vx5.miny = 0; vx5.maxy = VSID;
	vx5.minz = 0; vx5.maxz = MAXZDIM;

	for(i=0;i<VSID*VSID;i++)
	{
		j = ((((long)sptr[i])-(long)vbuf)>>2);
		for(v=sptr[i];v[0];v+=v[0]*4) { vbit[j>>5] &= ~(1<<j); j += 2; }
		vbit[j>>5] &= ~(1<<j);
	}

	for(y=0;y<VSID;y++)
		for(x=0;x<VSID;x++)
			hollowfillstart(x,y,0);

	bakcolfunc = vx5.colfunc; vx5.colfunc = curcolfunc;
	i = 0;
	for(y=0;y<VSID;y++)
		for(x=0;x<VSID;x++,i++)
		{
			j = ((((long)sptr[i])-(long)vbuf)>>2);
			v = sptr[i]; z0 = MAXZDIM;
			while (1)
			{
				z1 = (long)(v[1]);
				if ((z0 < z1) && (!(vbit[j>>5]&(1<<j))))
				{
					vbit[j>>5] |= (1<<j);
					insslab(scum2(x,y),z0,z1);
				}
				if (!v[0]) break;
				v += v[0]*4; j += 2;
				z0 = (long)(v[3]);
			}
		}
	scum2finish();
	vx5.colfunc = bakcolfunc;
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,0);
}

//FLOODFILL3D ends ----------------------------------------------------------

void setsphere (lpoint3d *hit, long hitrad, long dacol)
{
	void (*modslab)(long *, long, long);
	long i, x, y, xs, ys, zs, xe, ye, ze, sq;
	float f, ff;

	xs = MAX(hit->x-hitrad,0); xe = MIN(hit->x+hitrad,VSID-1);
	ys = MAX(hit->y-hitrad,0); ye = MIN(hit->y+hitrad,VSID-1);
	zs = MAX(hit->z-hitrad,0); ze = MIN(hit->z+hitrad,MAXZDIM-1);
	vx5.minx = xs; vx5.maxx = xe+1;
	vx5.miny = ys; vx5.maxy = ye+1;
	vx5.minz = zs; vx5.maxz = ze+1;
	if ((xs > xe) || (ys > ye) || (zs > ze)) return;

	if (vx5.colfunc == sphcolfunc)
	{
		vx5.cen = hit->x+hit->y+hit->z;
		vx5.daf = 1.f/(hitrad*sqrt(3.f));
	}

	if (hitrad >= SETSPHMAXRAD-1) hitrad = SETSPHMAXRAD-2;
	if (dacol == -1) modslab = delslab; else modslab = insslab;

	tempfloatbuf[0] = 0.0f;
#if 0
		//Totally unoptimized
	for(i=1;i<=hitrad;i++) tempfloatbuf[i] = pow((float)i,vx5.curpow);
#else
	tempfloatbuf[1] = 1.0f;
	for(i=2;i<=hitrad;i++)
	{
		if (!factr[i][0]) tempfloatbuf[i] = exp(logint[i]*vx5.curpow);
		else tempfloatbuf[i] = tempfloatbuf[factr[i][0]]*tempfloatbuf[factr[i][1]];
	}
#endif
	*(long *)&tempfloatbuf[hitrad+1] = 0x7f7fffff; //3.4028235e38f; //Highest float

	sq = 0; //pow(fabs(x-hit->x),vx5.curpow) + "y + "z < pow(vx5.currad,vx5.curpow)
	for(y=ys;y<=ye;y++)
	{
		ff = tempfloatbuf[hitrad]-tempfloatbuf[labs(y-hit->y)];
		if (*(long *)&ff <= 0) continue;
		for(x=xs;x<=xe;x++)
		{
			f = ff-tempfloatbuf[labs(x-hit->x)]; if (*(long *)&f <= 0) continue;
			while (*(long *)&tempfloatbuf[sq] <  *(long *)&f) sq++;
			while (*(long *)&tempfloatbuf[sq] >= *(long *)&f) sq--;
			modslab(scum2(x,y),MAX(hit->z-sq,zs),MIN(hit->z+sq+1,ze));
		}
	}
	scum2finish();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,dacol);
}

void setellipsoid (lpoint3d *hit, lpoint3d *hit2, long hitrad, long dacol, long bakit)
{
	void (*modslab)(long *, long, long);
	long x, y, xs, ys, zs, xe, ye, ze;
	float a, b, c, d, e, f, g, h, r, t, u, Za, Zb, fx0, fy0, fz0, fx1, fy1, fz1;

	xs = MIN(hit->x,hit2->x)-hitrad; xs = MAX(xs,0);
	ys = MIN(hit->y,hit2->y)-hitrad; ys = MAX(ys,0);
	zs = MIN(hit->z,hit2->z)-hitrad; zs = MAX(zs,0);
	xe = MAX(hit->x,hit2->x)+hitrad; xe = MIN(xe,VSID-1);
	ye = MAX(hit->y,hit2->y)+hitrad; ye = MIN(ye,VSID-1);
	ze = MAX(hit->z,hit2->z)+hitrad; ze = MIN(ze,MAXZDIM-1);
	vx5.minx = xs; vx5.maxx = xe+1;
	vx5.miny = ys; vx5.maxy = ye+1;
	vx5.minz = zs; vx5.maxz = ze+1;
	if ((xs > xe) || (ys > ye) || (zs > ze))
		{ if (bakit) voxbackup(xs,ys,xs,ys,bakit); return; }

	fx0 = (float)hit->x; fy0 = (float)hit->y; fz0 = (float)hit->z;
	fx1 = (float)hit2->x; fy1 = (float)hit2->y; fz1 = (float)hit2->z;

	r = (fx1-fx0)*(fx1-fx0) + (fy1-fy0)*(fy1-fy0) + (fz1-fz0)*(fz1-fz0);
	r = sqrt((float)hitrad*(float)hitrad + r*.25);
	c = fz0*fz0 - fz1*fz1; d = r*r*-4; e = d*4;
	f = c*c + fz1*fz1 * e; g = c + c; h = (fz1-fz0)*2; c = c*h - fz1*e;
	Za = -h*h - e; if (Za <= 0) { if (bakit) voxbackup(xs,ys,xs,ys,bakit); return; }
	u = 1 / Za;

	if (vx5.colfunc == jitcolfunc) vx5.amount = 0x70707;

	if (dacol == -1) modslab = delslab; else modslab = insslab;

	if (bakit) voxbackup(xs,ys,xe+1,ye+1,bakit);

	for(y=ys;y<=ye;y++)
		for(x=xs;x<=xe;x++)
		{
			a = (x-fx0)*(x-fx0) + (y-fy0)*(y-fy0);
			b = (x-fx1)*(x-fx1) + (y-fy1)*(y-fy1);
			t = a-b+d; Zb = t*h + c;
			t = ((t+g)*t + b*e + f)*Za + Zb*Zb; if (t <= 0) continue;
			t = sqrt(t);
			ftol((Zb - t)*u,&zs); if (zs < 0) zs = 0;
			ftol((Zb + t)*u,&ze); if (ze > MAXZDIM) ze = MAXZDIM;
			modslab(scum2(x,y),zs,ze);
		}
	scum2finish();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,dacol);
}

	//Draws a cylinder, given: 2 points, a radius, and a color
	//Code mostly optimized - original code from CYLINDER.BAS:drawcylinder
void setcylinder (lpoint3d *p0, lpoint3d *p1, long cr, long dacol, long bakit)
{
	void (*modslab)(long *, long, long);

	float t, ax, ay, az, bx, by, bz, cx, cy, cz, ux, uy, uz, vx, vy, vz;
	float Za, Zb, Zc, tcr, xxyy, rcz, rZa;
	float fx, fxi, xof, vx0, vy0, vz0, vz0i, vxo, vyo, vzo;
	long i, j, ix, iy, ix0, ix1, iz0, iz1, minx, maxx, miny, maxy;
	long x0, y0, z0, x1, y1, z1;

		//Map generic cylinder into unit space:  (0,0,0), (0,0,1), cr = 1
		//   x*x + y*y < 1, z >= 0, z < 1
	if (p0->z > p1->z)
	{
		x0 = p1->x; y0 = p1->y; z0 = p1->z;
		x1 = p0->x; y1 = p0->y; z1 = p0->z;
	}
	else
	{
		x0 = p0->x; y0 = p0->y; z0 = p0->z;
		x1 = p1->x; y1 = p1->y; z1 = p1->z;
	}

	xxyy = (float)((x1-x0)*(x1-x0)+(y1-y0)*(y1-y0));
	t = xxyy + (float)(z1-z0)*(z1-z0);
	if ((t == 0) || (cr == 0))
	{
		vx5.minx = x0; vx5.maxx = x0+1;
		vx5.miny = y0; vx5.maxy = y0+1;
		vx5.minz = z0; vx5.maxz = z0+1;
		if (bakit) voxbackup(x0,y0,x0,y0,bakit);
		return;
	}
	t = 1 / t; cx = ((float)(x1-x0))*t; cy = ((float)(y1-y0))*t; cz = ((float)(z1-z0))*t;
	t = sqrt(t); ux = ((float)(x1-x0))*t; uy = ((float)(y1-y0))*t; uz = ((float)(z1-z0))*t;

	if (vx5.colfunc == jitcolfunc) vx5.amount = 0x70707;

	if (dacol == -1) modslab = delslab; else modslab = insslab;

	if (xxyy == 0)
	{
		iz0 = MAX(z0,0); iz1 = MIN(z1,MAXZDIM);
		minx = MAX(x0-cr,0); maxx = MIN(x0+cr,VSID-1);
		miny = MAX(y0-cr,0); maxy = MIN(y0+cr,VSID-1);

		vx5.minx = minx; vx5.maxx = maxx+1;
		vx5.miny = miny; vx5.maxy = maxy+1;
		vx5.minz = iz0; vx5.maxz = iz1;
		if (bakit) voxbackup(minx,miny,maxx+1,maxy+1,bakit);

		j = cr*cr;
		for(iy=miny;iy<=maxy;iy++)
		{
			i = j-(iy-y0)*(iy-y0);
			for(ix=minx;ix<=maxx;ix++)
				if ((ix-x0)*(ix-x0) < i) modslab(scum2(ix,iy),iz0,iz1);
		}
		scum2finish();
		updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,dacol);
		return;
	}

	if (x0 < x1) { minx = x0; maxx = x1; } else { minx = x1; maxx = x0; }
	if (y0 < y1) { miny = y0; maxy = y1; } else { miny = y1; maxy = y0; }
	tcr = cr / sqrt(xxyy); vx = fabs((float)(x1-x0))*tcr; vy = fabs((float)(y1-y0))*tcr;
	t = vx*uz + vy;
	ftol((float)minx-t,&minx); if (minx < 0) minx = 0;
	ftol((float)maxx+t,&maxx); if (maxx >= VSID) maxx = VSID-1;
	t = vy*uz + vx;
	ftol((float)miny-t,&miny); if (miny < 0) miny = 0;
	ftol((float)maxy+t,&maxy); if (maxy >= VSID) maxy = VSID-1;

	vx5.minx = minx; vx5.maxx = maxx+1;
	vx5.miny = miny; vx5.maxy = maxy+1;
	vx5.minz = z0-cr; vx5.maxz = z1+cr+1;
	if (bakit) voxbackup(minx,miny,maxx+1,maxy+1,bakit);

	vx = (fabs(ux) < fabs(uy)); vy = 1.0f-vx; vz = 0;
	ax = uy*vz - uz*vy; ay = uz*vx - ux*vz; az = ux*vy - uy*vx;
	t = 1.0 / (sqrt(ax*ax + ay*ay + az*az)*cr);
	ax *= t; ay *= t; az *= t;
	bx = ay*uz - az*uy; by = az*ux - ax*uz; bz = ax*uy - ay*ux;

	Za = az*az + bz*bz; rZa = 1.0f / Za;
	if (cz != 0) { rcz = 1.0f / cz; vz0i = -rcz*cx; }
	if (y0 != y1)
	{
		t = 1.0f / ((float)(y1-y0)); fxi = ((float)(x1-x0))*t;
		fx = ((float)miny-y0)*fxi + x0; xof = fabs(tcr*xxyy*t);
	}
	else { fx = (float)minx; fxi = 0.0; xof = (float)(maxx-minx); }

	vy = (float)(miny-y0);
	vxo = vy*ay - z0*az;
	vyo = vy*by - z0*bz;
	vzo = vy*cy - z0*cz;
	for(iy=miny;iy<=maxy;iy++)
	{
		ftol(fx-xof,&ix0); if (ix0 < minx) ix0 = minx;
		ftol(fx+xof,&ix1); if (ix1 > maxx) ix1 = maxx;
		fx += fxi;

		vx = (float)(ix0-x0);
		vx0 = vx*ax + vxo; vxo += ay;
		vy0 = vx*bx + vyo; vyo += by;
		vz0 = vx*cx + vzo; vzo += cy;

		if (cz != 0)   //(vx0 + vx1*t)ý + (vy0 + vy1*t)ý = 1
		{
			vz0 *= -rcz;
			for(ix=ix0;ix<=ix1;ix++,vx0+=ax,vy0+=bx,vz0+=vz0i)
			{
				Zb = vx0*az + vy0*bz; Zc = vx0*vx0 + vy0*vy0 - 1;
				t = Zb*Zb - Za*Zc; if (*(long *)&t <= 0) continue; t = sqrt(t);
				ftol(MAX((-Zb-t)*rZa,vz0    ),&iz0); if (iz0 < 0) iz0 = 0;
				ftol(MIN((-Zb+t)*rZa,vz0+rcz),&iz1); if (iz1 > MAXZDIM) iz1 = MAXZDIM;
				modslab(scum2(ix,iy),iz0,iz1);
			}
		}
		else
		{
			for(ix=ix0;ix<=ix1;ix++,vx0+=ax,vy0+=bx,vz0+=cx)
			{
				if (*(unsigned long *)&vz0 >= 0x3f800000) continue; //vz0<0||vz0>=1
				Zb = vx0*az + vy0*bz; Zc = vx0*vx0 + vy0*vy0 - 1;
				t = Zb*Zb - Za*Zc; if (*(long *)&t <= 0) continue; t = sqrt(t);
				ftol((-Zb-t)*rZa,&iz0); if (iz0 < 0) iz0 = 0;
				ftol((-Zb+t)*rZa,&iz1); if (iz1 > MAXZDIM) iz1 = MAXZDIM;
				modslab(scum2(ix,iy),iz0,iz1);
			}
		}
	}
	scum2finish();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,dacol);
}

	//Draws a rectangle, given: 2 points as opposite corners, and a color
void setrect (lpoint3d *hit, lpoint3d *hit2, long dacol)
{
	long x, y, xs, ys, zs, xe, ye, ze;

		//WARNING: do NOT use lbound because 'c' not guaranteed to be >= 'b'
	xs = MAX(MIN(hit->x,hit2->x),0); xe = MIN(MAX(hit->x,hit2->x),VSID-1);
	ys = MAX(MIN(hit->y,hit2->y),0); ye = MIN(MAX(hit->y,hit2->y),VSID-1);
	zs = MAX(MIN(hit->z,hit2->z),0); ze = MIN(MAX(hit->z,hit2->z),MAXZDIM-1);
	vx5.minx = xs; vx5.maxx = xe+1;
	vx5.miny = ys; vx5.maxy = ye+1;
	vx5.minz = zs; vx5.maxz = ze+1;
	if ((xs > xe) || (ys > ye) || (zs > ze)) return;

	if (vx5.colfunc == jitcolfunc) vx5.amount = 0x70707;

	ze++;
	if (dacol == -1)
	{
		for(y=ys;y<=ye;y++)
			for(x=xs;x<=xe;x++)
				delslab(scum2(x,y),zs,ze);
	}
	else
	{
		for(y=ys;y<=ye;y++)
			for(x=xs;x<=xe;x++)
				insslab(scum2(x,y),zs,ze);
	}
	scum2finish();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,dacol);
}

	//Does CSG using pre-sorted spanlist
void setspans (vspans *lst, long lstnum, lpoint3d *offs, long dacol)
{
	void (*modslab)(long *, long, long);
	long i, j, x, y, z0, z1, *lptr;
	char ox, oy;

	if (lstnum <= 0) return;
	if (dacol == -1) modslab = delslab; else modslab = insslab;
	vx5.minx = vx5.maxx = ((long)lst[0].x)+offs->x;
	vx5.miny = ((long)lst[       0].y)+offs->y;
	vx5.maxy = ((long)lst[lstnum-1].y)+offs->y+1;
	vx5.minz = vx5.maxz = ((long)lst[0].z0)+offs->z;

	i = 0; goto in2setlist;
	do
	{
		if ((ox != lst[i].x) || (oy != lst[i].y))
		{
in2setlist:;
			ox = lst[i].x; oy = lst[i].y;
			x = ((long)lst[i].x)+offs->x;
			y = ((long)lst[i].y)+offs->y;
				  if (x < vx5.minx) vx5.minx = x;
			else if (x > vx5.maxx) vx5.maxx = x;
			lptr = scum2(x,y);
		}
		if ((x|y)&(~(VSID-1))) { i++; continue; }
		z0 = ((long)lst[i].z0)+offs->z;   if (z0 < 0) z0 = 0;
		z1 = ((long)lst[i].z1)+offs->z+1; if (z1 > MAXZDIM) z1 = MAXZDIM;
		if (z0 < vx5.minz) vx5.minz = z0;
		if (z1 > vx5.maxz) vx5.maxz = z1;
		modslab(lptr,z0,z1);
		i++;
	} while (i < lstnum);
	vx5.maxx++; vx5.maxz++;
	if (vx5.minx < 0) vx5.minx = 0;
	if (vx5.miny < 0) vx5.miny = 0;
	if (vx5.maxx > VSID) vx5.maxx = VSID;
	if (vx5.maxy > VSID) vx5.maxy = VSID;

	scum2finish();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,dacol);
}

void setheightmap (const unsigned char *hptr, long hpitch, long hxdim, long hydim,
						 long x0, long y0, long x1, long y1)
{
	long x, y, su, sv, u, v;

	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > VSID) x1 = VSID;
	if (y1 > VSID) y1 = VSID;
	vx5.minx = x0; vx5.maxx = x1;
	vx5.miny = y0; vx5.maxy = y1;
	vx5.minz = 0; vx5.maxz = MAXZDIM;
	if ((x0 >= x1) || (y0 >= y1)) return;

	su = x0%hxdim; sv = y0%hydim;
	for(y=y0,v=sv;y<y1;y++)
	{
		for(x=x0,u=su;x<x1;x++)
		{
			insslab(scum2(x,y),hptr[v*hpitch+u],MAXZDIM);
			u++; if (u >= hxdim) u = 0;
		}
		v++; if (v >= hydim) v = 0;
	}
	scum2finish();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,0);
}

//------------------- editing backup / restore begins ------------------------


//Quake3 .BSP loading code begins --------------------------------------------
typedef struct { long c, i; float z, z1; } vlinerectyp;
static point3d q3pln[5250];
static float q3pld[5250], q3vz[256];
static long q3nod[4850][3], q3lf[4850];
long vlinebsp (float x, float y, float z0, float z1, float *dvz)
{
	vlinerectyp vlrec[64];
	float z, t;
	long i, j, vcnt, vlcnt;
	char vt[256];

	vcnt = 1; i = 0; vlcnt = 0; vt[0] = 17;
	while (1)
	{
		if (i < 0)
		{
			if (vt[vcnt-1] != (q3lf[~i]&255))
				{ dvz[vcnt] = z0; vt[vcnt] = (q3lf[~i]&255); vcnt++; }
		}
		else
		{
			j = q3nod[i][0]; z = q3pld[j] - q3pln[j].x*x - q3pln[j].y*y;
			t = q3pln[j].z*z0-z;
			if ((t < 0) == (q3pln[j].z*z1 < z))
				{ vlrec[vlcnt].c = 0; i = q3nod[i][(t<0)+1]; }
			else
			{
				z /= q3pln[j].z; j = (q3pln[j].z<0)+1;
				vlrec[vlcnt].c = 1; vlrec[vlcnt].i = q3nod[i][j];
				vlrec[vlcnt].z = z; vlrec[vlcnt].z1 = z1;
				i = q3nod[i][3-j]; z1 = z;
			}
			vlcnt++; continue;
		}
		do { vlcnt--; if (vlcnt < 0) return(vcnt); } while (!vlrec[vlcnt].c);
		vlrec[vlcnt].c = 0; i = vlrec[vlcnt].i;
		z0 = vlrec[vlcnt].z; z1 = vlrec[vlcnt].z1;
		vlcnt++;
	}
	return(0);
}


//Quake3 .BSP loading code ends ----------------------------------------------


void genmipvxl (long x0, long y0, long x1, long y1)
{
	long i, n, oldn, x, y, z, xsiz, ysiz, zsiz, oxsiz, oysiz;
	long cz, oz, nz, zz, besti, cstat, curz[4], curzn[4][4], mipnum, mipmax;
	char *v[4], *tv, **sr, **sw, **ssr, **ssw;

	if ((!(x0|y0)) && (x1 == VSID) && (y1 == VSID)) mipmax = vx5.vxlmipuse;
															 else mipmax = gmipnum;
	if (mipmax <= 0) return;
	mipnum = 1;

	vx5.colfunc = curcolfunc;
	xsiz = VSID; ysiz = VSID; zsiz = MAXZDIM;
	ssr = sptr; ssw = sptr+xsiz*ysiz;
	while ((xsiz > 1) && (ysiz > 1) && (zsiz > 1) && (mipnum < mipmax))
	{
		oxsiz = xsiz; xsiz >>= 1;
		oysiz = ysiz; ysiz >>= 1;
						  zsiz >>= 1;

		x0--; if (x0 < 0) x0 = 0;
		y0--; if (y0 < 0) y0 = 0;
		x1++; if (x1 > VSID) x1 = VSID;
		y1++; if (y1 > VSID) y1 = VSID;

		x0 >>= 1; x1 = ((x1+1)>>1);
		y0 >>= 1; y1 = ((y1+1)>>1);
		for(y=y0;y<y1;y++)
		{
			sr = ssr+oxsiz*(y<<1)+(x0<<1);
			sw = ssw+xsiz*y+x0;
			for(x=x0;x<x1;x++)
			{
					//ÚÄÄÄÂÄÄÄÂÄÄÄÂÄÄÄ¿
					//³npt³z1 ³z1c³dum³
					//³ b ³ g ³ r ³ i ³
					//³ b ³ g ³ r ³ i ³
					//³npt³z1 ³z1c³z0 ³
					//³ b ³ g ³ r ³ i ³
					//ÀÄÄÄÁÄÄÄÁÄÄÄÁÄÄÄÙ
				v[0] = sr[      0];
				v[1] = sr[      1];
				v[2] = sr[oysiz  ];
				v[3] = sr[oysiz+1];
				for(i=3;i>=0;i--)
				{
					curz[i] = curzn[i][0] = (long)v[i][1];
					curzn[i][1] = ((long)v[i][2])+1;

					tv = v[i];
					while (1)
					{
						oz = (long)tv[1];
						for(z=oz;z<=((long)tv[2]);z++)
						{
							nz = (z>>1);
							mixc[nz][mixn[nz]++] = *(long *)(&tv[((z-oz)<<2)+4]);
						}
						z = (z-oz) - (((long)tv[0])-1);
						if (!tv[0]) break;
						tv += (((long)tv[0])<<2);
						oz = (long)tv[3];
						for(;z<0;z++)
						{
							nz = ((z+oz)>>1);
							mixc[nz][mixn[nz]++] = *(long *)(&tv[z<<2]);
						}
					}
				}
				cstat = 0; oldn = 0; n = 4; tbuf[3] = 0; z = 0x80000000;
				while (1)
				{
					oz = z;

						//z,besti = min,argMIN(curz[0],curz[1],curz[2],curz[3])
					besti = (((unsigned long)(curz[1]-curz[    0]))>>31);
						 i = (((unsigned long)(curz[3]-curz[    2]))>>31)+2;
					besti +=(((( signed long)(curz[i]-curz[besti]))>>31)&(i-besti));
					z = curz[besti]; if (z >= MAXZDIM) break;

					if ((!cstat) && ((z>>1) >= ((oz+1)>>1)))
					{
						if (oz >= 0)
						{
							tbuf[oldn] = ((n-oldn)>>2);
							tbuf[oldn+2]--;
							tbuf[n+3] = ((oz+1)>>1);
							oldn = n; n += 4;
						}
						tbuf[oldn] = 0;
						tbuf[oldn+1] = tbuf[oldn+2] = (z>>1); cz = -1;
					}
					if (cstat&0x1111)
					{
						if (((((long)tbuf[oldn+2])<<1)+1 >= oz) && (cz < 0))
						{
							while ((((long)tbuf[oldn+2])<<1) < z)
							{
								zz = (long)tbuf[oldn+2];
								*(long *)&tbuf[n] = mixc[zz][rand()%mixn[zz]];
								mixn[zz] = 0;
								tbuf[oldn+2]++; n += 4;
							}
						}
						else
						{
							if (cz < 0) cz = (oz>>1);
							else if ((cz<<1)+1 < oz)
							{
									//Insert fake slab
								tbuf[oldn] = ((n-oldn)>>2);
								tbuf[oldn+2]--;
								tbuf[n] = 0;
								tbuf[n+1] = tbuf[n+2] = tbuf[n+3] = cz;
								oldn = n; n += 4;
								cz = (oz>>1);
							}
							while ((cz<<1) < z)
							{
								*(long *)&tbuf[n] = mixc[cz][rand()%mixn[cz]];
								mixn[cz] = 0;
								cz++; n += 4;
							}
						}
					}

					i = (besti<<2);
					cstat = (((1<<i)+cstat)&0x3333); //--33--22--11--00
					switch ((cstat>>i)&3)
					{
						case 0: curz[besti] = curzn[besti][0]; break;
						case 1: curz[besti] = curzn[besti][1]; break;
						case 2:
							if (!(v[besti][0])) { curz[besti] = MAXZDIM; }
							else
							{
								tv = v[besti]; i = (((long)tv[2])-((long)tv[1])+1)-(((long)tv[0])-1);
								tv += (((long)tv[0])<<2);
								curz[besti] = ((long)(tv[3])) + i;
								curzn[besti][3] = (long)(tv[3]);
								curzn[besti][0] = (long)(tv[1]);
								curzn[besti][1] = ((long)tv[2])+1;
								v[besti] = tv;
							}
							break;
						case 3: curz[besti] = curzn[besti][3]; break;
						//default: _gtfo(); //tells MSVC default can't be reached
					}
				}
				tbuf[oldn+2]--;
				if (cz >= 0)
				{
					tbuf[oldn] = ((n-oldn)>>2);
					tbuf[n] = 0;
					tbuf[n+1] = tbuf[n+3] = cz;
					tbuf[n+2] = cz-1;
					n += 4;
				}

					//De-allocate column (x,y) if it exists
				if (sw[0]) voxdealloc(sw[0]);

					//Allocate & copy to new column (x,y)
				sw[0] = voxalloc(n);
				copybuf((void *)tbuf,(void *)sw[0],n>>2);
				sw++; sr += 2;
			}
			sr += ysiz*2;
		}
		ssr = ssw; ssw += xsiz*ysiz;
		mipnum++; if (mipnum > gmipnum) gmipnum = mipnum;
	}

		//Remove extra mips (bbox must be 0,0,VSID,VSID to get inside this)
	while ((xsiz > 1) && (ysiz > 1) && (zsiz > 1) && (mipnum < gmipnum))
	{
		xsiz >>= 1; ysiz >>= 1; zsiz >>= 1;
		for(i=xsiz*ysiz;i>0;i--)
		{
			if (ssw[0]) voxdealloc(ssw[0]); //De-allocate column if it exists
			ssw++;
		}
		gmipnum--;
	}
}
/** Draw a textured wuad using voxels, given 3 points (4th is calculated)
 *  and their UV coordinates, plus an optional pointer to a bitmap.
*/
void drawpolyquad (long rpic, long rbpl, long rxsiz, long rysiz,
				   float x0, float y0, float z0, float u0, float v0,
				   float x1, float y1, float z1, float u1, float v1,
				   float x2, float y2, float z2, float u2, float v2,
				   float x3, float y3, float z3)
{
	point3d fp, fp2;
	float px[6], py[6], pz[6], pu[6], pv[6], px2[4], py2[4], pz2[4], pu2[4], pv2[4];
	float f, t, u, v, r, nx, ny, nz, ox, oy, oz, scaler;
	float dx, dy, db, ux, uy, ub, vx, vy, vb;
	long i, j, k, l, imin, imax, sx, sxe, sy, sy1;
	long x, xi, *p, *pe, uvmax, iu, iv, n;
	long dd, uu, vv, ddi, uui, vvi, distlutoffs;

	#if defined(__GNUC__) && !defined(NOASM) //only for gcc inline asm
	//register lpoint2d reg0 asm("xmm0");
	//register lpoint2d reg1 asm("xmm1");
	//register lpoint2d reg2 asm("xmm2");
	//register lpoint2d reg3 asm("xmm3");
	//register lpoint2d reg4 asm("xmm4");
	//register lpoint2d reg5 asm("xmm5");
	register lpoint2d reg6 asm("xmm6");
	register lpoint2d reg7 asm("xmm7");
	#endif

	px2[0] = x0; py2[0] = y0; pz2[0] = z0; pu2[0] = u0; pv2[0] = v0;
	px2[1] = x1; py2[1] = y1; pz2[1] = z1; pu2[1] = u1; pv2[1] = v1;
	px2[2] = x2; py2[2] = y2; pz2[2] = z2; pu2[2] = u2; pv2[2] = v2;
	px2[3] = x3; py2[3] = y3; pz2[3] = z3;

		//Calculate U-V coordinate of 4th point on quad (based on 1st 3)
	nx = (y1-y0)*(z2-z0) - (z1-z0)*(y2-y0);
	ny = (z1-z0)*(x2-x0) - (x1-x0)*(z2-z0);
	nz = (x1-x0)*(y2-y0) - (y1-y0)*(x2-x0);
	if ((fabs(nx) > fabs(ny)) && (fabs(nx) > fabs(nz)))
	{     //(y1-y0)*u + (y2-y0)*v = (y3-y0)
			//(z1-z0)*u + (z2-z0)*v = (z3-z0)
		f = 1/nx;
		u = ((y3-y0)*(z2-z0) - (z3-z0)*(y2-y0))*f;
		v = ((y1-y0)*(z3-z0) - (z1-z0)*(y3-y0))*f;
	}
	else if (fabs(ny) > fabs(nz))
	{     //(x1-x0)*u + (x2-x0)*v = (x3-x0)
			//(z1-z0)*u + (z2-z0)*v = (z3-z0)
		f = -1/ny;
		u = ((x3-x0)*(z2-z0) - (z3-z0)*(x2-x0))*f;
		v = ((x1-x0)*(z3-z0) - (z1-z0)*(x3-x0))*f;
	}
	else
	{     //(x1-x0)*u + (x2-x0)*v = (x3-x0)
			//(y1-y0)*u + (y2-y0)*v = (y3-y0)
		f = 1/nz;
		u = ((x3-x0)*(y2-y0) - (y3-y0)*(x2-x0))*f;
		v = ((x1-x0)*(y3-y0) - (y1-y0)*(x3-x0))*f;
	}
	pu2[3] = (u1-u0)*u + (u2-u0)*v + u0;
	pv2[3] = (v1-v0)*u + (v2-v0)*v + v0;


	for(i=4-1;i>=0;i--) //rotation
	{
		fp.x = px2[i]-gipos.x; fp.y = py2[i]-gipos.y; fp.z = pz2[i]-gipos.z;
		px2[i] = fp.x*gistr.x + fp.y*gistr.y + fp.z*gistr.z;
		py2[i] = fp.x*gihei.x + fp.y*gihei.y + fp.z*gihei.z;
		pz2[i] = fp.x*gifor.x + fp.y*gifor.y + fp.z*gifor.z;
	}

		//Clip to SCISDIST plane
	n = 0;
	for(i=0;i<4;i++)
	{
		j = ((i+1)&3);
		if (pz2[i] >= SCISDIST) { px[n] = px2[i]; py[n] = py2[i]; pz[n] = pz2[i]; pu[n] = pu2[i]; pv[n] = pv2[i]; n++; }
		if ((pz2[i] >= SCISDIST) != (pz2[j] >= SCISDIST))
		{
			f = (SCISDIST-pz2[i])/(pz2[j]-pz2[i]);
			px[n] = (px2[j]-px2[i])*f + px2[i];
			py[n] = (py2[j]-py2[i])*f + py2[i];
			pz[n] = SCISDIST;
			pu[n] = (pu2[j]-pu2[i])*f + pu2[i];
			pv[n] = (pv2[j]-pv2[i])*f + pv2[i]; n++;
		}
	}
	if (n < 3) return;

	for(i=n-1;i>=0;i--) //projection
	{
		pz[i] = 1/pz[i]; f = pz[i]*gihz;
		px[i] = px[i]*f + gihx;
		py[i] = py[i]*f + gihy;
	}

		//General equations:
		//pz[i] = (px[i]*gdx + py[i]*gdy + gdo)
		//pu[i] = (px[i]*gux + py[i]*guy + guo)/pz[i]
		//pv[i] = (px[i]*gvx + py[i]*gvy + gvo)/pz[i]
		//
		//px[0]*gdx + py[0]*gdy + 1*gdo = pz[0]
		//px[1]*gdx + py[1]*gdy + 1*gdo = pz[1]
		//px[2]*gdx + py[2]*gdy + 1*gdo = pz[2]
		//
		//px[0]*gux + py[0]*guy + 1*guo = pu[0]*pz[0] (pu[i] premultiplied by pz[i] above)
		//px[1]*gux + py[1]*guy + 1*guo = pu[1]*pz[1]
		//px[2]*gux + py[2]*guy + 1*guo = pu[2]*pz[2]
		//
		//px[0]*gvx + py[0]*gvy + 1*gvo = pv[0]*pz[0] (pv[i] premultiplied by pz[i] above)
		//px[1]*gvx + py[1]*gvy + 1*gvo = pv[1]*pz[1]
		//px[2]*gvx + py[2]*gvy + 1*gvo = pv[2]*pz[2]
	pu[0] *= pz[0]; pu[1] *= pz[1]; pu[2] *= pz[2];
	pv[0] *= pz[0]; pv[1] *= pz[1]; pv[2] *= pz[2];
	ox = py[1]-py[2]; oy = py[2]-py[0]; oz = py[0]-py[1];
	r = 1.0 / (ox*px[0] + oy*px[1] + oz*px[2]);
	dx = (ox*pz[0] + oy*pz[1] + oz*pz[2])*r;
	ux = (ox*pu[0] + oy*pu[1] + oz*pu[2])*r;
	vx = (ox*pv[0] + oy*pv[1] + oz*pv[2])*r;
	ox = px[2]-px[1]; oy = px[0]-px[2]; oz = px[1]-px[0];
	dy = (ox*pz[0] + oy*pz[1] + oz*pz[2])*r;
	uy = (ox*pu[0] + oy*pu[1] + oz*pu[2])*r;
	vy = (ox*pv[0] + oy*pv[1] + oz*pv[2])*r;
	db = pz[0] - px[0]*dx - py[0]*dy;
	ub = pu[0] - px[0]*ux - py[0]*uy;
	vb = pv[0] - px[0]*vx - py[0]*vy;

#if 1
		//Make sure k's are in good range for conversion to integers...
	t = fabs(ux);
	if (fabs(uy) > t) t = fabs(uy);
	if (fabs(ub) > t) t = fabs(ub);
	if (fabs(vx) > t) t = fabs(vx);
	if (fabs(vy) > t) t = fabs(vy);
	if (fabs(vb) > t) t = fabs(vb);
	if (fabs(dx) > t) t = fabs(dx);
	if (fabs(dy) > t) t = fabs(dy);
	if (fabs(db) > t) t = fabs(db);
	scaler = -268435456.0 / t;
	ux *= scaler; uy *= scaler; ub *= scaler;
	vx *= scaler; vy *= scaler; vb *= scaler;
	dx *= scaler; dy *= scaler; db *= scaler;
	ftol(dx,&ddi);
	uvmax = (rysiz-1)*rbpl + (rxsiz<<2);

	scaler = 1.f/scaler; t = dx*scaler;
	if (cputype&(1<<25))
	{
		#if defined( __GNUC__ )
		__asm__ __volatile__ //SSE
		(
			                            //xmm6: -,-,-,dx*scaler
			"shufps	$0, %[x6], %[x6]\n" //xmm6: dx*scaler,dx*scaler,dx*scaler,dx*scaler
			"movaps	%[x6], %[x7]\n"     //xmm7: dx*scaler,dx*scaler,dx*scaler,dx*scaler
			"mulps	%c[mulv], %[x6]\n"  //xmm6: dx*scaler*3,dx*scaler*2,dx*scaler*1,0
			"mulps	%c[four], %[x6]\n"  //xmm7: dx*scaler*4,dx*scaler*4,dx*scaler*4,dx*scaler*4
			:     "=x" (reg6), [x7] "=x" (reg7)
			: [x6] "0" (t),
			  [mulv] "p" (&dpqmulval), [four] "p" (&dpqfour)
			:
		);
		#elif defined(_MSC_VER)
		_asm //SSE
		{
			movss	xmm6, t         //xmm6: -,-,-,dx*scaler
			shufps	xmm6, xmm6, 0   //xmm6: dx*scaler,dx*scaler,dx*scaler,dx*scaler
			movaps	xmm7, xmm6      //xmm7: dx*scaler,dx*scaler,dx*scaler,dx*scaler
			mulps	xmm6, dpqmulval //xmm6: dx*scaler*3,dx*scaler*2,dx*scaler*1,0
			mulps	xmm7, dpqfour   //xmm7: dx*scaler*4,dx*scaler*4,dx*scaler*4,dx*scaler*4
		}
		#endif
	}
	else { dpq3dn[0] = 0; dpq3dn[1] = t; dpq3dn[2] = dpq3dn[3] = t+t; } //3DNow!
#endif

	imin = (py[1]<py[0]); imax = 1-imin;
	for(i=n-1;i>1;i--)
	{
		if (py[i] < py[imin]) imin = i;
		if (py[i] > py[imax]) imax = i;
	}

	i = imax;
	do
	{
		j = i+1; if (j >= n) j = 0;
			//offset would normally be -.5, but need to bias by +1.0
		ftol(py[j]+.5,&sy); if (sy < 0) sy = 0;
		ftol(py[i]+.5,&sy1); if (sy1 > yres_voxlap) sy1 = yres_voxlap;
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
		j = i+1; if (j >= n) j = 0;
			//offset would normally be -.5, but need to bias by +1.0
		ftol(py[i]+.5,&sy); if (sy < 0) sy = 0;
		ftol(py[j]+.5,&sy1); if (sy1 > yres_voxlap) sy1 = yres_voxlap;
		if (sy1 > sy)
		{
			ftol((px[j]-px[i])*4096.0/(py[j]-py[i]),&xi);
			ftol(((float)sy-py[i])*(float)xi + px[i]*4096.0 + 4096.0,&x);
			for(;sy<sy1;sy++,x+=xi)
			{
				sx = lastx[sy]; if (sx < 0) sx = 0;
				sxe = (x>>12); if (sxe > xres_voxlap) sxe = xres_voxlap;
				if (sx >= sxe) continue;
				p  = (long *)(sy*bytesperline+(sx<<2)+frameplace);
				pe = (long *)(sy*bytesperline+(sxe<<2)+frameplace);
			#if 0
					//Brute force
				do
				{
					f = 1.f/(dx*(float)sx + dy*(float)sy + db);
					if (f < *(float *)(((long)p)+zbufoff))
					{
						*(float *)(((long)p)+zbufoff) = f;
						ftol((ux*(float)sx + uy*(float)sy + ub)*f-.5,&iu);
						ftol((vx*(float)sx + vy*(float)sy + vb)*f-.5,&iv);
						if ((unsigned long)iu >= rxsiz) iu = 0;
						if ((unsigned long)iv >= rysiz) iv = 0;
						p[0] = *(long *)(iv*rbpl+(iu<<2)+rpic);
					}
					p++; sx++;
				} while (p < pe);
			#else
					//Optimized (in C) hyperbolic texture-mapping (Added Z-buffer using SSE/3DNow! for recip's)
				t = dx*(float)sx + dy*(float)sy + db; r = 1.0 / t;
				u = ux*(float)sx + uy*(float)sy + ub; ftol(u*r-.5,&iu);
				v = vx*(float)sx + vy*(float)sy + vb; ftol(v*r-.5,&iv);
				ftol(t,&dd);
				ftol((float)iu*dx - ux,&uui); ftol((float)iu*t - u,&uu);
				ftol((float)iv*dx - vx,&vvi); ftol((float)iv*t - v,&vv);
				if (ux*t < u*dx) k =    -4; else { uui = -(uui+ddi); uu = -(uu+dd); k =    4; }
				if (vx*t < v*dx) l = -rbpl; else { vvi = -(vvi+ddi); vv = -(vv+dd); l = rbpl; }
				iu = iv*rbpl + (iu<<2);

				t *= scaler;

				if (cputype&(1<<25)) {
					#ifdef __GNUC__ //gcc inline asm
					__asm__ __volatile__
					(
						"sub	%[c], %[a]\n"
						"add	%[distlut], %[c]\n" //distlut not deref

						//dd+ddi*3 dd+ddi*2 dd+ddi*1 dd+ddi*0
						"shufps	$0, %[x0], %[x0]\n"
						"addps	%[x6], %[x0]\n"
					".Ldpqbegsse:\n"
						"rcpps	%[x0], %[x1]\n"
						"addps  %[x7], %[x0]\n"
						"movaps	%[x1], (%[a],%[c])\n"
						"add	$16, %[a]\n"
						"jl	.Ldpqbegsse\n"
						"femms\n"
						:
						: [a] "r" (0), [c] "r" (4*(sxe - sx)),
						  [distlut] "p" (&dpqdistlut),
						  [x0] "x" (t), [x1] "x" (0),
						  [x6] "x" (reg6), [x7] "x" (reg7)
						:
					);
					#endif
					#ifdef _MSC_VER //msvc inline asm
					_asm
					{
						mov	ecx, sxe
						sub	ecx, sx
						xor	eax, eax
						lea	ecx, [ecx*4]
						sub	eax, ecx
						add	ecx, offset dpqdistlut

						movss	xmm0, t //dd+ddi*3 dd+ddi*2 dd+ddi*1 dd+ddi*0
						shufps	xmm0, xmm0, 0
						addps	xmm0, xmm6
					dpqbegsse:
						rcpps	xmm1, xmm0
						addps	xmm0, xmm7
						movaps	[eax+ecx], xmm1
						add	eax, 16
						jl	short dpqbegsse
						femms
					}
					#endif
				}
				else
				{
					#ifdef __GNUC__ //gcc inline asm
					__asm__ __volatile__
					(
						"sub	%[c], %[a]\n"
						"add	%[distlut], %[c]\n" //distlut not deref

						"punpckldq	%[y0], %[y0]\n"
						"pfadd	%c[dpq],%[y0]\n"
					".Ldpqbeg3dn:\n"
						"pswapd	%[y0], %[y2]\n"
						"pfrcp	%[y0], %[y1]\n"     //mm1: 1/mm0l 1/mm0l
						"pfrcp	%[y2], %[y2]\n"     //mm2: 1/mm0h 1/mm0h
						"punpckldq	%[y2], %[y1]\n" //mm1: 1/mm0h 1/mm0l
						"pfadd	%[y7], %[y0]\n"
						"movq	%[y0], (%[a],%[c])\n"
						"add	$8, %[a]\n"
						"jl	.Ldpqbeg3dn\n"
						"femms\n"
						:
						: [a] "r" (0), [c] "r" (4*(sxe - sx)),
						  [distlut] "p" (&dpqdistlut), [dpq] "p" (&dpq3dn), //(lpoint2d*)
						  [y1] "y" (0), [y2] "y" (0),
						  [y0] "y" (t), [y7] "y" (dpq3dn[8])
						:
					);
					#endif
					#ifdef _MSC_VER //msvc inline asm
					_asm
					{
						mov	ecx, sxe
						sub	ecx, sx
						xor	eax, eax
						lea	ecx, [ecx*4]
						sub	eax, ecx
						add	ecx, offset dpqdistlut

						movd	mm0, t           //dd+ddi*1 dd+ddi*0
						punpckldq	mm0, mm0
						pfadd	mm0, dpq3dn[0]
						movq	mm7, dpq3dn[8]
					dpqbeg3dn:
						pswapd	mm2, mm0
						pfrcp	mm1, mm0         //mm1: 1/mm0l 1/mm0l
						pfrcp	mm2, mm2         //mm2: 1/mm0h 1/mm0h
						punpckldq	mm1, mm2     //mm1: 1/mm0h 1/mm0l
						pfadd	mm0, mm7
						movq	[eax+ecx], mm1
						add	eax, 8
						jl	short dpqbeg3dn
						femms
					}
					#endif
				}

				distlutoffs = ((long)dpqdistlut)-((long)p);
				do
				{
				#if (USEZBUFFER != 0)
					if (*(long *)(((long)p)+zbufoff) > *(long *)(((long)p)+distlutoffs))
					{
						*(long *)(((long)p)+zbufoff) = *(long *)(((long)p)+distlutoffs);
				#endif
						if ((unsigned long)iu < uvmax) p[0] = *(long *)(rpic+iu);
				#if (USEZBUFFER != 0)
					}
				#endif
					dd += ddi;
					uu += uui; while (uu < 0) { iu += k; uui -= ddi; uu -= dd; }
					vv += vvi; while (vv < 0) { iu += l; vvi -= ddi; vv -= dd; }
					p++;
				} while (p < pe);
			#endif
			}
		}
		i = j;
	} while (i != imax);
}

	//WARNING! Make sure to set vx5.colfunc before calling this function!
	//This function is here for simplicity only - it is NOT optimal.
	//
	//   -1: set air
	//   -2: use vx5.colfunc
void setcube (long px, long py, long pz, long col)
{
	long bakcol, (*bakcolfunc)(lpoint3d *), *lptr;

	vx5.minx = px; vx5.maxx = px+1;
	vx5.miny = py; vx5.maxy = py+1;
	vx5.minz = pz; vx5.maxz = pz+1;
	if ((unsigned long)pz >= MAXZDIM) return;
	if ((unsigned long)col >= (unsigned long)0xfffffffe) //-1 or -2
	{
		lptr = scum2(px,py);
		if (col == -1) delslab(lptr,pz,pz+1); else insslab(lptr,pz,pz+1);
		scum2finish();
		updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,col);
		return;
	}

	bakcol = getcube(px,py,pz);
	if (bakcol == 1) return; //Unexposed solid
	if (bakcol != 0) //Not 0 (air)
		*(long *)bakcol = col;
	else
	{
		bakcolfunc = vx5.colfunc; bakcol = vx5.curcol;
		vx5.colfunc = curcolfunc; vx5.curcol = col;
		insslab(scum2(px,py),pz,pz+1); scum2finish();
		vx5.colfunc = bakcolfunc; vx5.curcol = bakcol;
	}
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,0);
}

void settri (point3d *p0, point3d *p1, point3d *p2, long bakit)
{
	point3d n;
	float f, x0, y0, z0, x1, y1, z1, rx, ry, k0, k1;
	long i, x, y, z, iz0, iz1, minx, maxx, miny, maxy;

	if (p0->x < p1->x) { x0 = p0->x; x1 = p1->x; } else { x0 = p1->x; x1 = p0->x; }
	if (p2->x < x0) x0 = p2->x;
	if (p2->x > x1) x1 = p2->x;
	if (p0->y < p1->y) { y0 = p0->y; y1 = p1->y; } else { y0 = p1->y; y1 = p0->y; }
	if (p2->y < y0) y0 = p2->y;
	if (p2->y > y1) y1 = p2->y;
	if (p0->z < p1->z) { z0 = p0->z; z1 = p1->z; } else { z0 = p1->z; z1 = p0->z; }
	if (p2->z < z0) z0 = p2->z;
	if (p2->z > z1) z1 = p2->z;

	ftol(x0-.5,&minx); ftol(y0-.5,&miny);
	ftol(x1-.5,&maxx); ftol(y1-.5,&maxy);
	vx5.minx = minx; vx5.maxx = maxx+1;
	vx5.miny = miny; vx5.maxy = maxy+1;
	ftol(z0-.5,&vx5.minz); ftol(z1+.5,&vx5.maxz);
	if (bakit) voxbackup(minx,miny,maxx+1,maxy+1,bakit);

	for(i=miny;i<=maxy;i++) { min0[i] = 0x7fffffff; max0[i] = 0x80000000; }
	for(i=minx;i<=maxx;i++) { min1[i] = 0x7fffffff; max1[i] = 0x80000000; }
	for(i=miny;i<=maxy;i++) { min2[i] = 0x7fffffff; max2[i] = 0x80000000; }

	canseerange(p0,p1);
	canseerange(p1,p2);
	canseerange(p2,p0);

	n.x = (p1->z-p0->z)*(p2->y-p1->y) - (p1->y-p0->y) * (p2->z-p1->z);
	n.y = (p1->x-p0->x)*(p2->z-p1->z) - (p1->z-p0->z) * (p2->x-p1->x);
	n.z = (p1->y-p0->y)*(p2->x-p1->x) - (p1->x-p0->x) * (p2->y-p1->y);
	f = 1.0 / sqrt(n.x*n.x + n.y*n.y + n.z*n.z); if (n.z < 0) f = -f;
	n.x *= f; n.y *= f; n.z *= f;

	if (n.z > .01)
	{
		f = -1.0 / n.z; rx = n.x*f; ry = n.y*f;
		k0 = ((n.x>=0)-p0->x)*rx + ((n.y>=0)-p0->y)*ry - ((n.z>=0)-p0->z) + .5;
		k1 = ((n.x< 0)-p0->x)*rx + ((n.y< 0)-p0->y)*ry - ((n.z< 0)-p0->z) - .5;
	}
	else { rx = 0; ry = 0; k0 = -2147000000.0; k1 = 2147000000.0; }

	for(y=miny;y<=maxy;y++)
		for(x=min0[y];x<=max0[y];x++)
		{
			f = (float)x*rx + (float)y*ry; ftol(f+k0,&iz0); ftol(f+k1,&iz1);
			if (iz0 < min1[x]) iz0 = min1[x];
			if (iz1 > max1[x]) iz1 = max1[x];
			if (iz0 < min2[y]) iz0 = min2[y];
			if (iz1 > max2[y]) iz1 = max2[y];

				//set: (x,y,iz0) to (x,y,iz1) (inclusive)
			insslab(scum2(x,y),iz0,iz1+1);
	}
	scum2finish();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,0);
}

	//Known problems:
	//1. Need to test faces for intersections on p1<->p2 line (not just edges)
	//2. Doesn't guarantee that hit point/line is purely air (but very close)
	//3. Piescan is more useful for parts of rope code :/
static long tripind[24] = {0,4,1,5,2,6,3,7,0,2,1,3,4,6,5,7,0,1,2,3,4,5,6,7};
long triscan (point3d *p0, point3d *p1, point3d *p2, point3d *hit, lpoint3d *lhit)
{
	point3d n, d[8], cp2;
	float f, g, x0, x1, y0, y1, rx, ry, k0, k1, fx, fy, fz, pval[8];
	long i, j, k, x, y, z, iz0, iz1, minx, maxx, miny, maxy, didhit;

	didhit = 0;

	if (p0->x < p1->x) { x0 = p0->x; x1 = p1->x; } else { x0 = p1->x; x1 = p0->x; }
	if (p2->x < x0) x0 = p2->x;
	if (p2->x > x1) x1 = p2->x;
	if (p0->y < p1->y) { y0 = p0->y; y1 = p1->y; } else { y0 = p1->y; y1 = p0->y; }
	if (p2->y < y0) y0 = p2->y;
	if (p2->y > y1) y1 = p2->y;
	ftol(x0-.5,&minx); ftol(y0-.5,&miny);
	ftol(x1-.5,&maxx); ftol(y1-.5,&maxy);
	for(i=miny;i<=maxy;i++) { min0[i] = 0x7fffffff; max0[i] = 0x80000000; }
	for(i=minx;i<=maxx;i++) { min1[i] = 0x7fffffff; max1[i] = 0x80000000; }
	for(i=miny;i<=maxy;i++) { min2[i] = 0x7fffffff; max2[i] = 0x80000000; }

	canseerange(p0,p1);
	canseerange(p1,p2);
	canseerange(p2,p0);

	n.x = (p1->z-p0->z)*(p2->y-p1->y) - (p1->y-p0->y) * (p2->z-p1->z);
	n.y = (p1->x-p0->x)*(p2->z-p1->z) - (p1->z-p0->z) * (p2->x-p1->x);
	n.z = (p1->y-p0->y)*(p2->x-p1->x) - (p1->x-p0->x) * (p2->y-p1->y);
	f = 1.0 / sqrt(n.x*n.x + n.y*n.y + n.z*n.z); if (n.z < 0) f = -f;
	n.x *= f; n.y *= f; n.z *= f;

	if (n.z > .01)
	{
		f = -1.0 / n.z; rx = n.x*f; ry = n.y*f;
		k0 = ((n.x>=0)-p0->x)*rx + ((n.y>=0)-p0->y)*ry - ((n.z>=0)-p0->z) + .5;
		k1 = ((n.x< 0)-p0->x)*rx + ((n.y< 0)-p0->y)*ry - ((n.z< 0)-p0->z) - .5;
	}
	else { rx = 0; ry = 0; k0 = -2147000000.0; k1 = 2147000000.0; }

	cp2.x = p2->x; cp2.y = p2->y; cp2.z = p2->z;

	for(y=miny;y<=maxy;y++)
		for(x=min0[y];x<=max0[y];x++)
		{
			f = (float)x*rx + (float)y*ry; ftol(f+k0,&iz0); ftol(f+k1,&iz1);
			if (iz0 < min1[x]) iz0 = min1[x];
			if (iz1 > max1[x]) iz1 = max1[x];
			if (iz0 < min2[y]) iz0 = min2[y];
			if (iz1 > max2[y]) iz1 = max2[y];
			for(z=iz0;z<=iz1;z++)
			{
				if (!isvoxelsolid(x,y,z)) continue;

				for(i=0;i<8;i++)
				{
					d[i].x = (float)(( i    &1)+x);
					d[i].y = (float)(((i>>1)&1)+y);
					d[i].z = (float)(((i>>2)&1)+z);
					pval[i] = (d[i].x-p0->x)*n.x + (d[i].y-p0->y)*n.y + (d[i].z-p0->z)*n.z;
				}
				for(i=0;i<24;i+=2)
				{
					j = tripind[i+0];
					k = tripind[i+1];
					if (((*(long *)&pval[j])^(*(long *)&pval[k])) < 0)
					{
						f = pval[j]/(pval[j]-pval[k]);
						fx = (d[k].x-d[j].x)*f + d[j].x;
						fy = (d[k].y-d[j].y)*f + d[j].y;
						fz = (d[k].z-d[j].z)*f + d[j].z;

							//         (p0->x,p0->y,p0->z)
							//             _|     |_
							//           _|     .   |_
							//         _|  (fx,fy,fz) |_
							//       _|                 |_
							//(p1->x,p1->y,p1->z)-.----(cp2.x,cp2.y,cp2.z)

						if ((fabs(n.z) > fabs(n.x)) && (fabs(n.z) > fabs(n.y)))
						{ //x,y
						  // ix = p1->x + (cp2.x-p1->x)*t;
						  // iy = p1->y + (cp2.y-p1->y)*t;
						  //(iz = p1->z + (cp2.z-p1->z)*t;)
						  // ix = p0->x + (fx-p0->x)*u;
						  // iy = p0->y + (fy-p0->y)*u;
						  // (p1->x-cp2.x)*t + (fx-p0->x)*u = p1->x-p0->x;
						  // (p1->y-cp2.y)*t + (fy-p0->y)*u = p1->y-p0->y;

							f = (p1->x-cp2.x)*(fy-p0->y) - (p1->y-cp2.y)*(fx-p0->x);
							if ((*(long *)&f) == 0) continue;
							f = 1.0 / f;
							g = ((p1->x-cp2.x)*(p1->y-p0->y) - (p1->y-cp2.y)*(p1->x-p0->x))*f;
							//NOTE: The following trick assumes g not * or / by f!
							//if (((*(long *)&g)-(*(long *)&f))^(*(long *)&f)) >= 0) continue;
							if ((*(long *)&g) < 0x3f800000) continue;
							g = ((p1->x-p0->x)*(fy-p0->y) - (p1->y-p0->y)*(fx-p0->x))*f;
						}
						else if (fabs(n.y) > fabs(n.x))
						{ //x,z
							f = (p1->x-cp2.x)*(fz-p0->z) - (p1->z-cp2.z)*(fx-p0->x);
							if ((*(long *)&f) == 0) continue;
							f = 1.0 / f;
							g = ((p1->x-cp2.x)*(p1->z-p0->z) - (p1->z-cp2.z)*(p1->x-p0->x))*f;
							if ((*(long *)&g) < 0x3f800000) continue;
							g = ((p1->x-p0->x)*(fz-p0->z) - (p1->z-p0->z)*(fx-p0->x))*f;
						}
						else
						{ //y,z
							f = (p1->y-cp2.y)*(fz-p0->z) - (p1->z-cp2.z)*(fy-p0->y);
							if ((*(long *)&f) == 0) continue;
							f = 1.0 / f;
							g = ((p1->y-cp2.y)*(p1->z-p0->z) - (p1->z-cp2.z)*(p1->y-p0->y))*f;
							if ((*(long *)&g) < 0x3f800000) continue;
							g = ((p1->y-p0->y)*(fz-p0->z) - (p1->z-p0->z)*(fy-p0->y))*f;
						}
						if ((*(unsigned long *)&g) >= 0x3f800000) continue;
						(hit->x) = fx; (hit->y) = fy; (hit->z) = fz;
						(lhit->x) = x; (lhit->y) = y; (lhit->z) = z; didhit = 1;
						(cp2.x) = (cp2.x-p1->x)*g + p1->x;
						(cp2.y) = (cp2.y-p1->y)*g + p1->y;
						(cp2.z) = (cp2.z-p1->z)*g + p1->z;
					}
				}
			}
		}
	return(didhit);
}

#endif //KVOXEL_MODELLING_H
