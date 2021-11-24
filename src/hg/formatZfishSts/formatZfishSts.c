/* Copyright (C) 2005 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

/*
  File: formatZfishSts.c
  Author: Rachel Harte
  Date: 4/11/2005
  Description: Bring together and format sources of STS data for Zebrafish from
  NCBI and ZFIN. There are sometimes multiple mappings of markers in the same
  panel to different positions either on the same or different chromosome. The 
  zebrafish mapping panels information from http://www.zfin.org have replicate 
  entries so sort and uniq this file before using as input.
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

#define NUMPANELS 6
#define NUMEXT 15
#define NUMACCS 10
#define MAXALIASES 32
#define MAXIDS	8
#define NUMPOS 5

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

struct panel
{
char *name;
char *panel;
char *chrom[NUMPOS];
double pos[NUMPOS];
char *units;
};

struct zfin
{
char *zfinId;
char *acc;
char *name;
char *zfAlias;
struct panel **panelArray;
};

struct alias
{
char *names[MAXALIASES];
};

struct aliasId
{
char *ids[MAXIDS];
};

struct hash *stsHash;
struct hash *stsIdHash;
struct hash *zfinIdHash;
struct hash *zfinMarkerHash;
struct hash *aliasHash;
struct hash *aliasByNameHash;

char *markerPanels[] = {"GAT", "HS", "LN54", "MGH", "MOP", "T51"};
char *extensions[] = {".t7", ".sp6", ".y", ".y1", ".ya", ".yb", ".yc", ".z", ".za", ".zb", ".zc", ".x1", ".y1", ".r1", ".s1"};
struct hashEl *addHashElUnique(struct hash *hash, char *name, void *val)
/* Adds new element to hash table. If not unique, remove old element */
/* before adding new one. Avoids replicate entries in the table */
{
if (hashLookup(hash, name) != NULL)
    /* if item in hash already, remove first */
    hashRemove(hash, name);
/* then add element to hash table */
return hashAdd(hash, name, val);
}

void readStsMarkers(struct lineFile *stf)
/* Read in STS markers information from UniSTS markers file */
{
struct sts *s = NULL, *s2 = NULL;
char *words[52], *id = NULL, *name = NULL;
char *nameLower = NULL, *acc = NULL;

stsHash = newHash(16);
stsIdHash = newHash(16);

/* Read in all rows */
while (lineFileChopTab(stf, words))
    {
    id = cloneString(words[0]);
    acc = cloneString(words[6]);
    name = cloneString(words[4]);
    nameLower = cloneString(words[4]);
    /* Change name to upper case */
    touppers(name);
    /* allocate memory for sts structs */
    AllocVar(s);
    AllocVar(s2);
    s->id = cloneString(id);
    s2->id = cloneString(id);
    s->name = cloneString(nameLower);
    s2->name = cloneString(nameLower);
    if (sameString(acc, "-"))
        {
        s->acc = NULL;
        s2->acc = NULL;
        }
    else
        {
        s->acc = cloneString(acc);
        s2->acc = cloneString(acc);
        }
    /* add UniSTS info to hash keyed by name */
    addHashElUnique(stsHash, name, s);
    /* add UniSTS info to hash keyed by UniSTS ID, key must be a string */
    addHashElUnique(stsIdHash, id, s2);
    }
}

void readZfinMapping(struct lineFile *zmf)
/* Read in ZFIN IDs and mapping panel data */
{
char *words[6], *name = NULL, *nameLower = NULL, *id = NULL, *panel = NULL;
char *chrom = NULL, *units = NULL;
double pos;
int i = 0, j, index = -1, k;
struct zfin *zf = NULL;
struct panel *p = NULL;
boolean foundPos = FALSE;

zfinMarkerHash = newHash(16);
zfinIdHash = newHash(16);

while (lineFileChopTab(zmf, words) )
    {
    /* get ZFIN Id, name and panel */
    id = cloneString(words[0]);
    name = cloneString(words[1]);
    nameLower = cloneString(words[1]);
    panel = cloneString(words[2]);
    chrom = cloneString(words[3]);
    pos = sqlDouble(words[4]);
    units = cloneString(words[5]);
    /* check if this name exists already in hash */
    touppers(name);
    zf = hashFindVal(zfinMarkerHash, name);
    if (zf == NULL)    
        {
        /* allocate memory for zfin struct if not in hash */
        AllocVar(zf);
        /* find panel and add marker information */
        /* first initialize */
        zf->panelArray = needMem(sizeof(struct panel*) * NUMPANELS);
        for (i = 0; i < NUMPANELS; i++)
            {
            zf->panelArray[i] = NULL;
            }
     
        zf->zfinId = cloneString(id);
        zf->name = cloneString(nameLower);
        zf->acc = NULL;
        zf->zfAlias = NULL;
        }
    /* find index number for this panel */
    for (j = 0; j < NUMPANELS; j++)
        {
        if (sameString(panel, markerPanels[j]))
           index = j;
        }
    if (zf->panelArray[index] == NULL)
        {
        /* allocate memory for panel struct for panel data */
        AllocVar(p);
        /* initialize chrom and pos arrays */
        for (k = 0; k < NUMPOS; k++)
            {
            p->chrom[k] = NULL;
            p->pos[k] = -1;
            }
 
        p->name = cloneString(nameLower);
        p->panel = cloneString(panel);
        p->units = cloneString(units);
        } 
    else
       p = zf->panelArray[index];
    foundPos = FALSE;
    for (k = 0; k < NUMPOS && (!foundPos); k++)
        { 
        if (p->chrom[k] == NULL)
            {
            if (!sameString(p->name, nameLower))
                fprintf(stderr, "The entry in position %d does not match %s\n", index, nameLower);
            else 
                {
                p->chrom[k] = cloneString(chrom);
                p->pos[k] = pos;
                foundPos = TRUE;
                }
            }
        }
    zf->panelArray[index] = p;
        
    /* name is already in upper case, add zfin to zfinMarkerHash */
    addHashElUnique(zfinMarkerHash, name, zf);
    /* if new ZFIN struct add the name to the zfinIdHash keyed by ZFIN ID */
    if (hashFindVal(zfinIdHash, id) == NULL)
        addHashElUnique(zfinIdHash, id, nameLower);
    }
}

void readGbZfin (struct lineFile *gzf)
/* Read in Genbank IDs and ZFIN IDs */
{
char *words[24], *acc = NULL, addAcc[20], *name = NULL, *newAcc = NULL;
struct zfin *gz = NULL;

while (lineFileChopTab(gzf, words) )
    {
    /* copy name and change to upper case */
    name = cloneString(words[1]);
    touppers(name);
    if ((gz = hashFindVal(zfinMarkerHash, name) ) != NULL)
        {
        acc = gz->acc;
        /* add new accession */
        if (acc != NULL)
            {
            safef(addAcc, sizeof(addAcc), ",%s", words[2]);
            newAcc = addSuffix(acc, addAcc);
            gz->acc = cloneString(newAcc);
            }
        else
            gz->acc = cloneString(words[2]);
        /* add structure back to hash */
        addHashElUnique(zfinMarkerHash, name, gz);
        }
    else 
        fprintf(stderr, "The marker, %s, with ZFIN ID, %s, is not found in the mapping panels \n", words[1], words[0]);
    }
}

void readUnistsAliases(struct lineFile *uaf)
/* Read in UniSTS alias names */
{
char *words[4], *name = NULL, *id = NULL;
int nameCount, i = 0, j, k, l = 0;
struct alias *aliases = NULL, *testAlias = NULL;
struct aliasId *aliasId = NULL;
boolean idAdded = FALSE;

/* store marker names keyed by ID and marker UniSTS IDs indexed by name. */
/* there can be many names per ID and many IDs per name */

aliasHash = newHash(16);
aliasByNameHash = newHash(18);

while (lineFileChopNext(uaf, words, 2) )
    {
    id = cloneString(words[0]);
    /* allocate memory for alias structure */
    AllocVar(aliases);
    nameCount = chopByChar(words[1], ';', aliases->names, ArraySize(aliases->names));
    for (i = 0; i < nameCount; i++)
        {
        /* add alias to hash keyed by alias, value is UniSTS ID. */
        /* alias can have multiple IDs */
        idAdded = FALSE;
        name = cloneString(aliases->names[i]);
        /* get existing aliasId struct from hash for this name if exists */
        /* else allocate memory for an aliasId struct */
        /* and initialize array of ids */
        if ((aliasId = hashFindVal(aliasByNameHash, name)) == NULL)
           {
           AllocVar(aliasId);
           for (j = 0; j < MAXIDS; j++)
               {
               aliasId->ids[j] = NULL;
               }
          }
        for (k = 0; (k < MAXIDS) && !idAdded; k++)
            {  
            /* find next empty slot in array to add UniSTS ID */
            if ((aliasId->ids[k] == NULL) && (!idAdded))
               {
               aliasId->ids[k] = id;  
               idAdded = TRUE; 
               }
            }     
        addHashElUnique(aliasByNameHash, name, aliasId);
        }
    /* add entry of "0" to signify the end of the array */
    aliases->names[i] = "0";  
    /* Add this entry to a hash keyed by UniSTS id, hash key must be a string */
    addHashElUnique(aliasHash, id, aliases);
    }
}

void readZfinAliases(struct lineFile *zaf)
/* read in ZFIN IDs and old marker names (aliases), there can be */
/* more than one alias per ID. add to zfin aliases in zfinMarkerHash */
/* and to the list of aliases in the aliasHash */
{
char *words[4], *name = NULL, *zfId = NULL, *alias = NULL, *al = NULL, addAlias[64], *newAlias = NULL;
struct zfin *zf = NULL;
char **aliasArray;

while (lineFileChopTab(zaf, words))
    {
    /* need to add to a hash keyed by ZFIN ID */
    if (words[0] != NULL)
        {
        /* find current name by searching on ZFIN ID in zfinIdHash */
        zfId = cloneString(words[0]);
        al = cloneString(words[1]);
        name = hashFindVal(zfinIdHash, zfId);
        if (name != NULL)
            {
            /* names are in lower case so change to upper to use as hash key */
            touppers(name); 
            zf = hashFindVal(zfinMarkerHash, name);
            if (zf != NULL)
                {
                if (zf->zfAlias != NULL)
                    {
                    alias = cloneString(zf->zfAlias);      
                    safef(addAlias, sizeof(addAlias), ",%s", al);
                    newAlias = addSuffix(alias, addAlias);
                    zf->zfAlias = cloneString(newAlias);
                    }
                else
                    zf->zfAlias = cloneString(al);
                /* add back to zfinMarkerHash using current name as key */
                addHashElUnique(zfinMarkerHash, name, zf);
                }
            }
        }
    }
}

void *addExtensionAndSearch(char *name, struct hash *hash, boolean alias)
{
char *addName = NULL, *newName = NULL, *nameLower = NULL;
void *result = NULL;
boolean found = FALSE;
int i;

addName = cloneString(name);
if (alias)
    {
    nameLower = cloneString(name);
    touppers(addName);
    }

for (i = 0; (i < NUMEXT) && (!found); i++)
    {
    newName = NULL;
    newName = addSuffix(addName, extensions[i]);
    /* for alias, check the lower case name */
    if (alias && ((result = hashFindVal(hash, newName)) != NULL) )
        found = TRUE;
    /* change name to upper case and check in hash */
    touppers(newName);
    if (!found && (result = hashFindVal(hash, newName)) != NULL)
       found = TRUE;
    else if (!found)
        {
        /* remove the suffix after the last '.' and compare */
        chopSuffix(addName);
        if (alias)
            chopSuffix(nameLower);
        if (!sameString(name, addName) && 
              (result = hashFindVal(hash, addName)) != NULL)
            found = TRUE; 
        else if (alias)  /* check also lower case name */
            {
            if (!sameString(name, nameLower) &&
                (result = hashFindVal(hash, nameLower)) != NULL)
                found = TRUE; 
            }
        }
    }
    if (found)
        return result;
    else
        return NULL;
}

void printMarkers(FILE *sto, struct sts *s, struct zfin *zf, char **aliases)
/* print out formatted STS markers data */
{
int i = 0, j = 0, p;
/* for each marker panel, write a separate line */
for (p = 0; p < NUMPANELS; p++)
    {
    if (zf->panelArray[p] != NULL)
        {
        if (s != NULL)
            {
            fprintf(sto, "%s\t", s->id);
            if (s->acc != NULL)
                fprintf(sto, "%s\t", s->acc);
            else
                fprintf(sto, "\t");
            fprintf(sto, "%s\t", s->name);
            }
        else
            fprintf(sto, "\t\t\t");
        fprintf(sto,"%s\t", zf->name);
        fprintf(sto, "%s\t", zf->zfinId);
        if (aliases != NULL)
            {
            i = 0;
            while(!sameString(aliases[i], "0"))
                {
                /* print out aliases */
                fprintf(sto, "%s,", aliases[i]);
                i++;
                }
            fprintf(sto, "\t");
            }
        else 
            fprintf(sto, "\t"); 
        if (zf->zfAlias != NULL)
            fprintf(sto, "%s\t", zf->zfAlias);    
        else
            fprintf(sto, "\t");            
        if (zf->acc != NULL)
            fprintf(sto, "%s\t", zf->acc);
        else
            fprintf(sto, "\t");
        /* print out marker panel information */  
        fprintf(sto, "%s\t", zf->panelArray[p]->panel);
        for (j = 0; j < NUMPOS && (zf->panelArray[p]->chrom[j] != NULL); j++)
            {
            fprintf(sto, "chr%s,", zf->panelArray[p]->chrom[j]);
            }
        fprintf(sto, "\t");        
        for (j = 0; j < NUMPOS && (zf->panelArray[p]->pos[j] != -1); j++)
            {
            fprintf(sto, "%.2lf,", zf->panelArray[p]->pos[j]);
            }
        fprintf(sto, "\t");
        fprintf(sto, "%s\n", zf->panelArray[p]->units);
        
        fflush(sto);
        }
    }
}

void writeOut(FILE *sto)
/* write out STS marker information to a file */
{
struct hashEl *mapList = NULL, *mapEl = NULL, *stsList = NULL, *stsEl = NULL;
struct hashEl *aliasList = NULL, *aliasEl = NULL;
struct zfin *zfEl = NULL;
struct sts *sts = NULL, *s = NULL, *stsVal = NULL;
struct alias *aliases = NULL;
struct aliasId *aliasId = NULL;
char *alias = NULL, *id = NULL, *al = NULL, *addName = NULL, *newName = NULL;
boolean found = FALSE, aliasFound = FALSE;
int i = 0, j = 0;
char **aliasNames = NULL;

mapList = hashElListHash(zfinMarkerHash);
stsList = hashElListHash(stsHash);
aliasList = hashElListHash(aliasByNameHash);

fprintf(sto, "UniSTS ID\tUniSTS Accession\tUniSTS Name\tZFIN name\tZFIN ID\tUniSTS Alias\tZFIN Alias\tZFIN Gb Acc\tPanel\tChrom\tPosition\tUnits\n \n");

if (mapList != NULL)
    {
    /* walk through list of hash elements */
    for (mapEl = mapList; mapEl != NULL; mapEl = mapEl->next)
        {
        found = FALSE;
        zfEl = (struct zfin *)mapEl->val;
       /* check aliases in all combinations */
       /* if this name is a key in the stsHash */
        if ((sts = hashFindVal(stsHash, mapEl->name)) != NULL)
            found = TRUE;
        /* check by ZFIN alias instead */
        else if ((zfEl->zfAlias != NULL) && (!found)) 
            {
            alias = cloneString(zfEl->zfAlias);
            touppers(alias);
            stsVal = NULL;
            if ((stsVal = hashFindVal(stsHash, alias)) != NULL)
                {
                found = TRUE;
                sts = stsVal;
                }
            }
        /* check stsHash for name with extension or remove extension from zfin name */
        else if (!found)
            {
            sts = (struct sts *)addExtensionAndSearch(mapEl->name, stsHash, FALSE);
            if (sts != NULL)
               found = TRUE;
            }
        /* check if this is an alias in alias hash keyed by alias names */
        /* need to get list of aliases in all cases but if sts ID not found */
        /* can find it here */
        /* mapEl->name is in upper case, so get lower case, zfinEl->name */
        aliasFound = FALSE; 
        if (((aliasId = hashFindVal(aliasByNameHash, mapEl->name))!=NULL) || 
             ((aliasId = hashFindVal(aliasByNameHash, zfEl->name))!=NULL)) 
            aliasFound = TRUE;
        else /* alias is not found, try different extensions */
            {
            aliasId = (struct aliasId *)addExtensionAndSearch(zfEl->name, aliasByNameHash, TRUE);
            if (aliasId != NULL)
               aliasFound = TRUE;
            }
        if (!found && sts != NULL)
           sts = NULL;
        /* all UniSTS IDs have an entry in sts and alias files so the */
        /* UniSTS ID in the sts found will also be in aliases IDs list */

        if (aliasFound)
           {
           /* go through each ID in the ids array and get sts struct */
           /* and the list of aliases */
           for (j = 0; j < MAXIDS; j++)
               {     
               if (aliasId->ids[j] != NULL)
                   {    
                   /* get sts and alias names */
                   sts = hashFindVal(stsIdHash, aliasId->ids[j]);
                   aliases = hashFindVal(aliasHash, aliasId->ids[j]); 
                   printMarkers(sto, sts, zfEl, aliases->names);
                   }
               }
           }
        else
           printMarkers(sto, sts, zfEl, NULL); 
        }
    }
    hashElFreeList(&mapList);
    hashElFreeList(&stsList);
    hashElFreeList(&aliasList);
}

int main(int argc, char *argv[])
{
struct lineFile *stf,*zmf, *gzf, *uaf, *zaf;
FILE *sto; 
char filename[256];
int verb = 0;

verboseSetLevel(0);
optionInit(&argc, argv, optionSpecs);
if (argc < 7)
    {
fprintf(stdout, "USAGE: formatZfishSts [-verbose=<level>] <UniSTS file> <genbank.txt> <UniSTS aliases> <ZFIN aliases> <ZFIN mappings.txt> <dbSTS map panels>\n");
    return 1;
    }
verb = optionInt("verbose", 0);
verboseSetLevel(verb);

stf = lineFileOpen(argv[1], TRUE);
zmf = lineFileOpen(argv[2], TRUE);
gzf = lineFileOpen(argv[3], TRUE);
uaf = lineFileOpen(argv[4], TRUE);
zaf = lineFileOpen(argv[5], TRUE);

stderr = mustOpen("/cluster/bluearc/danRer1/stsMarkers/formatOut/error.log", "w");
sprintf(filename, "%s.format", argv[6]);
sto = mustOpen(filename, "w");

/* Read in STS markers file */
verbose(1, "Reading STS markers file\n");
readStsMarkers(stf);

/* Read in ZFIN Ids, maps and marker names from mapping.txt */
verbose(1, "Reading mapping.txt file\n");
readZfinMapping(zmf);

/* Read in genbank accessions that have ZFIN IDs */
verbose(1, "Reading genbank accession and ZFIN ID file\n");
readGbZfin(gzf);

/* Read in UniSTS alias information */
verbose(1, "Reading UniSTS aliases file\n");
readUnistsAliases(uaf);
/* Read in ZFIN marker IDs with old names */
verbose(1, "Reading ZFIN marker IDs and old names - aliases from a file\n");
readZfinAliases(zaf);

/* Print out the STS marker information to a file */
verbose(1, "Creating output file\n");
writeOut(sto);

lineFileClose(&stf);
lineFileClose(&zmf);
lineFileClose(&gzf);
lineFileClose(&uaf);
lineFileClose(&zaf);
fclose(sto);
fclose(stderr);

freeHash(&stsHash);
freeHash(&stsIdHash);
freeHash(&zfinMarkerHash);
freeHash(&zfinIdHash);
freeHash(&aliasByNameHash);
freeHash(&aliasHash);
return(0);
}

