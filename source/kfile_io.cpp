#include "kfile_io.h"
#include "kplib.h"
#include "ksnippits.h"
//------------------------- SXL parsing code begins --------------------------

/**
 * Loads and begins parsing of an .SXL file. Always call this first before
 * using parspr().
 * @param sxlnam .SXL filename
 * @param vxlnam pointer to .VXL filename (written by loadsxl)
 * @param vxlnam pointer to .SKY filename (written by loadsxl)
 * @param globst pointer to global user string. You parse this yourself!
 *               You can edit this in Voxed by pressing F6.
 * @return 0: loadsxl failed (file not found or malloc failed)
 *         1: loadsxl successful; call parspr()!
 */
long loadsxl (const char *sxlnam, char **vxlnam, char **skynam, char **globst)
{
	long j, k, m, n;

		//NOTE: MUST buffer file because insertsprite uses kz file code :/
	if (!kzopen(sxlnam)) return(0);
	sxlparslen = kzfilelength();
	if (sxlbuf) { free(sxlbuf); sxlbuf = 0; }
	if (!(sxlbuf = (char *)malloc(sxlparslen))) return(0);
	kzread(sxlbuf,sxlparslen);
	kzclose();

	j = n = 0;

		//parse vxlnam
	(*vxlnam) = &sxlbuf[j];
	while ((sxlbuf[j]!=13)&&(sxlbuf[j]!=10) && (j < sxlparslen)) j++; sxlbuf[j++] = 0;
	while (((sxlbuf[j]==13)||(sxlbuf[j]==10)) && (j < sxlparslen)) j++;

		//parse skynam
	(*skynam) = &sxlbuf[j];
	while ((sxlbuf[j]!=13)&&(sxlbuf[j]!=10) && (j < sxlparslen)) j++; sxlbuf[j++] = 0;
	while (((sxlbuf[j]==13)||(sxlbuf[j]==10)) && (j < sxlparslen)) j++;

		//parse globst
	m = n = j; (*globst) = &sxlbuf[n];
	while (((sxlbuf[j] == ' ') || (sxlbuf[j] == 9)) && (j < sxlparslen))
	{
		j++;
		while ((sxlbuf[j]!=13) && (sxlbuf[j]!=10) && (j < sxlparslen)) sxlbuf[n++] = sxlbuf[j++];
		sxlbuf[n++] = 13; j++;
		while (((sxlbuf[j]==13) || (sxlbuf[j]==10)) && (j < sxlparslen)) j++;
	}
	if (n > m) sxlbuf[n-1] = 0; else (*globst) = &nullst;

		//Count number of sprites in .SXL file (helpful for GAME)
	sxlparspos = j;
	return(1);
}
/**
 * Loads a sky into memory. Sky must be PNG,JPG,TGA,GIF,BMP,PCX formatted as
 * a Mercator projection on its side. This means x-coordinate is latitude
 * and y-coordinate is longitude. Loadsky() can be called at any time.
 *
 * If for some reason you don't want to load a textured sky, you call call
 * loadsky with these 2 built-in skies:
 * loadsky("BLACK");  //pitch black
 * loadsky("BLUE");   //a cool ramp of bright blue to blue to greenish
 *
 * @param skyfilnam the name of the image to load
 * @return -1:bad, 0:good
 */
long loadsky (const char *skyfilnam)
{
	long x, y, xoff, yoff;
	float ang, f;

	if (skypic) { free((void *)skypic); skypic = skyoff = 0; }
	xoff = yoff = 0;

	if (!strcasecmp(skyfilnam,"BLACK")) return(0);
	if (!strcasecmp(skyfilnam,"BLUE")) goto loadbluesky;

	kpzload(skyfilnam,(int *)&skypic,(int *)&skybpl,(int *)&skyxsiz,(int *)&skyysiz);
	if (!skypic)
	{
		long r, g, b, *p;
loadbluesky:;
			//Load default sky
		skyxsiz = 512; skyysiz = 1; skybpl = skyxsiz*4;
		if (!(skypic = (long)malloc(skyysiz*skybpl))) return(-1);

		p = (long *)skypic; y = skyxsiz*skyxsiz;
		for(x=0;x<=(skyxsiz>>1);x++)
		{
			p[x] = ((((x*1081 - skyxsiz*252)*x)/y + 35)<<16)+
					 ((((x* 950 - skyxsiz*198)*x)/y + 53)<<8)+
					  (((x* 439 - skyxsiz* 21)*x)/y + 98);
		}
		p[skyxsiz-1] = 0x50903c;
		r = ((p[skyxsiz>>1]>>16)&255);
		g = ((p[skyxsiz>>1]>>8)&255);
		b = ((p[skyxsiz>>1])&255);
		for(x=(skyxsiz>>1)+1;x<skyxsiz;x++)
		{
			p[x] = ((((0x50-r)*(x-(skyxsiz>>1)))/(skyxsiz-1-(skyxsiz>>1))+r)<<16)+
					 ((((0x90-g)*(x-(skyxsiz>>1)))/(skyxsiz-1-(skyxsiz>>1))+g)<<8)+
					 ((((0x3c-b)*(x-(skyxsiz>>1)))/(skyxsiz-1-(skyxsiz>>1))+b));
		}
		y = skyxsiz*skyysiz;
		for(x=skyxsiz;x<y;x++) p[x] = p[x-skyxsiz];
	}

		//Initialize look-up table for longitudes
	if (skylng) free((void *)skylng);
	if (!(skylng = (point2d *)malloc(skyysiz*8))) return(-1);
	f = PI*2.0 / ((float)skyysiz);
	for(y=skyysiz-1;y>=0;y--)
		fcossin((float)y*f+PI,&skylng[y].x,&skylng[y].y);
	skylngmul = (float)skyysiz/(PI*2);
		//This makes those while loops in gline() not lockup when skyysiz==1
	if (skyysiz == 1) { skylng[0].x = 0; skylng[0].y = 0; }

		//Initialize look-up table for latitudes
	if (skylat) free((void *)skylat);
	if (!(skylat = (long *)malloc(skyxsiz*4))) return(-1);
	f = PI*.5 / ((float)skyxsiz);
	for(x=skyxsiz-1;x;x--)
	{
		ang = (float)((x<<1)-skyxsiz)*f;
		ftol(cos(ang)*32767.0,&xoff);
		ftol(sin(ang)*32767.0,&yoff);
		skylat[x] = (xoff<<16)+((-yoff)&65535);
	}
	skylat[0] = 0; //Hack to make sure assembly index never goes < 0
	skyxsiz--; //Hack for assembly code

	return(0);
}



/**
 * Saves a native Voxlap5 .VXL file & specified position to disk
 * @param filnam .VXL map formatted like this: "UNTITLED.VXL"
 * @param ipo default starting camera position
 * @param ist RIGHT unit vector
 * @param ihe DOWN unit vector
 * @param ifo FORWARD unit vector
 * @return 0:bad, 1:good
 */
long savevxl (const char *savfilnam, dpoint3d *ipo, dpoint3d *ist, dpoint3d *ihe, dpoint3d *ifo)
{
	FILE *fil;
	long i;

	if (!(fil = fopen(savfilnam,"wb"))) return(0);
	i = 0x09072000; fwrite(&i,4,1,fil);  //Version
	i = VSID; fwrite(&i,4,1,fil);
	i = VSID; fwrite(&i,4,1,fil);
	fwrite(ipo,24,1,fil);
	fwrite(ist,24,1,fil);
	fwrite(ihe,24,1,fil);
	fwrite(ifo,24,1,fil);
	for(i=0;i<VSID*VSID;i++) fwrite((void *)sptr[i],slng(sptr[i]),1,fil);
	fclose(fil);
	return(1);
}


/**
 * If loadsxl returns a 1, then you should call parspr with a while loop
 * that terminates when the return value is 0.
 * @param spr pointer to sprite structure (written by parspr) You allocate
 *            the vx5sprite, & parspr fills in the position&orientation.
 * @param userst pointer to user string associated with the given sprite.
 *               You can edit this in Voxed by right-clicking the sprite.
 * @return pointer to .KV6 filename OR NULL if no more sprites left.
 *         You must load the .KV6 to memory yourself by doing:
 *         char *kv6filename = parspr(...)
 *         if (kv6filename) spr->voxnum = getkv6(kv6filename);
 */
char *parspr (vx5sprite *spr, char **userst)
{
	float f;
	long j, k, m, n;
	char *namptr;

	j = sxlparspos; //unnecessary temp variable (to shorten code)

		//Automatically free temp sxlbuf when done reading sprites
	if (((j+2 < sxlparslen) && (sxlbuf[j] == 'e') && (sxlbuf[j+1] == 'n') && (sxlbuf[j+2] == 'd') &&
		((j+3 == sxlparslen) || (sxlbuf[j+3] == 13) || (sxlbuf[j+3] == 10))) || (j > sxlparslen))
		return(0);

		//parse kv6name
	for(k=j;(sxlbuf[k]!=',') && (k < sxlparslen);k++); sxlbuf[k] = 0;
	namptr = &sxlbuf[j]; j = k+1;

		//parse 12 floats
	for(m=0;m<12;m++)
	{
		if (m < 11) { for(k=j;(sxlbuf[k]!=',') && (k < sxlparslen);k++); }
		else { for(k=j;(sxlbuf[k]!=13) && (sxlbuf[k]!=10) && (k < sxlparslen);k++); }

		sxlbuf[k] = 0; f = atof(&sxlbuf[j]); j = k+1;
		switch(m)
		{
			case  0: spr->p.x = f; break;
			case  1: spr->p.y = f; break;
			case  2: spr->p.z = f; break;
			case  3: spr->s.x = f; break;
			case  4: spr->s.y = f; break;
			case  5: spr->s.z = f; break;
			case  6: spr->h.x = f; break;
			case  7: spr->h.y = f; break;
			case  8: spr->h.z = f; break;
			case  9: spr->f.x = f; break;
			case 10: spr->f.y = f; break;
			case 11: spr->f.z = f; break;
			default: _gtfo(); //tells MSVC default can't be reached
		}
	}
	while (((sxlbuf[j]==13) || (sxlbuf[j]==10)) && (j < sxlparslen)) j++;

	spr->flags = 0;

		//parse userst
	m = n = j; (*userst) = &sxlbuf[n];
	while (((sxlbuf[j] == ' ') || (sxlbuf[j] == 9)) && (j < sxlparslen))
	{
		j++;
		while ((sxlbuf[j]!=13) && (sxlbuf[j]!=10) && (j < sxlparslen)) sxlbuf[n++] = sxlbuf[j++];
		sxlbuf[n++] = 13; j++;
		while (((sxlbuf[j]==13) || (sxlbuf[j]==10)) && (j < sxlparslen)) j++;
	}
	if (n > m) sxlbuf[n-1] = 0; else (*userst) = &nullst;

	sxlparspos = j; //unnecessary temp variable (for short code)
	return(namptr);
}

/**
 * Loads a Comanche format map into memory.
 * @param filename Should be formatted like this: "C1.DTA"
 *                 It replaces the first letter with C&D to get both height&color
 * @return 0:bad, 1:good
 */
long loaddta (const char *filename, dpoint3d *ipo, dpoint3d *ist, dpoint3d *ihe, dpoint3d *ifo)
{
	long i, j, p, leng, minz = 255, maxz = 0, h[5], longpal[256];
	char dat, *dtahei, *dtacol, *v, dafilename[MAX_PATH];
	float f;
	FILE *fp;

	if (!vbuf) { vbuf = (long *)malloc((VOXSIZ>>2)<<2); if (!vbuf) evilquit("vbuf malloc failed"); }
	if (!vbit) { vbit = (long *)malloc((VOXSIZ>>7)<<2); if (!vbit) evilquit("vbuf malloc failed"); }

	if (VSID != 1024) return(0);
	v = (char *)(&vbuf[1]); //1st dword for voxalloc compare logic optimization

	strcpy(dafilename,filename);

	dtahei = (char *)(&vbuf[(VOXSIZ-2097152)>>2]);
	dtacol = (char *)(&vbuf[(VOXSIZ-1048576)>>2]);

	dafilename[0] = 'd';
	if (!kzopen(dafilename)) return(0);
	kzseek(128,SEEK_SET); p = 0;
	while (p < 1024*1024)
	{
		dat = kzgetc();
		if (dat >= 192) { leng = dat-192; dat = kzgetc(); }
					  else { leng = 1; }
		dat = 255-dat;
		if (dat < minz) minz = dat;
		if (dat > maxz) maxz = dat;
		while (leng-- > 0) dtahei[p++] = dat;
	}
	kzclose();

	dafilename[0] = 'c';
	if (!kzopen(dafilename)) return(0);
	kzseek(128,SEEK_SET);
	p = 0;
	while (p < 1024*1024)
	{
		dat = kzgetc();
		if (dat >= 192) { leng = dat-192; dat = kzgetc(); }
					  else { leng = 1; }
		while (leng-- > 0) dtacol[p++] = dat;
	}

	dat = kzgetc();
	if (dat == 0xc)
		for(i=0;i<256;i++)
		{
			longpal[i] = kzgetc();
			longpal[i] = (longpal[i]<<8)+kzgetc();
			longpal[i] = (longpal[i]<<8)+kzgetc() + 0x80000000;
		}

	kzclose();

		//Fill board data
	minz = lbound(128-((minz+maxz)>>1),-minz,255-maxz);
	for(p=0;p<1024*1024;p++)
	{
		h[0] = (long)dtahei[p];
		h[1] = (long)dtahei[((p-1)&0x3ff)+((p     )&0xffc00)];
		h[2] = (long)dtahei[((p+1)&0x3ff)+((p     )&0xffc00)];
		h[3] = (long)dtahei[((p  )&0x3ff)+((p-1024)&0xffc00)];
		h[4] = (long)dtahei[((p  )&0x3ff)+((p+1024)&0xffc00)];

		j = 1;
		for(i=4;i>0;i--) if (h[i]-h[0] > j) j = h[i]-h[0];

		sptr[p] = v;
		v[0] = 0;
		v[1] = dtahei[p]+minz;
		v[2] = dtahei[p]+minz+j-1;
		v[3] = 0; //dummy (z top)
		v += 4;
		for(;j;j--) { *(long *)v = colorjit(longpal[dtacol[p]],0x70707); v += 4; }
	}

	memset(&sptr[VSID*VSID],0,sizeof(sptr)-VSID*VSID*4);
	vbiti = (((long)v-(long)vbuf)>>2); //# vbuf longs/vbit bits allocated
	clearbuf((void *)vbit,vbiti>>5,-1);
	clearbuf((void *)&vbit[vbiti>>5],(VOXSIZ>>7)-(vbiti>>5),0);
	vbit[vbiti>>5] = (1<<vbiti)-1;

	vx5.globalmass = calcglobalmass();

	ipo->x = VSID*.5; ipo->y = VSID*.5; ipo->z = 128;
	f = 0.0*PI/180.0;
	ist->x = cos(f); ist->y = sin(f); ist->z = 0;
	ihe->x = 0; ihe->y = 0; ihe->z = 1;
	ifo->x = sin(f); ifo->y = -cos(f); ifo->z = 0;

	gmipnum = 1; vx5.flstnum = 0;
	updatebbox(0,0,0,VSID,VSID,MAXZDIM,0);
	return(1);
}

/**
 * Loads a heightmap from PNG (or TGA) into memory; alpha channel is height.
 * @param filename Any 1024x1024 PNG or TGA file with alpha channel.
 * @return 0:bad, 1:good
 */
long loadpng (const char *filename, dpoint3d *ipo, dpoint3d *ist, dpoint3d *ihe, dpoint3d *ifo)
{
	unsigned long *pngdat, dat[5];
	long i, j, k, l, p, leng, minz = 255, maxz = 0;
	char *v, *buf;
	float f;
	FILE *fp;

	if (!vbuf) { vbuf = (long *)malloc((VOXSIZ>>2)<<2); if (!vbuf) evilquit("vbuf malloc failed"); }
	if (!vbit) { vbit = (long *)malloc((VOXSIZ>>7)<<2); if (!vbit) evilquit("vbuf malloc failed"); }

	if (VSID != 1024) return(0);
	v = (char *)(&vbuf[1]); //1st dword for voxalloc compare logic optimization

	if (!kzopen(filename)) return(0);
	leng = kzfilelength();
	buf = (char *)malloc(leng); if (!buf) { kzclose(); return(0); }
	kzread(buf,leng);
	kzclose();

	kpgetdim(buf,leng,(int *)&i,(int *)&j); if ((i != VSID) && (j != VSID)) { free(buf); return(0); }
	pngdat = (unsigned long *)(&vbuf[(VOXSIZ-VSID*VSID*4)>>2]);
	if (kprender(buf,leng,(long)pngdat,VSID<<2,VSID,VSID,0,0) < 0) return(0);
	free(buf);

	for(i=0;i<VSID*VSID;i++)
	{
		if ((pngdat[i]>>24) < minz) minz = (pngdat[i]>>24);
		if ((pngdat[i]>>24) > maxz) maxz = (pngdat[i]>>24);
	}

		//Fill board data
	minz = lbound(128-((minz+maxz)>>1),-minz,255-maxz);
	for(p=0;p<VSID*VSID;p++)
	{
		dat[0] = pngdat[p];
		dat[1] = pngdat[((p-1)&(VSID-1))+((p     )&((VSID-1)*VSID))];
		dat[2] = pngdat[((p+1)&(VSID-1))+((p     )&((VSID-1)*VSID))];
		dat[3] = pngdat[((p  )&(VSID-1))+((p-VSID)&((VSID-1)*VSID))];
		dat[4] = pngdat[((p  )&(VSID-1))+((p+VSID)&((VSID-1)*VSID))];

		j = 1; l = dat[0];
		for(i=4;i>0;i--)
			if (((signed long)((dat[i]>>24)-(dat[0]>>24))) > j)
				{ j = (dat[i]>>24)-(dat[0]>>24); l = dat[i]; }

		sptr[p] = v;
		v[0] = 0;
		v[1] = (pngdat[p]>>24)+minz;
		v[2] = (pngdat[p]>>24)+minz+j-1;
		v[3] = 0; //dummy (z top)
		v += 4;
		k = (pngdat[p]&0xffffff)|0x80000000;
		if (j == 2)
		{
			l = (((  l     &255)-( k     &255))>>1)      +
				 (((((l>> 8)&255)-((k>> 8)&255))>>1)<< 8) +
				 (((((l>>16)&255)-((k>>16)&255))>>1)<<16);
		}
		else if (j > 2)
		{
			l = (((  l     &255)-( k     &255))/j)      +
				 (((((l>> 8)&255)-((k>> 8)&255))/j)<< 8) +
				 (((((l>>16)&255)-((k>>16)&255))/j)<<16);
		}
		*(long *)v = k; v += 4; j--;
		while (j) { k += l; *(long *)v = colorjit(k,0x30303); v += 4; j--; }
	}

	memset(&sptr[VSID*VSID],0,sizeof(sptr)-VSID*VSID*4);
	vbiti = (((long)v-(long)vbuf)>>2); //# vbuf longs/vbit bits allocated
	clearbuf((void *)vbit,vbiti>>5,-1);
	clearbuf((void *)&vbit[vbiti>>5],(VOXSIZ>>7)-(vbiti>>5),0);
	vbit[vbiti>>5] = (1<<vbiti)-1;

	vx5.globalmass = calcglobalmass();

	ipo->x = VSID*.5; ipo->y = VSID*.5; ipo->z = 128;
	f = 0.0*PI/180.0;
	ist->x = cos(f); ist->y = sin(f); ist->z = 0;
	ihe->x = 0; ihe->y = 0; ihe->z = 1;
	ifo->x = sin(f); ifo->y = -cos(f); ifo->z = 0;

	gmipnum = 1; vx5.flstnum = 0;
	updatebbox(0,0,0,VSID,VSID,MAXZDIM,0);
	return(1);
}

/**
 * Loads a Quake 3 Arena .BSP format map into memory. First extract the map
 * from the .PAK file. NOTE: only tested with Q3DM(1,7,17) and Q3TOURNEY
 * @param filnam .BSP map formatted like this: "Q3DM17.BSP"
 * @param ipo default starting camera position
 * @param ist RIGHT unit vector
 * @param ihe DOWN unit vector
 * @param ifo FORWARD unit vector
 */
void loadbsp (const char *filnam, dpoint3d *ipo, dpoint3d *ist, dpoint3d *ihe, dpoint3d *ifo)
{
	FILE *fp;
	dpoint3d dp;
	float f, xof, yof, zof, sc, rsc;
	long numplanes, numnodes, numleafs, fpos[17], flng[17];
	long i, x, y, z, z0, z1, vcnt, *lptr, minx, miny, minz, maxx, maxy, maxz;
	char *v;

	if (!vbuf) { vbuf = (long *)malloc((VOXSIZ>>2)<<2); if (!vbuf) evilquit("vbuf malloc failed"); }
	if (!vbit) { vbit = (long *)malloc((VOXSIZ>>7)<<2); if (!vbit) evilquit("vbuf malloc failed"); }

		//Completely re-compile vbuf
	v = (char *)(&vbuf[1]); //1st dword for voxalloc compare logic optimization
	for(x=0;x<VSID;x++)
		for(y=0;y<VSID;y++)
		{
			sptr[y*VSID+x] = v; v[0] = 0; v[1] = 0; v[2] = 0; v[3] = 0; v += 4;
			(*(long *)v) = ((x^y)&15)*0x10101+0x807c7c7c; v += 4;
		}

	memset(&sptr[VSID*VSID],0,sizeof(sptr)-VSID*VSID*4);
	vbiti = (((long)v-(long)vbuf)>>2); //# vbuf longs/vbit bits allocated
	clearbuf((void *)vbit,vbiti>>5,-1);
	clearbuf((void *)&vbit[vbiti>>5],(VOXSIZ>>7)-(vbiti>>5),0);
	vbit[vbiti>>5] = (1<<vbiti)-1;

	if (!kzopen(filnam)) return;
	kzread(&i,4); if (i != 0x50534249) { kzclose(); return; }
	kzread(&i,4); if (i != 0x2e) { kzclose(); return; }
	for(i=0;i<17;i++) { kzread(&fpos[i],4); kzread(&flng[i],4); }
	kzseek(fpos[2],SEEK_SET); numplanes = flng[2]/16;
	for(i=0;i<numplanes;i++) { kzread(&q3pln[i].x,12); kzread(&q3pld[i],4); }
	kzseek(fpos[3],SEEK_SET); numnodes = flng[3]/36;
	minx = 0x7fffffff; miny = 0x7fffffff; minz = 0x7fffffff;
	maxx = 0x80000000; maxy = 0x80000000; maxz = 0x80000000;
	for(i=0;i<numnodes;i++)
	{
		kzread(&q3nod[i][0],12);
		kzread(&x,4); if (x < minx) minx = x;
		kzread(&x,4); if (x < miny) miny = x;
		kzread(&x,4); if (x < minz) minz = x;
		kzread(&x,4); if (x > maxx) maxx = x;
		kzread(&x,4); if (x > maxy) maxy = x;
		kzread(&x,4); if (x > maxz) maxz = x;
	}
	kzseek(fpos[4]+4,SEEK_SET); numleafs = flng[4]/48;
	for(i=0;i<numleafs;i++) { kzread(&q3lf[i],4); kzseek(44,SEEK_CUR); }
	kzclose();

	sc = (float)(VSID-2)/(float)(maxx-minx);
	rsc = (float)(VSID-2)/(float)(maxy-miny); if (rsc < sc) sc = rsc;
	rsc = (float)(MAXZDIM-2)/(float)(maxz-minz); if (rsc < sc) sc = rsc;
	//i = *(long *)sc; i &= 0xff800000; sc = *(float *)i;
	xof = (-(float)(minx+maxx)*sc + VSID   )*.5;
	yof = (+(float)(miny+maxy)*sc + VSID   )*.5;
	zof = (+(float)(minz+maxz)*sc + MAXZDIM)*.5;

	rsc = 1.0 / sc;
	vx5.colfunc = curcolfunc; //0<x0<x1<VSID, 0<y0<y1<VSID, 0<z0<z1<256,
	for(y=0;y<VSID;y++)
		for(x=0;x<VSID;x++)
		{
			lptr = scum2(x,y);

				//voxx = q3x*+sc + xof;
				//voxy = q3y*-sc + yof;
				//voxz = q3z*-sc + zof;
			vcnt = vlinebsp(((float)x-xof)*rsc,((float)y-yof)*-rsc,-65536.0,65536.0,q3vz);
			for(i=vcnt-2;i>0;i-=2)
			{
				ftol(-q3vz[i+1]*sc+zof,&z0); if (z0 < 0) z0 = 0;
				ftol(-q3vz[i  ]*sc+zof,&z1); if (z1 > MAXZDIM) z1 = MAXZDIM;
				delslab(lptr,z0,z1);
			}
		}
	scum2finish();

	vx5.globalmass = calcglobalmass();

		//Find a spot that isn't too close to a wall
	sc = -1; ipo->x = VSID*.5; ipo->y = VSID*.5; ipo->z = -16;
	for(i=4096;i>=0;i--)
	{
		x = (rand()%VSID); y = (rand()%VSID); z = (rand()%MAXZDIM);
		if (!isvoxelsolid(x,y,z))
		{
			rsc = findmaxcr((double)x+.5,(double)y+.5,(double)z+.5,5.0);
			if (rsc <= sc) continue;
			ipo->x = (double)x+.5; ipo->y = (double)x+.5; ipo->z = (double)x+.5;
			sc = rsc; if (sc >= 5.0) break;
		}
	}
	f = 0.0*PI/180.0;
	ist->x = cos(f); ist->y = sin(f); ist->z = 0;
	ihe->x = 0; ihe->y = 0; ihe->z = 1;
	ifo->x = sin(f); ifo->y = -cos(f); ifo->z = 0;

	gmipnum = 1; vx5.flstnum = 0;
	updatebbox(0,0,0,VSID,VSID,MAXZDIM,0);
}
