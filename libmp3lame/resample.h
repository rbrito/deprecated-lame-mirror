
typedef struct {
    long double  sample_freq_in;
    long double  sample_freq_out;
    float        lowpass_freq;
    int          scale_in;
    int          scale_out;
    sample_t**   fir;
    sample_t*    lastsamples [2];
    ... 
} resample_t;


// Creates and initialize a resample_t structure on the heap and 
// returns a pointer to it. This pointer you can add to 
// lame_internal_flags or lame_global_flags

resample_t*  open_resample (
        OUT long double  sample_freq_in,        // [Hz]
        OUT long double  sample_freq_out,       // [Hz]
        OUT double       lowpass_freq,          // [Hz] or 0 for auto mode
        OUT int          quality );             // Proposal: 0 default, 1 sample select, 2 linear interpol, 4 4-point interpolation, 20 20-point interpolation


// Destroys such a structure and frees all memory
// returns 0 for success or 1 for internal errors

int  close_resample ( INOUT resample_t* const r );


// Do the resampling

int  fill_buffer_resample (			// return code, 0 for success
        INOUT resample_t *const   r,            // internal structure
        IN    sample_t   *const   out,          // where to write the output data
        INOUT int        *const   out_req_len,  // requested output data len/really written output data len
        OUT   sample_t   *const   in,           // where are the input data?
        INOUT int        *const   in_avail_len, // available input data len/consumed input data len
        OUT   int                 channel );    // number of the channel (needed for buffering)


// returns relative frequency error for resampling, 
// 0.01 means 1%

double  resample_freq_error ( OUT resample_t* const r );
{
    return  (sample_freq_in*scale_out - sample_freq_out*scale_in) 
          / (sample_freq_out*scale_in);
}

Vorteile:
   - abgetrennte Funktionalität (Codekapslung)
   - abgetrennter Speicher (Datenkapslung)
   - Entschlacken von lame_internal_flags, was derzeitig ein formloser
     Kuhhaufen ist, auf dem sich jeder bedient und rumschreibt, wer will

Das ganze hat etwas Hauch von C++ und RPC, vielleicht auch etwas winziges in
Richtung Ada95. Wird etlich stören ...

-- 
Frank Klemm
