/*
 *  Place to define all lame types needed by more than one source file
 *  Also there limits are defined here.
 *
 *
 */
 
#ifndef  LAMETYPE_H_DEFINED
# define LAMETYPE_H_DEFINED
 
#include <limits.h>
#include <float.h>
 
/* simple scalar types */			/* limits of the type */

typedef unsigned int		uint;		/* UINT_MIN   ... UINT_MAX   */
typedef unsigned short int	ushort;		/* USHRT_MIN  ... USHRT_MAX  */
typedef unsigned long int	ulong;		/* ULONG_MIN  ... ULONG_MAX  */

typedef signed short int        sample_t;	/* SAMPLE_MIN ... SAMPLE_MAX */
#define SAMPLE_MIN		(-32768)
#define SAMPLE_MAX		(+32767)

 
/* more complex structs and unions */
 
 
/* the two big global lame structs */
 
typedef struct {
   ...
} lame_internal_flags;

typedef struct {
   ...
} lame_global_flags;
 
#endif  /* LAMETYPE_H_DEFINED */
