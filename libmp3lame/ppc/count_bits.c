/**********************************************************
* count_bit_ESC
**********************************************************/
int
count_bit_ESC_altivec(
                        const int *       ix,
                        const int * const end,
                        int         t1,
                        const int         t2,
                        int * const s )
{
    int simdCount;
    int scalarCount;
    int *vtos;
    int linbits = ht[t1].xlen * 65536 + ht[t2].xlen;
    int sum = 0, sum2 = 0;
    int a,b;
    vector signed int *vix;
    vector signed int vxy;
    vector signed int vData0;
    vector signed int vData1;
    vector signed int vSum;
    vector signed int vSumt;
    vector signed int vFourteen = (vector signed int) (14,14,14,14);
    vector signed int vFifthteen = (vector signed int) (15,15,15,15);
    vector unsigned int vShiftLeft = (vector unsigned int) (4,0,4,0);
    vector signed int vLinbits;
    vector bool int vMask;
    vector unsigned char vShiftData;
    vector signed int vTemp;

    scalarCount = ((int)end) - ((int)ix);
    scalarCount >>= 2;
    simdCount = scalarCount >> 2;
    scalarCount -= simdCount << 2;

    if(simdCount > 0) {
       vix = (vector signed int *) ix;
       ix = (signed int *) &vix[simdCount];

       vtos = (signed int *) &vTemp;
       vtos[0] = linbits;
       vLinbits = vec_splat(vTemp, 0);

       vSumt = vec_splat_s32(0);

       vShiftData  = vec_lvsl(0, (int *) vix);
       vData0 = vec_ld(0, vix);
       vData1 = vec_ld(16, vix);
       vix++;

       do {
          vxy    = vec_perm(vData0, vData1, vShiftData);
          vData0 = vData1;
          vData1 = vec_ld(16, vix);

          vMask  = vec_cmpgt(vxy, vFourteen);
          vxy    = vec_sel(vxy, vFifthteen, vMask);
          vSum   = vec_sel(vec_splat_s32(0), vLinbits, vMask);
          vxy    = vec_sl(vxy, vShiftLeft);
          vSumt  = vec_add(vSumt, vSum);
          vxy    = vec_sum2s(vxy, vec_splat_s32(0));

          vTemp = vxy;
          sum += largetbl[vtos[1]];
          sum += largetbl[vtos[3]];

          vix++;

       } while(--simdCount > 0);
    }

    vTemp = vec_sums(vSumt, vec_splat_s32(0));
    sum += vtos[3];


    if(scalarCount > 0) {
       do {
          int x = *ix++;
          int y = *ix++;

          if (x != 0) {
             if (x > 14) {
                x = 15;
                sum += linbits;
             }
             x *= 16;
          }

          if (y != 0) {
             if (y > 14) {
                y = 15;
                sum += linbits;
             }
             x += y;
          }

          sum += largetbl[x];
       } while (ix < end);
    }

    sum2 = sum & 0xffff;
    sum >>= 16;

    if (sum > sum2) {
       sum = sum2;
       t1 = t2;
    }

    *s += sum;
    return t1;
}
