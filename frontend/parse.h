int  usage (FILE* const fp, const char* ProgramName );
int  display_bitrates(FILE* const fp );

int  parse_args(lame_t gfc, int argc, char** argv, char * const inPath,
		char * const outPath, char * nogap_inPath[], int *max_nogap);

/* end of parse.h */
