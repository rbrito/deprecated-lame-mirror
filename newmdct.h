#ifndef MDCT_DOT_H
#define MDCT_DOT_H
void mdct_sub48(short *w0, short *w1,
	      FLOAT8 mdct_freq[2][2][576], int stereo,
	      III_side_info_t *l3_side, int mode_gr);
#endif
