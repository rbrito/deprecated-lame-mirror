/* -*- mode: C; mode: fold -*- */

/* For jed: Style Guide file for lame available on request */


/* 
 * Short function prototypes (less than 72 characters and max. 2 arguments)
 */

static void  name ( int param1 )
{
    ...
} /* end of name () */

/*
 * Long function prototypes (more than 72 characters or more than 2 arguments)
 */

static void  name ( 
        int                param1, 
        const char*        p2,      /* comment */ 
        struct BlockInfo*  param3 )
{
    /* comment end if function is longer than 24 lines */
    /* in C++ also comment prototype */
    ...
} /* end of name(int,const char*,struct BlockInfo*) */

/*
 * Note: You can additional comment parameters (better is to use meaningful expressive names)
 *       Indent as shown above, break this role if it eats to much spaces)
 *       Use, if possible, at least 2 spaces between type and argument name
 */

/*
 * Some examples for indentation
 */
    
void function ( int param ) 
{  

    /* normal if */
    if ( a == b )
        c = d;
    
    /* short if's and statements with very similar structure */
    if ( a == b ) c = d;
    if ( e == f ) g = h;
    if ( j == k ) l = m;
    
    /* normal if/else */
    if ( a == b )
        c = d;
    else
        d = e;
    
    /* use {} if _one_ tree contains more than 1 statement or a more complex statement */
    if ( a == b ) {
        c = d;
    } else {
        e -> tiere = fux_du_hast_die_ganz_gestohlen ();
    }
    
    if ( a == b ) {
        c = d;
    } else {
        if ( g == h )
            i = j;
    }
    
    /* comment else and } on long function (longer than 24 lines)
     * compact this comments
     * use for the "} else {" two lines on such functions 
     */
    
    if ( a == b ) {
        c = d;
        e = f;
        g = h;
        ...
    }
    else { /* (a==b) */
        i = j;
        k = l;
        ...
    } /* end if (a==b) */
    
    
    /* simple while loop */
    while ( a == b )
        c = d;
    
    while ( a == b ) {
        c = d;
        e = f;
    }
    
    while ( a == b ) {
        c = d;
        e = f;
        ...
    } /* end while (a==b) */
    
    
    /* 
     * may be avoid this and use always {} for do {} while ()
     * It needs the same line count and avoids confusion with while () {}
     */
    do
        a = b;
    while ( c == d );
    
    do {
        a = b;
        c = d;
    } while ( e == f );
    
    do { /* begin while (g==h) */
        a = b;
        c = d;
        e = f;
    } while ( g == h );


    /* 
     * comment wanted fall throughs
     * have cases for all possible input of the switch expression
     * 
     */
    
    switch ( a ) {
    case b:
        f = g;
        /* fall through */
    case c:  
        a = b;      
        break;
    case d:  
        c = d;      
        break;
    case e:
        break;
    default: 
        assert (0); 
        break;
    }
    
    /*
     * For a lot of short and nearly identical statements also this can be used
     * Don't mix break and return in this notation
     */
    
    switch ( a ) {
    case c:  a = b;      break;
    case d:  c = d;      break;
    case e:  f = g;      break;
    default: assert (0); break;
    }

    /* Both notations can be mixed, if you use that, begin with the compact form
     * and switch once to the long form 
     */

    switch ( a ) {
    case c:  a = b;      break;
    case d:  c = d;      break;
    case e:  f = g;      break;
    case f:
        f = g;
        h = i;
        /* fall through */
    case g:  
        a = b;
        j = k;      
        return; /* early return, ugly for debugging */
    case h:  
        c = d;
        m = n;      
        break;
    default: 
        assert (0); 
        break;
    }
    
    /* 
     * Be very miserly to use goto's, but don't damn it
     * It is much more readable (and faster) than often used expressions 
     * to avoid goto's with a waste of effort
     * 
     * Also it can be used to prevent early returns in the middle
     * of a function. Early returns are difficult to debug.
     */
    
    if ( error )
        goto end;
    
    /* for loops should not misuse the commata operator */
    
    sum = 0.;
    for ( i = 0; i < 10; i++ )
        sum += c[i];
    
    /* for loop with a very complex expression or more than 1 statement */
    for ( i = 0; i < 10; i++ ) {
        sum1 += c[i];
        sum2 += d[i];
    }
    
    /* labels should be begin in the first column and have a useful name, not things like l1 */
end:
    fprintf ( stderr, "Result is %u\n", c);
    return c;
}

/* Indentation of macro statements */

#ifdef __WIN32__
# include <windows.h>
#else
# include <unistd.h>
#endif

/* avoid mixing of #ifdef ... and #elif defined(...) */

#if   defined (__WIN32__)
# include <windows.h>
#elif defined (__WIN16__)
# include <unistd.h>
#endif


/* Using of spaces */

int testfunction ( void )
{
    /* define one variable per file, between type and name there should be
     * at least two spaces. Break this rule if it eats to much spaces.
     * Define simple variables at the end, more complex at the beginning
     * Group, if possible, functionality
     */
    
    static const volatile struct food**  food_dst;
    const struct llama*  Animal;
    const char*          name;
    unsigned long int    i;
    char                 ch;

    /*
     * Initialized variables, try to align kommatas
     */
    
    static char*         namearray [] = { "llama", "ant" };
    static char*         trees [] = {
        "binary", "oak", "beech"   , "alder", "pine",
        "spruce", "yew", "chestnut", "maple", "sequoià" 
    };
    
    /* add an empty line after more than 1 variable definition, 
     * two lines after more than 10 definitions */

    
    /* All operator should be separated by spaces. There are some exceptions
     */
    
    /* structure element operator '.' don't need this so much */
    /* assign operator '=' always needs the spaces */
    
    ptr.elem = 4;
    
    /* If token are very short and expressions are very long, you can save some
     * inner spaces. Note precedences to avoid irritations */
    
    a = s[0]*a[0] + s[1]*a[1] + s[2]*a[2] + s[3]*a[3] + noise ();
    a = sideinfo + tableindex [4] + internal [ ptr -> verydifficultelement ];
    a = s + t[4] + in [ p -> val ];
    a = c*d + e*f;
    
    /* ? : should use two spaces, () for compare operators are prefered */
    a = (c == d)  ?  e  :  f;
    
    /* alternative forms for long expressions */
    
    a = flag  ?  this_is_the_first_expression
              :  this_is_the_second_expression;
    
    a = a_very_difficult_boolean_expression
        ?  expr1  :  expr2;
    
    /* function calls: separate arguments and function name and () by spaces,
     * use a little bit less spaces for in expression spaces
     */
    
    c = array + 4;
    a = function ( 4, "Hase", "Igel", array+4 ); 
    
    /* Break very long call over multiple lines, document parameters
     * Try to avoid functions with 6 and more arguments. This reeks like bad design.
     * 
     * Notice the possiblities of using different spacing for the array index.
     * Don't toggle them too much (like here).
     */
    
    array [ index1 ] [ index ] = "Hello"; 
    array [index1] [index2] = this_is_a_function ( param1,
                                                   param2,  /* number of quirks */
                                                   param3,
                                                   param4 );
    array [i] [j] = "Hello";
    a[i][j] = "Hallo";
    variable = this_is_a_function ( param1, param2, param3,
                                    a [i],  param4, a [12] );
    

---------------------------------------------------------------------------------------    
    
This is the first try of a Coding Style:

* Don't use tabulators (the character with the value '\t') in source code,
  especially these with a width of unequal 8. Lame sources are using
  different sizes for tabulators.

* Don't set the macro NDEBUG in alpha and beta versions.

* If you assume something, check this with an assert().

* Functions should be not longer than 50 lines of code.
  Every function should only do ONE thing, and this should
  be done well.

* Document functions.

* Use spaces in source code. They increase readablity.

* Use self explaining variable names, especially for interfaces.

* Use an intentation of 4. It's a good compromise between the
  2 and the 8.


This is the first try of a Programming Style:

* Don't use single 'short' variables to save storage.
  Short variables are especially on Pentium Class Computer much slower than
  int's. DEC alpha also hates short variables.

  Example:   float bla [1024];
             short i;
             for ( i = 0; i < 1024; i++ )
                bla [i] = i;  

* To store PCM samples, use the type 'sample_t' defined in "lame.h". 
  Currently this is a 'signed short int' and I planed to move this to
  'float'.

* Use the 'size_t' to store sizes of memory objects. Use 'off_t'
  to store file offsets.

* Use the 'const' attribute if you don't intend to change the variable.

* use the 'unsigned' attribute for integer if a negative value makes
  absolutely no sense.

* Use SI base units. Lame mixes Hz, kHz, kbps, bps. This is mess.

  Example:
        float     wavelength_green = 555.e-9;
        unsigned  data_rate        = 128000;
        float     lowpass_freq     = 12500.;
  
  Converting between user input and internal representation should be done
  near the user interface, not in the most inner loop of an utility
  function.

----------------------------------------------------------------------------------

Snafucations:

   The winner is: stereo
   
   Mostly the structure element 'stereo' contains the number of channels, 
   so a stereo = TRUE setups mono. But there are also pieces of code, where 
   a stereo == 1 really has the meaning of stereo. Converting is done by
   
     int stereo = fpc -> stereo - 1;
   
   And there is also code, where 1 and 2 are valid values for the variable,
   and there is code like:
   
       if ( fpc -> stereo ) ...
    
   which makes absolutely no sense.
   
----------------------------------------------------------------------------------

   version = 0            MPEG-2
   brhist_vbrmin        is nether bps nor kbps but the index in the table (1...14)


----------------------------------------------------------------------------------

                Chapter 1: Indentation

Tabs are 8 characters, and thus indentations are also 8 characters. 
There are heretic movements that try to make indentations 4 (or even 2!)
characters deep, and that is akin to trying to define the value of PI to
be 3. 

Rationale: The whole idea behind indentation is to clearly define where
a block of control starts and ends.  Especially when you've been looking
at your screen for 20 straight hours, you'll find it a lot easier to see
how the indentation works if you have large indentations. 

Now, some people will claim that having 8-character indentations makes
the code move too far to the right, and makes it hard to read on a
80-character terminal screen.  The answer to that is that if you need
more than 3 levels of indentation, you're screwed anyway, and should fix
your program. 

In short, 8-char indents make things easier to read, and have the added
benefit of warning you when you're nesting your functions too deep. 
Heed that warning. 


                Chapter 2: Placing Braces

The other issue that always comes up in C styling is the placement of
braces.  Unlike the indent size, there are few technical reasons to
choose one placement strategy over the other, but the preferred way, as
shown to us by the prophets Kernighan and Ritchie, is to put the opening
brace last on the line, and put the closing brace first, thusly:

        if (x is true) {
                we do y
        }

However, there is one special case, namely functions: they have the
opening brace at the beginning of the next line, thus:

        int function(int x)
        {
                body of function
        }

Heretic people all over the world have claimed that this inconsistency
is ...  well ...  inconsistent, but all right-thinking people know that
(a) K&R are _right_ and (b) K&R are right.  Besides, functions are
special anyway (you can't nest them in C). 

Note that the closing brace is empty on a line of its own, _except_ in
the cases where it is followed by a continuation of the same statement,
ie a "while" in a do-statement or an "else" in an if-statement, like
this:

        do {
                body of do-loop
        } while (condition);

and

        if (x == y) {
                ..
        } else if (x > y) {
                ...
        } else {
                ....
        }
                        
Rationale: K&R. 

Also, note that this brace-placement also minimizes the number of empty
(or almost empty) lines, without any loss of readability.  Thus, as the
supply of new-lines on your screen is not a renewable resource (think
25-line terminal screens here), you have more empty lines to put
comments on. 


                Chapter 3: Naming

C is a Spartan language, and so should your naming be.  Unlike Modula-2
and Pascal programmers, C programmers do not use cute names like
ThisVariableIsATemporaryCounter.  A C programmer would call that
variable "tmp", which is much easier to write, and not the least more
difficult to understand. 

HOWEVER, while mixed-case names are frowned upon, descriptive names for
global variables are a must.  To call a global function "foo" is a
shooting offense. 

GLOBAL variables (to be used only if you _really_ need them) need to
have descriptive names, as do global functions.  If you have a function
that counts the number of active users, you should call that
"count_active_users()" or similar, you should _not_ call it "cntusr()". 

Encoding the type of a function into the name (so-called Hungarian
notation) is brain damaged - the compiler knows the types anyway and can
check those, and it only confuses the programmer.  No wonder MicroSoft
makes buggy programs. 

LOCAL variable names should be short, and to the point.  If you have
some random integer loop counter, it should probably be called "i". 
Calling it "loop_counter" is non-productive, if there is no chance of it
being mis-understood.  Similarly, "tmp" can be just about any type of
variable that is used to hold a temporary value. 

If you are afraid to mix up your local variable names, you have another
problem, which is called the function-growth-hormone-imbalance syndrome. 
See next chapter. 

                
                Chapter 4: Functions

Functions should be short and sweet, and do just one thing.  They should
fit on one or two screenfuls of text (the ISO/ANSI screen size is 80x24,
as we all know), and do one thing and do that well. 

The maximum length of a function is inversely proportional to the
complexity and indentation level of that function.  So, if you have a
conceptually simple function that is just one long (but simple)
case-statement, where you have to do lots of small things for a lot of
different cases, it's OK to have a longer function. 

However, if you have a complex function, and you suspect that a
less-than-gifted first-year high-school student might not even
understand what the function is all about, you should adhere to the
maximum limits all the more closely.  Use helper functions with
descriptive names (you can ask the compiler to in-line them if you think
it's performance-critical, and it will probably do a better job of it
that you would have done). 

Another measure of the function is the number of local variables.  They
shouldn't exceed 5-10, or you're doing something wrong.  Re-think the
function, and split it into smaller pieces.  A human brain can
generally easily keep track of about 7 different things, anything more
and it gets confused.  You know you're brilliant, but maybe you'd like
to understand what you did 2 weeks from now. 


                Chapter 5: Commenting

Comments are good, but there is also a danger of over-commenting.  NEVER
try to explain HOW your code works in a comment: it's much better to
write the code so that the _working_ is obvious, and it's a waste of
time to explain badly written code. 

Generally, you want your comments to tell WHAT your code does, not HOW. 
Also, try to avoid putting comments inside a function body: if the
function is so complex that you need to separately comment parts of it,
you should probably go back to chapter 4 for a while.  You can make
small comments to note or warn about something particularly clever (or
ugly), but try to avoid excess.  Instead, put the comments at the head
of the function, telling people what it does, and possibly WHY it does
it. 


                Chapter 6: You've made a mess of it

That's OK, we all do.  You've probably been told by your long-time Unix
user helper that "GNU emacs" automatically formats the C sources for
you, and you've noticed that yes, it does do that, but the defaults it
uses are less than desirable (in fact, they are worse than random
typing - a infinite number of monkeys typing into GNU emacs would never
make a good program). 

So, you can either get rid of GNU emacs, or change it to use saner
values.  To do the latter, you can stick the following in your .emacs file:

(defun linux-c-mode ()
  "C mode with adjusted defaults for use with the Linux kernel."
  (interactive)
  (c-mode)
  (c-set-style "K&R")
  (setq c-basic-offset 8))

This will define the M-x linux-c-mode command.  When hacking on a
module, if you put the string -*- linux-c -*- somewhere on the first
two lines, this mode will be automatically invoked. Also, you may want
to add

(setq auto-mode-alist (cons '("/usr/src/linux.*/.*\\.[ch]$" . linux-c-mode)
                       auto-mode-alist))

to your .emacs file if you want to have linux-c-mode switched on
automagically when you edit source files under /usr/src/linux.

But even if you fail in getting emacs to do sane formatting, not
everything is lost: use "indent".

Now, again, GNU indent has the same brain dead settings that GNU emacs
has, which is why you need to give it a few command line options. 
However, that's not too bad, because even the makers of GNU indent
recognize the authority of K&R (the GNU people aren't evil, they are
just severely misguided in this matter), so you just give indent the
options "-kr -i8" (stands for "K&R, 8 character indents"). 

"indent" has a lot of options, and especially when it comes to comment
re-formatting you may want to take a look at the manual page.  But
remember: "indent" is not a fix for bad programming. 
   