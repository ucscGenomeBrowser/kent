/*
  File: fishClones.c
  Author: Terry Furey
  Date: 6/3/2003
  Description: Create Fish Clones track from FISH info file and clone 
               placement information
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
#include "hgConfig.h"
#include "clonePos.h"
#include "stsAlias.h"
#include "stsMap.h"
#include "lfs.h"

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
  {"fhcrc", OPTION_STRING},
  {"psl", OPTION_STRING},
  {"verbose", OPTION_STRING},
  {"noRandom", OPTION_BOOLEAN},
  {"noBin", OPTION_BOOLEAN},
  {"endInfo", OPTION_BOOLEAN},
  {NULL, 0}
};

int verb = 0;
boolean NORANDOM = FALSE;
boolean NOBIN = FALSE;
boolean ENDINFO = FALSE;

struct position
{
  struct position *next;
  char *name;
  char *chrom;
  int start;
  int end;
  char orien;
  char *type;
  int phase;
};

struct name
{
  struct name *next;
  char *name;
  int phase;
};

struct place
{
  struct place *next;
  char *chrom;
  int start;
  int end;
  char *type;
  int numAcc;
  struct name *acc;
  int numSts;
  struct name *sts;
  int numBacEndPair;
  struct name *bePair;
  int numBacEnd;
  struct name *bacEnd;
  int score;
  int startBE;
  int endBE;
};

struct map
{
  struct map *next;
  char *chrom;
  char *band1;
  char *band2;
  char *center;
};

struct fishClone
{
  struct fishClone *next;
  char *cloneName;
  int numGood;
  struct place *good;
  struct position *acc;
  struct position *sts;
  struct position *endPair;
  struct position *end;
  int numCyto;
  struct map *cyto;
  struct name *missingAcc;
} *fcList = NULL;

struct hash *fishClones = NULL;
struct hash *positions = NULL;
struct hash *posClone = NULL;

void mapFree(struct map **map)
/* Free a dynamically allocated map */
{
  struct map *m;
  if ((m = *map) == NULL) return;
  free(m->chrom);
  free(m->band1);
  free(m->band2);
  free(m->center);
  freez(map);
}

void mapFreeList(struct map **mapList)
/* Free a dynamically allocated map list */
{
  struct map *m, *next;
  
  for (m = *mapList; m != NULL; m = next)
    {
      next = m->next;
      mapFree(&m);
    }
  *mapList = NULL;
}

void positionFree(struct position **pos)
/* Free a dynamically allocated position */
{
  struct position *p;

  if ((p = *pos) == NULL) return;
  free(p->name);
  free(p->chrom);
  freez(pos);
}

void positionFreeList(struct position **posList)
/* Free a dynamically allocated position list */
{
  struct position *p, *next;
  
  for (p = *posList; p != NULL; p = next)
    {
      next = p->next;
      positionFree(&p);
    }
  *posList = NULL;
}

void nameFree(struct name **name)
/* Free a dynamically allocated name */
{
  struct name *n;

  if ((n = *name) == NULL) return;
  free(n->name);
  freez(name);
}

void nameFreeList(struct name **nameList)
/* Free a dynamically allocated name list */
{
  struct name *n, *next;
  
  for (n = *nameList; n != NULL; n = next)
    {
      next = n->next;
      nameFree(&n);
    }
  *nameList = NULL;
}

void placeFree(struct place **place)
/* Free a dynamically allocated place */
{
  struct place *p;

  if ((p = *place) == NULL) return;
  free(p->chrom);
  free(p->type);
  nameFreeList(&p->acc);
  nameFreeList(&p->bePair);
  nameFreeList(&p->sts);
  nameFreeList(&p->bacEnd);
  freez(place);
}

void placeFreeList(struct place **placeList)
/* Free a dynamically allocated place list */
{
  struct place *p, *next;
  
  for (p = *placeList; p != NULL; p = next)
    {
      next = p->next;
      placeFree(&p);
    }
  *placeList = NULL;
}

void fishCloneFree(struct fishClone **fc)
/* Free a dynamically allocated fishClone */
{
  struct fishClone *f;
  if ((f = *fc) == NULL) return;
  mapFreeList(&(f->cyto));
  positionFreeList(&(f->acc));
  positionFreeList(&(f->sts));
  positionFreeList(&(f->endPair));
  positionFreeList(&(f->end));
  nameFreeList(&(f->missingAcc));
  free(f->cloneName);
  freez(fc);
}

struct fishClone *createFishClone(char *name)
/* Create a fishClone structure */
{
  struct fishClone *ret;

  AllocVar(ret);
  ret->next = NULL;
  ret->cloneName = cloneString(name);
  touppers(ret->cloneName);
  ret->numGood = 0;
  ret->good = NULL;
  ret->acc = NULL;
  ret->sts = NULL;
  ret->endPair = NULL;
  ret->end = NULL;
  ret->numCyto = 0;
  ret->cyto = NULL;
  ret->missingAcc = NULL;

  return(ret);
}

struct map *createMap(char *mapInfo, char *center, char *chr)
/* Create a map record */
{
  struct map *ret;
  char *bands[16], band[16];
  int wordCount;
  char chrom[16];
  char *mi, *words[10];

  AllocVar(ret);
  ret->next = NULL;

  /* Determine chromosome */
  mi = cloneString(mapInfo);
  if (stringIn("p", mapInfo))
    wordCount = chopByChar(mi, 'p', words, ArraySize(words));      
  else if (stringIn("q", mapInfo))
    wordCount = chopByChar(mi, 'q', words, ArraySize(words));      
  else if (stringIn("cen", mapInfo))
    wordCount = chopByChar(mi, 'c', words, ArraySize(words));
  else
    errAbort("Couldn't determine chrom for %s\n", mapInfo);
  safef(chrom, sizeof(chrom), "chr%s", words[0]);
  verbose(4, "\tchromosome for %s is %s\n", mapInfo, chrom);
  ret->chrom = cloneString(chrom);
  free(mi);

  if (stringIn("~\0", mapInfo))
    {
      wordCount = chopByChar(mapInfo, '~', bands, ArraySize(bands));
      if ((wordCount == 2) && stringIn("p\0", bands[0]))
	{
	  safef(band, sizeof(band), "%sp%s", chr, bands[1]); 
	  bands[1] = cloneString(band);
	}
      else if ((wordCount == 2) && stringIn("q\0", bands[0]))
	{
	  safef(band, sizeof(band), "%sq%s", chr, bands[1]); 
	  bands[1] = cloneString(band);
	}
    }
  else
    wordCount = chopByChar(mapInfo, '-', bands, ArraySize(bands));
  ret->band1 = cloneString(bands[0]);
  if (wordCount == 1 || *bands[1] == 0) 
    /* check for empty end band safeguards against bad input data (e.g. "Yp0")*/
    ret->band2 = cloneString(bands[0]);
  else 
    ret->band2 = cloneString(bands[1]);
  ret->center = cloneString(center);

  return(ret);
}

struct position *createPosition(char *name, char *type, char orien)
/* Create a position record */
{
  struct position *ret;
  
  AllocVar(ret);
  ret->next = NULL;
  ret->name = cloneString(name);
  touppers(ret->name);
  ret->chrom = NULL;
  ret->start = 0;
  ret->end = 0;
  ret->orien = orien;
  ret->type = type;
  ret->phase = 0;

  return(ret);
}

struct name *createName(char *n)
/* create a name record */
{
  struct name *ret;

  AllocVar(ret);
  ret->next = NULL;
  ret->name = cloneString(n);
  touppers(ret->name);
  ret->phase = 0;
  return(ret);
}

struct place *createPlace(struct position *pos)
/* Create a place record */
{
  struct place *ret;
  char *names, *words[2];
  struct name *n;
  int wordCount;

  AllocVar(ret);
  ret->next = NULL;
  ret->chrom = cloneString(pos->chrom);
  ret->start = pos->start;
  ret->end = pos->end;
  ret->type = cloneString(pos->type);
  ret->numAcc = 0;
  ret->acc = NULL;
  ret->numSts = 0;
  ret->sts = NULL;
  ret->numBacEndPair = 0;
  ret->bePair = NULL;
  ret->numBacEnd = 0;
  ret->bacEnd = NULL;
  ret->startBE = -1;
  ret->endBE = -1;
  if (sameString(ret->type, "Accession"))
    {
      ret->numAcc++;
      ret->acc = createName(pos->name);
      ret->acc->phase = pos->phase;
    } 
  if (sameString(ret->type, "BAC End Pair"))
    {
      names = cloneString(pos->name);
      wordCount = chopString(names, ",",words,2);
      ret->bePair = createName(words[0]);
      n = createName(words[1]);
      slAddHead(&ret->bePair, n);
      ret->numBacEndPair++;
      ret->startBE = 0;
      ret->endBE = 0;
    } 
  if (sameString(ret->type, "STS Marker"))
    {
      ret->numSts++;
      ret->sts = createName(pos->name);
    } 
  if (sameString(ret->type, "BAC End"))
    {
      if (pos->orien == '+') 
	ret->startBE = 0;
      else if (pos->orien == '-') 
	ret->endBE = 0;
      else 
	errAbort("BAC end without orientation, %s\n", pos->name);
      ret->numBacEnd++;
      ret->bacEnd = createName(pos->name);
    } 
  ret->score = 0;

  return(ret);
}

boolean sameName(struct name *n1, struct name *n2)
/* Determine if two names the same */
{
  if ((n1 != NULL) && (n2 != NULL) && (sameString(n1->name, n2->name)))
    return(TRUE);
  
  return(FALSE);
}

boolean inNameList(struct name *n, struct name *nList)
/* Determine if n is in nList */
{
  struct name *n1;

  for (n1 = nList; n1 != NULL; n1 = n1->next)
    if (sameName(n, n1))
      return(TRUE);
  
  return(FALSE);
}

boolean samePos(struct position *p1, struct position *p2)
/* Determine of p1 and p2 are the same */
{
  if ((sameString(p1->name, p2->name))
      && (p1->start == p2->start) && (p1->end == p2->end) 
      && ((p1->chrom == NULL && p2->chrom == NULL) || 
	  ((p1->chrom != NULL) && (p2->chrom != NULL) && sameString(p1->chrom, p2->chrom))))
    return(TRUE);

  return(FALSE);
}

boolean inPosList(struct position *p, struct position *pList)
/* Determine if p is in pList */
{
  struct position *p1;

  for (p1 = pList; p1 != NULL; p1 = p1->next)
    if (samePos(p, p1))
      return(TRUE);
  
  return(FALSE);
}

void readFishInfo(struct lineFile *ff)
/* Read in FISH information from NCBI */
{
  int wordCount, wordCount1, i, j;
  char *words[7], *chr, *clone, *map, *acc, *sts, *bacEnd, name[32];
  char *accs[16], *stss[16], *bacEnds[16], *stsName[16], *maps[16], *mapInfo[16], *mapInfoCen[16];
  char *p, *center;
  struct fishClone *fc;
  struct position *pos;
  struct map *m;
  int lineCount = 0;

  fishClones = newHash(16);
  positions = newHash(16);
  posClone = newHash(16);

  /* Read in line and create fishClone record */
  verbose(1, "\treading fishInfo file %s\n", ff->fileName);
  while (lineFileChopTab(ff, words))
    {
      ++lineCount;
      chr = cloneString(words[0]);
      clone = cloneString(words[1]);

      /* Create and add to hash */
      verbose(4,"\tadding %s to hash for %s, line %d\n", clone, chr, lineCount);
      fc = createFishClone(clone);
      slAddHead(&fcList, fc);
      hashAdd(fishClones, clone, fc);

      /* Record cytogenetic map positions */
      map = cloneString(words[3]);
      verbose(4, "\tprocessing map info %s\n", map);
      wordCount = chopByChar(map, ';', maps, ArraySize(maps));
      for (i = 0; i < wordCount; i++)
	{
	  p = strrchr(maps[i], ')');
	  *p = '\0';
	  wordCount1 = chopByChar(maps[i], '(', mapInfoCen, ArraySize(mapInfo)); 
	  center = cloneString(mapInfoCen[1]);
	  wordCount1 = chopByChar(mapInfoCen[0], ',', mapInfo, ArraySize(mapInfo));
	  for (j = 0; j < wordCount1; j++)
	    {
	      m =  createMap(mapInfo[j], center, chr);
	      slAddHead(&fc->cyto, m);
	      fc->numCyto++;
	    }
	}

      /* Record accessions assigned to clone */
      acc = cloneString(words[4]);
      verbose(4, "\tprocessing acc info %s\n", acc);
      wordCount = chopByChar(acc, ';', accs, ArraySize(accs));
      for (i = 0; i < wordCount; i++)
	{
	  /* Remove version */
	  p = strrchr(accs[i], '.');
	  if (p != NULL)
	    *p = '\0';
	  verbose(4, "\tprocessing acc info for %s\n", accs[i]);
	  pos = createPosition(accs[i], "Accession", ' ');
	  if (!hashLookup(positions, accs[i]))
	      hashAdd(positions, accs[i], pos);
	  if (!inPosList(pos, fc->acc))
	    {
	      slAddHead(&fc->acc, pos);
	      hashAdd(posClone, accs[i], fc);
	    }
	}

      /* Record sts markers assigned to clone */
      sts = cloneString(words[5]);
      verbose(4, "\tprocessing sts info %s\n", sts);
      wordCount = chopByChar(sts, ';', stss, ArraySize(stss));
      for (i = 0; i < wordCount; i++)
	{
	  /* Split off UniSTS id */
	  wordCount1 = chopByWhite(stss[i], stsName, ArraySize(stsName));
	  verbose(4, "\tprocessing sts info for %s\n", stsName[0]);
	  safef(name, sizeof(name), "%s_%s", clone, stsName[0]);
	  pos = createPosition(stsName[0], "STS Marker", ' ');
	  if (!hashLookup(positions, name))
	      hashAdd(positions, name, pos);
	  if (!inPosList(pos, fc->sts))
	    {
	      slAddHead(&fc->sts, pos);
	      hashAdd(posClone, name, fc);
	    }
	}

      /* Record bac ends assigned to clone */
      bacEnd = cloneString(words[6]);
      verbose(4, "\tprocessing bacend info %s\n", bacEnd);
      wordCount = chopByChar(bacEnd, ';', bacEnds, ArraySize(bacEnds));
      for (i = 0; i < wordCount; i++)
	{
	  /* Split off version */
	  p = strrchr(bacEnds[i], '.');
	  *p = '\0';
	  verbose(4, "\tprocessing bacEnd info for %s\n", bacEnds[i]);
	  pos = createPosition(bacEnds[i], "BAC End", ' ');
	  if (!hashLookup(positions, bacEnds[i]))
	      hashAdd(positions, bacEnds[i], pos);
	  if (!inPosList(pos, fc->end))
	    {
	      slAddHead(&fc->end, pos);
	      hashAdd(posClone, bacEnds[i], fc);
	    }
	}
    }
}

void readCloneAcc(struct lineFile *cf)
/* Read in clone/acc information from NCBI (Clone Registry) */
{
  char *words[4], *clone, *acc, *p;
  struct position *pos;
  struct fishClone *fc;

  /* Read in clone/acc pairings */
  lineFileChopNext(cf, words, 3);
  lineFileChopNext(cf, words, 3);
  while (lineFileChopNext(cf, words, 3))
    {
      clone = cloneString(words[0]);
      acc = cloneString(words[1]);
      p = strrchr(acc, '.');
      if (p != NULL) 
	*p = '\0';
      /* Check if clone a fish clone, and that accession not previously recorded */
      if (hashLookup(fishClones, clone)) 
	{
	  fc = hashMustFindVal(fishClones, clone);
	  pos = createPosition(acc, "Accession", ' ');
	  if (!hashLookup(positions, acc))
	      hashAdd(positions, acc, pos);
	  if (!inPosList(pos, fc->acc))
	    {
	      hashAdd(posClone, acc, fc);
	      slAddHead(&fc->acc, pos);	  
	    }
	}
      free(clone);
    }
}

void readBacEnds(struct lineFile *bef)
/* Read in clone/bac ends information from NCBI */
{
  char *words[8], *clone, *be;
  struct position *pos;
  struct fishClone *fc;

  /* Read in clone/bac ends pairings */
  while (lineFileChopNext(bef, words, 6))
    {
      clone = cloneString(words[0]);
      be = cloneString(words[1]);
      /* Check if clone a fish clone, and that accession not previously recorded */
      if (hashLookup(fishClones, clone))
	{
	  fc = hashMustFindVal(fishClones, clone);
	  pos = createPosition(be, "BAC End", ' ');
	  if (!hashLookup(positions, be))
	      hashAdd(positions, be, pos);
	  if (!inPosList(pos, fc->end))
	    {
	      hashAdd(posClone, be, fc);
	      slAddHead(&fc->end, pos);
	    }
	}
      free(clone);
    }
}

void readStsMarkers(struct lineFile *sf)
/* Read in STS Marker information from FHCRC */
{
  char *words[4], *clone, *sts, name[32];
  struct position *pos;
  struct fishClone *fc;

  /* Read in clone/STS pairings */
  while (lineFileChopNext(sf, words, 2))
    {
      clone = cloneString(words[0]);
      sts = cloneString(words[1]);
      safef(name, sizeof(name), "%s_%s", clone, sts);
      /* Check if clone a fish clone, and that accession not previously recorded */
      if (hashLookup(fishClones, clone)) 
	{
	  fc = hashMustFindVal(fishClones, clone);
	  pos = createPosition(sts, "STS Marker", ' ');
	  /* Make sure it isn't an accession or end sequence */
	  if (!hashLookup(positions, sts))
	    {
	      if (!hashLookup(positions, name))
		hashAdd(positions, name, pos);
	      verbose(4, "\tchecking sts marker %s for %s\n", sts, clone);
	      if (!inPosList(pos, fc->sts)) 
		{
		  verbose(4, "\tadding sts marker %s for %s\n", sts, clone);
		  hashAdd(posClone, name, fc);
		  slAddHead(&fc->sts, pos);
		}
	    }
	}
    }
}

void readBacEndsPsl(struct lineFile *bpf)
/* Read in bac ends pslFile */
{
  int lineSize, wordCount;
  char *line, *words[32];
  struct psl *psl;
  struct position *pos;
  struct fishClone *fc;

  while (lineFileNext(bpf, &line, &lineSize))
    {
    wordCount = chopTabs(line, words);
    if (wordCount != 21)
	errAbort("Bad line %d of %s\n", bpf->lineIx, bpf->fileName);
    psl = pslLoad(words);
    if (hashLookup(positions, psl->qName))
      {
	pos = createPosition(psl->qName, "BAC End", psl->strand[0]);
	pos->chrom = cloneString(psl->tName);
	pos->start = psl->tStart;
	pos->end = psl->tEnd;
	fc = hashMustFindVal(posClone, psl->qName);
	/* Check if this is a duplicate */
	if (!inPosList(pos, fc->end))
	  slAddHead(&fc->end, pos);
	else
	  positionFree(&pos);
      }
    }
}

void readClonePsl(struct lineFile *cpf)
/* Read in clone psl file */
{
  int lineSize, wordCount;
  char *line, *words[32];
  char *acc, *p;
  struct psl *psl;
  struct position *pos;
  struct fishClone *fc;

  while (lineFileNext(cpf, &line, &lineSize))
    {
    wordCount = chopTabs(line, words);
    if (wordCount != 21)
	errAbort("Bad line %d of %s\n", cpf->lineIx, cpf->fileName);
    psl = pslLoad(words);
    acc = cloneString(psl->qName);
    p = strrchr(acc, '.');
    *p = '\0';    
    if (hashLookup(positions, acc))
      {
	pos = createPosition(acc, "Accession", psl->strand[0]);
	pos->chrom = cloneString(psl->tName);
	pos->start = psl->tStart;
	pos->end = psl->tEnd;
	fc = hashMustFindVal(posClone, acc);
	/* Check if this is a duplicate */
	if (!inPosList(pos, fc->acc))
	  slAddHead(&fc->acc, pos);
	else
	  positionFree(&pos);
      }
    }
}

void findAccPosition(struct sqlConnection *conn, struct position *pos, struct fishClone *fc)
/* Determine the position of an accession */
{
  char query[256];
  struct sqlResult *sr;
  char **row, *name;
  struct clonePos *cp;
  struct position *newPos;
  struct name *missing;

  name = cloneString(pos->name);
  strcat(name, "%"); 
  safef(query, sizeof(query), "select * from clonePos where name like '%s'", name);
  sr = sqlGetResult(conn, query);
  if ((row = sqlNextRow(sr)) != NULL) 
    {
      cp = clonePosLoad(row);
      free(pos->name);
      pos->name = cloneString(cp->name);
      pos->chrom = cloneString(cp->chrom);
      pos->start = cp->chromStart;
      pos->end = cp->chromEnd;
      pos->phase = cp->phase;
      clonePosFree(&cp);
      while ((row = sqlNextRow(sr)) != NULL) 
	{
	  cp = clonePosLoad(row);
	  newPos = createPosition(cp->name, "Accession", ' ');
	  newPos->chrom = cloneString(cp->chrom);
	  newPos->start = cp->chromStart;
	  newPos->end = cp->chromEnd;
	  newPos->phase = cp->phase;
	  slAddHead(&fc->acc, newPos);
	  clonePosFree(&cp);
	}
    }
  else
    {
      missing = createName(pos->name);
      slAddHead(&fc->missingAcc, missing);
    }
  
  sqlFreeResult(&sr);
}

void findStsPosition(struct sqlConnection *conn, struct position *pos, struct fishClone *fc)
/* Determine the position of an sts marker */
{
  struct sqlConnection *conn1 = hAllocConn(sqlGetDatabase(conn));
  char query[256];
  struct sqlResult *sr, *sr1;
  char **row, **row1;
  struct stsAlias *a;
  struct stsMap *s;
  struct position *newPos;
  boolean found = FALSE;

  safef(query, sizeof(query), "select * from stsMap where name = '%s'", pos->name);
  sr = sqlGetResult(conn, query);
  if ((row = sqlNextRow(sr)) == NULL)
    {
      sqlFreeResult(&sr);
      safef(query, sizeof(query), "select * from stsAlias where alias = '%s'", pos->name);
      sr1 = sqlGetResult(conn1, query);
      if ((row1 = sqlNextRow(sr1)) != NULL)
	{
	  a = stsAliasLoad(row1);
	  safef(query, sizeof(query), "select * from stsMap where name = '%s'", a->trueName);
	  stsAliasFree(&a);
	  sr = sqlGetResult(conn, query);      
	  if ((row = sqlNextRow(sr)) != NULL)
	    found = TRUE;
	}
      sqlFreeResult(&sr1);
    }
  else 
    found = TRUE;
  
  if (found)
    {
      s = stsMapLoad(row);
      pos->chrom = cloneString(s->chrom);
      pos->start = s->chromStart;
      pos->end = s->chromEnd;
      stsMapFree(&s);
      while ((row = sqlNextRow(sr)) != NULL) 
	{
	  newPos = createPosition(pos->name, "STS Marker", ' ');
	  s = stsMapLoad(row);
	  newPos->chrom = cloneString(s->chrom);
	  newPos->start = s->chromStart;
	  newPos->end = s->chromEnd;
	  slAddHead(&fc->sts, newPos);
	  stsMapFree(&s);
	}
    }
  sqlFreeResult(&sr);
  hFreeConn(&conn1);
}

void findBacEndPairPosition(struct sqlConnection *conn, struct fishClone *fc)
/* Determine the positions of bac end pairs for a clone */
{
  char query[256], names[256];
  struct sqlResult *sr;
  char **row;
  struct lfs *be;
  struct position *newPos;

  safef(query, sizeof(query), "select * from bacEndPairs where name = '%s' order by (chromEnd - chromStart)", fc->cloneName);
  sr = sqlGetResult(conn, query);
  while ((row = sqlNextRow(sr)) != NULL)
    {
      be = lfsLoad(row+1);
      safef(names, sizeof(names), "%s,%s", be->lfNames[0], be->lfNames[1]);
      newPos = createPosition(names, "BAC End Pair", ' ');
      newPos->chrom = cloneString(be->chrom);
      newPos->start = be->chromStart;
      newPos->end = be->chromEnd;
      lfsFree(&be);
      slAddHead(&fc->endPair, newPos);
    }
  sqlFreeResult(&sr);
}

int combinePos(struct place *p, struct position *pos)
/* See if current position can be added to placement */
{
  int ret = 0;
  int d1 = abs(p->start - pos->start);
  int d2 = abs(p->end - pos->end);
  struct name *n;
  char *words[2], *names;
  int wordCount;
  struct name *pos2;
  boolean badBE = FALSE, sameEnd = FALSE;

  /* Check if BAC end and facing the wrong way */
  n = createName(pos->name);
  if ((!inNameList(n, p->bacEnd)) &&
      (!inNameList(n, p->bePair)))    
    {
    if ((sameString(pos->type, "BAC End")) &&
	( ((pos->orien == '+') && ((pos->start - p->start) > 25000)) || 
	((pos->orien == '-') && ((p->end - pos->end) > 25000)) )) 
      badBE = TRUE;
    else if ((((p->startBE != -1) && ((p->start - pos->start) > 25000)) || 
	      ((p->endBE != -1) && ((pos->end - p->end) > 25000))) &&
	     (((!sameString(pos->type, "BAC End Pair")) || (p->bePair == NULL)) && (p->acc == NULL)))
      badBE = TRUE;
    }
  nameFree(&n);

  /* Check if BAC end and all positions in placement are same accession */
  if ((sameString(pos->type, "BAC End")) && (p->acc == NULL) &&
      (p->bePair == NULL) && (p->sts == NULL) && (p->bacEnd !=NULL))
    {
      sameEnd = TRUE;
      for (pos2 = p->bacEnd; pos2 != NULL; pos2 = pos2->next)
	if (!sameString(pos->name, pos2->name)) 
	  sameEnd = FALSE;
    }

  if ((sameString(p->chrom, pos->chrom)) && (!badBE) && 
      ((d1 < 200000) || ((pos->start >= p->start) && (pos->start <= p->end))) && 
      ((d2 < 200000) || ((pos->end >= p->start) && (pos->end <= p->end))))
    {
      if (pos->start < p->start)
	{
	  if (sameEnd) 
	    p->end = pos->end;
	  p->start = pos->start;
	  if ((sameString(pos->type, "BAC End Pair")) || 
	      ((sameString(pos->type, "BAC End")) && (pos->orien == '+')))
	      p->startBE = 0;
	  else 
	    {
	      if ((p->start - pos->start) > 25000)
		p->startBE = -1;
	    }
	}
      else if ((sameString(pos->type, "BAC End Pair")) || 
	       ( ((sameString(pos->type, "BAC End")) && (pos->orien == '+')) &&
	       ((pos->start - p->start) <= 25000) && 
	       ((p->startBE < 0) || ((pos->start - p->start) <= p->startBE))) )
	p->startBE = pos->start - p->start;

      if (pos->end > p->end)
	{
	  if (sameEnd) 
	    p->start = pos->start;
	  p->end = pos->end;
	  if ((sameString(pos->type, "BAC End Pair")) || 
	      ((sameString(pos->type, "BAC End")) && (pos->orien == '-')))
	    p->endBE = 0;
	  else 
	    {
	      if ((pos->end - p->end) > 25000)
		p->endBE = -1;
	    }
	}
      else if ((sameString(pos->type, "BAC End Pair")) || 
	       ( ((sameString(pos->type, "BAC End")) && (pos->orien == '-')) &&
	       ((p->end - pos->end) <= 25000) && 
	       ((p->endBE < 0) || ((p->end - pos->end) < p->endBE))) )
	p->endBE = p->end - pos->end;

      if (sameString(pos->type, "Accession"))
	{
	  p->numAcc++;
	  n = createName(pos->name);
	  n->phase = pos->phase;
	  slAddHead(&p->acc, n);
	} 
      if (sameString(pos->type, "BAC End Pair"))
	{
	  names = cloneString(pos->name);
	  wordCount = chopString(names, ",",words,2);
	  n = createName(words[0]);
	  slAddHead(&p->bePair, n);
	  n = createName(words[1]);
	  slAddHead(&p->bePair, n);
	  p->numBacEndPair++;
	} 
      if (sameString(pos->type, "STS Marker"))
	{
	  /* Check if used for STS Marker */
	  n = createName(pos->name);
	  if (!inNameList(n, p->sts))
	    {
	      p->numSts++;
	      n = createName(pos->name);
	      slAddHead(&p->sts, n);
	    } 
	  else 
	    nameFree(&n);
	} 
      if (sameString(pos->type, "BAC End"))
	{
	  /* Check if used for end pair */
	  n = createName(pos->name);
	  if ((!inNameList(n, p->bePair)) && (!inNameList(n, p->bacEnd)))
	    {
	      p->numBacEnd++;
	      slAddHead(&p->bacEnd, n);
	    }
	  else
	    nameFree(&n);
	} 
      ret = 1;
    }

  return(ret);
}

int scorePlace(struct place *p, struct map *m)
/* Score a placement */
{
  int ret = 0, wordCount;
  struct map *m1;
  struct name *n, *nList=NULL, *new;  
  char *chrom, *words[2];
  
  /* Check if chromosome agress with any fish mapping */  
  verbose(3, "\tdetermining if in fish chrom\n");
  chrom = cloneString(p->chrom);
  /* Get rid of "random" */
  wordCount = chopString(chrom, "_",words, 2);
  for (m1 = m; m1 != NULL; m1 = m1->next)
    {
      verbose(3, "\tchecking %s vs. %s\n", m1->chrom, words[0]);
      if (sameString(m1->chrom, words[0]))
	{
	  if (wordCount == 1)
	    ret += 3;
	  else
	    ret += 2;
	  break;
	}
    }

  /* Score accessions based on phase of sequencing */
  verbose(3, "\tscoring accessions\n");
  for (n = p->acc; n != NULL; n = n->next)
    if (!inNameList(n, nList))
      {
	if (n->phase == 3)
	  ret += 5;
	else if (n->phase == 2)
	  ret += 3;
	else if (n->phase == 1)
	  ret += 2;
	else
	  ret += 1;
	new = createName(n->name);
	slAddHead(&nList, new);
      }

  ret += p->numBacEndPair * 3;

  /* Make sure each name only counted once */
  verbose(3, "\tscoring sts markers\n");
  for (n = p->sts; n != NULL; n = n->next)
    if (!inNameList(n, nList))
      {
	ret += 2;
	new = createName(n->name);
	slAddHead(&nList, new);
      }
  verbose(3, "\tscoring bac ends\n");
  for (n = p->bacEnd; n != NULL; n = n->next)
    if (!inNameList(n, nList))
      {
	ret += 1;
	new = createName(n->name);
	slAddHead(&nList, new);
      }
  nameFreeList(&nList);

  return(ret);
}

void filterGoodPlace(struct fishClone *fc)
/* Filter placements to only retain the best */
{
  int bestScore = 0;
  struct place *p, *lastp = NULL;
  
  /* Determine best score */
  for (p = fc->good; p != NULL; p = p->next)
    if (p->score > bestScore)
      bestScore = p->score;

  /* Filter out anyone not within 1 or best score */
  lastp = NULL;
  p = fc->good;
  while (p != NULL)
    {
      if (p->score < (bestScore - 1))
	{
	  if (lastp == NULL) 
	    {
	      fc->good = fc->good->next;
	      placeFree(&p);
	      fc->numGood--;
	      p = fc->good;
	    } 
	  else
	    {
	      lastp->next = p->next;
	      placeFree(&p);
	      fc->numGood--;
	      p = lastp->next;
	    }
	}
      else 
	{
	  lastp = p;
	  p = p->next;
	}
    }
}

void addOrCombinePos(struct fishClone *fc, struct position *pos)
/* Determine if position can be combined or a new possible good placement added */
{
  boolean combined = FALSE;
  struct place *p, *newP;

  verbose(3, "\tchecking if position can be combined\n");
  if (pos->chrom)
    {
      combined = FALSE;
      for (p = fc->good; p != NULL; p = p->next)
	if (!combined)
	  if (combinePos(p, pos))
	    combined = TRUE;
      if (!combined)
	{
	  newP = createPlace(pos);
	  slAddHead(&fc->good, newP);
	  fc->numGood++;
	}
    }
}

void findGoodPlace(struct fishClone *fc)
/* Determine the good positions for a clone */
{
  struct place *p;
  struct position *pos;

  /* Check clone acc positions */
  verbose(3, "\tchecking if can combine for %s\n", fc->cloneName);
  for (pos = fc->acc; pos != NULL; pos = pos->next)
    addOrCombinePos(fc, pos);
  /* Check bac end pair positions */
  for (pos = fc->endPair; pos != NULL; pos = pos->next)
    addOrCombinePos(fc, pos);
  /* Check clone sts marker positions */
  for (pos = fc->sts; pos != NULL; pos = pos->next)
    addOrCombinePos(fc, pos);
  /* Check clone end positions */
  for (pos = fc->end; pos != NULL; pos = pos->next)
    addOrCombinePos(fc, pos);

  /* Score each of the placements */
  verbose(3, "\tscoring placements for %s\n", fc->cloneName);
  for (p = fc->good; p != NULL; p = p->next)
    {
      p->score = scorePlace(p, fc->cyto);
      verbose(3, "\tscore on %s is %d\n", p->chrom,p->score);
    }
  /* Filter to retain only best placements */
  filterGoodPlace(fc);
  
}

void findClonePos(char *db)
/* Determine the best positions for each of the FISH clones */
{
  struct sqlConnection *conn = hAllocConn(db);
  struct fishClone *fc;
  struct position *pos;

  /* Process each clone */
  verbose(2, "\tfindClonePos: determining positions of fish clones\n");
  for (fc = fcList; fc != NULL; fc=fc->next)
    {
      verbose(3, "\tfinding pos of %s\n", fc->cloneName);
      /* Determine positions of clone accessions */
      for (pos = fc->acc; pos != NULL; pos = pos->next)
	{
	  /* Check if position already set from psl file */
	  if (!pos->chrom)
	    {
	      verbose(3, "\tfinding acc pos of %s\n", pos->name);
	      findAccPosition(conn, pos, fc);
	    }
	}
      /* Determing positions of sts markers */
      for (pos = fc->sts; pos != NULL; pos = pos->next)
	{
	  verbose(3, "\tfinding sts pos of %s\n", pos->name);
	  findStsPosition(conn, pos, fc);
	}
      /* Find positions of bac end pairs */
      verbose(3, "\tfinding bac end pairs of %s\n", fc->cloneName);
      findBacEndPairPosition(conn, fc);
      /* Determine good placements, if any */
      findGoodPlace(fc);
    }
  hFreeConn(&conn);
}

void writeOut(FILE *of, FILE *af)
/* Print out the fishClones.bed file */
{
  int score=0;
  struct map *m;
  struct fishClone *fc;
  struct place *p;
  struct name *n;
    int linesOut = 0;

  for (fc = fcList; fc != NULL; fc = fc->next)
    {
      if (fc->numGood > 10) 
	continue;
      if (fc->numGood == 1)
	score = 1000;
      else 
	if (fc->numGood > 0)
	  score = 1500/(fc->numGood);
	else
	  if (fc->missingAcc != NULL)
	    for (n = fc->missingAcc; n != NULL; n = n->next)
	      fprintf(af, "%s\n",n->name);
      for (p = fc->good; p != NULL; p=p->next)
	{
	    ++linesOut;
	    if (NULL != fc->cyto)
	    {
	  fprintf(of, "%s\t%d\t%d\t%s\t%d\t%d\t",
	      p->chrom, p->start, p->end, fc->cloneName, score, fc->numCyto);
	  m = fc->cyto;
	  fprintf(of, "%s", m->band1); 
	  for (m = fc->cyto->next; m != NULL; m=m->next)
	      fprintf(of, ",%s", m->band1);
	  fprintf(of, "\t"); 
	  m = fc->cyto;
	  fprintf(of, "%s", m->band2); 
	  for (m = fc->cyto->next; m != NULL; m=m->next)
	      fprintf(of, ",%s", m->band2);
	  fprintf(of, "\t"); 
	  m = fc->cyto;
	  fprintf(of, "%s", m->center); 
	  for (m = fc->cyto->next; m != NULL; m=m->next)
	      fprintf(of, ",%s", m->center);
	  fprintf(of, "\t"); 
	  fprintf(of, "%s\t", p->type);
	  fprintf(of, "%d\t", p->numAcc);
	  if (p->numAcc) 
	    {
	      fprintf(of, "%s", p->acc->name);
	      for (n = p->acc->next; n != NULL; n=n->next)
		fprintf(of, ",%s", n->name);
	    }
	  fprintf(of, "\t");
	  fprintf(of, "%d\t", p->numSts);
	  if (p->numSts) 
	    {
	      fprintf(of, "%s", p->sts->name);
	      for (n = p->sts->next; n != NULL; n=n->next)
		fprintf(of, ",%s", n->name);
	    }
	  fprintf(of, "\t");
	  fprintf(of, "%d\t", p->numBacEnd + p->numBacEndPair*2);
	  if (p->numBacEndPair) 
	    {
	      fprintf(of, "%s", p->bePair->name);
	      for (n = p->bePair->next; n != NULL; n=n->next)
		fprintf(of, ",%s", n->name);
	    }
	  if (p->numBacEnd) 
	    {
	      if (p->numBacEndPair)
		fprintf(of, ",%s", p->bacEnd->name);
	      else
		fprintf(of, "%s", p->bacEnd->name);
	      for (n = p->bacEnd->next; n != NULL; n=n->next)
		fprintf(of, ",%s", n->name);
	    }
	  if (ENDINFO) {
	    fprintf(of, "\t%d\t%d", p->startBE, p->endBE);
	  }
	  fprintf(of, "\n");
	    } else {
verbose(1, "# ERROR: at line # %d, no cytoband info for %s:%d-%d\t%s\n",
	      linesOut, p->chrom, p->start, p->end, fc->cloneName);
	    }
	}	/*	for (p = fc->good; p != NULL; p=p->next)	*/
    }
}

int main(int argc, char *argv[])
{
  struct lineFile *ff, *cf, *bef, *bpf, *sf, *cpf;
  FILE *of, *af;
  char filename[PATH_LEN], *db, *stsName=NULL, *pslName=NULL;

  optionInit(&argc, argv, optionSpecs);
  if (argc < 4)
    {
      fprintf(stderr, "USAGE: fishClones <database> <hbrc> <clac.out> <cl_acc_gi_len> <bacEnds.psl> <out file prefix>\n");
      fprintf(stderr, "Options:\n");
      fprintf(stderr, "\t-fhcrc=<file>\tSTS marker associations from FHCRC\n");
      fprintf(stderr, "\t-psl=<psl_file>\tpsl file of clone placements\n");
      fprintf(stderr, "\t-verbose=<level>\tdisplay all messages\n");
      fprintf(stderr, "\t-noBin\t\tdo not include bin column in output file\n");
      fprintf(stderr, "\t-noRandom\tdo not include placements on random portions\n");
      fprintf(stderr, "\t-endInfo\tprint out whether start/end determined by BAC end\n");
      return 1;
    }
  db = argv[1];

  ff = lineFileOpen(argv[2], FALSE);
  cf = lineFileOpen(argv[3], FALSE);
  bef = lineFileOpen(argv[4], FALSE);
  bpf = pslFileOpen(argv[5]);
  safef(filename, sizeof(filename), "%s.bed", argv[6]);
  of = mustOpen(filename, "w");
  safef(filename, sizeof(filename), "%s.acc", argv[6]);
  af = mustOpen(filename, "w");

  verb = optionInt("verbose", 0);
  verboseSetLevel(verb);
  NORANDOM = optionExists("noRandom");
  NOBIN = optionExists("noBin");
  ENDINFO = optionExists("endInfo");
  stsName = optionVal("fhcrc", NULL);
  if (stsName) 
    sf = lineFileOpen(stsName, FALSE);
  pslName = optionVal("psl", NULL);
  if (pslName) 
    cpf = pslFileOpen(pslName);

  verbose(1, "Reading Fish Clones file %s\n", argv[2]);
  readFishInfo(ff);
  lineFileClose(&ff);
  writeOut(of, af);
  verbose(1, "Reading Clone/Acc (clac.out) file %s\n", argv[3]);
  readCloneAcc(cf);
  lineFileClose(&cf);
  writeOut(of, af);
  verbose(1, "Reading BAC Ends file %s\n", argv[4]);
  readBacEnds(bef);
  lineFileClose(&bef);
  writeOut(of, af);
  verbose(1, "Reading BAC Ends psl file %s\n", argv[5]);
  readBacEndsPsl(bpf);
  lineFileClose(&bpf);
  writeOut(of, af);
  if (stsName)
    {
      verbose(1, "Reading additional STS Marker links %s\n", stsName);
      readStsMarkers(sf);
      lineFileClose(&sf);
    }
  writeOut(of, af);
  if (pslName)
    {
      verbose(1, "Reading additional clone psl positions %s\n", pslName);
      readClonePsl(cpf);
      lineFileClose(&cpf);
    }
  verbose(1, "Determining good positions\n");
  findClonePos(db);
  verbose(1, "Writing output file\n");
  writeOut(of, af);
  fclose(of);
  fclose(af);

  return 0;
}
