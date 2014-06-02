/* chromBins - object for storing per-chrom binKeeper objects */

/* Copyright (C) 2005 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef CHROMBINS_H
#define CHROMBINS_H
struct slName;


/* prototype of function to free entries (optional). Should
 * take a pointer to a pointer to an entry */
typedef void (chromBinsFreeFunc)();

struct chromBins
/* per-chromosome binRange objects */
{
    chromBinsFreeFunc *freeFunc;  /* entry free function if not null */ 
    struct hash* chromTbl;        /* map chrom-> binRange */
};

struct chromBins *chromBinsNew(chromBinsFreeFunc *freeFunc);
/* create a new chromBins object */

void chromBinsFree(struct chromBins **chromBinsPtr);
/* free chromBins object, calling freeFunc on each entry if it was specified */

struct slName *chromBinsGetChroms(struct chromBins *chromBins);
/* get list of chromosome names in the object.  Result should
 * be freed with slFreeList() */

struct binKeeper *chromBinsGet(struct chromBins* chromBins, char *chrom,
                               boolean create);
/* get chromosome binKeeper, optionally creating if it doesn't exist */

struct binElement *chromBinsFind(struct chromBins *chromBins, char *chrom,
                                 int start, int end);
/* get list of overlaping objects in a by chrom binRange hash */

void chromBinsAdd(struct chromBins *chromBins, char *chrom, int start, int end,
                  void *obj);
/* add an object to a by-chrom binKeeper hash */

#endif
