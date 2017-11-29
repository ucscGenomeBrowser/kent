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
char *name;
unsigned s;
unsigned sw;
int sOrient;
char *sChrom;
unsigned e;
unsigned ew;
int eOrient;
char *eChrom;
unsigned height;
double score;
unsigned id;
};

void longRangeCfgUi(struct cart *cart, struct trackDb *tdb, char *name, char *title, boolean boxed);
/* Complete track controls for long range interaction. */

struct longRange *parseLongTabix(struct bed *beds, unsigned *maxWidth, double minScore);
/* Parse longTabix format into longRange structures */

struct asObject *longTabixAsObj();
/* Return asObject describing fields of longTabix file. */

#endif//def LONGRANGE_H
