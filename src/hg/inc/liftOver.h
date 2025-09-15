/* lift genome annotations between assemblies using chain files */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef LIFTOVER_H
#define LIFTOVER_H

#include "chain.h"
#include "genePred.h"

#define LIFTOVER_MINMATCH        0.95
#define LIFTOVER_MINBLOCKS       1.00

#define liftOverChainTableConfVariable      "liftOverChainName"
#define defaultLiftOverChainTableName       "liftOverChain"

enum liftOverFileType
{
    none = 0,
    bed = 1,
    positions = 2,
};

struct liftOverChain *liftOverChainList();
/* Get list of all liftOver chains in the central database */

void filterOutMissingChains(struct liftOverChain **pChainList);
/* Filter out chains that don't exist.  Helps partially mirrored sites. */

struct liftOverChain *liftOverChainListFiltered();
/* Get list of all liftOver chains in the central database
 * filtered to include only those chains whose liftover files exist.
 * This helps partially mirrored sites */

struct liftOverChain *liftOverChainForDb(char *fromDb);
/* Return list of liftOverChains for this database. */

struct liftOverChain *liftOverChainListForDbFiltered(char *fromDb);
/* Get list of all liftOver chains in the central database for fromDb,
 * filtered to include only those chains whose liftover files exist. */

char *liftOverChainFile(char *fromDb, char *toDb);
/* Get filename of liftOver chain */

enum liftOverFileType liftOverSniff(char *fileName);
/* the file-sniffing bit used to distinguish bed from positions files */
/* returns enum concerning the file type */

int liftOverBed(char *fileName, struct hash *chainHash, 
                        double minMatch,  double minBlocks, 
                        int minSizeT, int minSizeQ,
                        int minChainT, int minChainQ,
                        bool fudgeThick, FILE *f, FILE *unmapped, 
                        bool multiple, bool noSerial, char *chainTable, int *errCt,
                        bool preserveInput);
/* Open up file, decide what type of bed it is, and lift it. 
 * Return the number of records successfully converted */

int liftOverBedPlus(char *fileName, struct hash *chainHash, double minMatch,  
                    double minBlocks, int minSizeT, int minSizeQ, int minChainT,
                    int minChainQ, bool fudgeThick, FILE *f, FILE *unmapped, 
                    bool multiple, bool noSerial, char *chainTable, int bedPlus, bool hasBin,
                    bool tabSep, int *errCt, bool preserveInput);
/* Lift bed with N+ (where n=bedPlus param) format.
 * Return the number of records successfully converted */

int liftOverBedPlusEnds(char *fileName, struct hash *chainHash, double minMatch,  
                    double minBlocks, int minSizeT, int minSizeQ, int minChainT,
                    int minChainQ, bool fudgeThick, FILE *f, FILE *unmapped, 
                    bool multiple, bool noSerial, char *chainTable, int bedPlus, bool hasBin,
			bool tabSep, int ends, int *errCt, bool preserveInput);
/* Lift bed N+ file.
 * Return the number of records successfully converted */

int liftOverPositions(char *fileName, struct hash *chainHash, 
                      double minMatch,  double minBlocks, 
                      int minSizeT, int minSizeQ,
                      int minChainT, int minChainQ,
                      bool fudgeThick, FILE *f, FILE *unmapped, 
		      bool multiple, char *chainTable, int *errCt);
/* Create bed file from positions (chrom:start-end) and lift.
 * Return the number of records successfully converted */

int liftOverBedOrPositions(char *fileName, struct hash *chainHash, 
                        double minMatch,  double minBlocks, 
                        int minSizeT, int minSizeQ,
                        int minChainT, int minChainQ,
		        bool fudgeThick, FILE *mapped, FILE *unmapped, 
		        bool multiple, bool noSerial, char *chainTable, int *errCt);
/* Sniff the first line of the file, and determine whether it's a */
/* bed, a positions file, or neither. */

char *liftOverRemapRange(struct hash *chainHash, double minRatio,
			char *chrom, int s, int e, char strand, double minMatch,
			 char **retChrom, int *retStart, int *retEnd, char *retStrand);
/* Remap a range through chain hash.  If all is well return NULL
 * and results in retChrom, retStart, retEnd.  Otherwise
 * return a string describing the problem. */

void liftOverGff(char *fileName, struct hash *chainHash, 
                    double minMatch, double minBlocks, 
                    FILE *mapped, FILE *unmapped, bool preserveInput);
/* Lift over GFF file */

void liftOverPsl(char *fileName, struct hash *chainHash, 
                            double minMatch, double minBlocks, bool fudgeThick,
                                FILE *f, FILE *unmapped);
/* Open up PSL file, and lift it. */

void liftOverGenePred(char *fileName, struct hash *chainHash, 
                        double minMatch, double minBlocks, bool fudgeThick,
                      FILE *mapped, FILE *unmapped, boolean multiple, bool preserveInput);
/* Lift over file in genePred format. */

void calcLiftOverGenePreds( struct genePred *gpList, struct hash *chainHash, 
                        double minMatch, double minBlocks, bool fudgeThick,
                      FILE *mapped, FILE *unmapped, boolean multiple, bool preserveInput);
/* worker function for liftOverGenePred. */

void liftOverSample(char *fileName, struct hash *chainHash, 
                        double minMatch, double minBlocks, bool fudgeThick,
                        FILE *mapped, FILE *unmapped, bool preserveInput);
/* Open up sample file,  and lift it */

void readLiftOverMap(char *fileName, struct hash *chainHash);
/* Read map file into hashes. */

char *liftOverErrHelp();
/* Help message explaining liftOver failures */

char *remapBlockedBed(struct hash *chainHash, struct bed *bed,  
                             double minMatch, double minBlocks, bool fudgeThick,
                             bool multiple, char *db, char *chainTable);
/* Remap blocks in bed, and also chromStart/chromEnd.  If multiple, then bed->next may be
 * changed to point to additional newly allocated mapped beds, and bed's pointer members may
 * be free'd so be sure to pass in a properly allocated bed.
 * Return NULL on success, an error string on failure. */

void liftOverAddChainHash(struct hash *chainHash, struct chain *chain);
/* Add this chain to the hash of chains used by remapBlockedBed */

char *liftOverChainTable();
/* Return the name of the liftOverChain table. */
#endif


