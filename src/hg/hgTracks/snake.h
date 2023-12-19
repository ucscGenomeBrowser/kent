/* snake.h - data structures and routines for drawing snakes */

/* Copyright (C) 2023 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef SNAKE_H

struct snakeInfo
{
int maxLevel;
} ;

struct snakeFeature
    {
    struct snakeFeature *next;
    int start, end;			/* Start/end in browser coordinates. */
    int qStart, qEnd;			/* query start/end */
    int level;				/* level in snake */
    int orientation;			/* strand... -1 is '-', 1 is '+' */
    boolean drawn;			/* did we draw this feature? */
    char *qSequence;			/* may have sequence, or NULL */
    char *tSequence;			/* may have sequence, or NULL */
    char *qName;			/* chrom name on other species */
    unsigned pixX1, pixX2;              /* pixel coordinates within window */
    double score;                       /* score from the alignment */
    };

void maybeLoadSnake(struct track *track);
#endif /* SNAKE_H */
