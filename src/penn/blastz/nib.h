#ifndef NIB_H
#define NIB_H
#include "inttypes.h"
#include "util.h"

unsigned char *seq_freadnib(FILE *, int32_t , int32_t , int32_t *);
unsigned char *seq_readnib(const char *, int32_t , int32_t , int32_t *);
void seq_fwritenib(FILE *, unsigned const char *, uint32_t );
void seq_writenib(char *, unsigned const char *, uint32_t );
#endif
