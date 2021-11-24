/* pbStampPic.h define geometri parameter for drawing a Proteom Browser stamp */

/* Copyright (C) 2004 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef PBSTAMPPICT_H
#define PBSTAMPPICT_H

struct pbStampPict
/* Geometric Info needed for drawing a Proteom Browser Stamp */
    {
    int xOrig; 	/* x origin */
    int yOrig;  /* y origin */
    int width;  /* width */
    int height; /* width */
    struct pbStamp *stampDataPtr;
    };

#endif  /* PBSTAMPPICT_H */

