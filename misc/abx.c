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
 *          
 */

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
#include <linux/soundcard.h>


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
    int  fd    = open ("/dev/dsp", O_RDWR);
    int  rnd   = random_number ();
    int  state = rnd;
    int  index = 0;

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

void readwave ( stereo_t* buff, size_t maxlen, const char* name, size_t* len )
{
    char*           command = malloc (strlen(name) + 128);
    unsigned short  header [22];
    FILE*           fp;

    fprintf (stderr, "Reading %s", name );
    sprintf ( command, "/usr/local/bin/lame --decode '%s' - 2> /dev/null", name );    
    
    fp = popen (command, "r");
    if ( fp == NULL ) {
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


int main ( int argc, char** argv )
{
    static stereo_t  A [180 * 44100];
    static stereo_t  B [180 * 44100];
    size_t           len_A;
    size_t           len_B;
    
    readwave ( A, sizeof(A)/sizeof(*A), argv[1], &len_A );
    readwave ( B, sizeof(B)/sizeof(*B), argv[2], &len_B );

    set ();
    testing ( A, B, len_A < len_B  ?  len_A  :  len_B );
    reset ();
    
    return 0;
}




#if 0 /******************************************************************************************************/

static struct termios current,	/* new terminal settings             */
                      initial;	/* initial state for restoring later */

/* Restore term-settings to those saved when term_init was called */

void  term_restore (void)  
  {
    tcsetattr(0, TCSANOW, &initial);
  }  /* term_restore */

/* Clean up terminal; called on exit */

void  term_exit ()  
  {
    term_restore();
  }  /* term_exit */

/* Will be called when ctrl-Z is pressed, this correctly handles the terminal */

void  term_ctrl_z ()  
  {
    signal(SIGTSTP, term_ctrl_z);
    term_restore();
    kill(getpid(), SIGSTOP);
  }  /* term_ctrlz */

/* Will be called when application is continued after having been stopped */

void  term_cont ()  
  {
    signal(SIGCONT, term_cont);
    tcsetattr(0, TCSANOW, &current);
  }  /* term_cont */

/* Needs to be called to initialize the terminal */

void  term_init (void)
  /* if stdin isn't a terminal this fails. But then so does tcsetattr so it doesn't matter. */
  {
    tcgetattr(0, &initial);
    current = initial;			/* Save a copy to work with later  */
    signal(SIGINT,  term_exit);		/* We _must_ clean up when we exit */
    signal(SIGQUIT, term_exit);
    signal(SIGTSTP, term_ctrl_z);	/* Ctrl-Z must also be handled     */
    signal(SIGCONT, term_cont);
    atexit(term_exit);
  }  /* term_init */

/* Set character-by-character input mode */

void  term_character (void)  
  {
  /* One or more characters are sufficient to cause a read to return */
    current.c_cc[VMIN]   = 1;
    current.c_cc[VTIME]  = 0;  			/* No timeout; read forever until ready */
    current.c_iflag     &= ~(INLCR|ICRNL|ISTRIP|IGNCR|IUCLC|ISIG|IXON); 
    						/* Disables any translations, I hope */
    current.c_iflag     |=  (IGNBRK);
    current.c_lflag     &= ~(ICANON|ECHO);	/* Line-by-line mode off, echo off */
    
    tcsetattr(0, TCSANOW, &current);
  }  /* term_character */


/* Set character-by-character input mode, version of pfk */

void  term_character_2 (void)  
  {
  /* One or more characters are sufficient to cause a read to return */
    cfmakeraw(&current);			/* Vielleicht geht das */

    current.c_oflag     |= ONLCR|OPOST;		/* enables NL => CRLF on output */
    
    tcsetattr(0, TCSANOW, &current);
  }  /* term_character */

/*
  Note:
       cfmakeraw sets the terminal attributes as follows:
                   termios_p->c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP
                                          |INLCR|IGNCR|ICRNL|IXON);
                   termios_p->c_oflag &= ~OPOST;
                   termios_p->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
                   termios_p->c_cflag &= ~(CSIZE|PARENB);
                   termios_p->c_cflag |= CS8;
*/

/* Return to line-by-line input mode */

void  term_line (void)
  {
    current.c_cc[VEOF]  = 4;
    current.c_lflag    |= ICANON;
    tcsetattr(0, TCSANOW, &current);
  }  /* term_line */

/* Key pressed ? */

int  kbhit  (void)  {
  struct timeval tv;
  fd_set         read_fd;

  /* Do not wait at all, not even a microsecond */
  tv.tv_sec  = 0;
  tv.tv_usec = 0;

  /* Must be done first to initialize read_fd */
  FD_ZERO(&read_fd);

  /* Makes select() ask if input is ready;  0 is file descriptor for stdin */
  FD_SET(0, &read_fd);

  /* The first parameter is the number of the largest fd to check + 1 */
  if (select(1, &read_fd,
                NULL,		/* No writes        */
                NULL,		/* No exceptions    */
                &tv)  == -1)
     return 0;			/* an error occured */

  /* read_fd now holds a bit map of files that are readable. */
  /* We test the entry for the standard input (file 0).      */
  return(FD_ISSET(0, &read_fd)  ?  1  :  0);
}  /* kbhit */

/* Small testing program */

int  main (void)
  {
    register c;

    term_init();
    term_character_2();
    printf("Press a key, ^D to exit ...\n");

    while ((c = getchar()) != 0x04)
      if (c  <  ' ' || c == 0x7F)
        printf("non-pritable character $%02X = ^%c\n", c, c==0x7F ? '?' : c+'@');
      else if (c >= 0x80 && c < 0xA0)
        printf("non-pritable character $%02X\n", c);
      else
        printf("printable    character `%c'\n",  c);
      
    printf("\n^D pressed. Now press any key to continue ...");
    getchar();
    printf("\b\b\b\b. Done.\nEnd of program.\n");
    return 0;
  }

#endif

