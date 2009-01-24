/* bigWig - interface to binary file with wiggle-style values (that is a bunch of
 * small non-overlapping regions each associated with a floating point value. */

#ifndef BIGWIG_H
#define BIGWIG_H

enum bigWigSectType 
/* Code to indicate section type. */
    {
    bigWigTypeBedGraph=1,
    bigWigTypeVariableStep=2,
    bigWigTypeFixedStep=3,
    };

#endif /* BIGWIG_H */

