/*
** FFT and FHT routines
**  Copyright 1988, 1993; Ron Mayer
**  
**  fht(fz,n);
**      Does a hartley transform of "n" points in the array "fz".
**      
** NOTE: This routine uses at least 2 patented algorithms, and may be
**       under the restrictions of a bunch of different organizations.
**       Although I wrote it completely myself; it is kind of a derivative
**       of a routine I once authored and released under the GPL, so it
**       may fall under the free software foundation's restrictions;
**       it was worked on as a Stanford Univ project, so they claim
**       some rights to it; it was further optimized at work here, so
**       I think this company claims parts of it.  The patents are
**       held by R. Bracewell (the FHT algorithm) and O. Buneman (the
**       trig generator), both at Stanford Univ.
**       If it were up to me, I'd say go do whatever you want with it;
**       but it would be polite to give credit to the following people
**       if you use this anywhere:
**           Euler     - probable inventor of the fourier transform.
**           Gauss     - probable inventor of the FFT.
**           Hartley   - probable inventor of the hartley transform.
**           Buneman   - for a really cool trig generator
**           Mayer(me) - for authoring this particular version and
**                       including all the optimizations in one package.
**       Thanks,
**       Ron Mayer; mayer@acuson.com
**
*/

#include <math.h>
#include "util.h"

static FLOAT costab[20]=
    {
     .00000000000000000000000000000000000000000000000000,
     .70710678118654752440084436210484903928483593768847,
     .92387953251128675612818318939678828682241662586364,
     .98078528040323044912618223613423903697393373089333,
     .99518472667219688624483695310947992157547486872985,
     .99879545620517239271477160475910069444320361470461,
     .99969881869620422011576564966617219685006108125772,
     .99992470183914454092164649119638322435060646880221,
     .99998117528260114265699043772856771617391725094433,
     .99999529380957617151158012570011989955298763362218,
     .99999882345170190992902571017152601904826792288976,
     .99999970586288221916022821773876567711626389934930,
     .99999992646571785114473148070738785694820115568892,
     .99999998161642929380834691540290971450507605124278,
     .99999999540410731289097193313960614895889430318945,
     .99999999885102682756267330779455410840053741619428
    };
static FLOAT sintab[20]=
    {
     1.0000000000000000000000000000000000000000000000000,
     .70710678118654752440084436210484903928483593768846,
     .38268343236508977172845998403039886676134456248561,
     .19509032201612826784828486847702224092769161775195,
     .09801714032956060199419556388864184586113667316749,
     .04906767432741801425495497694268265831474536302574,
     .02454122852291228803173452945928292506546611923944,
     .01227153828571992607940826195100321214037231959176,
     .00613588464915447535964023459037258091705788631738,
     .00306795676296597627014536549091984251894461021344,
     .00153398018628476561230369715026407907995486457522,
     .00076699031874270452693856835794857664314091945205,
     .00038349518757139558907246168118138126339502603495,
     .00019174759731070330743990956198900093346887403385,
     .00009587379909597734587051721097647635118706561284,
     .00004793689960306688454900399049465887274686668768
    };

/* This is a simplified version for n an even power of 2 */

static void fht(FLOAT *fz, int n)
{
 int i,k,k1,k2,k3,k4,kx;
 FLOAT *fi,*fn,*gi;
 FLOAT t_c,t_s;

 for (k1=1,k2=0;k1<n;k1++)
    {
     FLOAT a;
     for (k=n>>1; (!((k2^=k)&k)); k>>=1);
     if (k1>k2)
	{
	     a=fz[k1];fz[k1]=fz[k2];fz[k2]=a;
	}
    }
  for (fi=fz,fn=fz+n;fi<fn;fi+=4)
     {
      FLOAT f0,f1,f2,f3;
      f1     = fi[0 ]-fi[1 ];
      f0     = fi[0 ]+fi[1 ];
      f3     = fi[2 ]-fi[3 ];
      f2     = fi[2 ]+fi[3 ];
      fi[2 ] = (f0-f2);	
      fi[0 ] = (f0+f2);
      fi[3 ] = (f1-f3);	
      fi[1 ] = (f1+f3);
     }

 k=0;
 do
    {
     FLOAT s1,c1;
     k  += 2;
     k1  = 1  << k;
     k2  = k1 << 1;
     k4  = k2 << 1;
     k3  = k2 + k1;
     kx  = k1 >> 1;
	 fi  = fz;
	 gi  = fi + kx;
	 fn  = fz + n;
	 do
	    {
	     FLOAT g0,f0,f1,g1,f2,g2,f3,g3;
	     f1      = fi[0 ] - fi[k1];
	     f0      = fi[0 ] + fi[k1];
	     f3      = fi[k2] - fi[k3];
	     f2      = fi[k2] + fi[k3];
	     fi[k2]  = f0	  - f2;
	     fi[0 ]  = f0	  + f2;
	     fi[k3]  = f1	  - f3;
	     fi[k1]  = f1	  + f3;
	     g1      = gi[0 ] - gi[k1];
	     g0      = gi[0 ] + gi[k1];
	     g3      = SQRT2  * gi[k3];
	     g2      = SQRT2  * gi[k2];
	     gi[k2]  = g0	  - g2;
	     gi[0 ]  = g0	  + g2;
	     gi[k3]  = g1	  - g3;
	     gi[k1]  = g1	  + g3;
	     gi     += k4;
	     fi     += k4;
	    } while (fi<fn);
     t_c = costab[k];
     t_s = sintab[k];
     c1 = 1;
     s1 = 0;
     for (i=1;i<kx;i++)
        {
	 FLOAT c2,s2;
         FLOAT t = c1;
         c1 = t*t_c - s1*t_s;
         s1 = t*t_s + s1*t_c;
         c2 = c1*c1 - s1*s1;
         s2 = 2*(c1*s1);
	     fn = fz + n;
	     fi = fz +i;
	     gi = fz +k1-i;
	     do
		{
		 FLOAT a,b,g0,f0,f1,g1,f2,g2,f3,g3;
		 b       = s2*fi[k1] - c2*gi[k1];
		 a       = c2*fi[k1] + s2*gi[k1];
		 f1      = fi[0 ]    - a;
		 f0      = fi[0 ]    + a;
		 g1      = gi[0 ]    - b;
		 g0      = gi[0 ]    + b;
		 b       = s2*fi[k3] - c2*gi[k3];
		 a       = c2*fi[k3] + s2*gi[k3];
		 f3      = fi[k2]    - a;
		 f2      = fi[k2]    + a;
		 g3      = gi[k2]    - b;
		 g2      = gi[k2]    + b;
		 b       = s1*f2     - c1*g3;
		 a       = c1*f2     + s1*g3;
		 fi[k2]  = f0        - a;
		 fi[0 ]  = f0        + a;
		 gi[k3]  = g1        - b;
		 gi[k1]  = g1        + b;
		 b       = c1*g2     - s1*f3;
		 a       = s1*g2     + c1*f3;
		 gi[k2]  = g0        - a;
		 gi[0 ]  = g0        + a;
		 fi[k3]  = f1        - b;
		 fi[k1]  = f1        + b;
		 gi     += k4;
		 fi     += k4;
		} while (fi<fn);
        }
    } while (k4<n);
}

void fft(FLOAT *x_real, FLOAT *energy, FLOAT *ax, FLOAT *bx, int N)
{
 FLOAT a,b;
 int i,j;

 fht(x_real,N);


 energy[0] = x_real[0] * x_real[0];
 ax[0] = bx[0] = x_real[0];

 for (i=1,j=N-1;i<N/2;i++,j--) {
   a = ax[i] = x_real[i];
   b = bx[i] = x_real[j];
   energy[i]=(a*a + b*b)/2;
   
   if (energy[i] < 0.0005) {
     energy[i] = 0.0005;
     ax[i] = bx[i] =  0.0223606797749978970790696308768019662239; /* was sqrt(0.0005) */
   }
 }
 energy[N/2] = x_real[N/2] * x_real[N/2];
 ax[N/2] = bx[N/2] = x_real[N/2];
}




#if 0
/* mt 7/99
   Note: fft_side() can be used to compute energy, ax & bx for the
   mid and side channels (chn=2,3) without calling additional FFTs. 
   But it requires wsamp_r to be saved from channels 0 and 1.  
   My tests show that the FFT is so fast that this gives no savings.
   Probably the extra memory hurts the cache performance.
*/

void fft_side(FLOAT x_real0[],FLOAT x_real1[], FLOAT *energy, FLOAT *ax, FLOAT *bx, 
int N, int sign)
/*
x_real0 = x_real from channel 0
x_real1 = x_real from channel 1
sign = 1:    compute mid channel energy, ax, bx
sign =-1:    compute side channel energy, ax, bx
*/
{
 FLOAT a,b;
 int i,j;

#define XREAL(j) ((x_real0[j]+sign*x_real1[j])/SQRT2)

 energy[0] = XREAL(0) * XREAL(0);
 ax[0] = bx[0] = XREAL(0);

 for (i=1,j=N-1;i<N/2;i++,j--) {
   a = ax[i] = XREAL(i);
   b = bx[i] = XREAL(j);
   energy[i]=(a*a + b*b)/2;
   
   if (energy[i] < 0.0005) {
     energy[i] = 0.0005;
     ax[i] = bx[i] =  0.0223606797749978970790696308768019662239; /* was sqrt(0.0005) */
   }
 }
 energy[N/2] = XREAL(N/2) * XREAL(N/2);
 ax[N/2] = bx[N/2] = XREAL(N/2);


}
#endif


