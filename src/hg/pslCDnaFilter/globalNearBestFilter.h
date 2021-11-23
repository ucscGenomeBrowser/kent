/* Copyright (C) 2011 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef GLOBALNEARBESTFILTER_H
#define GLOBALNEARBESTFILTER_H
extern struct cDnaQuery *cdna;

void globalNearBestFilter(struct cDnaQuery *cdna, float globalNearBest);
/* global near best in genome filter. */
#endif
