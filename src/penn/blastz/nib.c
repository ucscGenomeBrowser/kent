#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include "nib.h"

// XXX -- BUG: after a seek, we are not synchronized.
// skip to end?

static const char rcsid[] =
"$Id: nib.c,v 1.1 2002/09/25 05:44:07 kent Exp $";

// 
// First 4 bytes of the nib file are the signature: 0x6BE93D3A. 
// This can be in either byte order.  The next 4 bytes are the size
// of the sequence (in bases); in the same byte order as the signature.
// 
// See nibOpenVerify() in kent/src/lib/nib.c for details.
// 
// The rest of the files has the data, two bases per byte, with the encoding:
//     T = 0
//     C = 1
//     A = 2
//     G = 3
//     N = 4
// 
// as of about 10 minutes ago, if the high bit of the nibble is set, it
// indicates a repeat-masked base.
// 
// thats about it...
// 
// SES -- note, most significant nibble first.
//

static const uint32_t NIB_SIG = 0x6BE93D3A;
static const uint32_t NIB_GIS = 0x3A3DE96B;
enum { NIB_MSK = 1U<<3 };

// -- library --
#if 0
static void fatalfr(char *s)
{
    perror(s);
    exit(1);
}

static void fatal(char *s)
{
    fprintf(stderr, "%s\n", s);
    exit(1);
}

static void *ckalloc(unsigned int n)
{
    int i = 3;
    do {
	void *p = malloc(n);
	if (p != 0) return p;
	sleep(10);
    } while (i-- > 0);
    fatalfr("malloc");
}

static FILE *ckopen(const char *fname, const char *mode)
{
	FILE *fp = fopen(fname, mode);
	if (fp == 0) fatalfr("fopen");
	return fp;
}
#endif

static int ckgetc(FILE *fp)
{
    int c = getc(fp);
    if (c == -1) fatalfr("getc");
    return c;
}

static void ckfputc(int n, FILE *fp)
{
    if (fputc(n, fp) == -1)
	fatalfr("fputc");
}

// -- util --

static int c2i(int b)
{
	switch (b & 0xFF) {
	case 'T': return 0;
	case 'C': return 1;
	case 'A': return 2;
	case 'G': return 3;
	case 'N': return 4;
	case 'X': return 4;
	case 't': return 0 | NIB_MSK;
	case 'c': return 1 | NIB_MSK;
	case 'a': return 2 | NIB_MSK;
	case 'g': return 3 | NIB_MSK;
	case 'n': return 4 | NIB_MSK;
	case 'x': return 4 | NIB_MSK;
	}
	return 0xF;
}

static const unsigned char i2c[] = "TCAGNXXXtcagnxxx";

static void bpskip(FILE *fp, int n)
{
	if (n < 0) fatal("skip < 0");

	if (n == 0) return;
	n /= 2;
	if (fseek(fp, n, SEEK_CUR) == -1) {
		if (errno != ESPIPE)
			fatalfr("fseek");
		else
			while (n--) ckgetc(fp);
	}
}

static uint32_t getlen(FILE *fp, uint32_t sig)
{
	uint32_t a, b, c, d;

	a = getc(fp);
	b = getc(fp);
	c = getc(fp);
	d = getc(fp);
	if (sig == NIB_SIG)
		return (a | b<<8 | c<<16 | d<<24);
	else if (sig == NIB_GIS)
		return (a<<24 | b<<16 | c<<8 | d);
	else
		fatal("seq_open_nib: not a nib file");
	return ~0;
}

static uint32_t getsig(FILE *fp)
{
	uint32_t i = getc(fp);
        i |= getc(fp)<<8;
        i |= getc(fp)<<16;
        i |= getc(fp)<<24;
	return i;
}

// -- entry points --

unsigned char *seq_freadnib(FILE *fp, int32_t rbase, int32_t rlen, int32_t *slen)
{
	uint32_t sig, len, i;
	unsigned char *s;

	if (feof(fp) || ferror(fp)) return 0;

	sig = getsig(fp);
	if (feof(fp)) return 0;
	len = getlen(fp, sig);
	if (len == ~0U) return 0;

	if (rbase < 0) fatal("rbase<0");
	if (rlen < 0) fatal("rlen<0");
	//if (len < 0) fatal("len<0");
	if ((uint32_t)rbase > len) fatal("rbase>len");
	// if (rbase+rlen > len) fatal("rbase+rlen>len");

	bpskip(fp, rbase);
	len -= rbase;
	if ((uint32_t)rlen < len) len = rlen;

	s = ckalloc(len+2); // '\0' plus possible extra nibble
	i = 0;
	if (rbase&1) s[i++] = i2c[ckgetc(fp)&0xF];
	while (i<len) {
		int c = ckgetc(fp);
		s[i++] = i2c[(c>>4)&0xF];
		s[i++] = i2c[(c>>0)&0xF];
	}
	s[i] = 0;
	if (i==len+1) s[i-1] = 0; // clean up if we went too far

	fseek(fp, 0, SEEK_END); // XXX 
	if (slen) *slen = len;
	return s;
}

unsigned char *seq_readnib(const char *fname, int32_t rbase, int32_t rlen, int32_t *slen)
{
	FILE *fp = ckopen(fname, "rb");
	unsigned char *s = seq_freadnib(fp, rbase, rlen, slen);
	fclose(fp);
	return s;
}

static void write50(FILE *fp, const unsigned char *s, int len)
{
	int i;
        for (i=0; i<len; i+=50) {
                int n = len-i;
                if (n>50) n = 50;
                if (fwrite(s+i, n, 1, fp) != 1) fatalfr("fwrite");
                putchar('\n');
        }
}

void seq_fwritenib(FILE *fp, unsigned const char *s, uint32_t len)
{
	uint32_t i;

	i = NIB_SIG;
	ckfputc(i & 0xFF, fp); i >>= 8;
	ckfputc(i & 0xFF, fp); i >>= 8;
 	ckfputc(i & 0xFF, fp); i >>= 8;
	ckfputc(i & 0xFF, fp); i >>= 8;
	i = len;
	ckfputc(i & 0xFF, fp); i >>= 8;
	ckfputc(i & 0xFF, fp); i >>= 8;
 	ckfputc(i & 0xFF, fp); i >>= 8;
	ckfputc(i & 0xFF, fp); i >>= 8;

	for (i=0; i<len; ) {
		int        n  = c2i(s[i++]) << 4;
		if (i<len) n |= c2i(s[i++]) << 0;
		ckfputc(n, fp);
	}
}

void seq_writenib(char *fname, unsigned const char *s, uint32_t len)
{
	FILE *fp = ckopen(fname, "wb");
	seq_fwritenib(fp, s, len);
	fclose(fp);
}
