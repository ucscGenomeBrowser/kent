/*
  File: pslPairs.c
  Author: Terry Furey
  Date: 10/1/2003
  Description: Create end sequence pairs from psl files
*/

#include "common.h"
#include "linefile.h"
#include "memalloc.h"
#include "hash.h"
#include "jksql.h"
#include "hdb.h"
#include "fuzzyFind.h"
#include "portable.h"
#include "psl.h"
#include "options.h"
#include "binRange.h"

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"verbose", OPTION_BOOLEAN},
    {"slop", OPTION_BOOLEAN},
    {"short", OPTION_BOOLEAN},
    {"long", OPTION_BOOLEAN},
    {"mismatch", OPTION_BOOLEAN},
    {"orphan", OPTION_BOOLEAN},
    {"noRandom", OPTION_BOOLEAN},
    {"noBin", OPTION_BOOLEAN},
    {"min", OPTION_INT},
    {"max", OPTION_INT},
    {"slopval", OPTION_INT},
    {"nearTop", OPTION_FLOAT},
    {"tInsert", OPTION_INT},
    {"hardMax", OPTION_INT},
    {NULL, 0}
};

boolean VERBOSE = FALSE;
boolean SLOP = FALSE;
boolean SHORT = FALSE;
boolean LONG = FALSE;
boolean MISMATCH = FALSE;
boolean ORPHAN = FALSE;
boolean NORANDOM = FALSE;
boolean NOBIN = FALSE;
int MIN;
int MAX;
int SLOPVAL;
float NEARTOP;
int TINSERT;
int HARDMAX;

FILE *of, *orf, *sf, *mf, *esf, *elf;

struct cloneName
{
  char name[32];
};

struct pslAli
{
  struct pslAli *next;
  int score;
  float id;
  float cov;
  struct psl *psl;
};

struct pslPair
{
  struct pslPair *next;
  struct pslAli *f;
  struct pslAli *r;
  int score;
  int distance;
  boolean orien;
  boolean random;
};

struct clone
{
  struct clone *next;
  char name[32];
  char endName1[32];
  char endName2[32];
  struct pslAli *end1;
  struct pslAli *end2;
  struct pslPair *pairs;
  struct pslPair *pairsRandom;
  struct pslPair *pairsSlop;
  struct pslPair *pairsExtremeS;
  struct pslPair *pairsExtremeL;
  struct pslPair *pairsMM;
  boolean orphan;
} *cloneList = NULL;

struct hash *clones = NULL;
struct hash *endNames = NULL;

void pslAliFree(struct pslAli **pa)
{
  struct pslAli *el;

  if ((el = *pa) == NULL) return;
  pslFree(&(el->psl));
  freez(pa);
}

void pslAliListFree(struct pslAli **pList)
{
  struct pslAli *i, *next;

  for (i = *pList; i != NULL; i = next)
    {
      next = i->next;
      pslAliFree(&i);
    }
  *pList = NULL;
}

void pslPairFree(struct pslPair **pp)
{
  freez(pp);
}

void pslPairListFree(struct pslPair **pList)
{
  struct pslPair *i, *next;

  for (i = *pList; i != NULL; i = next)
    {
      next = i->next;
      pslPairFree(&i);
    }
  *pList = NULL;
}

void cloneFree(struct clone **c)
{
  struct clone *el;

  if ((el = *c) == NULL) return;
  pslAliListFree(&(el->end1));
  pslAliListFree(&(el->end2));
  pslPairListFree(&(el->pairs));
  freez(c);
}

struct pslAli *createPslAli(struct psl *psl)
/* Create a pslAli element from a psl record */
{
  struct pslAli *pa;
  
  AllocVar(pa);
  pa->next = NULL;
  pa->psl = psl;
  pa->score = pslScore(psl);
  pa->id = (float)(psl->match + psl->repMatch + psl->nCount)/(psl->match + psl->misMatch + psl->repMatch + psl->nCount);
  pa->cov = (float)(psl->match + psl->misMatch + psl->repMatch + psl->nCount)/psl->qSize;
  return(pa);
}

struct clone *createClone(char *name, char *eName1, char *eName2)
/* Create a clone element from the names */
{
  struct clone *clone;

  AllocVar(clone);
  clone->next = NULL;
  sprintf(clone->name,"%s",name);
  sprintf(clone->endName1, "%s",eName1);
  sprintf(clone->endName2, "%s",eName2);
  clone->end1 = NULL;
  clone->end2 = NULL;
  clone->pairs = NULL;
  clone->orphan = FALSE;

  return(clone);
}
struct pslPair *createPslPair(struct pslAli *e1, struct pslAli *e2)
{
  struct pslPair *pp;

  if (strcmp(e1->psl->tName, e2->psl->tName)) return NULL;
  if ((e1->id < 0.96) && (e2->id < 0.96)) return NULL;
  AllocVar(pp);
  pp->next = NULL;
  if ((e1->psl->tStart) < (e2->psl->tStart))
    {
      pp->f = e1;
      pp->r = e2;
    }
  else
    {
       pp->f = e2;
      pp->r = e1;
    }
  pp->distance = pp->r->psl->tEnd - pp->f->psl->tStart;
  if ((pp->f->psl->strand[0] == '+') && (pp->r->psl->strand[0] == '-'))
    pp->orien = TRUE;
  else 
    pp->orien = FALSE;
  if (strlen(e1->psl->tName) > 6)
    pp->random = TRUE;
  else 
    pp->random = FALSE;
  pp->score = e1->score + e2->score;

  return(pp);
}

void readPairFile(struct lineFile *prf)
/* Read in pairs and initialize clone list */
{
  int lineSize;
  char *line;
  char *words[4];
  int wordCount;
  struct clone *clone;
  struct cloneName *cloneName;
  
  while (lineFileNext(prf, &line, &lineSize))
    {
      wordCount = chopTabs(line,words);
      if (wordCount != 3)
	errAbort("Bad line %d of %s\n", prf->lineIx, prf->fileName);
      if (!hashLookup(clones, words[2])) 
	{
	  char *test;
	  AllocVar(cloneName);
	  clone = createClone(words[2],words[0],words[1]);
	  hashAdd(clones, words[2], clone);
	  sprintf(cloneName->name, "%s", words[2]);
	  hashAdd(endNames, words[0], cloneName);
	  test = hashMustFindVal(endNames, words[0]);
	  if (strcmp(test, words[2]))
	    errAbort("%s ne %s for %s\n",test,words[2],words[0]);
	  hashAdd(endNames, words[1], cloneName);
	  test = hashMustFindVal(endNames, words[1]);
	  if (strcmp(test, words[2]))
	    errAbort("%s ne %s for %s\n",test,words[2],words[1]);
	  slAddHead(&cloneList,clone);
	}
    }     
}

void readPslFile(struct lineFile *pf)
/* Process all records in a psl file of mRNA alignments */
{
 int lineSize;
 char *line;
 char *words[32];
 int  wordCount;
 struct psl *psl;
 struct clone *clone;
 struct pslAli *pa = NULL;
 struct cloneName *cloneName;
 
 while (lineFileNext(pf, &line, &lineSize))
   {
     wordCount = chopTabs(line, words);
     if (wordCount != 21)
       errAbort("Bad line %d of %s\n", pf->lineIx, pf->fileName);
     psl = pslLoad(words);
     if (hashLookup(endNames, psl->qName))
       {
	 cloneName = hashMustFindVal(endNames, psl->qName);
	 clone = hashMustFindVal(clones, cloneName->name);
	 if ((psl->tBaseInsert < TINSERT) && ((!NORANDOM) || (strlen(psl->tName) < 7))) 
	   {
	     pa = createPslAli(psl);
	     if (!strcmp(psl->qName, clone->endName1))
	       slAddHead(&(clone->end1), pa);
	     else
	       slAddHead(&(clone->end2), pa);
	   }
       }
   }
}

void pslPairs()
{
  struct clone *clone;
  struct pslAli *e1, *e2;
  struct pslPair *pp;

  for (clone = cloneList; clone != NULL; clone = clone->next)
    {
      for (e1 = clone->end1; e1 != NULL; e1 = e1->next)
	for (e2 = clone->end2; e2 != NULL; e2 = e2->next)
	  {
	    if ((pp = createPslPair(e1, e2)) != NULL)
	      {		
		if ((pp->distance >= MIN) && (pp->distance <= MAX))
		  {
		    if ((!pp->orien) && (!pp->random) && (MISMATCH)) 
		      slAddHead(&(clone->pairsMM),pp);
		    else if ((pp->orien) && (pp->random) && (!NORANDOM))
		      slAddHead(&(clone->pairsRandom),pp);
		    else if ((pp->orien) && (!pp->random))
		      slAddHead(&(clone->pairs),pp);
		    else 
		      pslPairFree(&pp);
		  }
		else if (!pp->random && pp->orien)
		  {
		    if ((pp->distance >= (MIN-SLOPVAL)) && (pp->distance <= (MAX+SLOPVAL)) && (SLOP))
		      slAddHead(&(clone->pairsSlop),pp);
		    else if ((pp->distance < MIN) && (SHORT))
		      slAddHead(&(clone->pairsExtremeS),pp);
		    else if ((pp->distance > MAX) && (pp->distance <= HARDMAX) && (LONG))
		      slAddHead(&(clone->pairsExtremeL),pp);
		    else 
		      pslPairFree(&pp);
		  }
		else 
		  pslPairFree(&pp);
	      }
	  }
      if (!(clone->pairs) && !(clone->pairsRandom) && !(clone->pairsSlop) 
	  && !(clone->pairsExtremeS) && !(clone->pairsExtremeL) && !(clone->pairsMM))
	{
	  clone->orphan = TRUE;
	}
    }
}

void printBed(FILE *out, struct pslPair *ppList, struct clone *clone)
{
  struct pslPair *pp, *ppPrev;
  int count = 0, best = 0;

  for (pp = ppList; pp != NULL; pp=pp->next)
    if (pp->score > best)
      best = pp->score;
  while (((ppList->score)/best) < NEARTOP)
    {
      struct pslPair *temp = ppList;
      ppList = ppList->next;
      pslPairFree(&temp);
    }
  ppPrev = ppList;
  for (pp = ppList; pp != NULL; pp=pp->next)
    {
      if (((pp->score)/best) >= NEARTOP)
	count++;
      else
	{
	  ppPrev->next = pp->next;
	  pslPairFree(&pp);
	  pp = ppPrev;
	}
      ppPrev = pp;
    }
  for (pp = ppList; pp != NULL; pp=pp->next)
    {
      int bin = binFromRange(pp->f->psl->tStart,pp->r->psl->tEnd);
      int score = 1000;
      char *strand;
      int d1, d2;

      if (count != 1)
	score = 1500/count;
      if (!strcmp(clone->endName1,pp->f->psl->qName))
	strand = "+";
      else 
	strand = "-";
      d1 = pp->f->psl->tEnd - pp->f->psl->tStart + 1;
      d2 = pp->r->psl->tEnd - pp->r->psl->tStart + 1;
      
      if (!NOBIN) 
	fprintf(out, "%d\t",bin);
      fprintf(out, "%s\t%d\t%d\t%s\t%d\t%s\tall_fosends\t2\t%d,%d\t%d,%d\t%s,%s\n",
	      pp->f->psl->tName,pp->f->psl->tStart,pp->r->psl->tEnd,clone->name,
	      score, strand, pp->f->psl->tStart, pp->r->psl->tStart, d1, d2, 
	      pp->f->psl->qName, pp->r->psl->qName);
    }
}

void printOrphan(FILE *out, struct pslAli *paList, struct clone *clone)
{
  struct pslAli *pa;
  int best = 0, count = 0;

  for (pa = paList; pa != NULL; pa=pa->next)
    if (pa->score > best)
      best = pa->score;
  for (pa = paList; pa != NULL; pa=pa->next)
    if ((((pa->score)/best) > NEARTOP) && (pa->id >= 0.98))
      count++;
  for (pa = paList; pa != NULL; pa=pa->next)
    if ((((pa->score)/best) > NEARTOP) && (pa->id >= 0.98))
      {
      int bin = binFromRange(pa->psl->tStart,pa->psl->tEnd);
      int score = 1000;
      char *strand;
      int d1;

      if (count != 1)
	score = 1500/count;
      if (((!strcmp(clone->endName1,pa->psl->qName)) && (pa->psl->strand[0] == '+')) ||
	  ((!strcmp(clone->endName2,pa->psl->qName)) && (pa->psl->strand[0] == '-')))
	strand = "+";
      else 
	strand = "-";
      d1 = pa->psl->tEnd - pa->psl->tStart + 1;

      if (!NOBIN) 
	fprintf(out, "%d\t",bin);
      fprintf(out, "%s\t%d\t%d\t%s\t%d\t%s\tall_fosends\t1\t%d\t%d\t%s\n",
	      pa->psl->tName,pa->psl->tStart,pa->psl->tEnd,clone->name,
	      score, strand, pa->psl->tStart, d1, pa->psl->qName);        
      }
}

void printOut()
{
  struct clone *clone = NULL;

  for (clone = cloneList; clone != NULL; clone = clone->next)
    {
      if (clone->pairs)
	printBed(of, clone->pairs, clone);
      else if (clone->pairsRandom)
	printBed(of, clone->pairsRandom, clone);
      else if ((clone->pairsSlop) && (SLOP))
	printBed(sf, clone->pairsSlop, clone);
      else if ((clone->pairsMM) && (MISMATCH))
	printBed(mf, clone->pairsMM, clone);
      else if ((clone->pairsExtremeS) && (SHORT))
	printBed(esf, clone->pairsExtremeS, clone);
      else if ((clone->pairsExtremeL) && (LONG))
	printBed(elf, clone->pairsExtremeL, clone);
      else if ((clone->orphan) && (ORPHAN))
	{
	printOrphan(orf, clone->end1, clone);
	printOrphan(orf, clone->end2, clone);
	}
    }
}

int main(int argc, char *argv[])
{
  struct lineFile *pf, *prf;
  char filename[64], *db;
  
  optionInit(&argc, argv, optionSpecs);
  if (argc < 2)
    {
      fprintf(stderr, "USAGE: pslPairs <psl file> <pair file> <out file prefix>\n  Options:\n\t-max=N\t\tmaximum length of clone sequence (default=50000)\n\t-min=N\t\tminimum length of clone sequence (default=30000)\n\t-slopval=N\tdeviation from max/min clone lengths allowed for slop report (default=5000)\n\t-nearTop=N\tmaximium deviation from best match allowed (default=0.001)\n\t-tInsert=N\tmaximum insert bases allowed in sequence alignment (default=500)\n\t-hardMax=N\tabsolute maximum clone length for long report (default=75000)\n\t-verbose\tdisplay all informational messages\n\t-slop\t\tcreate file of pairs that fall within slop length\n\t-short\t\tcreate file of pairs shorter than min size\n\t-long\t\tcreate file of pairs longer than max size, but less than hardMax size\n\t-mismatch\tcreate file of pairs with bad orientation of ends\n\t-orphan\t\tcreate file of unmatched end sequences\n");
      return 1;
    }
  VERBOSE = optionExists("verbose");
  SLOP = optionExists("slop");
  SHORT = optionExists("short");
  LONG = optionExists("long");
  MISMATCH = optionExists("mismatch");
  ORPHAN = optionExists("orphan");
  NORANDOM = optionExists("noRandom");
  NOBIN = optionExists("noBin");
  MIN = optionInt("min",32000);
  MAX = optionInt("max",47000);
  TINSERT = optionInt("tInsert",500);
  HARDMAX = optionInt("hardMax",75000);
  if (SLOP)
      SLOPVAL = optionInt("slopval",5000);
  else
      SLOPVAL = 0;
  NEARTOP = optionFloat("nearTop",0.001);
  NEARTOP = 1 - NEARTOP;

  pf = pslFileOpen(argv[1]);
  prf = lineFileOpen(argv[2],TRUE);
  
  sprintf(filename, "%s.pairs", argv[3]);
  of = mustOpen(filename, "w");
  if (SLOP)
    {
      sprintf(filename, "%s.slop", argv[3]);
      sf = mustOpen(filename, "w");
    }
  if (ORPHAN)
    {
      sprintf(filename, "%s.orphan", argv[3]);
      orf = mustOpen(filename, "w");
    }
  if (MISMATCH)
    {
      sprintf(filename, "%s.mismatch", argv[3]);
      mf = mustOpen(filename, "w");
    }
  if (SHORT)
    {
      sprintf(filename, "%s.short", argv[3]);
      esf = mustOpen(filename, "w");
    }
  if (LONG)
    {
      sprintf(filename, "%s.long", argv[3]);
      elf = mustOpen(filename, "w");
    }
      
  if (VERBOSE)
    printf("Reading pair file\n");
  clones = newHash(23);
  endNames = newHash(23);
  readPairFile(prf);
  if (VERBOSE)
    printf("Reading psl file\n");
  readPslFile(pf);
  if (VERBOSE)
    printf("Creating Pairs\n");
  pslPairs();
  if (VERBOSE) 
    printf("Writing to files\n");
  printOut();
  
  lineFileClose(&pf);
  lineFileClose(&prf);
  fclose(of);
  if (ORPHAN)
    fclose(orf);
  if (SLOP)
    fclose(sf);
  if (MISMATCH)
    fclose(mf);
  if (SHORT)
    fclose(esf);
  if (LONG)
    fclose(elf);
 
  return 0;
}


