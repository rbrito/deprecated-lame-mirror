void print_config(lame_global_flags* gfp);
void usage(lame_global_flags *, const char *);
void help(lame_global_flags *, const char *);
void short_help(lame_global_flags *, const char *);
void parse_args(lame_global_flags* gfp, int argc, char** argv, char *, char *);
void display_bitrates(FILE *out_fh);
