/*
  File: updateStsInfo.c
  Author: Terry Furey
  Date: 7/6/2004
  Description: Update stsInfo file with new information from NCBI
*/

/* Copyright (C) 2009 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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

/*	over time these listings and numbers are growing. */
#define MAX_ID_LIST	262144
#define MAX_STS_ID	9000000

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"gb", OPTION_STRING},
    {"verbose", OPTION_STRING},
    {NULL, 0}
};

struct sts
{
  struct sts *next;
  struct stsInfo2 *si;
  struct dnaSeq *fa;
  char *faAcc;
  boolean mapped;
  boolean dbstsIdExists;
} *sList=NULL;

struct primer
{
  struct primer *next;
  unsigned dbStsId;
  char* left;
  char* right;
  char* dist;
  unsigned ucscId;
};

struct gb
{
  struct gb *next;
  char* acc;
  struct sts *s;
  boolean gbSeq;
};

struct hash *stsHash;
struct hash *primerHash;
struct hash *nameHash;
struct hash *dbStsIdHash;
struct hash *gbAccHash;
struct hash *ucscIdHash;
struct hash *orgHash;

struct stsInfo2 *siList = NULL;

int nextUcscId = 0;

boolean isMapped(struct stsInfo2 *si)
/* Determine whether a stsInfo2 record contains mapping information */
{
  if (differentString(si->genethonName, "\0"))
    return(1);
  if (differentString(si->marshfieldName, "\0"))
    return(1);
  if (differentString(si->decodeName, "\0"))
    return(1);
  if (differentString(si->wiyacName, "\0"))
    return(1);
  if (differentString(si->wirhName, "\0"))
    return(1);
  if (differentString(si->gm99gb4Name, "\0"))
    return(1);
  if (differentString(si->gm99g3Name, "\0"))
    return(1);
  if (differentString(si->tngName, "\0"))
    return(1);

  return(0);
}

boolean checkGb(char *name)
/* Check if this is a genBank accession */
{
  if (hashLookup(gbAccHash, name))
    return(TRUE);

  return(FALSE);
}

boolean checkGdb(char *name)
/* Check if this is a GDB id */
{
  return(startsWith("GDB", name));
}

boolean inArray(char *el, char **array, int size)
/* Check if element is in array */
{
  int i;
  
  for (i = 0; i < size; i++)
    if (sameString(el, array[i]))
      return(TRUE);

  return(FALSE);

}

boolean inArrayInt(unsigned el, unsigned *array, int size)
/* Check if element is in array */
{
  int i;
  
  for (i = 0; i < size; i++)
    if (el == array[i])
      return(TRUE);

  return(FALSE);

}

void addElement(char *el, char ***array, unsigned *count)
/* Add a new element to a array of elements */
{
  char *arrayCurr, arrayNew[MAX_ID_LIST];
  int sizeOne, size;
  char **cArray, **rArray=NULL;
  // char ***dArray;

  /* Check if already present in array */
  if (!inArray(el, *array, *count))
    {
      size = *count;
      arrayCurr = sqlStringArrayToString(*array, *count);
      safef(arrayNew, ArraySize(arrayNew), "%s%s,", arrayCurr, el);
      size++;
      // dArray = array;
      /* if (*dArray) 
	 freeMem(dArray); */
      sqlStringDynamicArray(arrayNew, &cArray, &sizeOne);
      assert(sizeOne == size);
      *count = size;
      AllocArray(rArray, size);
      CopyArray(cArray, rArray, size);
      *array = rArray;
    }
}

void removeElement(char *el, char ***array, unsigned *count)
/* Add a new element to a array of elements */
{
  char *arrayCurr, *arrayCurrDel, del[128];
  int sizeOne, size;
  char **cArray, **rArray=NULL;
  // char ***dArray;

  if (*count > 0)
    {
      size = *count;
      arrayCurr = sqlStringArrayToString(*array, *count);
      safef(del, ArraySize(del), "%s,", el);
      arrayCurrDel = replaceChars(arrayCurr, del, "");
      if (differentString(arrayCurr, arrayCurrDel))
	  size--;
      // dArray = array;
      /* if (*dArray) 
	 freeMem(dArray); */
      sqlStringDynamicArray(arrayCurrDel, &cArray, &sizeOne);
      assert(sizeOne == size);
      *count = size;
      if (size > 0) 
	{
	  AllocArray(rArray, size);
	  CopyArray(cArray, rArray, size);
	  *array = rArray;
	}
      else
	*array = NULL;
    }
}
  
void addElementInt(unsigned el, unsigned **array, unsigned *count)
/* Add a new element to a array of elements */
{
  char *arrayCurr, arrayNew[MAX_ID_LIST];
  int sizeOne, size;
  unsigned *uArray;
//  unsigned **dArray;  unused

  if (!inArrayInt(el, *array, *count))
    {
      size = *count;
      arrayCurr = sqlUnsignedArrayToString(*array, *count);
      safef(arrayNew, ArraySize(arrayNew), "%s%d,", arrayCurr, el);
      size++;
      // dArray = array;
      /* if (*count > 0)
	 freeMem(dArray); */
      sqlUnsignedDynamicArray(arrayNew, &uArray, &sizeOne);
      assert(sizeOne == size);
      *count = size;
      *array = uArray;
    }
}

void addName(struct sts *s, char *name)
/* Add a name to a sts record */
{
    struct gb *gb;

  /* See if it is a genBank record */
  if (checkGb(name))
    {
      addElement(name, &s->si->genbank, &s->si->gbCount);
      if (hashLookup(gbAccHash, name))
	{
	  gb = hashMustFindVal(gbAccHash, name);
	  gb->s = s;
	}
      else
	{
	  AllocVar(gb);
	  gb->next = NULL;
	  gb->acc = cloneString(name);
	  gb->s = s;
	}
    }
  else if (checkGdb(name))
    {
      addElement(name, &s->si->gdb, &s->si->gdbCount);
    }    
  else
    {
      addElement(name, &s->si->otherNames, &s->si->nameCount);
    }
}

void readStsInfo(struct lineFile *sif)
/* Read in current stsInfo file */
{
  struct stsInfo2 *si;
  struct sts *s;
  struct primer *p;
  struct gb *gb;
  char name[16], *words[52];
  int i;
  
  stsHash = newHash(20);
  dbStsIdHash = newHash(20);
  primerHash = newHash(20);
  nameHash = newHash(24);
  gbAccHash = newHash(20);
  ucscIdHash = newHash(20);

  /* Read in all rows */  
  while (lineFileChopTab(sif, words))
    {
      si = stsInfo2Load(words);
      /* Determine next ucsc id to be used */
      if (si->identNo >= nextUcscId)
	nextUcscId = si->identNo + 1;

      /* Create sts struct */
      if (sameString(si->organism, "Homo sapiens\0"))
	{
	  AllocVar(s);
	  s->next = NULL;
	  s->si = si;
	  s->fa = NULL;
	  s->mapped = isMapped(si);
	  s->dbstsIdExists = FALSE;
	  slAddHead(&sList, s);
	  safef(name, ArraySize(name), "%d", si->identNo);
	  hashAdd(stsHash, name, s);
	  
	  /* Add ids to dbStsIdHash */
	  if (si->dbSTSid)
	    hashAddInt(dbStsIdHash, name, si->dbSTSid);
	  
	  /* Add sts records to ucscId hash */
	  if (si->dbSTSid)
	    {
	      safef(name, ArraySize(name),  "%d", si->dbSTSid);
	      hashAdd(ucscIdHash, name, s);
	    }
	  for (i = 0; i < si->otherDbstsCount; i++)
	    {
	      safef(name, ArraySize(name), "%d", si->otherDbSTS[i]);
	      if (!hashLookup(ucscIdHash, name))
		hashAdd(ucscIdHash, name, s);
	    }
	  
	  /* Add names to name hash and genbank hash */
	  hashAdd(nameHash, si->name, s);
      	  for (i = 0; i < si->gbCount; i++) 
	    {
	      hashAdd(nameHash, si->genbank[i], s);
	      AllocVar(gb);
	      gb->next = NULL;
	      gb->acc = cloneString(si->genbank[i]);
	      gb->s = s;
	      gb->gbSeq = FALSE;
	      hashAdd(gbAccHash, gb->acc, gb);
	    }
	  for (i = 0; i < si->gdbCount; i++) 
	    hashAdd(nameHash, si->gdb[i], s);
	  for (i = 0; i < si->nameCount; i++) 
	    hashAdd(nameHash, si->otherNames[i], s);
	  
	  /* Create primer info if available and add to hash */
	  if (differentString(si->leftPrimer, "\0"))
	    {
	      AllocVar(p);
	      p->next = NULL;
	      p->dbStsId = si->dbSTSid;
	      p->left = cloneString(si->leftPrimer);
	      p->right = cloneString(si->rightPrimer);
	      p->dist = cloneString(si->distance);
	      p->ucscId = si->identNo;
	      safef(name, ArraySize(name), "%d", p->dbStsId);
	      hashAdd(primerHash, name, p);
	    }
	}
      else 
	{
	  stsInfo2Free(&si);
	}
    }
}

void readGbAcc(struct lineFile *gaf)
/* Read in and record all genbank accessions that have sequences */
{
  struct gb *gb;
  char *acc[1];
  struct sts *s;

  while (lineFileNextRow(gaf, acc, 1))
    {
      if (!hashLookup(gbAccHash, acc[0]))
	{
	  AllocVar(gb);
	  gb->next = NULL;
	  gb->acc = cloneString(acc[0]);
	  gb->s = NULL;
	  gb->gbSeq = TRUE;
	  hashAdd(gbAccHash, acc[0], gb);
	  if (hashLookup(nameHash, acc[0]))
	    {
	      s = hashMustFindVal(nameHash, acc[0]);
	      addElement(acc[0], &s->si->genbank, &s->si->gbCount);
	      removeElement(acc[0], &s->si->otherNames, &s->si->nameCount);
	    }
	} 
      else 
	{
	  gb = hashMustFindVal(gbAccHash, acc[0]);
	  gb->gbSeq = TRUE;
	}
    }
}

void updatePrimerInfo(struct primer *p, struct sts *s)
/* Update primer information for an STS marker from a primer record */
{
  s->si->dbSTSid = p->dbStsId;
  s->si->leftPrimer = cloneString(p->left);
  s->si->rightPrimer = cloneString(p->right);
  s->si->distance = cloneString(p->dist);
  s->dbstsIdExists = TRUE;
  p->ucscId = s->si->identNo;
}

void readDbstsPrimers(struct lineFile *dsf)
/* Read in primer and organism info from dbSTS.sts */
{
  struct primer *p;
  struct sts *s;
  char *words[8], *name, *org;
  int dbStsId, newId;

  orgHash = newHash(20);

  while (lineFileChopNextTab(dsf, words, 8))
    {
      /* Check that the organism is human, or at least that a human record 
	 has not already been read in for this dbSTS id */
      org = cloneString(words[7]);
      if (!hashLookup(orgHash, words[0]) || (sameString(org, "Homo sapiens\0")))
	hashAdd(orgHash, words[0], org);
      /* If not human, then don't process any further */
      if (differentString(org, "Homo sapiens\0"))
	continue;

      /* See if this dbSTS id is currently in use by a STS marker */
      dbStsId = sqlUnsigned(words[0]);
      if (hashLookup(ucscIdHash, words[0]))
	s = hashMustFindVal(ucscIdHash, words[0]);
      else
	s = NULL;
      /* See if primers already recorded for this dbSTS id from STS Info file */
      if (hashLookup(primerHash, words[0]))
	{
	  p = hashMustFindVal(primerHash, words[0]);
	  /* If STS marker not mapped, update primer information */
	  if (s == NULL && !s->mapped)
	    {
	      freez(&(p->left));
	      p->left = cloneString(words[1]);
	      freez(&(p->right));
	      p->left = cloneString(words[2]);
	      freez(&(p->dist));
	      p->left = cloneString(words[3]);
	    }
	}
      /* If no record of this primer, create one */
      else
	{
	  AllocVar(p);
	  p->next = NULL;
	  p->dbStsId = dbStsId;
	  p->left = cloneString(words[1]);
	  p->right = cloneString(words[2]);
	  p->dist = cloneString(words[3]);
	  if (s != NULL)
	    p->ucscId = s->si->identNo;
	  else
	    p->ucscId = 0;
	  name = cloneString(words[0]);
	  hashAdd(primerHash, name, p);
	}
      /* If dbSTS id linked to a STS marker already */
      if (s != NULL)
	{
	  /* If linked to ucsc record and record not mapped or doesn't have primer information,
	     update primer info */
	  if (((!s->mapped) || (sameString(s->si->leftPrimer, "\0"))) && (s->si->dbSTSid == dbStsId))
	    updatePrimerInfo(p, s);
	}
      /* If not linked to a ucsc record and human, check if the name is already in use */
      else if ((sameString(org, "Homo sapiens\0")) && (hashLookup(nameHash, words[4])))
	    {
	      s = hashMustFindVal(nameHash, words[4]);

	      /* Update the marker record with the dbSTS id and primer info if none exists */
	      if ((s->si->dbSTSid == 0) || (s->si->dbSTSid >= MAX_STS_ID) || 
		  (sameString(s->si->leftPrimer, "\0")))
		updatePrimerInfo(p, s);
	      /* If the record is already linked to another dbSTS id, add this to other list */
	      else 
		{
		  newId = sqlUnsigned(words[0]);
		  addElementInt(newId, &s->si->otherDbSTS, &s->si->otherDbstsCount);
		}
	      hashAdd(ucscIdHash, words[0], s);
	    }
    }
}	/*	void readDbstsPrimers(struct lineFile *dsf)	*/

void readDbstsNames(struct lineFile *daf)
/* Read in dbSTS names and create new stsInfo record, if necessary */
{
  struct sts *s;
  struct stsInfo2 *si;
  struct primer *p;
  char *words[4], *names[64], name[64], *org;
  int dbstsId, nameCount, i;

  while (lineFileChopNext(daf, words, 2))
    {
      /* Make sure this is a human marker */
      org = hashFindVal(orgHash, words[0]);
      if (hashLookup(orgHash, words[0]) && !sameString(org, "Homo sapiens\0") && !sameString(org, "\0"))
	  continue;
      dbstsId = sqlUnsigned(words[0]);
      /* Find the primers for this dbSTS id */
      if (hashLookup(primerHash, words[0])) 
	p = hashMustFindVal(primerHash, words[0]);
      /* Determine if this id is already being used */
      if (hashLookup(ucscIdHash, words[0]))
	{
	s = hashMustFindVal(ucscIdHash, words[0]);
	}
      else
	{
	s = NULL;
	}
      /* If the id has not been assigned, see any of the names are being used */
      if (s == NULL) 
	{
	  nameCount = chopByChar(words[1], ';', names, ArraySize(names));
	  for (i = 0; i < nameCount; i++) 
	    {
	      touppers(names[i]);
	      /* See if this name associated with a ucsc record already */
	      if (hashLookup(nameHash, names[i]))
		{
		  s = hashMustFindVal(nameHash, names[i]);
		  /* See if this record needs an dbSTS id */
		  if ((s->si->dbSTSid == 0) || (s->si->dbSTSid >= MAX_STS_ID) ||  
		      (sameString(s->si->leftPrimer, "\0")))
		    {
		      s->si->dbSTSid = dbstsId;
		      /* If no primer info recorded, add it if possible */
		      if (((!s->mapped) || (sameString(s->si->leftPrimer, "\0")))
			  && (hashLookup(primerHash, words[0])))
			{
			  p = hashMustFindVal(primerHash, words[0]);
			  s->si->leftPrimer = cloneString(p->left);
			  s->si->rightPrimer = cloneString(p->right);
			  s->si->distance = cloneString(p->dist);
			}
		      i = nameCount;
		    }
		  else
		    {
	    addElementInt(dbstsId, &s->si->otherDbSTS, &s->si->otherDbstsCount);
		    }
		}
	    }
	}
      if (s != NULL)
	{
	  /* Determine if all of the names are recorded */
	  if (s->si->dbSTSid == dbstsId)
	    s->dbstsIdExists = TRUE;
	  nameCount = chopByChar(words[1], ';', names, ArraySize(names));
	  for (i = 0; i < nameCount; i++) 
	    {
	      touppers(names[i]);
	      if (!hashLookup(nameHash, names[i]))
		{
		  subChar(names[i],',',':');
		  addName(s, names[i]);
		  hashAdd(nameHash, names[i], s);
		}
	    }
	}
      else
	{
	  /* If valid primers exist, then add record */
	  if (hashLookup(primerHash, words[0]))
	    p = hashMustFindVal(primerHash, words[0]);
	  else
	    p = NULL;
	  if (p != NULL)
	    {
	      nameCount = chopByChar(words[1], ';', names, ArraySize(names));
	      AllocVar(s);
	      AllocVar(si);
	      si->next = NULL;
	      s->si = si;
	      s->mapped = FALSE;
	      s->dbstsIdExists = TRUE;
	      s->fa = NULL;
	      si->next = NULL;
	      si->identNo = nextUcscId;
	      nextUcscId++;
	      touppers(names[0]);
	      si->name = cloneString(names[0]);
	      si->gbCount = 0;
	      si->genbank = NULL;
	      si->gdbCount = 0;
	      si->gdb = NULL;
	      si->nameCount = 0;
	      si->otherNames = NULL;
	      if (checkGb(names[0]) || checkGdb(names[0]))
		addName(s, names[0]);
	      hashAdd(nameHash, names[0], s);
	      for (i = 1; i < nameCount; i++) 
		{
		  subChar(names[i], ',', ':');
		  touppers(names[i]);
		  addName(s, names[i]);
		  hashAdd(nameHash, names[i], s);
		}
	      si->dbSTSid = dbstsId;
	      si->otherDbstsCount = 0;
	      si->otherDbSTS = NULL;
	      si->leftPrimer = cloneString(p->left);
	      si->rightPrimer = cloneString(p->right);
	      si->distance = cloneString(p->dist);
	      si->organism = cloneString("Homo sapiens");
	      si->sequence = 0;
	      si->otherUCSCcount = 0;
	      si->otherUCSC = NULL;
	      si->mergeUCSCcount = 0;
	      si->mergeUCSC = NULL;
	      si->genethonName = cloneString("");
	      si->genethonChr = cloneString("");
	      si->marshfieldName = cloneString("");
	      si->marshfieldChr = cloneString("");
	      si->wiyacName = cloneString("");
	      si->wiyacChr = cloneString("");
	      si->wirhName = cloneString("");
	      si->wirhChr = cloneString("");
	      si->gm99gb4Name = cloneString("");
	      si->gm99gb4Chr = cloneString("");
	      si->gm99g3Name = cloneString("");
	      si->gm99g3Chr = cloneString("");
	      si->tngName = cloneString("");
	      si->tngChr = cloneString("");
	      si->decodeName = cloneString("");
	      si->decodeChr = cloneString("");
	      slAddHead(&sList, s);
	      hashAdd(ucscIdHash, words[0], s);
	      safef(name, ArraySize(name), "%d", s->si->identNo);
	      hashAdd(stsHash, name, s);
	      hashAddInt(dbStsIdHash, name, dbstsId); 
	      p->ucscId = s->si->identNo;
	    }
	}
    }     
}

void readAllSts(FILE *asf) 
/* Read in current sequences for sts markers */
{
  struct dnaSeq *ds;
  struct sts *s;
  char *words[8], *acc=NULL, *line;
  int wordCount;

  while (faReadMixedNext(asf, 0, "default", TRUE, &line, &ds))
    {
      /* Determine the UCSC id */
      wordCount = chopByWhite(line, words, ArraySize(words));
      stripString(words[0], ">");
      if (wordCount == 3)
	acc = cloneString(words[2]);
      else
	acc = NULL;
      /* Find the record and attach */
      if (hashLookup(stsHash, ds->name))
	{
	  s = hashMustFindVal(stsHash, ds->name);
	  s->fa = ds;
	  s->faAcc = acc;
	  s->si->sequence = 1;
	}
      else 
	{
	  dnaSeqFree(&ds);
	  freez(&line);
	  if (acc != NULL)
	    freez(&acc);
	}
    }
}

void readDbstsFa(FILE *dff)
/* Read in sequences from dbSTS.fa and add, if possible */
{
  struct dnaSeq *ds;
  struct sts *s;
  struct gb *gb;
  char name[256], *line;

  while (faReadMixedNext(dff, 0, "default", TRUE, &line, &ds))
    {
      /* Determine the UCSC id */
      if (hashLookup(gbAccHash, ds->name)) 
	{
	  /* Determine if this is linked to a marker */
	  gb = hashMustFindVal(gbAccHash, ds->name);
	  if (gb->s != NULL) 
	    {
	      /* If no recorded sequence, then add */ 
	      s = gb->s;
	      if (s->fa == NULL) 
		{
		  s->faAcc = cloneString(ds->name);
		  safef(name, ArraySize(name), "%d", s->si->identNo);
		  ds->name = cloneString(name);
		  s->fa = ds;
		  s->si->sequence = 1;
		} 
	      /* If no accession recorded, see if sequences are the same */
	      else if (s->faAcc == NULL) 
		{
		  if (sameString(s->fa->dna, ds->dna))
		    {
		      s->faAcc = cloneString(ds->name);
		      s->si->sequence = 1;		  
		    }
		  freeDnaSeq(&ds);
		}	  
	      /* If same accession as recorded, the update sequence */
	      else if (sameString(s->faAcc, ds->name))
		{
		  ds->name = cloneString(s->fa->name);
		  freeDnaSeq(&s->fa);
		  s->fa = ds;
		  s->si->sequence = 1;
		}
	      else
		freeDnaSeq(&ds);	    
	    }
	  else
	    freeDnaSeq(&ds);	    
	}
    }
}

void writeOut(FILE *of, FILE *opf, FILE *oaf, FILE *off)
/* Write out update files for info, primers, and sequences */
{
  struct sts *s;
  struct stsInfo2 *si;
  char name[256];
  int i;

  slReverse(&sList);
  for (s = sList; s != NULL; s = s->next)
    {
      if (s->fa != NULL)
	s->si->sequence = 1;
      else
	s->si->sequence = 0;	
      si = s->si;
      if ((s->dbstsIdExists)||(si->dbSTSid == 0)||(si->dbSTSid >= MAX_STS_ID))
	{
	  fprintf(of, "%d\t%s\t%d\t", si->identNo, si->name, si->gbCount);
	  for (i = 0; i < si->gbCount; i++) 
	    fprintf(of, "%s,", si->genbank[i]);
	  fprintf(of, "\t%d\t", si->gdbCount);
	  for (i = 0; i < si->gdbCount; i++) 
	    fprintf(of, "%s,", si->gdb[i]);
	  fprintf(of, "\t%d\t", si->nameCount);
	  for (i = 0; i < si->nameCount; i++) 
	    fprintf(of, "%s,", si->otherNames[i]);
	  fprintf(of, "\t%d\t%d\t", si->dbSTSid, si->otherDbstsCount);
	  for (i = 0; i < si->otherDbstsCount; i++) 
	    fprintf(of, "%d,", si->otherDbSTS[i]);
	  fprintf(of, "\t%s\t%s\t%s\t%s\t%d\t%d\t", si->leftPrimer, si->rightPrimer,
		  si->distance, si->organism, si->sequence, si->otherUCSCcount);
	  for (i = 0; i < si->otherUCSCcount; i++) 
	    fprintf(of, "%d,", si->otherUCSC[i]);
	  fprintf(of, "\t%d\t", si->mergeUCSCcount);
	  for (i = 0; i < si->mergeUCSCcount; i++) 
	    fprintf(of, "%d,", si->mergeUCSC[i]);
	  fprintf(of, "\t%s\t%s\t%.2f\t%.2f", si->genethonName, si->genethonChr,
		  si->genethonPos, si->genethonLOD);
	  fprintf(of, "\t%s\t%s\t%.2f\t%.2f", si->marshfieldName, si->marshfieldChr,
		  si->marshfieldPos, si->marshfieldLOD);
	  fprintf(of, "\t%s\t%s\t%.2f\t%.2f", si->wiyacName, si->wiyacChr,
		  si->wiyacPos, si->wiyacLOD);
	  fprintf(of, "\t%s\t%s\t%.2f\t%.2f", si->wirhName, si->wirhChr,
		  si->wirhPos, si->wirhLOD);
	  fprintf(of, "\t%s\t%s\t%.2f\t%.2f", si->gm99gb4Name, si->gm99gb4Chr,
		  si->gm99gb4Pos, si->gm99gb4LOD);
	  fprintf(of, "\t%s\t%s\t%.2f\t%.2f", si->gm99g3Name, si->gm99g3Chr,
		  si->gm99g3Pos, si->gm99g3LOD);
	  fprintf(of, "\t%s\t%s\t%.2f\t%.2f", si->tngName, si->tngChr,
		  si->tngPos, si->tngLOD);
	  fprintf(of, "\t%s\t%s\t%.2f\t%.2f\n", si->decodeName, si->decodeChr,
		  si->decodePos, si->decodeLOD);
	  
	  /* Write out to primers file */
	  if (differentString(si->leftPrimer, "\0"))
	    fprintf(opf, "%d\t%s\t%s\t%s\t%d\n", si->dbSTSid, si->leftPrimer,
		    si->rightPrimer, si->distance, si->identNo);

	  /* Write out to alias file */
	  fprintf(oaf, "%s\t%d\t%s\n", si->name, si->identNo, si->name);
	  for (i = 0; i < si->gbCount; i++) 
	    if (differentString(si->genbank[i], si->name))
	      fprintf(oaf, "%s\t%d\t%s\n", si->genbank[i], si->identNo, si->name);
	  for (i = 0; i < si->gdbCount; i++) 
	    if (differentString(si->gdb[i], si->name))
	      fprintf(oaf, "%s\t%d\t%s\n", si->gdb[i], si->identNo, si->name);
	  for (i = 0; i < si->nameCount; i++) 
	    if (differentString(si->otherNames[i], si->name))
	      fprintf(oaf, "%s\t%d\t%s\n", si->otherNames[i], si->identNo, si->name);
	  
	  /* Write out to fa file */
	  if (s->fa != NULL)
	    {
	      /* gb = sqlStringArrayToString(si->genbank, si->gbCount); */
		if (s->faAcc && differentWord(s->faAcc,"(null)"))
	      safef(name, ArraySize(name), "%s %s %s", s->fa->name, si->name, s->faAcc);
		else
	      safef(name, ArraySize(name), "%s %s %s", s->fa->name, si->name, s->si->name);
	      faWriteNext(off, name, s->fa->dna, s->fa->size);
	    }
	}
      else
	verbose(1, "%d\t%s\t%d\t(%d) not in dbSTS anymore\n", si->identNo, si->name, si->dbSTSid, s->dbstsIdExists);
    }
}

int main(int argc, char *argv[])
{
  struct lineFile *sif, *dsf, *daf, *gbf;
  FILE *of, *opf, *oaf, *off, *asf, *dff;
  char filename[256], *gbName;
  int verb = 0;

verboseSetLevel(0);
optionInit(&argc, argv, optionSpecs);
if (argc < 3)
    {
      fprintf(stderr, "USAGE: updateStsInfo [-verbose=<level> -gb=<file>] <stsInfo file> <all.STS.fa> <dbSTS.sts> <dbSTS.aliases> <dbSTS.convert.fa> <outfile prefix>\n");
    return 1;
    }
verb = optionInt("verbose", 0);
verboseSetLevel(verb);

 gbName = optionVal("gb", NULL);
 if (gbName) 
   gbf = lineFileOpen(gbName, TRUE);
 sif = lineFileOpen(argv[1], TRUE);
 asf = mustOpen(argv[2], "r");
 dsf = lineFileOpen(argv[3], TRUE);
 daf = lineFileOpen(argv[4], TRUE);
 dff = mustOpen(argv[5], "r");

 safef(filename, ArraySize(filename), "%s.info", argv[6]);
 of = mustOpen(filename, "w");
 safef(filename, ArraySize(filename), "%s.primers", argv[6]);
 opf = mustOpen(filename, "w");
 safef(filename, ArraySize(filename), "%s.alias", argv[6]);
 oaf = mustOpen(filename, "w");
 safef(filename, ArraySize(filename), "%s.fa", argv[6]);
 off = mustOpen(filename, "w");

 /* Read in current stsInfo file */
 verbose(1, "Reading current stsInfo file: %s\n", argv[1]);
 readStsInfo(sif);

 /* Read in genbank accessions that have sequences */ 
 if (gbName)
   {
     verbose(1, "Reading genbank accession file: %s\n", gbName);
     readGbAcc(gbf);
   }

 /* Read in primer and organism information from dbSTS.sts */
 verbose(1, "Reading current dbSTS.sts file: %s\n", argv[3]);
 readDbstsPrimers(dsf);

 /* Read in names from dbSTS.alias and create new stsInfo records if needed */
 verbose(1, "Reading current dbSTS.aliases file: %s\n", argv[4]);
 readDbstsNames(daf);

 /* Read in current sequences for sts markers */
 verbose(1, "Reading current all.STS file: %s\n", argv[2]);
 readAllSts(asf);

 /* Read in new sequences from dbSTS.fa */
 verbose(1, "Reading dbSTS.fa file: %s\n", argv[5]);
 readDbstsFa(dff);

 /* Print out the new files */
 verbose(1, "Creating output files: %s .info .primers .alias .fa\n", argv[6]);
 writeOut(of, opf, oaf, off);

 fclose(asf);
 lineFileClose(&dsf);
 lineFileClose(&daf);
 fclose(dff);
 if (gbName)
   lineFileClose(&gbf);   
 fclose(of);
 fclose(opf);
 fclose(oaf);
 fclose(off);

 return(0);
}
