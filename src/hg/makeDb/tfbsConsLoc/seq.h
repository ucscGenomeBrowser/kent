/* $Id: seq.h,v 1.1 2004/03/18 22:35:45 braney Exp $ */

/* Copyright (C) 2004 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef PIPLIB_SEQ
#define PIPLIB_SEQ

#include "util.h"

typedef unsigned char uchar;

#define ZFREE(p) /*CONSTCOND*/do{ckfree(p);(p)=0;}while(0)

extern const signed char fasta_encoding[256];
extern const signed char nfasta_encoding[256];

enum {SEQ_IS_REVCOMP  = (1<<1)}; /* seq is reverse complement of a file */

typedef struct seq_file {
	FILE *fp;
	int flags;
	int count;  /* how many contigs we have already read */
	long offset; /* the starting offset of the contig we just read */

	char *maskname;

	char *fname;
	int from; /* 1 based */

	char *header;
	int hlen; /* bytes in header, not including '\0' */

	uchar *seq;
	int slen; /* bytes in seq, not including '\0' */
} SEQ;

#define SEQ_NAME(s) ((s)->fname)
#define SEQ_LEN(s) ((s)->slen)
#define SEQ_TO(s)  ((s)->slen + (s)->from - 1)
#define SEQ_FROM(s) ((s)->from)
#define SEQ_AT(s,i) ((s)->seq[i])
#define SEQ_HEAD(s) ((s)->header)
#define SEQ_HLEN(s) ((s)->hlen)
#define SEQ_CHARS(s) ((s)->seq)
#define SEQ_SAME(a,b) ((a)==(b))

SEQ* seq_open(const char *fname);
SEQ* seq_close(SEQ *s);
int seq_read(SEQ *seq);
SEQ *seq_copy(const SEQ *s);
SEQ *seq_revcomp_inplace(SEQ *seq);
SEQ *seq_get(const char *fname);	/* seq_open + seq_read */
void do_revcomp(uchar *s, int len);
uchar dna_cmpl(unsigned char);

#endif
