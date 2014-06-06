/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef GLOBALNEARBESTFILTER_H
#define GLOBALNEARBESTFILTER_H
struct cDnaQuery *cdna;

void globalNearBestFilter(struct cDnaQuery *cdna, float globalNearBest);
/* global near best in genome filter. */
#endif
