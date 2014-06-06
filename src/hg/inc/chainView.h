/* Copyright (C) 2002 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef CHAINVIEW_H
#define CHAINVIEW_H

struct chainView
/* A chain of blocks.  Used for output of chainBlocks. */
    {
    struct chainView *next;	  	  /* Next in list. */
    struct boxIn *blockList;      /* List of blocks. */
    double score;	  	  /* Total score for chain. */
    char *tName;		  /* target name, allocated here. */
    int tSize;			  /* Overall size of target. */
    /* tStrand always + */
    int tStart,tEnd;		  /* Range covered in query. */
    char *qName;		  /* query name, allocated here. */
    int qSize;			  /* Overall size of query. */
    char qStrand;		  /* Query strand. */
    int qStart,qEnd;		  /* Range covered in query. */
    int id;			  /* ID of chain in file. */
    unsigned gtStart;	/* Alignment start position in target */
    unsigned gtEnd;	/* Alignment end position in target */
    unsigned gqStart;	/* start in query */
    unsigned chainId;	/* chain id in chain table */
    };

struct chainView *chainViewLoad(char **row);
/* Load a chain from row fetched with select * from chain
 * from database.  Dispose of this with chainFree(). */

void chainViewFree(struct chainView **pEl);
/* Free a single dynamically allocated chain such as created
 * with chainViewLoad(). */

void chainViewFreeList(struct chainView **pList);
/* Free a list of dynamically allocated chain's */

#endif /* CHAINVIEW_H */

