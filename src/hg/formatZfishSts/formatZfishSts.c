/*
  File: formatZfishSts.c
  Author: Rachel Harte
  Date: 2/28/2005
  Description: Bring together and format sources of STS data for Zebrafish from 
  NCBI and ZFIN.
*/

#include "common.h"
#include "linefile.h"
#include "memalloc.h"
#include "hash.h"
#include "jksql.h"
#include "fuzzyFind.h"
#include "portable.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "psl.h"
#include "fa.h"
#include "psl.h"
#include "options.h"

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"verbose", OPTION_STRING},
    {NULL, 0}
};

struct sts 
{
char *id;
char *name;
char *acc;
};

struct zfin 
{
char *zfinId;
char *acc;
char *name;
char *zfAlias;
char *panel;
};

struct uniStsMap 
{
struct uniStsMap *next;
char *stsId;
char *name;
char *panel;
double lod;
};

struct hash *stsHash;
struct hash *zfinHash;
struct hash *zfinMarkHash;
struct hash *aliasHash;
struct uniStsMap *mapList = NULL;

void readStsMarkers(struct lineFile *stf)
/* Read in STS markers file from UniSTS*/
{
struct sts *s = NULL;
char *words[52], *idName;

stsHash = newHash(20);

/* Read in all rows */
while (lineFileChopTab(stf, words))
    {
    /* get UniSTS ID and name and concatenate to make hash key */
    /* Change name to upper case */
    touppers(words[4]);
    idName = addSuffix(words[0], words[4]);
    AllocVar(s);
    s->id = cloneString(words[0]);
    s->name = cloneString(words[4]);
    s->acc = cloneString(words[6]);
    hashAdd(stsHash, idName, s);
    }
}

void readGbZfin (struct lineFile *gzf)
/* Read in Genbank IDs and ZFIN IDs */
{
char *words[24], *acc = NULL, newAcc[10];
struct zfin *gz;

zfinMarkHash = newHash(20);
zfinHash = newHash(20);

while (lineFileChopTab(gzf, words) )
    {
    if ((gz = hashRemove(zfinMarkHash, words[1]) ) != NULL)
        {
        acc = gz->acc;
        /* add new accession */
        if (acc != NULL) 
            {
            safef(newAcc, sizeof(newAcc), ";%s", words[0]);
            addSuffix(acc, newAcc);
            }
        gz->acc = acc;
        }
    else 
        {
        AllocVar(gz);
        gz->zfinId = cloneString(words[0]);
        gz->name = cloneString(words[1]);
        gz->acc = cloneString(words[2]);
        gz->panel = NULL;
        gz->zfAlias = NULL;
        }
    /* change name to upper case */
    touppers(words[1]);
    /* add record to hash with marker name as key */
    hashAdd(zfinMarkHash, words[1], gz);
    /* add record to hash with ZFIN ID as key */
    hashAdd(zfinHash, words[0], gz);
    }
}

void readUnistsNames(struct lineFile *uaf)
/* Read in UniSTS alias names */
{
char *words[4], *names[64];
int nameCount, i;

aliasHash = newHash(20);
while (lineFileChopNext(uaf, words, 2) )
    {
    nameCount = chopByChar(words[1], ';', names, ArraySize(names));
    /* Add this entry to a hash keyed by id, hash key must be a string */
    hashAdd(aliasHash, words[0], names);
    }
}

void readZfinAliases(struct lineFile *zaf)
/* Read in ZFIN IDs and aliases */
{
char *words[4];
struct zfin *zf = NULL;

while (lineFileChopTab(zaf, words))
    {
    /* need to add to a hash keyed by ZFIN ID */
    if (words[0] != NULL) 
        {
        zf = hashRemove(zfinMarkHash, words[0]);
        if (zf != NULL)
            zf->zfAlias = cloneString(words[1]);
        }
        touppers(words[0]);
        hashAdd(zfinMarkHash, words[0], zf);
    }
}

void readZfinMapping(struct lineFile *zmf)
/* Read in ZFIN IDs, mapping panels and marker names */
{
char *words[8], *name = NULL;
struct zfin *zf = NULL;
while (lineFileChopTab(zmf, words) )
    {
    /* get ZFIN Id, name and panel */
    /* check if this ZFIN Id exists already in hash */
    /* set up hash of ZFIN Id and name already added */
    name = hashFindVal(zfinHash, words[0]);    
    if (name != NULL) 
        {
        /* change name to upper case and then check for it in hash */
        touppers(name);
        zf = hashRemove(zfinMarkHash, name);
        /* if found */
        if (zf != NULL) 
            {
            if (zf->panel == NULL) 
               zf->panel = cloneString(words[2]);
            }
        }
    /* If no record of this marker, create one */
    else 
        {
        AllocVar(zf);
        name = cloneString(words[1]);
        touppers(name);
        zf->zfinId = cloneString(words[0]);
        zf->name = cloneString(words[1]);
        zf->acc = NULL;
        zf->panel = cloneString(words[2]);
        }
    }
    /* Add STS marker record to hash */
    hashAdd(zfinMarkHash, name, zf);
}

void readUniStsPanels(struct lineFile *mpf) 
/* Read in info from UniSTS marker panels file for zebrafish */
{
char *words[5], *wordArray[5], *name = NULL, *panel = NULL;
int arraySize = 5, wordNum;
char *retStart;
int retSize;
char *ch = "#";
struct uniStsMap *map = NULL;

while (lineFileNext(mpf, &retStart, &retSize) )
    {
    /* get panel name */
    if (retStart != NULL) 
        {
        if (startsWith(ch, retStart))
            {
            wordNum = chopByWhite(retStart, wordArray, arraySize);
            if (wordNum != 0) 
                {
                if (sameString(wordArray[1], "Map") && sameString(wordArray[2], "Name:"))
                    panel = cloneString(wordArray[3]);
                }
            }
          
        else 
            {
            wordNum = chopTabs(retStart, words);
            if (wordNum != 0) 
                {
                AllocVar(map);
                /* if the first field is not empty, it is an id */
                if (!sameString(words[0], ""))
                    map->stsId = cloneString(words[0]);
                else
                    map->stsId = NULL;
                map->name = cloneString(words[1]);
                if (panel != NULL) 
                    map->panel = cloneString(panel);
                else 
                    map->panel = panel;
                map->lod = sqlDouble(words[3]);
                /* add this map struct for a marker to a singly linked list */
                if (mapList == NULL)
                    {
                    map->next = NULL;
                    mapList = map;
                    }
                else
                    slAddTail(mapList, map);
                }
            }
       }
    }
}

void *checkCompositeKey (struct hash *hash, char *compVal)
/* check value against a composite key e.g. id and name concatenated */
{
struct hashEl *hashList;
struct hashEl *hashEl;

hashList = hashElListHash(hash);
hashEl = slPopHead(hashList);
if (startsWith(compVal, hashEl->name)) 
    return hashEl->val;
else if(endsWith(hashEl->name, compVal))
    return hashEl->val;
return NULL;
}

void writeOut(FILE *sto)
/* Write out STS marker information required into an output file */
{
struct uniStsMap *mapEl;
struct sts *st = NULL;
struct hashEl *hashEl = NULL;
struct zfin *zf = NULL;
char *aliases, *uniStsId = NULL, *name = NULL;

while ((mapEl = slPopHead(mapEl)) != NULL)
    {
    if (mapEl->stsId != NULL) 
        {
        /* check for this in stsHash keys, composite of id and name */
        printf("the map id is %s\n", mapEl->stsId);
        st = checkCompositeKey(stsHash, mapEl->stsId);
        }
    else if (mapEl->name != NULL)
        {
        st = checkCompositeKey(stsHash, mapEl->name);
        }
    if (st != NULL)
        {
        /* get id, name and acc */
        /* use name to find zfin id - the ZFIN IDs may have "_" or "." */
        /* and extensions in the name */
        uniStsId = cloneString(st->id);
        name = cloneString(st->name);
        touppers(name);
        zf = checkCompositeKey(zfinMarkHash, name);
        if (zf != NULL)
             {}/* can get zfin ID, acc, name, zfAlias, and panel */
         // check that panels match
        }
    /* use stsId or name to check the alias Hash */
        if (uniStsId != 0)
           aliases = hashFindVal(aliasHash, uniStsId);
         // add zfin alias too if there is one
    }
}

int main(int argc, char *argv[])
{
struct lineFile *stf, *gzf, *uaf, *zaf, *zmf, *mpf;
FILE *sto;
char filename[256];
int verb = 0;

verboseSetLevel(0);
optionInit(&argc, argv, optionSpecs);
if (argc < 1)
    {
    fprintf(stderr, "USAGE: formatZfishSts [-verbose=<level> -gb=<file>] <UniSTS file> <genbank.txt> <UniSTS aliases> <ZFIN aliases> <ZFIN mappings.txt> <dbSTS map panels>\n");
    return 1;
    }
verb = optionInt("verbose", 0);
verboseSetLevel(verb);

stf = lineFileOpen(argv[1], TRUE);
gzf = lineFileOpen(argv[2], TRUE);
uaf = lineFileOpen(argv[3], TRUE);
zaf = lineFileOpen(argv[4], TRUE);
zmf = lineFileOpen(argv[5], TRUE);
mpf = lineFileOpen(argv[6], TRUE);

sprintf(filename, "%s.format", argv[6]);
sto = mustOpen(filename, "w");

/* Read in STS markers file */
verbose(1, "Reading STS markers file\n");
readStsMarkers(stf);

/* Read in genbank accessions that have ZFIN IDs */

verbose(1, "Reading genbank accession and ZFIN ID file\n");
readGbZfin(gzf);

/* Read in UniSTS alias information */
verbose(1, "Reading UniSTS aliases file\n");
readUnistsNames(uaf);

/* Read in ZFIN marker IDs with old names */
verbose(1, "Reading STS marker panels file\n");
readZfinAliases(zaf);

/* Read in ZFIN Ids, maps and marker names from mapping.txt */
verbose(1, "Reading mapping.txt file\n");
readZfinMapping(zmf);

/* Read in mapping panel information for STS markers from UniSTS*/
verbose(1, "Reading STS marker panels file\n");
readUniStsPanels(mpf);

/* Print out the new file */
verbose(1, "Creating output file\n");
writeOut(sto);

lineFileClose(&stf);
lineFileClose(&gzf);
lineFileClose(&uaf);
lineFileClose(&zaf);
lineFileClose(&zmf);
lineFileClose(&mpf);
fclose(sto);

return(0);
}
