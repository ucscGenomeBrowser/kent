/*
  File: updateStsInfo.c
  Author: Terry Furey
  Date: 7/6/2004
  Description: Update stsInfo file with new information from NCBI
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
    {"gb", OPTION_STRING},
    {"verbose", OPTION_STRING},
    {NULL, 0}
};

struct sts
{
  struct sts *next;
  struct stsInfo2 *si;
  struct dnaSeq *fa;
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
  struct stsInfo2 *si;
  boolean gbSeq;
};

struct hash *stsHash;
struct hash *primerHash;
struct hash *nameHash;
struct hash *dbStsIdHash;
struct hash *gbAccHash;
struct hash *ucscIdHash;

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

void addElement(char *el, char ***array, int *count)
/* Add a new element to a array of elements */
{
  char *arrayCurr, arrayNew[1024];
  int sizeOne, size;
  char **cArray, **rArray=NULL, ***dArray;

  size = *count;
  arrayCurr = sqlStringArrayToString(*array, *count);
  sprintf(arrayNew, "%s%s,", arrayCurr, el);
  size++;
  dArray = array;
  /* if (*dArray) 
     freeMem(dArray); */
  sqlStringDynamicArray(arrayNew, &cArray, &sizeOne);
  assert(sizeOne == size);
  *count = size;
  AllocArray(rArray, size);
  CopyArray(cArray, rArray, size);
  *array = rArray;
}

void removeElement(char *el, char ***array, int *count)
/* Add a new element to a array of elements */
{
  char *arrayCurr, *arrayCurrDel, del[128];
  int sizeOne, size;
  char **cArray, **rArray=NULL, ***dArray;

  if (*count > 0)
    {
      size = *count;
      arrayCurr = sqlStringArrayToString(*array, *count);
      sprintf(del, "%s,", el);
      arrayCurrDel = replaceChars(arrayCurr, del, "");
      if (differentString(arrayCurr, arrayCurrDel))
	  size--;
      dArray = array;
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
  
void addElementInt(unsigned el, unsigned **array, int *count)
/* Add a new element to a array of elements */
{
  char *arrayCurr, arrayNew[1024];
  int sizeOne, size;
  unsigned *uArray, **dArray;

  size = *count;
  arrayCurr = sqlUnsignedArrayToString(*array, *count);
  sprintf(arrayNew, "%s%d,", arrayCurr, el);
  size++;
  dArray = array;
  if (*dArray)
    freeMem(dArray);
  sqlUnsignedDynamicArray(arrayNew, &uArray, &sizeOne);
  assert(sizeOne == size);
  *count = size;
  *array = uArray;
}

void addName(struct sts *s, char *name)
/* Add a name to a sts record */
{
    char *namesCurr, namesNew[1024];
  int sizeOne;

  /* See if it is a genBank record */
  if (checkGb(name))
    {
      /*namesCurr = sqlStringArrayToString(s->si->genbank, s->si->gbCount);
      sprintf(namesNew, "%s%s,", namesCurr, name);
      s->si->gbCount++;
      freeMem(&s->si->genbank);
      sqlStringDynamicArray(namesNew, &s->si->genbank, &sizeOne);
      assert(sizeOne == s->si->gbCount);*/
      addElement(name, &s->si->genbank, &s->si->gbCount);
    }
  else if (checkGdb(name))
    {
      /* namesCurr = sqlStringArrayToString(s->si->gdb, s->si->gdbCount);
      sprintf(namesNew, "%s%s,", namesCurr, name);
      s->si->gdbCount++;
      freeMem(&s->si->gdb);
      sqlStringDynamicArray(namesNew, &s->si->gdb, &sizeOne);
      assert(sizeOne == s->si->gbCount); */
      addElement(name, &s->si->gdb, &s->si->gdbCount);
    }    
  else
    {
      /* namesCurr = sqlStringArrayToString(s->si->otherNames, s->si->nameCount);
      sprintf(namesNew, "%s%s,", namesCurr, name);
      s->si->nameCount++;
      freeMem(&s->si->otherNames);
      sqlStringDynamicArray(namesNew, &s->si->otherNames, &sizeOne);
      assert(sizeOne == s->si->nameCount);*/
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
  int i, id;
  
  stsHash = newHash(20);
  dbStsIdHash = newHash(20);
  primerHash = newHash(20);
  nameHash = newHash(24);
  gbAccHash = newHash(16);
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
	  sprintf(name, "%d", si->identNo);
	  hashAdd(stsHash, name, s);
	  
	  /* Add ids to dbStsIdHash */
	  if (si->dbSTSid)
	    hashAddInt(dbStsIdHash, name, si->dbSTSid);
	  
	  /* Add sts records to ucscId hash */
	  if (si->dbSTSid)
	    {
	      sprintf(name, "%d", si->dbSTSid);
	      hashAdd(ucscIdHash, name, s);
	    }
	  for (i = 0; i < si->otherDbstsCount; i++)
	    {
	      sprintf(name, "%d", si->otherDbSTS[i]);
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
	      gb->si = si;
	      gb->gbSeq = FALSE;
	      hashAdd(gbAccHash, gb->acc, gb);
	    }
	  for (i = 0; i < si->gdbCount; i++) 
	    hashAdd(nameHash, si->gdb[i], s);
	  for (i = 0; i < si->nameCount; i++) 
	    hashAdd(nameHash, si->otherNames[i], s);
	  
	  /* Create primer info and add to hash */
	  AllocVar(p);
	  p->next = NULL;
	  p->dbStsId = si->dbSTSid;
	  p->left = cloneString(si->leftPrimer);
	  p->right = cloneString(si->rightPrimer);
	  p->dist = cloneString(si->distance);
	  p->ucscId = si->identNo;
	  sprintf(name, "%d", p->dbStsId);
	  hashAdd(primerHash, name, p);
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
	  gb->si = NULL;
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

void readDbstsNames(struct lineFile *daf)
/* Read in dbSTS names and create new stsInfo record, if necessary */
{
  struct sts *s, *s2;
  struct stsInfo2 *si;
  struct primer *p;
  char *words[4], *names[32], prefix[0], name[16];
  int dbstsId, ucscId, nameCount, i;

  while (lineFileChopNext(daf, words, 2))
    {
      dbstsId = sqlUnsigned(words[0]);
      /* Determine if this id is already being used */
      if (hashLookup(ucscIdHash, words[0]))
	s = hashMustFindVal(ucscIdHash, words[0]);
      else
	s = NULL;
      if (s != NULL)
	{
	  /* Determine if all of the names are recorded */
	  s->dbstsIdExists = TRUE;
	  nameCount = chopByChar(words[1], ';', names, ArraySize(names));
	  for (i = 0; i < nameCount; i++) 
	      if (!hashLookup(nameHash, names[i]))
		{
		  subChar(names[i],',',':');
		  addName(s, names[i]);
		  hashAdd(nameHash, names[i], s);
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
	      si->gbCount = 0;
	      si->gdbCount = 0;
	      si->nameCount = 0;
	      si->name = cloneString(names[0]);
	      if (checkGb(names[0]) || checkGdb(names[0]))
		addName(s, names[0]);
	      hashAdd(nameHash, names[0], s);
	      for (i = 1; i < nameCount; i++) 
		{
		  subChar(names[i], ',', ':');
		  addName(s, names[i]);
		  hashAdd(nameHash, names[i], s);
		}
	      si->dbSTSid = dbstsId;
	      si->leftPrimer = cloneString(p->left);
	      si->rightPrimer = cloneString(p->right);
	      si->distance = cloneString(p->dist);
	      si->otherDbstsCount = 0;
	      si->organism = cloneString("Homo sapiens");
	      si->sequence = 0;
	      si->otherUCSCcount = 0;
	      si->mergeUCSCcount = 0;
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
	      sprintf(name, "%d", s->si->identNo);
	      hashAdd(stsHash, name, s);
	      hashAddInt(dbStsIdHash, name, dbstsId); 
	      p->ucscId = s->si->identNo;
	    }
	}
    }     
				 
}

void readDbstsPrimers(struct lineFile *dsf)
/* Read in primer and organism info from dbSTS.sts */
{
  struct primer *p;
  struct sts *s;
  char *words[8], *name;
  int dbStsId, newId;

  while (lineFileChopNextTab(dsf, words, 8))
    {
      dbStsId = sqlUnsigned(words[0]);
      if (hashLookup(ucscIdHash, words[0]))
	s = hashMustFindVal(ucscIdHash, words[0]);
      else
	s = NULL;
      if (hashLookup(primerHash, words[0]))
	p = hashMustFindVal(primerHash, words[0]);
      else
	p = NULL;
      /* If primer record already recorded */
      if (p != NULL)
	{
	  /* If linked to ucsc record and record not mapped */
	  if ((s != NULL) && (!s->mapped) && (s->si->dbSTSid == dbStsId))
	    {
	      freez(&(p->left));
	      p->left = cloneString(words[1]);
	      freez(&(p->right));
	      p->left = cloneString(words[2]);
	      freez(&(p->dist));
	      p->left = cloneString(words[3]);
	    }
	}
      else if ((s == NULL) && (sameString(words[7], "Homo sapiens\0")) && (hashLookup(nameHash, words[4])))
	    {
	      s = hashMustFindVal(nameHash, words[4]);

	      AllocVar(p);
	      p->next = NULL;
	      p->dbStsId = dbStsId;
	      p->left = cloneString(words[1]);
	      p->right = cloneString(words[2]);
	      p->dist = cloneString(words[3]);
	      p->ucscId = s->si->identNo;
	      hashAdd(primerHash, words[0], p);

	      if (s->si->dbSTSid == 0)
		{
		  s->si->dbSTSid = dbStsId;
		  s->si->leftPrimer = cloneString(p->left);
		  s->si->rightPrimer = cloneString(p->right);
		  s->si->distance = cloneString(p->dist);
		  s->dbstsIdExists = TRUE;
		}
	      else 
		{
		  newId = sqlUnsigned(words[0]);
		  addElementInt(newId, &s->si->otherDbSTS, &s->si->otherDbstsCount);
		}
	      hashAdd(ucscIdHash, words[0], s);
	    }
      else if ((s == NULL) && (sameString(words[7], "Homo sapiens\0")))
	{
	  AllocVar(p);
	  p->next = NULL;
	  p->dbStsId = dbStsId;
	  p->left = cloneString(words[1]);
	  p->right = cloneString(words[2]);
	  p->dist = cloneString(words[3]);
	  p->ucscId = 0;
	  name = cloneString(words[0]);
	  hashAdd(primerHash, name, p);
	}
    }
}

void writeOut(FILE *of, FILE *opf, FILE *off)
/* Write out update files for info, primers, and sequences */
{
  struct sts *s;
  struct stsInfo2 *si;
  int i;

  slReverse(&sList);
  for (s = sList; s != NULL; s = s->next)
    {
      si = s->si;
      if ((s->dbstsIdExists) || (si->dbSTSid == 0) || (si->dbSTSid >= 1000000))
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
	  
	  /*stsInfo2TabOut(s->si, of);*/
	  if (differentString(s->si->leftPrimer, "\0"))
	    fprintf(opf, "%d\t%s\t%s\t%s\t%d\n", s->si->dbSTSid, s->si->leftPrimer,
		    s->si->rightPrimer, s->si->distance, s->si->identNo);
	}
    }
}

int main(int argc, char *argv[])
{
  struct lineFile *sif, *asf, *dsf, *daf, *dff, *gbf;
  FILE *of, *opf, *off;
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
 asf = lineFileOpen(argv[2], TRUE);
 dsf = lineFileOpen(argv[3], TRUE);
 daf = lineFileOpen(argv[4], TRUE);
 dff = lineFileOpen(argv[5], TRUE);

 sprintf(filename, "%s.info", argv[6]);
 of = mustOpen(filename, "w");
 sprintf(filename, "%s.primers", argv[6]);
 opf = mustOpen(filename, "w");
 sprintf(filename, "%s.fa", argv[6]);
 off = mustOpen(filename, "w");

 /* Read in current stsInfo file */
 verbose(1, "Reading current stsInfo file\n");
 readStsInfo(sif);

 /* Read in genbank accessions that have sequences */ 
 if (gbName)
   {
     verbose(1, "Reading genbank accession file\n");
     readGbAcc(gbf);
   }

 /* Read in primer and organism information from dbSTS.sts */
 verbose(1, "Reading current dbSTS.sts file\n");
 readDbstsPrimers(dsf);

 /* Read in names from dbSTS.alias and create new stsInfo records if needed */
 verbose(1, "Reading current dbSTS.aliases file\n");
 readDbstsNames(daf);

 /* Print out the new files */
 writeOut(of, opf, off);

 lineFileClose(&asf);
 lineFileClose(&dsf);
 lineFileClose(&daf);
 lineFileClose(&dff);
 if (gbName)
   lineFileClose(&gbf);   
 fclose(of);
 fclose(opf);
 fclose(off);

 return(0);
}
