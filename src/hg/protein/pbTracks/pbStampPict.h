/* pbStampPic.h define geometri parameter for drawing a Proteom Browser stamp */

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

