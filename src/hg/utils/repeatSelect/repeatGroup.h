/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef REPEATGROUP_H
#define REPEATGROUP_H

#ifndef COMMON_H
#include "common.h"
#endif

struct repeatGroup 
/* Stores a bunch of different calculations for a set of repeats. */
/* The sets of repeats are repeats sharing the same name. */
    {
    struct repeatGroup *next;
    char *name;
    int copies;
    int bases;
    double meanPctId;
    double meanScore;
    double meanLength;
    };

void repeatGroupFree(struct repeatGroup **pRG);
/* Free up a repeatGroup item. */

void repeatGroupFreeList(struct repeatGroup **pList);
/* Free up a list of repeatGroups */

int repeatGroupCopyCmp(const void *va, const void *vb);
/* Compare by number of copies. */

int repeatGroupBasesCmp(const void *va, const void *vb);
/* Compare by number of bases covered for all copies of repeats. */

int repeatGroupLenCmp(const void *va, const void *vb);
/* Compare by mean length of a repeat. */

int repeatGroupScoreCmp(const void *va, const void *vb);
/* Compare by mean score of a repeat. */

#endif
