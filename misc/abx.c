/* $Id$ */
/*
 * Usage: abx original_file test_file
 * Ask you as long as the probability is below the given percentage
 * that you recognize differences
 *
 * Example: abx music.wav music.mp3
 *
 * Note:    lame must be within the current path
 *
 * Fehler:  Keine Crosskorrelationsanalyse (um Versätze zu erkennen und ...)
 *          kein DC canceling (Bewirkt starke Umschaltgeräusche)
 *          kein Normalisieren
 *          kein ordentliches WAV-header lesen
 *          keine Mausbedienbarkeit
 *          Konsole richtig setzen
 *	    Nur 16 bit Stereo, 44.1 kHz
 *          Use a, b, x, ^a, ^b, Q
 *          Nur 1200 tests möglich
 *          Nicht mehr als 2 Dateien vergleichbar
 *          
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#elif defined(HAVE_CONFIG_MS_H)
# include <configMS.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>                      
#include <math.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#ifdef HAVE_SYS_SOUNDCARD_H
# include <sys/soundcard.h>
#elif defined(HAVE_LINUX_SOUNDCARD_H)
# include <linux/soundcard.h>
#else
# include <linux/soundcard.h>
#endif

#define BF   4410

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


typedef signed short stereo_t [2];


int feed ( int fd, const stereo_t* p, int len )
{
    write ( fd, p, sizeof(stereo_t) * len );
    return len;
}


int feed2 ( int fd, const stereo_t* p1, const stereo_t* p2, int len )
{
    stereo_t  tmp [32768];
    int       i;
    
    for ( i = 0; i < len; i++ ) {
        double f = cos ( M_PI/2*i/len );
        f *= f;
        tmp [i] [0] = floor ( p1 [i] [0] * f + p2 [i] [0] * (1. - f) + 0.5 );
        tmp [i] [1] = floor ( p1 [i] [1] * f + p2 [i] [1] * (1. - f) + 0.5 );
    }

    write ( fd, tmp, sizeof(stereo_t) * len );
    return len;
}


void setup ( int fdd, int samples )
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

    org = arg = 44100;
    if ( -1 == (status = ioctl (fdd, SOUND_PCM_WRITE_RATE, &arg)) )
        perror ("SOUND_PCM_WRITE_WRITE ioctl failed");
    fprintf (stderr, "%5u Hz*%.3f s\n", arg, (double)samples/arg );
}

void testing ( const stereo_t* A, const stereo_t* B, size_t len )
{
    int  c;
    int  fd;
    int  rnd   = random_number ();
    int  state = rnd;
    int  index = 0;

    fd = open ("/dev/dsp", O_WRONLY);

    if ( -1 == fd ) {
        perror ("opening of /dev/dsp failed");
        return;
    }

    setup ( fd, len );
    
    fprintf (stderr, "\rListening A " );
    while ( 1 ) {
        switch ( c = sel() ) {
        case 'A' :
        case 'a' :
            fprintf (stderr, "\rListening A " );
            if ( state != 0 )
                state = 2;
            break;

        case 'B' :
        case 'b' :
            fprintf (stderr, "\rListening B " );
            if ( state != 1 )
                state = 3;
            break;

        case 'X' :
        case 'x' :
            fprintf (stderr, "\rListening X " );
            if ( state != rnd )
                state = rnd + 2;
            break;

        case 'M' :
        case 'm' :
            fprintf (stderr, "\rListening M " );
	    state = 4;
            break;
	    
        case 'A' & 0x1F:
            fprintf (stderr, "  X is A? " );
            eval ( rnd == 0 );
            rnd   = random_number ();
            if ( state != rnd )
                state = rnd + 2;
            fprintf (stderr, "\rListening X " );
            break;

        case 'B' & 0x1F:
            fprintf (stderr, "  X is B? " );
            eval ( rnd == 1 );
            rnd   = random_number ();
            if ( state != rnd )
                state = rnd + 2;
            fprintf (stderr, "\rListening X " );
            break;

        case -1: 
            break;

        default:
            fprintf (stderr, "\a" );
            break;
            
        case 'Q':
            fprintf ( stderr, "\n");
            close (fd);
            fprintf ( stderr, "\n");
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

        default:
            assert (0);
        }
        if (index >= len)
            index = 0;
    }
}

int  has_ext ( const char* name, const char* ext )
{
    if ( strlen (name) < strlen (ext) )
        return 0;
    name += strlen (name) - strlen (ext);
    return strcasecmp (name, ext)  ?  0  :  1;
}

void readwave ( stereo_t* buff, size_t maxlen, const char* name, size_t* len )
{
    char*           command = malloc (strlen(name) + 128);
    unsigned short  header [22];
    FILE*           fp;

    fprintf (stderr, "Reading %s", name );
    // MPEG Layer I, II, III: www.iis.fhg.de, www.lame.org, www.toolame.org
    if ( has_ext (name, ".mp1")  ||  has_ext (name, ".mp2")  ||  has_ext (name, ".mp3") )
        sprintf ( command, "/usr/local/bin/mpg123 -w - '%s' 2> /dev/null", name );
    // MPEGplus: www.stud.uni-hannover.de/user/73884/
    else if ( has_ext (name, ".mpp")  ||  has_ext (name, ".mp+") )
        sprintf ( command, "/usr/local/bin/mppdec '%s' - 2> /dev/null", name );
    // Advanced Audio Coding: www.psytel.com
    else if ( has_ext (name, ".aac") )
        sprintf ( command, "/usr/local/bin/faad '%s' - 2> /dev/null", name );
    // Ogg Vorbis: www.ogg.org
    else if ( has_ext (name, ".ogg") )
        sprintf ( command, "echo '%s'  - 2> /dev/null", name );
    // Real Audio: www.real.com
    else if ( has_ext (name, ".ra") )
        sprintf ( command, "echo '%s'  - 2> /dev/null", name );
    // ePAC: www.audioveda.com
    else if ( has_ext (name, ".epac") )
        sprintf ( command, "echo '%s'  - 2> /dev/null", name );
    // QDesign Music 2: www.qdesign.com
    else if ( has_ext (name, ".qdm") )
        sprintf ( command, "echo '%s'  - 2> /dev/null", name );
    // TwinVQ: www.yamaha-xg.com
    else if ( has_ext (name, ".vqf") )
        sprintf ( command, "echo '%s'", name );
    // Microsoft Media Audio: www.windowsmedia.com
    else if ( has_ext (name, ".wma") )
        sprintf ( command, "echo '%s'", name );
    // Monkey's Audio Codec: www.monkeysaudio.com
    else if ( has_ext (name, ".mac") )
        sprintf ( command, "echo '%s'", name );
    // Lossless predictive Audio Compression: www-ft.ee.tu-berlin.de/~liebchen/lpac.html
    else if ( has_ext (name, ".lpac") )
        sprintf ( command, "echo '%s'", name );
    // Rest, may be possible with sox
    else 
        sprintf ( command, "sox '%s' -t wav - 2> /dev/null", name );
    
    if ( (fp = popen (command, "r")) == NULL ) {
        fprintf (stderr, "Can't exec:\n%s\n", command );
        exit (1);
    }
    free (command);
    
    fprintf (stderr, " ..." );
    fread ( header, sizeof(*header), sizeof(header)/sizeof(*header), fp );
    *len = fread ( buff, sizeof(stereo_t), maxlen, fp );
    pclose ( fp ); 
    fprintf (stderr, "\n" );
}


void print_usage(void)
{
    puts("Usage: abx <original_file> <test_file>");
}


int main ( int argc, char** argv )
{
    static stereo_t  A [180 * 44100];
    static stereo_t  B [180 * 44100];
    size_t           len_A;
    size_t           len_B;
    
    if (argc != 3) {
        print_usage();
        exit(1);
    }

    readwave ( A, sizeof(A)/sizeof(*A), argv[1], &len_A );
    readwave ( B, sizeof(B)/sizeof(*B), argv[2], &len_B );

    set ();
    testing ( A, B, len_A < len_B  ?  len_A  :  len_B );
    reset ();
    
    return 0;
}
