/* $Id$ */

/*
 *  Usage: abx original_file test_file
 *
 *  Ask you as long as the probability is below the given percentage that
 *  you recognize differences
 *
 *  Example: abx music.wav music.mp3
 *
 *  Note: several 'decoding' utilites must be on the 'right' place
 *
 *  Bugs: 
 *      fix path of decoding utilities
 *      no level analysis, no normalizing if both files are low amplitude
 *      only 16 bit support
 *      only support of the same sample frequency
 *      no exact WAV file header analysis
 *      no mouse or joystick support
 *      don't uses functionality of ath.c
 *      only 1200 tests possible
 *      only 2 files are comparable
 *      no AB repeat mode (like CD players)
 *      mpeg system stream support is missing
 *      worse user interface
 *      quick & dirty hack
 *      wastes memory
 *      compile time warnings
 *      buffer overruns possible
 *      no dithering if recalcs are necessary
 *      fading time depends on sampling frequency
 *      correlation only done with one channel
 *      correlation of deltas really better than the of the original signal?
 *      Exact calculation with scalar product better?
 */

#if   defined(HAVE_CONFIG_H)
# include <config.h>
#elif defined(HAVE_CONFIG_MS_H)
# include <configMS.h>
#endif

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <memory.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#define  MAX  (1<<17)

#ifdef HAVE_SYS_SOUNDCARD_H
# include <sys/soundcard.h>
#elif defined(HAVE_LINUX_SOUNDCARD_H)
# include <linux/soundcard.h>
#else
# include <linux/soundcard.h>         /* stand alone compilable for my tests */
#endif

#define  BF           4410
#define  MAX_LEN      (180*44100)

#define	FFT_ERR_OK	0		// no error
#define	FFT_ERR_LD	1		// len is not a power of 2
#define FFT_ERR_MAX	2		// len too large

typedef float	f_t;
typedef	f_t  	compl [2];

#define mult(x1,x2)	((x1)*(x2))
#define add(x1,x2)	((x1)+(x2))
#define sub(x1,x2)	((x1)-(x2))
#define h1(x)		(x)
#define h2(x)		(x)

compl        	root    [MAX >> 1];		// Sinus-/Kosinustabelle
size_t     	shuffle [MAX >> 1] [2];		// Shuffle-Tabelle
size_t		shuffle_len;

// Bitinversion

size_t  swap ( size_t number, int bits )
{
    size_t  ret;
    for ( ret = 0; bits--; number >>= 1 ) {
        ret = ret + ret + (number & 1);
    }
    return ret;
}

// Bestimmen des Logarithmus dualis

int  ld ( size_t number )
{
    size_t i;
    for ( i = 0; i < sizeof(size_t)*CHAR_BIT; i++ )
        if ( ((size_t)1 << i) == number )
            return i;
    return -1;
}

// Die eigentliche FFT

int  fft ( compl* fn, const size_t newlen )
{
    static size_t    len  = 0;
    static int       bits = 0;
    register size_t  i;
    register size_t  j;
    register size_t  k;
    size_t           p;

    /* Tabellen	initialisieren */

    if ( newlen != len ) {
        len  = newlen;

	if ( (bits=ld(len)) == -1 )
	    return FFT_ERR_LD;

	for ( i = 0; i < len; i++ ) {
	    j = swap ( i, bits );
	    if ( i < j ) {
	        shuffle [shuffle_len] [0] = i;
	        shuffle [shuffle_len] [1] = j;
		shuffle_len++;
	    }
        }
	for ( i = 0; i < (len>>1); i++ ) {
	    double x = (double) swap ( i+i, bits ) * 2*M_PI/len;
	    root [i] [0] = cos (x);
	    root [i] [1] = sin (x);
	}
    }

    /* Eigentliche Transformation */

    p = len >> 1;
    do {
        f_t*  bp = (f_t*) root;
	f_t*  si = (f_t*) fn;
	f_t*  di = (f_t*) fn+p+p;

	do {
	    k = p;
	    do {
	        f_t  mulr = h1 ( sub ( mult(bp[0],di[0]), mult(bp[1],di[1]) ) );
		f_t  muli = h1 ( add ( mult(bp[1],di[0]), mult(bp[0],di[1]) ) );
		f_t  addr = h2 ( si[0] );
		f_t  addi = h2 ( si[1] );

		si [0] = add (addr, mulr);
		si [1] = add (addi, muli);
		di [0] = sub (addr, mulr);
		di [1] = sub (addi, muli);

		si += 2, di += 2;
	    } while ( --k );
	    si += p+p, di += p+p, bp += 2;
	} while ( si < &fn[len][0] );
    } while (p >>= 1);

    /* Bitinversion */

    for	( k = 0; k < shuffle_len; k++ ) {
        f_t  tmp;
	i   = shuffle [k] [0];
	j   = shuffle [k] [1];
	tmp = fn [i][0]; fn [i][0] = fn [j][0]; fn [j][0] = tmp;
	tmp = fn [i][1]; fn [i][1] = fn [j][1]; fn [j][1] = tmp;
    }

    return FFT_ERR_OK;
}


static struct termios stored_settings;


void reset ( void )
{
    tcsetattr ( 0, TCSANOW, &stored_settings );
}


void set ( void )
{
    struct termios new_settings;
    
    tcgetattr ( 0, &stored_settings );
    new_settings = stored_settings;
    
    new_settings.c_lflag    &= ~ECHO;
    /* Disable canonical mode, and set buffer size to 1 byte */
    new_settings.c_lflag    &= ~ICANON;
    new_settings.c_cc[VTIME] = 0;
    new_settings.c_cc[VMIN]  = 1;
    
    tcsetattr(0,TCSANOW,&new_settings);
    return;
}


int sel ( void )
{
    struct timeval  t;
    fd_set          fd [1];
    int             ret;
    unsigned char   c;

    FD_SET (0, fd);    
    t.tv_sec  = 0;
    t.tv_usec = 0;

    ret = select ( 1, fd, NULL, NULL, &t );
    
    switch ( ret ) {
    case  0: 
        return -1;
    case  1: 
        ret = read (0, &c, 1);
        return ret == 1  ?  c  :  -1;
    default: 
        return -2;
    }
}


int  random_number ( void )
{
    struct timeval  t;
    unsigned long   val;

    gettimeofday ( &t, NULL );

    val  = t.tv_sec ^ t.tv_usec ^ rand();
    val ^= val >> 16;
    val ^= val >>  8;
    val ^= val >>  4;
    val ^= val >>  2;
    val ^= val >>  1;

    return val & 1;
}


double prob ( int last, int total )
{
    long double sum = 0.0;
    long double tmp = pow (0.5, total);
    int         i   = 0;
    
    
    for ( ; i <= last; i++ ) {
        sum += tmp;
        tmp = tmp * (total-i) / (1+i);
    }
    for ( ; i < total-last; i++ ) {
        tmp = tmp * (total-i) / (1+i);
    }
    for ( ; i <= total; i++ ) {
        sum += tmp;
        tmp = tmp * (total-i) / (1+i);
    }

    return sum;
}


void eval ( int right )
{
    static int  count = 0;
    static int  okay  = 0;
    double      val;
    
    count ++;
    okay  += right;
    
    val    = 1 / prob ( okay < count/2 ? okay : count-okay, count );
    
    fprintf (stderr, "%4u/%-4u %c %7.2f   %4.2f bit\n", okay, count, right ? ' ' : '!', val, count>1 ? log(val)/log(2)/(count-1) : 0.0 );
}


typedef signed short  sample_t;
typedef sample_t      mono_t   [1];
typedef sample_t      stereo_t [2];


int feed ( int fd, const stereo_t* p, int len )
{
    write ( fd, p, sizeof(stereo_t) * len );
    return len;
}


short round ( double f )
{
    int  x = (int)floor ( f + 0.5 );
    if ( x == (short)x ) return x;
    if ( x < 0 ) return -32768;
    return 32767;
}


int feed2 ( int fd, const stereo_t* p1, const stereo_t* p2, int len )
{
    stereo_t  tmp [32768];
    int       i;
    
    for ( i = 0; i < len; i++ ) {
        double f = cos ( M_PI/2*i/len );
        f *= f;
        tmp [i] [0] = round ( p1 [i] [0] * f + p2 [i] [0] * (1. - f) );
        tmp [i] [1] = round ( p1 [i] [1] * f + p2 [i] [1] * (1. - f) );
    }

    write ( fd, tmp, sizeof(stereo_t) * len );
    return len;
}


int feedfac ( int fd, const stereo_t* p1, const stereo_t* p2, int len, double fac1, double fac2 )
{
    stereo_t  tmp [32768];
    int       i;
    
    for ( i = 0; i < len; i++ ) {
        tmp [i] [0] = round ( p1 [i] [0] * fac1 + p2 [i] [0] * fac2 );
        tmp [i] [1] = round ( p1 [i] [1] * fac1 + p2 [i] [1] * fac2 );
    }

    write ( fd, tmp, sizeof(stereo_t) * len );
    return len;
}


void setup ( int fdd, int samples, long freq )
{
    int status, org, arg;

    // Nach vorn verschoben
    if ( -1 == (status = ioctl (fdd, SOUND_PCM_SYNC, 0)) )
        perror ("SOUND_PCM_SYNC ioctl failed");

    org = arg = 2;
    if ( -1 == (status = ioctl (fdd, SOUND_PCM_WRITE_CHANNELS, &arg)) )
        perror ("SOUND_PCM_WRITE_CHANNELS ioctl failed");
    if (arg != org)
        perror ("unable to set number of channels");
    fprintf (stderr, "%1u*", arg);

    org = arg = 16;
    if ( -1 == (status = ioctl (fdd, SOUND_PCM_WRITE_BITS, &arg)) )
        perror ("SOUND_PCM_WRITE_BITS ioctl failed");
    if (arg != org)
        perror ("unable to set sample size");
    fprintf (stderr, "%2u bit ", arg);

    org = arg = AFMT_S16_LE;
    if ( -1 == ioctl (fdd, SNDCTL_DSP_SETFMT, &arg) )
        perror ("SNDCTL_DSP_SETFMT ioctl failed");
    if ((arg & org) == 0)
        perror ("unable to set data format");

    org = arg = freq;
    if ( -1 == (status = ioctl (fdd, SOUND_PCM_WRITE_RATE, &arg)) )
        perror ("SOUND_PCM_WRITE_WRITE ioctl failed");
    fprintf (stderr, "%5u Hz*%.3f sec [%u.%03u.%03u samples]\n", arg, (double)samples/arg, samples/1000000, samples/1000%1000, samples%1000 );
}


void Message ( const char* s )
{
    fprintf ( stderr, "\rListening %s%*.*s\rListening %s",
              s, 68-(int)strlen(s), 68-(int)strlen(s), "", s );
    fflush  ( stderr );
}


void testing ( const stereo_t* A, const stereo_t* B, size_t len, long freq )
{
    int    c;
    int    fd    = open ( "/dev/dsp", O_WRONLY );
    int    rnd   = random_number ();   /* Auswahl von X */
    int    state = 0;                  /* derzeitiger Füttungsmodus */
    int    index = 0;                  /* derzitiger Offset auf den Audioströmen */
    float  fac1;
    float  fac2;
    char   buff [128];

    setup ( fd, len, freq );
    
    Message ( "A" );
    while ( 1 ) {
        c = sel ();
        if ( c == 27 )
            c = sel () + 0x100;
            
        switch ( c ) {
        case 'A' :
        case 'a' :
            Message ( "A" );
            if ( state != 0 )
                state = 2;
            break;

        case 'B' :
        case 'b' :
            Message ( "B" );
            if ( state != 1 )
                state = 3;
            break;

        case 'X' :
        case 'x' :
            Message ( "X" );
            if ( state != rnd )
                state = rnd + 2;
            break;

        case 'M' :
        case 'm' :
            Message ( "Mixing A and B" );
	    state = 4;
            break;

        case 'D'+0x100:
            Message ( "Difference (+40 dB)" );
            state = 5;
            fac1  = -100.;
            fac2  = +100.;
            break;
	    
        case 'd'+0x100:
            Message ( "Difference (+30 dB)" );
            state = 5;
            fac1  = -32.;
            fac2  = +32.;
            break;
	    
        case 'D' & 0x1F :
            Message ( "Difference (+20 dB)" );
            state = 5;
            fac1  = -10.;
            fac2  = +10.;
            break;
	    
        case 'D' :
            Message ( "Difference (+10 dB)" );
            state = 5;
            fac1  = -3.;
            fac2  = +3.;
            break;
	    
        case 'd' :
            Message ( "Difference (  0 dB)" );
            state = 5;
            fac1  = -1.;
            fac2  = +1.;
            break;

        case '0' :
        case '1' :
        case '2' :
        case '3' :
        case '4' :
        case '5' :
        case '6' :
        case '7' :
        case '8' :
        case '9' :
	    sprintf ( buff, "B (Errors +%c dB)", c );
            Message ( buff );
            state = 5;
            fac2  = pow (10., 0.05*(c-'0') );
            fac1  = 1. - fac2;
            break;
	    
        case 'A' & 0x1F:
            fprintf (stderr, "  Vote for X:=A  " );
            eval ( rnd == 0 );
            rnd   = random_number ();
            if ( state != rnd )
                state = rnd + 2;
            Message ("X" );
            break;

        case 'B' & 0x1F:
            fprintf (stderr, "  Vote for X:=B  " );
            eval ( rnd == 1 );
            rnd   = random_number ();
            if ( state != rnd )
                state = rnd + 2;
            Message ("X" );
            break;

        case -1: 
            break;

        default:
            fprintf (stderr, "\a" );
            break;
            
        case 'Q':
	case 'q':
            fprintf ( stderr, "\n%-79.79s", "Quit program" );
            close (fd);
            fprintf ( stderr, "\n\n");
            return;
	    
        }

        switch (state) {
        case 0: /* A */
            if ( index + BF >= len )
                index += feed (fd, A+index, len-index );
            else
                index += feed (fd, A+index, BF );

            break;

        case 1: /* B */
            if ( index + BF >= len )
                index += feed (fd, B+index, len-index );
            else
                index += feed (fd, B+index, BF );
            break;

        case 2: /* B => A */
            if ( index + BF >= len )
                index += feed2 (fd, B+index, A+index, len-index );
            else
                index += feed2 (fd, B+index, A+index, BF );
            state = 0;
            break;

        case 3: /* A => B */
            if ( index + BF >= len )
                index += feed2 (fd, A+index, B+index, len-index );
            else
                index += feed2 (fd, A+index, B+index, BF );
            state = 1;
            break;
	    
	case 4:
            if ( index + BF/4 >= len )
                index += feed2 (fd, A+index, B+index, len-index );
            else
                index += feed2 (fd, A+index, B+index, BF/4 );
            if ( index + BF/2 >= len )
                index += feed (fd, B+index, len-index );
            else
                index += feed (fd, B+index, BF/2 );
            if ( index + BF/4 >= len )
                index += feed2 (fd, B+index, A+index, len-index );
            else
                index += feed2 (fd, B+index, A+index, BF/4 );
            if ( index + BF/2 >= len )
                index += feed (fd, A+index, len-index );
            else
                index += feed (fd, A+index, BF/2 );
	    break;

        case 5: /* Liko */
            if ( index + BF >= len )
                index += feedfac (fd, A+index, B+index, len-index, fac1, fac2 );
            else
                index += feedfac (fd, A+index, B+index, BF       , fac1, fac2 );
            break;
            
        default:
            assert (0);
        }
        if (index >= len)
            index = 0;
	fflush ( stderr );
    }
}


int  has_ext ( const char* name, const char* ext )
{
    if ( strlen (name) < strlen (ext) )
        return 0;
    name += strlen (name) - strlen (ext);
    return strcasecmp (name, ext)  ?  0  :  1;
}


typedef struct {
    const char* const  extention;
    const char* const  command;
} decoder_t;

#define REDIR " 2> /dev/null"

const decoder_t  decoder [] = {
    { ".mp1"    , "/usr/local/bin/mpg123 -w - %s" REDIR },  // MPEG Layer I   : www.iis.fhg.de, www.lame.org, www.toolame.org
    { ".mp2"    , "/usr/local/bin/mpg123 -w - %s" REDIR },  // MPEG Layer II  : www.iis.fhg.de, www.lame.org, www.toolame.org
    { ".mp3"    , "/usr/local/bin/mpg123 -w - %s" REDIR },  // MPEG Layer III : www.iis.fhg.de, www.lame.org, www.toolame.org
    { ".mpp"    , "/usr/local/bin/mppdec %s - "   REDIR },  // MPEGplus       : www.stud.uni-hannover.de/user/73884/
    { ".mp+"    , "/usr/local/bin/mppdec %s - "   REDIR },  // MPEGplus       : www.stud.uni-hannover.de/user/73884/
    { ".aac"    , "/usr/local/bin/faad   %s -"    REDIR },  // Advanced Audio Coding: www.psytel.com
    { ".ac3"    , "/usr/local/bin/ac3dec %s"      REDIR },  // Dolby AC3      : www.att.com
    { ".ogg"    , "/usr/local/bin/ogg123 %s -"    REDIR },  // Ogg Vorbis     : www.xiph.org
    { ".mod"    , "xmp -b16 -c -f44100 --stereo -o- %s | sox -r44100 -sw -c2 -traw - -twav -"
                                                  REDIR },  // Amiga's Music on Disk
    { ".ra"     , "echo %s"                       REDIR },  // Real Audio     : www.real.com
    { ".epac"   , "echo %s"                       REDIR },  // ePAC           : www.audioveda.com
    { ".qdm"    , "echo %s"                       REDIR },  // QDesign Music 2: www.qdesign.com
    { ".vqf"    , "echo %s"                       REDIR },  // TwinVQ         : www.yamaha-xg.com
    { ".wma"    , "echo %s"                       REDIR },  // Microsoft Media Audio: www.windowsmedia.com
    { ".mac"    , "echo %s"                       REDIR },  // Monkey's Audio Codec: www.monkeysaudio.com
    { ".lpac"   , "echo %s"                       REDIR },  // Lossless predictive Audio Compression: www-ft.ee.tu-berlin.de/~liebchen/lpac.html
    { ".wav.gz" , "gzip  -d < %s | sox -twav - -twav -"  
                                                  REDIR },  // gziped WAV
    { ".wav.sz" , "szip  -d < %s | sox -twav - -twav -"  
                                                  REDIR },  // sziped WAV
    { ".wav.sz2", "szip2 -d < %s | sox -twav - -twav -"  
                                                  REDIR },  // sziped WAV
    { ".raw"    , "sox -r44100 -sw -c2 -traw %s -twav -" 
                                                  REDIR },  // raw files are treated as CD like audio
    { ""        , "sox %s -t wav -"               REDIR },  // Rest, may be possible with sox
};

#undef REDIR


int  readwave ( stereo_t* buff, size_t maxlen, const char* name, size_t* len )
{
    char*           command = malloc (2*strlen(name) + 512);
    char*           name_q  = malloc (2*strlen(name) + 128);
    unsigned short  header [22];
    FILE*           fp;
    size_t          i;
    size_t          j;

    // The *nice* shell quoting
    i = j = 0;
    if ( name[i] == '-' )
        name_q[j++] = '.',
        name_q[j++] = '/';
        
    while (name[i]) {
        if ( !isalnum (name[i]) && name[i]!='-' && name[i]!='_' && name[i]!='.' )
            name_q[j++] = '\\';
        name_q[j++] = name[i++];
    }
    name_q[j] = '\0';

    fprintf (stderr, "Reading %s", name );
    for ( i = 0; i < sizeof(decoder)/sizeof(*decoder); i++ )
        if ( has_ext (name, decoder[i].extention) ) {
            sprintf ( command, decoder[i].command, name_q );
	    break;
	}

    free (name_q);
    if ( (fp = popen (command, "r")) == NULL ) {
        fprintf (stderr, "Can't exec:\n%s\n", command );
        exit (1);
    }
    free (command);
    
    fprintf (stderr, " ..." );
    fread ( header, sizeof(*header), sizeof(header)/sizeof(*header), fp );
    switch ( header[11] ) {
        case 2:
            *len = fread ( buff, sizeof(stereo_t), maxlen, fp );
            break;
        case 1:
            *len = fread ( buff, sizeof(sample_t), maxlen, fp );
            for ( i = *len; i-- > 0; )
                buff[i][0] = buff[i][1] = ((sample_t*)buff) [i];
            break;
        default:
            pclose (fp);
            return -1;
    }
    pclose ( fp ); 
    fprintf (stderr, "\n" );
    return header[12];
}


double  cross_analyze_new ( const stereo_t* p1, const stereo_t *p2, size_t len )
{
    float   P1 [MAX][2];
    float   P2 [MAX][2];
    int     i;
    int     maxindex;
    double  sum1;
    double  sum2;
    double  max;
    double  y1;
    double  y2;
    double  y3;
    double  yo;
    double  xo;
    double  tmp;
    double  tmp1;
    double  tmp2;
    int     ret = 0;
    
    // Calculating effective voltage
    sum1 = sum2 = 0.;
    for ( i = 0; i < len; i++ ) {
        sum1 += (double)p1[i][0] * p1[i][0];
	sum2 += (double)p2[i][0] * p2[i][0];
    }
    sum1 = sqrt ( sum1/len );
    sum2 = sqrt ( sum2/len );
    
    // Searching beginning of signal
    for ( i = 0; i < len; i++ )
        if ( abs (p1[i][0]) >= sum1  &&  abs (p2[i][0]) >= sum2 )
	    break;
    p1  += i;
    p2  += i;
    len -= i;
    
    if ( len <= MAX )
        return 0;
       
    // Filling arrays for FFT
rep:sum1 = sum2 = 0.;
    for ( i = 0; i < MAX; i++ ) {
        tmp1  = p1 [i][0] - p1 [i+1][0];
        tmp2  = p2 [i+ret][0] - p2 [i+ret+1][0];
        sum1 += tmp1*tmp1;
        sum2 += tmp2*tmp2;
        P1 [i][0] = tmp1;
        P2 [i][0] = tmp2;
        P1 [i][1] = 0.;
        P2 [i][1] = 0.;
    }
	
    fft (P1, MAX);
    fft (P2, MAX);
    
    for ( i = 0; i < MAX; i++ ) {
        double  a0 = P1 [i][0];
	double  a1 = P1 [i][1];
        double  b0 = P2 [(MAX-i)&(MAX-1)][0];
	double  b1 = P2 [(MAX-i)&(MAX-1)][1];
        P1 [i][0] = a0*b0 - a1*b1;
	P1 [i][1] = a0*b1 + a1*b0;
    }
    
    fft (P1, MAX);

    max = P1 [maxindex = 0][0];
    for ( i = 1; i < MAX; i++ )
        if ( P1[i][0] > max )
            max = P1 [maxindex = i][0];

    y2 = P1 [ maxindex           ][0];
    y1 = P1 [(maxindex-1)&(MAX-1)][0] - y2;
    y3 = P1 [(maxindex+1)&(MAX-1)][0] - y2;

    xo = 0.5 * (y1-y3) / (y1+y3);
    yo = 0.5 * ( (y1+y3)*xo + (y3-y1) ) * xo;
        
    if (maxindex > MAX/2 )
        maxindex -= MAX;
        
    ret += maxindex;
    tmp = 100./MAX/sqrt(sum1*sum2);
    printf ( "[%5d]%8.4f  [%5d]%8.4f  [%5d]%8.4f  [%10.4f]%8.4f\n", 
             ret- 1, (y1+y2)*tmp, 
	     ret   ,     y2 *tmp, 
	     ret+ 1, (y3+y2)*tmp, 
	     ret+xo, (yo+y2)*tmp );
	     
    if (maxindex) goto rep;
    
    return ret + xo;
}


short  to_short ( int x )
{
    if ( x == (short)x ) return (short)x;
    if ( x > 32767 ) return 32767;
    return -32768;
}


void  DC_cancel ( stereo_t* p, size_t len )
{
    double  sum1 = 0;
    double  sum2 = 0;
    size_t  i;
    int     diff1;
    int     diff2;
    
    for (i = 0; i < len; i++ ) {
        sum1 += p[i][0];
        sum2 += p[i][1];
    }
    if ( fabs(sum1) < len  &&  fabs(sum2) < len )
        return;
        
    diff1 = round ( sum1 / len );
    diff2 = round ( sum2 / len );
    fprintf (stderr, "Removing DC (left=%d, right=%d)\n", diff1, diff2 );
    
    for (i = 0; i < len; i++ ) {
        p[i][0] = to_short (p[i][0] + diff1);
        p[i][1] = to_short (p[i][1] + diff2);
    }
}


void  usage ( void )
{
    fprintf ( stderr, 
        "usage:  abx File_A File_B\n"
	"\n"
	"File_A and File_B are loaded and played. You can press the following keys:\n"
	"\n"
	"  a/A:    Listen to File A\n"
	"  b/B:    Listen to File B\n"
	"  x/X:    Listen to the randomly selected File X, which is A or B\n"
	"  Ctrl-A: You vote for X=A\n"
	"  Ctrl-B: You vote for X=B\n"
	"  m/M:    Alternating playing A and B. The switching is done appr. 6 times/sec\n"
	"  d:      Listen to the difference A-B (+ 6 dB)\n"
	"  D:      Listen to the difference A-B (+16 dB)\n"
	"  Ctrl-D: Listen to the difference A-B (+26 dB)\n"
	"  0...9:  You listen to B, but the errors between A and B are amplified by 0-9 dB\n"
	"  Q:      Quit the program\n"
	"\n"
	"I advice to use the \"better\"/reference file as File_A and the other as File_B\n"
	"\n"
    );
}


int  main ( int argc, char** argv )
{
    stereo_t*  _A = calloc ( sizeof(stereo_t), MAX_LEN );
    stereo_t*  _B = calloc ( sizeof(stereo_t), MAX_LEN );
    stereo_t*  A  = _A;
    stereo_t*  B  = _B;
    size_t     len_A;
    size_t     len_B;
    long       freq1;
    long       freq2;
    int        shift;
    double     fshift;

    switch ( argc ) {
    case 0:
    case 1:
    case 2:
    default:
        usage ();
	return 1;
    case 3:
        break;
    }
    
    freq1 = readwave ( A, MAX_LEN, argv[1], &len_A );
    DC_cancel ( A, len_A );
    freq2 = readwave ( B, MAX_LEN, argv[2], &len_B );
    DC_cancel ( B, len_B );
    
    if ( freq1 != freq2 ) {
        fprintf ( stderr, "Different sample frequencies currently not supported\n");
	return 2;
    }
    
    fshift = cross_analyze_new     ( A, B, len_A < len_B  ?  len_A  :  len_B );
    shift  = floor ( fshift + 0.5 );
    
    if (shift > 0) {
        fprintf ( stderr, "Delaying A by %d samples\n", +shift);
        B     += shift;
        len_B -= shift;
    }
    if (shift < 0) {
        fprintf ( stderr, "Delaying B by %d samples\n", -shift);
        A     -= shift;
        len_A += shift;
    }

    set ();
    testing ( A, B, len_A < len_B  ?  len_A  :  len_B, freq1 );
    reset ();

    free (_A);
    free (_B);
    return 0;
}

/* end of abx.c */
