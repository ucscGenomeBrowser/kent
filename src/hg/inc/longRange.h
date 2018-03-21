/* longRange - Long Range Interaction Format functions */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef LONGRANGE_H
#define LONGRANGE_H

#include "common.h"
#include "cheapcgi.h"
#include "cart.h"
#include "trackDb.h"
#include "bed.h"

#define LONG_HEIGHT "heightPer"
#define LONG_MINHEIGHT 20
#define LONG_MAXHEIGHT 300
#define LONG_DEFHEIGHT "200"
#define LONG_MINSCORE "minScore"
#define LONG_DEFMINSCORE "0"

struct longRange
{
struct longRange *next;
char *name;     // for longTabix, Wash U field 4 (other region pos + interaction value)
                //      Unused in GB

// info for 'start' (lower genomic coords) region
unsigned s;     // position of 'center', 0-based
unsigned sw;    // width in bp
int sOrient;    // strand
char *sChrom;   // chrom

// info for 'end' (higher genomic coords) region
unsigned e;
unsigned ew;
int eOrient;
char *eChrom;

boolean hasColor; // Ensembl extension to Wash U format -- 
                 //        RGB color can be supplied instead of score
unsigned rgb;   // RGB color for display
double score;   // interaction numeric value; e.g. strength of interaction

unsigned id;    // unique integer identifier of an item in a longTabix file (not interaction id)
                // Required in Wash U spec. Unsed at UCSC, except for details page
};

void longRangeCfgUi(struct cart *cart, struct trackDb *tdb, char *name, char *title, boolean boxed);
/* Complete track controls for long range interaction. */

struct longRange *parseLongTabix(struct bed *beds, unsigned *maxWidth, double minScore);
/* Parse longTabix format into longRange structures */

struct asObject *longTabixAsObj();
/* Return asObject describing fields of longTabix file. */

int longRangeCmp(const void *va, const void *vb);
/* Compare based on coord position of s field */

#endif//def LONGRANGE_H
