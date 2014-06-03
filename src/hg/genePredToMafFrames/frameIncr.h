/* frameIncr - frame increment and manipulation.  Static functions for 
 * implict inlining */

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef FRAMEINCR_H
#define FRAMEINCR_H
/* Increment a frame by the specified amount, which maybe negative. frame
 * of -1 always returns -1. */
static inline int frameIncr(int frame, int amt) {
    if (frame < 0) {
        return frame;  /* no frame not changed */
    } else if (amt >= 0) {
        return (frame + amt) % 3;
    } else {
        int amt3 = ((-amt)%3);
        return (frame - (amt-amt3)) % 3;
    }
}
#endif
