#ifndef ID3TAG_H_INCLUDED
#define ID3TAG_H_INCLUDED
#include "lame.h"

void id3_inittag(ID3TAGDATA *tag);
void id3_buildtag(ID3TAGDATA *tag);
int id3_writetag(char* filename, ID3TAGDATA *tag);


/*
 * Array of all possible music genre. Grabbed from id3ed
 */
extern int genre_last;
extern char *genre_list[];
#endif
