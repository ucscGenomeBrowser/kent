/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* WormDNA - stuff that finds C. elegans sequence data. */
#ifndef WORMDNA_H
#define WORMDNA_H

#ifndef DNAUTIL_H
#include "dnautil.h"
#endif

#ifndef DNASEQ_H
#include "dnaseq.h"
#endif 

#ifndef NT4_H
#include "nt4.h"
#endif 

#ifndef GDF_H
#include "gdf.h"
#endif


struct wormCdnaInfo 
/* Extra info stored in cDNA database other than string. */
    {
    char *motherString;         /* Holds memory for other strings. */
    char *name;                 /* Name of cDNA. */
    char *gene;                 /* Something like unc-1 */
    char orientation;           /* + or - (relative to gene transcription direction) */
    char *product;              /* Something like "cyclin-dependent kinase" */
    int cdsStart, cdsEnd;       /* Start and stop of coding region within cDNA. */
    boolean knowStart, knowEnd; /* Start known?  End known? */
    boolean isEmbryonic;        /* True if derived from embryo culture. */
    boolean isAdult;            /* True if derived from adult culture. */
    boolean isMale;             /* True if males only. */
    boolean isHermaphrodite;    /* True if hermaphrodites only. */
    char *description;          /* One line description. */
    };

boolean wormCdnaInfo(char *name, struct wormCdnaInfo *retInfo);
/* Get info about cDNA sequence. */

void wormFaCommentIntoInfo(char *faComment, struct wormCdnaInfo *retInfo);
/* Process line from .fa file containing information about cDNA into binary
 * structure. */

void wormFreeCdnaInfo(struct wormCdnaInfo *ci);
/* Free the mother string in the cdnaInfo.  (The info structure itself normally isn't
 * dynamically allocated. */

boolean wormInfoForGene(char *orfName, struct wormCdnaInfo *retInfo);
/* Return info if any on gene/ORF, or NULL if none exists. wormFreeCdnaInfo retInfo when done.
 */


boolean wormCdnaSeq(char *name, struct dnaSeq **retDna, struct wormCdnaInfo *retInfo);
/* Get a single worm cDNA sequence. Optionally (if retInfo is non-null) get additional
 * info about the sequence. */

void wormCdnaUncache();
/* Get rid of any resources used caching or quickly accessing
 * worm cDNA */


/* Stuff for searching entire database of worm cDNA */

struct wormCdnaIterator
    {
    FILE *faFile;
    };

boolean wormSearchAllCdna(struct wormCdnaIterator **retSi);
/* Set up to search entire database or worm cDNA */

void freeWormCdnaIterator(struct wormCdnaIterator **pIt);
/* Free up iterator returned by wormSearchAllCdna() */

struct dnaSeq *nextWormCdna(struct wormCdnaIterator *it);
/* Return next sequence in database */

boolean nextWormCdnaAndInfo(struct wormCdnaIterator *it, struct dnaSeq **retSeq, struct wormCdnaInfo *retInfo);
/* Return next sequence and associated info from database. */

char *wormFeaturesDir();
/* Return the features directory. (Includes trailing slash.) */

char *wormChromDir();
/* Return the directory with the chromosomes. */

char *wormCdnaDir();
/* Return directory with cDNA data. */

char *wormSangerDir();
/* Return directory with Sanger specific gene predictions. */

char *wormGenieDir();
/* Return directory with Genie specific gene predictions. */

char *wormXenoDir();
/* Return directory with cross-species alignments. */

boolean wormIsGeneName(char *name);
/* See if it looks like a worm gene name - that is
 *   abc-12
 * letters followed by a dash followed by a number. */

boolean wormIsOrfName(char *in);
/* Check to see if the input is formatted correctly to be
 * an ORF. */

struct slName *wormGeneToOrfNames(char *name);
/* Returns list of cosmid.N type ORF names that are known by abc-12 type name. */

char *wormGeneFirstOrfName(char *geneName);
/* Return first ORF synonym to gene. */

boolean wormGeneForOrf(char *orfName, char *geneNameBuf, int bufSize);
/* Look for gene type (unc-12 or something) synonym for cosmid.N name. */

boolean getWormGeneExonDna(char *name, DNA **retDna);
/* get the exon sequence for a gene */

boolean getWormGeneDna(char *name, DNA **retDna, boolean upcExons);
/* Get the DNA associated with a gene.  Optionally upper case exons. */

void wormLoadNt4Genome(struct nt4Seq ***retNt4Seq, int *retNt4Count);
/* Load up entire packed worm genome into memory. */

void wormFreeNt4Genome(struct nt4Seq ***pNt4Seq);
/* Free up packed worm genome. */

int wormChromSize(char *chrom);
/* Return size of worm chromosome. */

DNA *wormChromPart(char *chromId, int start, int size);
/* Return part of a worm chromosome. */

DNA *wormChromPartExonsUpper(char *chromId, int start, int size);
/* Return part of a worm chromosome with exons in upper case. */

void wormChromNames(char ***retNames, int *retNameCount);
/* Get list of worm chromosome names. */

int wormChromIx(char *name);
/* Return index of worm chromosome. */

char *wormChromForIx(int ix);
/* Given ix, return worm chromosome official name. */

char *wormOfficialChromName(char *name);
/* This returns a pointer to our official string for the chromosome name.
 * (This allows some routines to do directo pointer comparisons rather
 * than string comparisons.) */

boolean wormGeneRange(char *name, char **retChromId, char *retStrand, int *retStart, int *retEnd);
/* Return chromosome position of a gene, ORF,  nameless cluster, or cosmid. Returns
 * FALSE if no such gene/cluster. */

boolean wormParseChromRange(char *in, char **retChromId, int *retStart, int *retEnd);
/* Chop up a string representation of a range within a chromosome and put the
 * pieces into the return variables. Return FALSE if it isn't formatted right. */

boolean wormIsChromRange(char *in);
/* Check to see if the input is formatted correctly to be
 * a range of a chromosome. */

void wormClipRangeToChrom(char *chrom, int *pStart, int *pEnd);
/* Make sure that we stay inside chromosome. */

boolean wormIsNamelessCluster(char *name);
/* Returns true if name is of correct format to be a nameless cluster. */

DNA *wormGetNamelessClusterDna(char *name);
/* Get DNA associated with nameless cluster */

struct wormFeature
/* This holds info on where something is in the genome. */
    {
    struct wormFeature *next;
    char *chrom;    /* One of names returned by */
    int start, end;
    char typeByte;
    char name[1];   /* Allocated to fit. */
    };

struct wormFeature *newWormFeature(char *name, char *chrom, int start, int end, char typeByte);
/* Allocate a new feature. */

struct wormFeature *wormCdnasInRange(char *chromId, int start, int end);
/* Get info on all cDNAs that overlap the range. */

struct wormFeature *wormGenesInRange(char *chromId, int start, int end);
/* Get info on all genes that overlap the range. */

struct wormFeature *wormSomeGenesInRange(char *chromId, int start, int end, char *gdfDir);
/* Get info on genes that overlap range in a particular set of gene predictions. */

struct wormFeature *wormCosmidsInRange(char *chromId, int start, int end);
/* Get info on all cosmids that overlap the range. */

struct cdaAli *wormCdaAlisInRange(char *chromId, int start, int end);
/* Return list of cdna alignments that overlap range. */

FILE *wormOpenGoodAli();
/* Opens good alignment file and reads signature. 
 * (You can then cdaLoadOne() it.) */

struct wormGdfCache
/* Helps managed fast indexed access to gene predictions. */
    {
    char **pDir;
    struct snof *snof;
    FILE *file;
    };
extern struct wormGdfCache wormSangerGdfCache;
extern struct wormGdfCache wormGenieGdfCache;

struct gdfGene *wormGetGdfGene(char *name);
/* Get the named gdfGene. */

struct gdfGene *wormGetSomeGdfGene(char *name, struct wormGdfCache *cache);
/* Get a single gdfGene of given name. */

struct gdfGene *wormGetGdfGeneList(char *baseName, int baseNameSize);
/* Get all gdfGenes that start with a given name. */

struct gdfGene *wormGetSomeGdfGeneList(char *baseName, int baseNameSize, struct wormGdfCache *cache);
/* Get all gdfGenes that start with a given name. */

struct gdfGene *wormGdfGenesInRange(char *chrom, int start, int end, 
    struct wormGdfCache *geneFinder);
/* Get list of genes in range according to given gene finder. */

void wormUncacheGdf();
/* Free up resources associated with fast GDF access. */

void wormUncacheSomeGdf(struct wormGdfCache *cache);
/* Uncache some gene prediction set. */

#endif /* WORMDNA_H */

