#ifndef FFT_H
#define FFT_H

#include "encoder.h"

void fft_long(FLOAT x_real[BLKSIZE], FLOAT energy[HBLKSIZE], int, short **);
void fft_short(FLOAT x_real[BLKSIZE], FLOAT energy_s[3][HBLKSIZE_s], int, short **);
void fft_long2(FLOAT x_real[BLKSIZE], FLOAT energy[HBLKSIZE], int, short **);
void fft_short2(FLOAT x_real[3][BLKSIZE_s], FLOAT energy_s[3][HBLKSIZE_s], int, short **);
void init_fft(void);

#endif
