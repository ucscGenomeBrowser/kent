
/* lolly.h - stuff unique to lollipop charts */

/* Copyright (C) 2019 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef LOLLY_H
#define LOLLY_H


/*	lollyCartOptions structure */
struct lollyCartOptions
    {
    int origHeight, height;
    double minY, maxY;
    enum wiggleScaleOptEnum autoScale;
    int radius;    // percentage of height to use for lollipop circle
    int typeWordCount;
    char **typeWords;
    boolean noStems;
    };
#endif 
