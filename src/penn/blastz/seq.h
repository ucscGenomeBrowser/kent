/* $Id: seq.h,v 1.1 2002/09/25 05:44:08 kent Exp $ */

#ifndef PIPLIB_SEQ
#define PIPLIB_SEQ

#include "util.h"
#include "nib.h"

typedef unsigned char uchar;

#define ZFREE(p) /*CONSTCOND*/do{ckfree(p);(p)=0;}while(0)

extern const signed char fasta_encoding[256];
extern const signed char nfasta_encoding[256];

enum /* powerset */
{ SEQ_IS_SUBRANGE = (1<<0) /* seq is a subrange of a file */
, SEQ_IS_REVCOMP  = (1<<1) /* seq is reverse compliment of a file */
, SEQ_IS_SUBSEQ   = (1<<2) /* seq is a reference to another seq */
, SEQ_HAS_MASK    = (1<<3) /* seq has a mask applied */
, SEQ_HAS_PIPE    = (1<<4) /* input fd is a pipe */
, SEQ_DO_REVCOMP  = (1<<5) /* make it so after open */
, SEQ_DO_SUBRANGE = (1<<6) /* make it so after open */
, SEQ_DO_MASK     = (1<<7) /* make it so after open */
, SEQ_ALLOW_AMB   = (1<<8) /* checked while reading */
, SEQ_DISALLOW_AMB  = (1<<9) /* checked while reading */
};

enum 
{ SEQ_TYPE_GUESS = 0
, SEQ_TYPE_FASTA
, SEQ_TYPE_NIB
};

typedef struct seq_file {
	uchar *seq;
	int slen; /* bytes in seq, not including '\0' */
	int from; /* 1 based */

	FILE *fp;
	int type; // guess, fasta, nib, ...
	int flags;
	int count;  /* how many contigs we have already read */
	long offset; /* the starting offset of the contig we just read */

	char *maskname;
	char *fname;
	char *header;
	int hlen; /* bytes in header, not including '\0' */
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
SEQ* seq_open_type(const char *fname, int type);
SEQ* seq_close(SEQ *s);
int seq_read(SEQ *seq);
int seq_read_nib(SEQ *seq);
int seq_read_fasta(SEQ *seq);
SEQ *seq_copy(const SEQ *s);
SEQ *seq_revcomp_inplace(SEQ *seq);
SEQ *seq_get(const char *fname);	/* seq_open + seq_read */
void do_revcomp(uchar *s, int len);
uchar dna_cmpl(unsigned char);

#endif
