/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* HGAP - Human Genome Annotation Project database. */
#ifndef HGAP_H
#define HGAP_H

#ifndef DNASEQ_H
#include "dnaseq.h"
#endif 

#ifndef UNFIN_H
#include "unfin.h"
#endif

#ifndef JKSQL_H
#include "jksql.h"
#endif

typedef unsigned int HGID;	/* A database ID. */

void hgSetDb(char *dbName);
/* Set the database name. */

char *hgGetDb();
/* Return the current database name. */

struct sqlConnection *hgAllocConn();
/* Get free connection if possible. If not allocate a new one. */

struct sqlConnection *hgFreeConn(struct sqlConnection **pConn);
/* Put back connection for reuse. */


HGID hgIdQuery(struct sqlConnection *conn, char *query);
/* Return first field of first table as HGID. 0 return ok. */

HGID hgRealIdQuery(struct sqlConnection *conn, char *query);
/* Return first field of first table as HGID- abort if 0. */


struct sqlConnection *hgStartUpdate();
/* Get a connection for an update.  (Starts allocating id's) */

void hgEndUpdate(struct sqlConnection **pConn, char *comment, ...);
/* Finish up connection with a printf format comment. */

HGID hgNextId();
/* Get next unique id.  (Should only be called after hgStartUpdate). */

FILE *hgCreateTabFile(char *tableName);
/* Open a tab file with name corresponding to tableName.  This
 * may just be fclosed when done. (Currently just makes
 * tableName.tab in the current directory.) */

void hgLoadTabFile(struct sqlConnection *conn, char *tableName);
/* Load tab delimited file corresponding to tableName. 
 * Should only be used after hgCreatTabFile, and only after
 * file closed. */


enum 
/* Various constants used. */
    {
    hgContigPad = 800,	  /* Number of N's between contigs. */
    };

struct hgBac
/* This represents a sequenced clone (BAC/PAC/cosmid) */
    {
    struct hgBac *next;	      /* Next in list. */
    struct hgNest *nest;      /* Coordinate space. */
    HGID id;		      /* HGAP ID. */
    char name[16];            /* GenBank accession. */
    int contigCount;          /* Number of contigs. */
    struct hgContig *contigs; /* Contig list. */
    };

struct hgContig
/* This represents a contig within a BAC. */
    {
    struct hgContig *next;    /* Next in list. */
    struct hgNest *nest;      /* Coordinate space. */
    HGID id;		      /* HGAP ID. */
    char name[20];            /* Name like AC000007.24 */
    struct hgBac *bac;        /* Bac this is in. */
    int ix;                   /* Contig index. */
    int submitOffset;         /* Position in genBank submission. */
    int size;                 /* Size in bases. */
    };

struct hgNest
/* This structure describes the contig tree
 * chromosomes->chromosome contigs->bacs->
 * bac contigs.  */
    {
    struct hgNest *next;	/* Pointer to next sibling. */
    struct hgNest *children;	/* Children. */
    struct hgNest *parent;      /* Parent if any. */
    HGID id;		        /* HGAP ID. */
    int orientation;            /* +1 or -1 relative to parent. */
    int offset;			/* Offset relative to parent. */
    int size;                   /* Size in bases. */
    struct hgContig *contig;    /* Associated contig if any. */
    };

struct hgBac *hgGetBac(char *acc);
/* Load BAC with given accession into memory. Don't free this, it's
 * managed by system. */

struct hgContig *hgGetContig(char *acc, int contigIx);
/* Get contig.  contigIx is position in submission, not position in
 * ordering. */

struct dnaSeq *hgContigSeq(struct hgContig *contig);
/* Return DNA associated with contig. */

struct dnaSeq *hgRnaSeq(char *acc);
/* Return sequence for RNA. */

void hgRnaSeqAndId(char *acc, struct dnaSeq **retSeq, HGID *retId);
/* Return sequence for RNA and it's database ID. */

struct dnaSeq *hgBacOrderedSeq(struct hgBac *bac);
/* Return DNA associated with BAC including NNN's between
 * contigs in ordered coordinates. */

struct dnaSeq *hgBacSubmittedSeq(char *acc);
/* Returns DNA associated with BAC in submitted ordering
 * and coordinates. */

struct dnaSeq *hgBacContigSeq(char *acc);
/* Returns list of sequences, one for each contig in BAC. */

int hgOffset(struct hgNest *source, int offset, struct hgNest *dest);
/* Translate offset from source to destination coordinate space.
 * Destination has to be an ancestor (or the same) as source. */

/* The following is a series of nested structures for
 * describing a range of DNA. The later structures include
 * the first fields of the earlier ones.  Routines that
 * work on the earlier structures will also work on
 * the later.  This is a crude but effective form of single 
 * inheritance. */

struct hgRange
/* Just start/end locations somewhere... */
    {
    struct hgRange *next; /* Next in list. */
    int tStart, tEnd;	  /* Position in target or only sequence tStart <= x < tEnd */
    };

int hgCmpTStart(const void *va, const void *vb);
/* Compare function to sort by tStart, then by tEnd. */

struct hgHit
/* A simple hit - an interesting range of a sequence. */
    {
    struct hgHit *next;   /* Next in list. */
    int tStart, tEnd;	  /* Position in target or only sequence tStart <= x < tEnd */
    int tOrientation;	  /* +1 or -1 orientation. */
    char *target;	  /* Name of target seq. (Not allocated here.) */
    };

int hgCmpTarget(const void *va, const void *vb);
/* Compare function to sort by target, orientaation, tStart, then tEnd. */

struct hgScoredHit
/* A hit with a log odds score. */
    {
    struct hgScoredHit *next;
    int tStart, tEnd;	  /* Position in target or only sequence tStart <= x < tEnd */
    int tOrientation;	  /* +1 or -1 orientation. */
    char *target;	  /* Name of target seq. (Not allocated here.) */
    int logOdds;          /* Log odds style score - scaled x 1000. */
    };

int hgCmpScore(const void *va, const void *vb);
/* Compare function to sort logOdds score. */

struct hgAliHit
/* A hit representing an alignment between two sequences without inserts. */
    {
    struct hgAliHit *next;
    int tStart, tEnd;	  /* Position in target or only sequence tStart <= x < tEnd */
    int tOrientation;	  /* +1 or -1 orientation. */
    char *target;	  /* Name of target seq. (Not allocated here.) */
    int logOdds;          /* Log odds style score - scaled x 1000. */
    int qStart, qEnd;     /* Position in query sequence. */
    int qOrientation;	  /* +1 or -1 query orientation. */
    char *query;	  /* Name of query seq. (Not allocated here.) */
    };

int hgCmpQStart(const void *va, const void *vb);
/* Compare function to sort by qStart, then by qEnd. */

int hgCmpQuery(const void *va, const void *vb);
/* Compare function to sort by query, orientation, qStart, then qEnd. */

struct hgBoundedHit
/* An alignment hit that can have soft or hard edges. */
    {
    struct hgBoundedHit *next;
    int tStart, tEnd;	  /* Position in target or only sequence tStart <= x < tEnd */
    int tOrientation;	  /* +1 or -1 orientation. */
    char *target;	  /* Name of target seq. (Not allocated here.) */
    int logOdds;          /* Log odds style score - scaled x 1000. */
    int qStart, qEnd;     /* Position in query sequence. */
    int qOrientation;	  /* +1 or -1 query orientation. */
    char *query;	  /* Name of query seq. (Not allocated here.) */
    bool hardStart;       /* Start position known */
    bool hardEnd;         /* End position known */
    };

struct hgAlignment
/* An alignment with gaps. */
    {
    struct hgAlignment *next;
    int tStart, tEnd;	  /* Position in target or only sequence tStart <= x < tEnd */
    int tOrientation;	  /* +1 or -1 orientation. */
    char *target;	  /* Name of target seq. (Not allocated here.) */
    int logOdds;          /* Log odds style score - scaled x 1000. */
    int qStart, qEnd;     /* Position in query sequence. */
    int qOrientation;     /* +1 or -1 orientation. */
    bool hardStart;       /* Start position known */
    bool hardEnd;         /* End position known */
    struct hgBoundedHit *hitList;  /*  Subalignments. */
    };

#endif /* HGAP_H */

