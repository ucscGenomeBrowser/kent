/* data types construct in main program and passed to other modules */

/* Copyright (C) 2007 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#ifndef GBGETSEQS_H
#define GBGETSEQS_H
struct seqIdSelect
/* Record in hash table of a sequence id that was specified  */
{
    char acc[GB_ACC_BUFSZ];
    short version;
    int selectCount;
};

#endif
