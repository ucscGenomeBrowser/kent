#ifndef SIM_DNA_H
#define SIM_DNA_H
/* $Id: dna.h,v 1.1 2002/09/24 18:04:18 kent Exp $ */

void *ckrealloc(void *, size_t);

typedef struct gap_scores {
	int E;
	int O;
} gap_scores_t;

#define CLEN(s) (sizeof((s))-1)
#define NACHARS 128

typedef int ss_t[NACHARS][NACHARS];

void DNA_scores(ss_t ss);
int is_dchar(int ch);
int scores_from_file(const char *fname, ss_t ss);
#endif
