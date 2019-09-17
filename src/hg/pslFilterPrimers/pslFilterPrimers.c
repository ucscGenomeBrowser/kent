/*
  File: pslFilterPrimers.c
  Author: Terry Furey
  Date: 6/29/2004
  Description: Filter primer alignments from isPCR and ePCR
*/

#include "common.h"
#include "linefile.h"
#include "memalloc.h"
#include "hash.h"
#include "jksql.h"
#include "hdb.h"
#include "fuzzyFind.h"
#include "portable.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "psl.h"
#include "fa.h"
#include "psl.h"
#include "options.h"
#include "hgConfig.h"
#include "stsInfo2.h"

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"epcr", OPTION_STRING},
    {"verbose", OPTION_STRING},
    {NULL, 0}
};

struct place
{
  struct place *next;
  struct psl *psl;
  int unali;
  int sizeDiff;
  int badBits;
};

struct epcr
{
  struct epcr *next;
  char* contig;
  char* bases;
  char* dbstsId;
  char* ucscId;
  int start;
  int end;
};

struct sts
{
  struct sts *next;
  char* ucscId;
  char* dbstsId;
  char* leftPrimer;
  char* rightPrimer;
  char* size;
  int minSize;
  int maxSize;
  struct place *place;
  int epcrCount;
  struct epcr *epcr;
  boolean found;
} *stsList;

struct hash *stsHash;

void placeFree(struct place **place)
/* Free a dynamically allocated place */
{
struct place *p;

 if ((p = *place) == NULL) return;
 pslFree(&p->psl); 
 freez(place);
}

void placeFreeList(struct place **pList)
/* Free a dynamically allocated list of place's */
{
struct place *p = NULL, *next = NULL;

for (p = *pList; p != NULL; p = next)
    {
    next = p->next;
    placeFree(&p);
    }
 *pList = NULL;
}

void epcrFree(struct epcr **epcr)
/* Free a dynamically allocated epcr */
{
struct epcr *e;

 if ((e = *epcr) == NULL) return;
 freez(&e->contig);
 freez(&e->bases);
 freez(&e->dbstsId);
 freez(&e->ucscId);
 freez(epcr);
}

void epcrFreeList(struct epcr **eList)
/* Free a dynamically allocated list of epcr's */
{
struct epcr *e = NULL, *next = NULL;

for (e = *eList; e != NULL; e = next)
    {
    next = e->next;
    epcrFree(&e);
    }
 *eList = NULL;
}

void stsFree(struct sts **sts)
/* Free a dynamically allocated sts */
{
struct sts *s;

 if ((s = *sts) == NULL) return;
 freez(&s->dbstsId);
 freez(&s->leftPrimer);
 freez(&s->rightPrimer);
 freez(&s->size);
 placeFreeList(&s->place);
 freez(sts);
}

void stsFreeList(struct sts **sList)
/* Free a dynamically allocated list of sts's */
{
struct sts *s = NULL, *next = NULL;

for (s = *sList; s != NULL; s = next)
    {
    next = s->next;
    stsFree(&s);
    }
*sList = NULL;
}


void readPrimerInfo(struct lineFile *sf)
/* Read in primer info from all.primers file */
{
int wordCount;
char *words[5];
char *dist1, *dist[2];
struct sts *sts;

stsHash = newHash(16);

while (lineFileChopCharNext(sf, '\t', words, 5))
    {
//      verbose(2, "# line %d words1-4: '%s' '%s' '%s' '%s'\n", sf->lineIx, words[1], words[2], words[3], words[4]);
      if (words[1] && words[2] && words[3] && words[4])
	{
	  AllocVar(sts);
	  sts->dbstsId = cloneString(words[0]);
	  sts->leftPrimer = cloneString(words[1]);
	  sts->rightPrimer = cloneString(words[2]);
	  sts->size = cloneString(words[3]);
	  sts->ucscId = cloneString(words[4]);
	  sts->found = FALSE;
	  dist1 = cloneString(words[3]);
	  if (sts->leftPrimer && dist1 && differentWord("-", dist1))
	    {
	      wordCount = chopByChar(dist1, '-', dist, ArraySize(dist));
	      sts->minSize = sqlUnsigned(dist[0]);
	      if (wordCount == 1) 
		sts->maxSize = sqlUnsigned(dist[0]);
	      else
		sts->maxSize = sqlUnsigned(dist[1]);
	      if (sts->maxSize == 0) 
		sts->maxSize = 1000;
	      sts->next = NULL;
	      sts->place = NULL;
	      sts->epcr = NULL;
	      hashAdd(stsHash, sts->dbstsId, sts);
	    }
	  slAddHead(&stsList, sts);
	}
    }
}

boolean epcrInList(struct epcr *eList, struct epcr *epcr)
/* Determine if current epcr record is already in the list */
{
  struct epcr *e;

  for (e = eList; e != NULL; e = e->next)
    if ((sameString(e->contig, epcr->contig)) &&
	(sameString(e->bases, epcr->bases)) &&
	(sameString(e->dbstsId, epcr->dbstsId)) &&
	(sameString(e->ucscId, epcr->ucscId)))
      return(1);

  return(0);
}

void readEpcr(struct lineFile *ef)
/* Read in and record epcr records */
{
int wordCount;
char *words[8];
char *pos[4];
struct epcr *epcr;
struct sts *sts;

while (lineFileChopNext(ef, words, 4))
    {
    verbose(2, "# line %d words0-3: '%s' '%s' '%s' '%s'\n", ef->lineIx, words[0], words[1], words[2], words[3]);
    if (words[3])
      {
	AllocVar(epcr);
	epcr->next = NULL;
	epcr->contig = cloneString(words[0]);
	epcr->bases = cloneString(words[1]);
	epcr->dbstsId = cloneString(words[2]);
	epcr->ucscId = cloneString(words[3]);
	wordCount = chopByChar(words[1], '.', pos, ArraySize(pos));
	if (wordCount != 3) 
	  errAbort("Not parsing epcr as expeceted\n");
	epcr->start = sqlUnsigned(pos[0]);
	epcr->end = sqlUnsigned(pos[2]);
	sts = hashFindVal(stsHash, epcr->dbstsId);
	if (sts && !epcrInList(sts->epcr, epcr))
	  {
	    slAddHead(&sts->epcr, epcr);
	    sts->epcrCount++;
	  } 	  
      }
    }
}

boolean pslInList(struct psl *pList, struct psl *psl)
/* Determine if current psl record is already in the list */
{
  struct psl *p;

  for (p = pList; p != NULL; p = p->next)
    if ((p->match == psl->match) &&
	(p->misMatch == psl->misMatch) &&
	(sameString(p->qName, psl->qName)) &&
	(p->qStart == psl->qStart) &&
	(p->qEnd == psl->qEnd) &&
	(sameString(p->tName, psl->tName)) &&
	(p->tStart == psl->tStart) &&
	(p->tEnd == psl->tEnd))
      return(1);

  return(0);
}

int calcUnali(struct sts *sts, struct psl *psl)
/* Determine number of primer bases not aligned */
{
  int ret = 0;

  ret = strlen(sts->leftPrimer) + strlen(sts->rightPrimer);
  ret = ret - psl->match - psl->misMatch;

  return(ret);
}

int calcSizeDiff(struct sts *sts, struct psl *psl)
/* Determine difference in estimated PCR product size and aligned size */
{
  int ret = 0;

  if (psl->qSize < sts->minSize)
    ret = sts->minSize - psl->qSize;
  if (psl->qSize > sts->maxSize)
    ret = psl->qSize - sts->maxSize;

  return(ret);
}

int calcBadBits(struct place *place)
/* Calculate the score of an alignment */
{
  int ret = (int)(place->psl->misMatch + 0.333 * place->unali);
  /* int size = place->psl->match + place->psl->misMatch + place->unali;

  ret = (int)((1000 * (place->psl->match - place->psl->misMatch - (0.5 * place->unali)))/size); */

  return(ret);
}

void filterPrimersAndWrite(FILE *of, struct sts *sts)
/* Filter placements and write out to file */
{
  int bestScore = 1000;
  struct place *p = NULL;

  for (p = sts->place; p != NULL; p=p->next)
    if (p->badBits < bestScore)
	bestScore = p->badBits;

  for (p = sts->place; p != NULL; p=p->next)
    if ((p->badBits - bestScore) < 2)
      {
	pslTabOut(p->psl, of);
	sts->found = TRUE;
      }
}


void writePrimersNotFound(FILE *nf)
/* Print out primer info for primers not found */
{
  struct sts *s;

  for (s = stsList; s != NULL; s = s->next)
      if (!(s->found))
	fprintf(nf, "%s\t%s\t%s\t%s\t%s\n", 
	       s->dbstsId, s->leftPrimer, s->rightPrimer, s->size, s->ucscId); 

}

void writeEpcrNotFound(FILE *enf)
/* Print out epcr info for primers not found by isPcr */
{
  struct sts *s;
  struct epcr *e;

  for (s = stsList; s != NULL; s = s->next)
      if (!(s->found) && (s->epcrCount > 0) && (s->epcrCount <=5))
	{
	  s->found = TRUE;
	  for (e = s->epcr; e != NULL; e = e->next)
	    fprintf(enf, "%s\t%s\t%s\t%s\n", 
		    e->contig, e->bases, e->dbstsId, e->ucscId); 
	}
}

void processPrimers(struct lineFile *pf, FILE *of)
/* Read and process isPCR file and sts locations */
{
int lineSize, wordCount;
char *line;
char *words[21];
char *dbsts_name, *dbsts[4], *currDbsts;
struct sts *sts=NULL;
struct psl *psl;
struct place *place;

 currDbsts = "\0";
while (lineFileNext(pf, &line, &lineSize))
    {
    wordCount = chopTabs(line, words);
    if (wordCount != 21)
	errAbort("Bad line %d of %s\n", pf->lineIx, pf->fileName);
    psl = pslLoad(words);
    dbsts_name = cloneString(psl->qName);
    wordCount = chopByChar(dbsts_name, '_', dbsts, ArraySize(dbsts));
    if (differentString(dbsts[1], currDbsts))
      {
	if (sts != NULL)
	  {
	    filterPrimersAndWrite(of, sts);
	    /* stsFree(&sts); */
	    freez(&currDbsts);
	  }
	currDbsts = cloneString(dbsts[1]);
	sts = NULL;
	if (hashLookup(stsHash, dbsts[1]))
	  sts = hashMustFindVal(stsHash, dbsts[1]);
      }
    if (sts)
      {
	AllocVar(place);
	/* Check if this psl record is already present */
	if (!pslInList(place->psl, psl))
	  {
	    slAddHead(&place->psl, psl);
	    place->unali = calcUnali(sts, psl);
	    place->sizeDiff = calcSizeDiff(sts, psl);
	    place->badBits = calcBadBits(place);
	    if (place->sizeDiff < (200 - (place->badBits * 50)))
	      slAddHead(&sts->place, place);
	    else
	      placeFree(&place);
	  }
      }
    }
 if (sts != NULL)
   filterPrimersAndWrite(of, sts);
}

int main(int argc, char *argv[])
{
  struct lineFile *pf, *ef, *apf;
  FILE *of, *nf, *enf=NULL;
  char *efName=NULL, filename[256], notFound[256];
  int verb = 0;

verboseSetLevel(0);
optionInit(&argc, argv, optionSpecs);
if (argc < 3)
    {
      verbose(0, "usage: pslFilterPrimers [-epcr=<file> -verbose=<level>] <isPCR psl file> <all.primers> <outfile>\n");
    return 1;
    }
verb = optionInt("verbose", 0);
verboseSetLevel(verb);

 efName = optionVal("epcr", NULL);
 pf = pslFileOpen(argv[1]);
 apf = lineFileOpen(argv[2], TRUE);

 of = mustOpen(argv[3], "w");
 safef(notFound, sizeof(filename), "%s.notfound.primers", argv[3]);
 nf = mustOpen(notFound, "w");

 verbose(1, "Reading all primers file: '%s'\n", argv[2]);
 readPrimerInfo(apf);

 if (efName)
   {
     ef = lineFileOpen(efName, TRUE);
     verbose(1, "Reading epcr file: '%s'\n", efName);
     readEpcr(ef);
   }

 verbose(1, "Reading isPCR file: '%s' processing output to: '%s'\n", argv[1], argv[3]);
 processPrimers(pf, of);

 if (efName)
   {
     safef(filename, sizeof(filename), "epcr.not.found");
     verbose(1, "Writing %s file\n", filename);
     enf = mustOpen(filename, "w");
     writeEpcrNotFound(enf);
   }

 verbose(1, "Writing primers not found to file: '%s'\n", notFound);
 writePrimersNotFound(nf);

 if (efName)
   {
     lineFileClose(&ef);
     fclose(enf);
   }
 lineFileClose(&pf);
 lineFileClose(&apf);
 fclose(of);
 fclose(nf);

 return(0);
}
