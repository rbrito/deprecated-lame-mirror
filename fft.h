#ifndef __FFT_H__
# define __FFT_H__

# include "encoder.h"

void fft_long  ( lame_global_flags* gfp, FLOAT x_real     [BLKSIZE  ], int, sample_t ** );
void fft_short ( lame_global_flags* gfp, FLOAT x_real [3] [BLKSIZE_s], int, sample_t ** );
void init_fft  ( lame_global_flags* gfp );

#endif

/* End of fft.h */
