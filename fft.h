#ifndef FFT_H
#define FFT_H

#include "encoder.h"

void fft(FLOAT x_real[BLKSIZE],
         FLOAT energy[BLKSIZE],
         FLOAT ax[BLKSIZE],FLOAT  bx[BLKSIZE],
         int N);

void fft_side(FLOAT x_real0[BLKSIZE],FLOAT x_real1[BLKSIZE],
         FLOAT energy[BLKSIZE],
         FLOAT ax[BLKSIZE],FLOAT  bx[BLKSIZE],
         int N,int sign);

#endif
