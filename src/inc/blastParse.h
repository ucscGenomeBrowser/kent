/* blastParse - read in blast output into C data structure. */

#ifndef BLASTPARSE_H
#define BLASTPARSE_H

struct blastFile
/* All the info in a single file. */
    {
    struct blastFile *next;
    char *fileName;			/* Name of file this is in. */
    char *program;                      /* Blastp, blastx, blastn, etc. */
    char *version;                      /* Version of program. */
    char *buildDate;                    /* Build date of program. */
    struct lineFile *lf;                /* File blast is in. */
    struct blastQuery *queries;         /* List of queries. */
    };

struct blastQuery
/* Info on one query. */
    {
    struct blastQuery *next;
    char *query;                        /* Name of query sequence. */
    int queryBaseCount;                 /* Number of bases in query. */
    char *database;                     /* Name of database. */
    int dbSeqCount;                     /* Number of sequences in database. */
    int dbBaseCount;                    /* Number of bases in database. */
    struct blastGappedAli *gapped;      /* List of gapped alignments. */
    };

struct blastGappedAli
/* Info about a gapped alignment. */
    {
    struct blastGappedAli *next;
    struct blastQuery *query;           /* Query associated with this alignment (not owned here). */
    char *targetName;                   /* Name of target sequence. */
    int targetSize;                     /* Size of target sequence. */
    struct blastBlock *blocks;          /* List of aligning blocks (no big gaps). */
    };

struct blastBlock
/* Info about a single block of gapped alignment. */
    {
    struct blastBlock *next;
    int bitScore;                       /* About 2 bits per aligning nucleotide. */
    double eVal;                        /* Expected number of alignments in database. */
    int matchCount;                     /* Number of matching nucleotides. */
    int totalCount;                     /* Total number of nucleotides. */
    int insertCount;                    /* Number of inserts. */
    BYTE qStrand;                       /* Query strand (+1 or -1) */
    BYTE tStrand;                       /* Target strand (+1 or -1) */
    int qStart;                         /* Query start position. */
    int tStart;                         /* Target start position. */
    int qEnd;                           /* Query end position. */
    int tEnd;                           /* Target end position. */
    char *qSym;                         /* Query letters (including '-') */
    char *tSym;                         /* Target letters (including '-') */
    };

struct blastFile *blastFileReadAll(char *fileName);
/* Read all blast alignment in file. */

struct blastFile *blastFileOpenVerify(char *fileName);
/* Open file, read and verify header. */

struct blastQuery *blastFileNextQuery(struct blastFile *bf);
/* Read all alignments associated with next query.  Return NULL at EOF. */

struct blastGappedAli *blastFileNextGapped(struct blastFile *bf, struct blastQuery *bq);
/* Read in next gapped alignment.   Does *not* put it on bf->gapped list. 
 * Return NULL at EOF or end of query. */

struct blastBlock *blastFileNextBlock(struct blastFile *bf, 
	struct blastQuery *bq, struct blastGappedAli *bga);
/* Read in next blast block.  Return NULL at EOF or end of
 * gapped alignment. */

void blastFileFree(struct blastFile **pBf);
/* Free blast file. */

void blastFileFreeList(struct blastFile **pList);
/* Free list of blast files. */

void blastQueryFree(struct blastQuery **pBq);
/* Free single blastQuery. */

void blastQueryFreeList(struct blastQuery **pList);
/* Free list of blastQuery's. */

void blastGappedAliFree(struct blastGappedAli **pBga);
/* Free blastGappedAli. */

void blastGappedAliFreeList(struct blastGappedAli **pList);
/* Free blastGappedAli list. */

void blastBlockFree(struct blastBlock **pBb);
/* Free a single blastBlock. */

void blastBlockFreeList(struct blastBlock **pList);
/* Free a list of blastBlocks. */

#endif /* BLASTPARSE_H */

