#ifndef TIMESTATUS_H_INCLUDED
#define TIMESTATUS_H_INCLUDED


void timestatus(int samp_rate,long frameNum,long totalframes, int framesize);
void timestatus_finish(void);

#if (defined LIBSNDFILE || defined LAMESNDFILE)
void decoder_progress(lame_global_flags *gfp);
void decoder_progress_finish(lame_global_flags *gfp);
#endif

#endif
