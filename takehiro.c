/*
 *	MP3 huffman table selecting and bit counting
 *
 *	Copyright (c) 1999 Takehiro TOMINAGA
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#include "util.h"
#include "l3side.h"
#include "tables.h"
#include "quantize-pvt.h"
#include "globalflags.h"

extern int *scalefac_band_long; 
extern int *scalefac_band_short;



struct
{
    unsigned region0_count;
    unsigned region1_count;
} subdv_table[ 23 ] =
{
{0, 0}, /* 0 bands */
{0, 0}, /* 1 bands */
{0, 0}, /* 2 bands */
{0, 0}, /* 3 bands */
{0, 0}, /* 4 bands */
{0, 1}, /* 5 bands */
{1, 1}, /* 6 bands */
{1, 1}, /* 7 bands */
{1, 2}, /* 8 bands */
{2, 2}, /* 9 bands */
{2, 3}, /* 10 bands */
{2, 3}, /* 11 bands */
{3, 4}, /* 12 bands */
{3, 4}, /* 13 bands */
{3, 4}, /* 14 bands */
{4, 5}, /* 15 bands */
{4, 5}, /* 16 bands */
{4, 6}, /* 17 bands */
{5, 6}, /* 18 bands */
{5, 6}, /* 19 bands */
{5, 7}, /* 20 bands */
{6, 7}, /* 21 bands */
{6, 7}, /* 22 bands */
};


/*************************************************************************/
/*	      ix_max							 */
/*************************************************************************/

 static int ix_max(int *ix, int *end)
{
    int max = 0;

    while (ix < end) {
	int x =	 *ix++;
	if (max < x) 
	    max = x;

	x = *ix++;
	if (max < x) 
	    max = x;
    }
    return max;
}


/*************************************************************************/
/*	      count_bit							 */
/*************************************************************************/

/*
 Function: Count the number of bits necessary to code the subregion. 
*/

static int cb_esc_buf[288];
static int cb_esc_sign;
static int *cb_esc_end;
static const int huf_tbl_noESC[15] = {
    1, 2, 5, 7, 7,10,10,13,13,13,13,13,13,13,13
};

 static int
count_bit_ESC(int *ix, int *end, int t1, int t2, int *s)
{
    /* ESC-table is used */
    int linbits1 = ht[t1].xlen;
    int linbits2 = ht[t2].xlen;
    int	sum = 0;
    int	sum1 = 0;
    int	sum2 = 0;

    while (ix < end) {
	int x = *ix++;
	int y = *ix++;

	if (x != 0) {
	    sum++;
	    if (x > 14) {
		x = 15;
		sum1 += linbits1;
		sum2 += linbits2;
	    }
	    x *= 16;
	}

	if (y != 0) {
	    sum++;
	    if (y > 14) {
		y = 15;
		sum1 += linbits1;
		sum2 += linbits2;
	    }
	    x += y;
	}

	sum1 += ht[16].hlen[x];
	sum2 += ht[24].hlen[x];
    }

    if (sum1 > sum2)  {
	sum1 = sum2;
	t1 = t2;
    }

    *s += sum + sum1;
    return t1;
}

 static int
count_bit_noESC(int *ix, int *end, unsigned int table) 
{
    /* No ESC-words */
    int	sum = 0, sign = 0;
    unsigned char *hlen = ht[table].hlen;
    int *p = cb_esc_buf;

    do {
	int x = *ix++;
	int y = *ix++;
	if (x != 0) {
	    sign++;
	    x *= 16;
	}

	if (y != 0) {
	    sign++;
	    x += y;
	}

	*p++ = x;
	sum += hlen[x];
    } while (ix < end);

    cb_esc_sign = sign;
    cb_esc_end = p;
    return sum + sign;
}



 static int
count_bit_noESC2(unsigned int table) 
{
    /* No ESC-words */
    int	sum = cb_esc_sign;
    int *p = cb_esc_buf;

    do {
	sum += ht[table].hlen[*p++];
    } while (p < cb_esc_end);

    return sum;
}



 static int
count_bit_short_ESC(int *ix, int *end, int t1, int t2, int *s)
{
    /* ESC-table is used */
    int linbits1 = ht[t1].xlen;
    int linbits2 = ht[t2].xlen;
    int	sum = 0;
    int	sum1 = 0;
    int	sum2 = 0;

    do {
	int i;
	for (i = 0; i < 3; i++) {
	    int y = *(ix + 3);
	    int x = *ix++;

	    if (x != 0) {
		sum++;
		if (x > 14) {
		    x = 15;
		    sum1 += linbits1;
		    sum2 += linbits2;
		}
		x *= 16;
	    }

	    if (y != 0) {
		sum++;
		if (y > 14) {
		    y = 15;
		    sum1 += linbits1;
		    sum2 += linbits2;
		}
		x += y;
	    }

	    sum1 += ht[16].hlen[x];
	    sum2 += ht[24].hlen[x];
	}
	ix += 3;
    } while (ix < end);

    if (sum1 > sum2)  {
	sum1 = sum2;
	t1 = t2;
    }

    *s += sum + sum1;
    return t1;
}



 static int
count_bit_short_noESC(int *ix, int *end, unsigned int table) 
{
    /* No ESC-words */
    int	sum = 0, sign = 0;
    unsigned char *hlen = ht[table].hlen;
    int *p = cb_esc_buf;

    do {
	int i;
	for (i = 0; i < 3; i++) {
	    int y = *(ix + 3);
	    int x = *ix++;
	    if (x != 0) {
		sign++;
		x *= 16;
	    }

	    if (y != 0) {
		sign++;
		x += y;
	    }

	    *p++ = x;
	    sum += hlen[x];
	}
	ix += 3;
    } while (ix < end);

    cb_esc_sign = sign;
    cb_esc_end = p;
    return sum + sign;
}



/*************************************************************************/
/*	      new_choose table						 */
/*************************************************************************/

/*
  Choose the Huffman table that will encode ix[begin..end] with
  the fewest bits.

  Note: This code contains knowledge about the sizes and characteristics
  of the Huffman tables as defined in the IS (Table B.7), and will not work
  with any arbitrary tables.
*/

static int choose_table(int *ix, int *end, int *s)
{
    int max;
    int choice0, sum0;
    int choice1, sum1;

    max = ix_max(ix, end);

    if (max > IXMAX_VAL) {
        *s = 100000;
        return -1;
    }

    if (max <= 15)  {
	if (max == 0) {
	    return 0;
	}
	/* try tables with no linbits */
	choice0 = huf_tbl_noESC[max - 1];
	sum0 = count_bit_noESC(ix, end, choice0);
	choice1 = choice0;

	switch (choice0) {
	case 7:
	case 10:
	    choice1++;
	    sum1 = count_bit_noESC2(choice1);
	    if (sum0 > sum1) {
		sum0 = sum1;
		choice0 = choice1;
	    }
	    /*fall*/
	case 2:
	case 5:
	    choice1++;
	    sum1 = count_bit_noESC2(choice1);
	    if (sum0 > sum1) {
		sum0 = sum1;
		choice0 = choice1;
	    }
	    break;

	case 13:
	    choice1 += 2;
	    sum1 = count_bit_noESC2(choice1);
	    if (sum0 > sum1) {
		sum0 = sum1;
		choice0 = choice1;
	    }
	    break;

	default:
	    break;
	}
	*s += sum0;
    } else {
	/* try tables with linbits */
	max -= 15;

	for (choice1 = 24; choice1 < 32; choice1++) {
	    if (ht[choice1].linmax >= max) {
		break;
	    }
	}

	for (choice0 = choice1 - 8; choice0 < 24; choice0++) {
	    if (ht[choice0].linmax >= max) {
		break;
	    }
	}

	choice0 = count_bit_ESC(ix, end, choice0, choice1, s);
    }

    return choice0;
}

static int choose_table_short(int *ix, int *end, int * s)
{
    int max;
    int choice0, sum0;
    int choice1, sum1;

    max = ix_max(ix, end);

    if (max > IXMAX_VAL) {
        *s = 100000;
        return -1;
    }

    if (max <= 15)  {
	if (max == 0) {
	    return 0;
	}
	/* try tables with no linbits */
	choice0 = huf_tbl_noESC[max - 1];
	sum0 = count_bit_short_noESC(ix, end, choice0);
	choice1 = choice0;

	switch (choice0) {
	case 7:
	case 10:
	    choice1++;
	    sum1 = count_bit_noESC2(choice1);
	    if (sum0 > sum1) {
		sum0 = sum1;
		choice0 = choice1;
	    }
	    /*fall*/
	case 2:
	case 5:
	    choice1++;
	    sum1 = count_bit_noESC2(choice1);
	    if (sum0 > sum1) {
		sum0 = sum1;
		choice0 = choice1;
	    }
	    break;

	case 13:
	    choice1 += 2;
	    sum1 = count_bit_noESC2(choice1);
	    if (sum0 > sum1) {
		sum0 = sum1;
		choice0 = choice1;
	    }
	    break;

	default:
	    break;
	}
	*s += sum0;
    } else {
	/* try tables with linbits */
	max -= 15;
	for (choice1 = 24; choice1 < 32; choice1++) {
	    if (ht[choice1].linmax >= max) {
		break;
	    }
	}

	for (choice0 = choice1 - 8; choice0 < 24; choice0++) {
	    if (ht[choice0].linmax >= max) {
		break;
	}
	}
	choice0 = count_bit_short_ESC(ix, end, choice0, choice1, s);
    }

    return choice0;
}


static int count_bits_long(int ix[576], gr_info *gi)
{
    int i, a1, a2;
    int bits = 0;

    i=576;
    for (; i > 1; i -= 2) 
	if (ix[i - 1] | ix[i - 2])
	    break;

    /* Determines the number of bits to encode the quadruples. */
    gi->count1 = i;
    a1 = 0;
	for (; i > 3; i -= 4) {
	    int p, v;
	    if ((unsigned int)(ix[i-1] | ix[i-2] | ix[i-3] | ix[i-4]) > 1)
		break;

	    v = ix[i-1];
	    p = v;
	    bits += v;

	    v = ix[i-2];
	    if (v != 0) {
		p += 2;
		bits++;
	    }

	    v = ix[i-3];
	    if (v != 0) {
		p += 4;
		bits++;
	    }

	    v = ix[i-4];
	    if (v != 0) {
		p += 8;
		bits++;
	    }

	a1 += ht[32].hlen[p];
	}
    a2 = gi->count1 - i;
    if (a1 < a2) {
	bits += a1;
	    gi->count1table_select = 0;
	} else {
	bits += a2;
	    gi->count1table_select = 1;
	}

    gi->big_values = i;
    if (i == 0)
	return bits;

    if (gi->block_type == NORM_TYPE) {
	int index;
	int scfb_anz = 0;

	while (scalefac_band_long[++scfb_anz] < i) 
	    ;

	index = subdv_table[scfb_anz].region0_count;
	while (scalefac_band_long[index + 1] > i)
	    index--;
	gi->region0_count = index;

	index = subdv_table[scfb_anz].region1_count;
	while (scalefac_band_long[index + gi->region0_count + 2] > i)
	    index--;
	gi->region1_count = index;

	a1 = scalefac_band_long[gi->region0_count + 1];
	a2 = scalefac_band_long[index + gi->region0_count + 2];
	gi->table_select[2] = choose_table(ix + a2, ix + i, &bits);
    } else {
	gi->region0_count = 7;
	/*gi->region1_count = SBPSY_l - 7 - 1;*/
	gi->region1_count = SBMAX_l -1 - 7 - 1;
	a1 = scalefac_band_long[7 + 1];
	a2 = i;
	if (a1 > a2) {
	    a1 = a2;
	}
    }

    /* Count the number of bits necessary to code the bigvalues region. */
    gi->table_select[0] = choose_table(ix, ix + a1, &bits);
    gi->table_select[1] = choose_table(ix + a1, ix + a2, &bits);
    return bits;
}




int count_bits(int *ix, FLOAT8 *xr, gr_info *cod_info)  
{
  int bits=0,i;
  if (highq) {
    FLOAT8 w = (IXMAX_VAL) * pow(2.0, cod_info->quantizerStepSize * 0.1875);
    for ( i = 0; i < 576; i++ )  {
      if (xr[i] > w)
	return 100000;
    }
  }
  if (highq)
    quantize_xrpow(xr, ix, cod_info);
  else
    quantize_xrpow_ISO(xr, ix, cod_info);


  if (cod_info->block_type==SHORT_TYPE) {
    cod_info->table_select[0] = choose_table_short(ix, ix + 36, &bits);
    cod_info->table_select[1] = choose_table_short(ix + 36, ix + 576, &bits);
  }else{
    bits=count_bits_long(ix, cod_info);
    cod_info->count1 = (cod_info->count1 - cod_info->big_values) / 4;
    cod_info->big_values /= 2;
  }
  return bits;

}

