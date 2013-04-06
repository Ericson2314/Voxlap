#ifndef KCOLORS_H
#define KCOLORS_H

static long gkrand = 0;
/** Jitter the colour in i by random jitamount
 *  @param i Acolour
 *  @param jitamount Amount of jitterage
 *  @return colorjit Jittered colour
*/
long colorjit (long i, long jitamount)
{
	gkrand = (gkrand*27584621)+1;
	return((gkrand&jitamount)^i);
}

/** Brighten a given colour */
long lightvox (long i)
{
	long r, g, b;

	b = ((unsigned long)i>>24);
	r = MIN((((i>>16)&255)*b)>>7,255);
	g = MIN((((i>>8 )&255)*b)>>7,255);
	b = MIN((((i    )&255)*b)>>7,255);
	return((r<<16)+(g<<8)+b);
}

	//Note: ebx = 512 is no change
	//If PENTIUM III:1.Replace punpcklwd&punpckldq with: pshufw mm1, mm1, 0
	//               2.Use pmulhuw, shift by 8 & mul by 256
	//  :(  Can't mix with floating point
//#pragma aux colormul =
//   "movd mm0, eax"
//   "pxor mm1, mm1"
//   "punpcklbw mm0, mm1"
//   "psllw mm0, 7"
//   "movd mm1, ebx"
//   "punpcklwd mm1, mm1"
//   "punpckldq mm1, mm1"
//   "pmulhw mm0, mm1"
//   "packsswb mm0, mm0"
//   "movd eax, mm0"
//   parm [eax][ebx]
//   modify exact [eax]
//   value [eax]

long colormul (long i, long mulup8)
{
	long r, g, b;

	r = ((((i>>16)&255)*mulup8)>>8); if (r > 255) r = 255;
	g = ((((i>>8 )&255)*mulup8)>>8); if (g > 255) g = 255;
	b = ((((i    )&255)*mulup8)>>8); if (b > 255) b = 255;
	return((i&0xff000000)+(r<<16)+(g<<8)+b);
}

long curcolfunc (lpoint3d *p) { return(vx5.curcol); }
long jitcolfunc (lpoint3d *p) { return(colorjit(vx5.curcol,vx5.amount)); }

static long manycolukup[64] =
{
	  0,  1,  2,  5, 10, 15, 21, 29, 37, 47, 57, 67, 79, 90,103,115,
	127,140,152,165,176,188,198,208,218,226,234,240,245,250,253,254,
	255,254,253,250,245,240,234,226,218,208,198,188,176,165,152,140,
	128,115,103, 90, 79, 67, 57, 47, 37, 29, 21, 15, 10,  5,  2,  1
};
long manycolfunc (lpoint3d *p)
{
	return((manycolukup[p->x&63]<<16)+(manycolukup[p->y&63]<<8)+manycolukup[p->z&63]+0x80000000);
}

long sphcolfunc (lpoint3d *p)
{
	long i;
	ftol(sin((p->x+p->y+p->z-vx5.cen)*vx5.daf)*-96,&i);
	return(((i+128)<<24)|(vx5.curcol&0xffffff));
}

#define WOODXSIZ 46
#define WOODYSIZ 24
#define WOODZSIZ 24
static float wx[256], wy[256], wz[256], vx[256], vy[256], vz[256];
long woodcolfunc (lpoint3d *p)
{
	float col, u, a, f, dx, dy, dz;
	long i, c, xof, yof, tx, ty, xoff;

	if (*(long *)&wx[0] == 0)
	{
		for(i=0;i<256;i++)
		{
			wx[i] = WOODXSIZ * ((float)rand()/32768.0f-.5f) * .5f;
			wy[i] = WOODXSIZ * ((float)rand()/32768.0f-.5f) * .5f;
			wz[i] = WOODXSIZ * ((float)rand()/32768.0f-.5f) * .5f;

				//UNIFORM spherical randomization (see spherand.c)
			dz = 1.0f-(float)rand()/32768.0f*.04f;
			a = (float)rand()/32768.0f*PI*2.0f; fcossin(a,&dx,&dy);
			f = sqrt(1.0f-dz*dz); dx *= f; dy *= f;
				//??z: rings,  ?z?: vertical,  z??: horizontal (nice)
			vx[i] = dz; vy[i] = fabs(dy); vz[i] = dx;
		}
	}

		//(tx&,ty&) = top-left corner of current panel
	ty = p->y - (p->y%WOODYSIZ);
	xoff = ((ty/WOODYSIZ)*(ty/WOODYSIZ)*51721 + (p->z/WOODZSIZ)*357) % WOODXSIZ;
	tx = ((p->x+xoff) - (p->x+xoff)%WOODXSIZ) - xoff;

	xof = p->x - (tx + (WOODXSIZ>>1));
	yof = p->y - (ty + (WOODYSIZ>>1));

	c = ((((tx*429 + 4695) ^ (ty*341 + 4355) ^ 13643) * 2797) & 255);
	dx = xof - wx[c];
	dy = yof - wy[c];
	dz = (p->z%WOODZSIZ) - wz[c];

		//u = distance to center of randomly oriented cylinder
	u = vx[c]*dx + vy[c]*dy + vz[c]*dz;
	u = sqrt(dx*dx + dy*dy + dz*dz - u*u);

		//ring randomness
	u += sin((float)xof*.12 + (float)yof*.15) * .5;
	u *= (sin(u)*.05 + 1);

		//Ring function: smooth saw-tooth wave
	col = sin(u*2)*24;
	col *= pow(1.f-vx[c],.3f);

		//Thin shaded borders
	if ((p->x-tx == 0) || (p->y-ty == 0)) col -= 5;
	if ((p->x-tx == WOODXSIZ-1) || (p->y-ty == WOODYSIZ-1)) col -= 3;

	//f = col+c*.12+72; i = ftolp3(&f);
	  ftol(col+c*.12f+72.0f,&i);

	return(colormul(vx5.curcol,i<<1));
}

long gxsizcache = 0, gysizcache = 0;
long pngcolfunc (lpoint3d *p)
{
	long x, y, z, u, v;
	float fx, fy, fz, rx, ry, rz;

	if (!vx5.pic) return(vx5.curcol);
	switch(vx5.picmode)
	{
		case 0:
			x = p->x-vx5.pico.x; y = p->y-vx5.pico.y; z = p->z-vx5.pico.z;
			u = (((x&vx5.picu.x) + (y&vx5.picu.y) + (z&vx5.picu.z))^vx5.xoru);
			v = (((x&vx5.picv.x) + (y&vx5.picv.y) + (z&vx5.picv.z))^vx5.xorv);
			break;
		case 1: case 2:
			fx = (float)p->x-vx5.fpico.x;
			fy = (float)p->y-vx5.fpico.y;
			fz = (float)p->z-vx5.fpico.z;
			rx = vx5.fpicu.x*fx + vx5.fpicu.y*fy + vx5.fpicu.z*fz;
			ry = vx5.fpicv.x*fx + vx5.fpicv.y*fy + vx5.fpicv.z*fz;
			rz = vx5.fpicw.x*fx + vx5.fpicw.y*fy + vx5.fpicw.z*fz;
			ftol(atan2(ry,rx)*vx5.xoru/(PI*2),&u);
			if (vx5.picmode == 1) ftol(rz,&v);
			else ftol((atan2(rz,sqrt(rx*rx+ry*ry))/PI+.5)*vx5.ysiz,&v);
			break;
		default: //case 3:
			fx = (float)p->x-vx5.fpico.x;
			fy = (float)p->y-vx5.fpico.y;
			fz = (float)p->z-vx5.fpico.z;
			ftol(vx5.fpicu.x*fx + vx5.fpicu.y*fy + vx5.fpicu.z*fz,&u);
			ftol(vx5.fpicv.x*fx + vx5.fpicv.y*fy + vx5.fpicv.z*fz,&v);
			break;
	}
	if (((unsigned long)u-gxsizcache) >= ((unsigned long)vx5.xsiz))
	{
		if (u < 0) gxsizcache = u-(u+1)%vx5.xsiz-vx5.xsiz+1;
		else gxsizcache = u-(u%vx5.xsiz);
	}
	if (((unsigned long)v-gysizcache) >= ((unsigned long)vx5.ysiz))
	{
		if (v < 0) gysizcache = v-(v+1)%vx5.ysiz-vx5.ysiz+1;
		else gysizcache = v-(v%vx5.ysiz);
	}
	return((vx5.pic[(v-gysizcache)*(vx5.bpl>>2)+(u-gxsizcache)]&0xffffff)|0x80000000);
}

	//Special case for SETSEC & SETCEI bumpmapping (vx5.picmode == 3)
	//no safety checks, returns alpha as signed char in range: (-128 to 127)
long hpngcolfunc (point3d *p)
{
	long u, v;
	float fx, fy, fz;

	fx = p->x-vx5.fpico.x;
	fy = p->y-vx5.fpico.y;
	fz = p->z-vx5.fpico.z;
	ftol(vx5.fpicu.x*fx + vx5.fpicu.y*fy + vx5.fpicu.z*fz,&u);
	ftol(vx5.fpicv.x*fx + vx5.fpicv.y*fy + vx5.fpicv.z*fz,&v);

	if (((unsigned long)u-gxsizcache) >= ((unsigned long)vx5.xsiz))
	{
		if (u < 0) gxsizcache = u-(u+1)%vx5.xsiz-vx5.xsiz+1;
		else gxsizcache = u-(u%vx5.xsiz);
	}
	if (((unsigned long)v-gysizcache) >= ((unsigned long)vx5.ysiz))
	{
		if (v < 0) gysizcache = v-(v+1)%vx5.ysiz-vx5.ysiz+1;
		else gysizcache = v-(v%vx5.ysiz);
	}
	return(vx5.pic[(v-gysizcache)*(vx5.bpl>>2)+(u-gxsizcache)]>>24);
}

#endif //KCOLORS_H
