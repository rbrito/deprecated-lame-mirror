#ifndef TIMESTATUS_H_INCLUDED
#define TIMESTATUS_H_INCLUDED

void timestatus ( unsigned long samp_rate, 
                  unsigned long frameNum, 
                  unsigned long totalframes, 
                  int           framesize);
void timestatus_finish(void);

#if (defined LIBSNDFILE || defined LAMESNDFILE)
void decoder_progress(lame_global_flags *gfp);
void decoder_progress_finish(lame_global_flags *gfp);
#endif


#endif
