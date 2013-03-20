/* gff - routines to read many types of gff and gtf files
 * and turn them into a relatively easy to deal with form
 * in memory.
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */
#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "gff.h"
#include "obscure.h"
#include "dystring.h"


void gffGroupFree(struct gffGroup **pGroup)
/* Free up a gffGroup including lineList. */
{
struct gffGroup *group;
if ((group = *pGroup) != NULL)
    {
    slFreeList(&group->lineList);
    freez(pGroup);
    }
}

void gffGroupFreeList(struct gffGroup **pList)
/* Free up a list of gffGroups. */
{
struct gffGroup *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gffGroupFree(&el);
    }
*pList = NULL;
}


void gffFileFree(struct gffFile **pGff)
/* Free up a gff file. */
{
struct gffFile *gff;
if ((gff = *pGff) != NULL)
    {
    freeMem(gff->fileName);
    freeHash(&gff->seqHash);
    freeHash(&gff->sourceHash);
    freeHash(&gff->featureHash);
    freeHash(&gff->groupHash);
    freeHash(&gff->geneIdHash);
    freeHash(&gff->strPool);
    slFreeList(&gff->lineList);
    slFreeList(&gff->seqList);
    slFreeList(&gff->sourceList);
    slFreeList(&gff->featureList);
    slFreeList(&gff->geneIdList);
    gffGroupFreeList(&gff->groupList);
    freez(pGff);
    }
}

static char *gffFileGetStr(struct gffFile *gff, char *str)
/* get a string from the string pool */
{
return hashStore(gff->strPool,  str)->name;
}

int gffLineCmp(const void *va, const void *vb)
/* Compare two gffLines. */
{
const struct gffLine *a = *((struct gffLine **)va);
const struct gffLine *b = *((struct gffLine **)vb);
int diff;

/* for overlaping starts, sort by end, genePredFromGroupedGtf() depends on
 * this */
diff = strcmp(a->seq, b->seq);
if (diff == 0)
    diff = a->start - b->start;
if (diff == 0)
    diff = a->end - b->end;
return diff;
}


static void gffSyntaxError(char *fileName, int line, char *msg)
/* Complain about syntax error in GFF file. */
{
errAbort("%s Bad line %d of %s:\n", msg, line, fileName);
}

static char *gffTnName(char *seqName, char *groupName)
/* Make name that encorperates seq and group names.... */
{
static struct dyString *nameBuf = NULL;
if (nameBuf == NULL)
    nameBuf = dyStringNew(0);
dyStringClear(nameBuf);
if (startsWith("gene-", groupName))
    groupName += 5;
if (startsWith("cc_", groupName))
    groupName += 3;
dyStringAppend(nameBuf, groupName);

return nameBuf->string;
}

static boolean isGtfGroup(char *group)
/* Return TRUE if group field looks like GTF */
{
if (findWordByDelimiter(group, ' ', "gene_id") == NULL)
    return FALSE;
if (countChars(group, '"') >= 2)
    return TRUE;
if (findWordByDelimiter(group, ' ', "transcript_id") != NULL)
    return TRUE;
return FALSE;
}

boolean gffHasGtfGroup(char *line)
/* Return TRUE if line has a GTF group field */
{
char *words[10];
char *dupe = cloneString(line);
int wordCt = chopTabs(dupe, words);
boolean isGtf = FALSE;
if (wordCt >= 9) 
    if (isGtfGroup(words[8]))
        isGtf = TRUE;
freeMem(dupe);
return isGtf;
}

static void readQuotedString(char *fileName, int lineIx, char *in, char *out, char **retNext)
/* Parse quoted string and abort on error. */
{
if (!parseQuotedString(in, out, retNext))
    errAbort("Line %d of %s\n", lineIx, fileName);
}

void addGroup(struct gffFile *gff, struct gffLine *gl, char *group)
/* Add group to gff if it's not there already, and attach it to gl. */
{
struct gffGroup *gg;
struct hashEl *hel;
if ((hel = hashLookup(gff->groupHash, group)) == NULL)
   {
   AllocVar(gg);
   hel = hashAdd(gff->groupHash, group, gg);
   gg->name = hel->name;
   gg->seq = gl->seq;
   gg->source = gl->source;
   slAddHead(&gff->groupList, gg);
   }
else
   {
   gg = hel->val;
   }
gl->group = gg->name;
}

static void parseGff2End(char *s, struct gffFile *gff, struct gffLine *gl, 
    char *fileName, int lineIx)
/* Read the semi-colon separated end bits of a GTF line into gl and
 * hashes. */
{
char *type, *val;
struct hashEl *hel;
bool gotSemi;

for (;;)
   {
   gotSemi = FALSE;
   if ((type = nextWord(&s)) == NULL)
       break;
   s = skipLeadingSpaces(s);
   if (NULL == s || s[0] == 0)
       errAbort("Unpaired type(%s)/val on end of gtf line %d of %s", type, lineIx, fileName);
   if (s[0] == '"' || s[0] == '\'')
       {
       val = s;
       readQuotedString(fileName, lineIx, s, val, &s);
       }
   else
       {
       int len;
       val = nextWord(&s);
       len = strlen(val) - 1;
       if (val[len] == ';')
	   {
	   val[len] = 0;
	   len -= 1;
           gotSemi = TRUE;
	   }
       if (len < 0)
           errAbort("Empty value for %s line %d of %s", type, lineIx, fileName);
       }
   if (s != NULL && !gotSemi)
      {
      s = strchr(s, ';');
      if (s != NULL)
         ++s;
      }
   /* only use the first occurance of gene_id and transcript_id */
   if (sameString("gene_id", type) && (gl->geneId == NULL))
       {
       struct gffGeneId *gg;
       if ((hel = hashLookup(gff->geneIdHash, val)) == NULL)
	   {
	   AllocVar(gg);
           hel = hashAdd(gff->geneIdHash, val, gg);
	   gg->name = hel->name;
	   slAddHead(&gff->geneIdList, gg);
	   }
	else
	   {
	   gg = hel->val;
	   }
       gl->geneId = gg->name;
       }
   else if (sameString("transcript_id", type) && (gl->group == NULL))
       {
       addGroup(gff, gl, val);
       }
   else if (sameString("exon_id", type))
       gl->exonId = gffFileGetStr(gff, val);
   else if (sameString("exon_number", type))
       {
       if (!isdigit(val[0]))
           errAbort("Expecting number after exon_number, got %s line %d of %s", val, lineIx, fileName);
       gl->exonNumber = atoi(val);
       }
   else if (sameString("intron_id", type))
       gl->intronId = gffFileGetStr(gff, val);
   else if (sameString("intron_status", type))
       gl->intronStatus = gffFileGetStr(gff, val);
   else if (sameString("protein_id", type))
       gl->proteinId = gffFileGetStr(gff, val);
   else if (sameString("gene_name", type))
       gl->geneName = gffFileGetStr(gff, val);
   else if (sameString("transcript_name", type))
       gl->transcriptName = gffFileGetStr(gff, val);
   }
if (gl->group == NULL)
    {
    if (gl->geneId != NULL)
        addGroup(gff, gl, gl->geneId);
    else
        verbose(2, "No gene_id or transcript_id line %d of %s", lineIx, fileName);
    }
}

void gffFileAddRow(struct gffFile *gff, int baseOffset, char *words[], int wordCount, 
    char *fileName, int lineIx)
/* Process one row of GFF file (a non-comment line parsed by tabs normally). */
{
struct hashEl *hel;
struct gffLine *gl;

if (wordCount < 8)
    gffSyntaxError(fileName, lineIx, "Word count less than 8 ");
AllocVar(gl);

if ((hel = hashLookup(gff->seqHash, words[0])) == NULL)
    {
    struct gffSeqName *el;
    AllocVar(el);
    hel = hashAdd(gff->seqHash, words[0], el);
    el->name = hel->name;
    slAddHead(&gff->seqList, el);
    }
gl->seq = hel->name;

if ((hel = hashLookup(gff->sourceHash, words[1])) == NULL)
    {
    struct gffSource *el;
    AllocVar(el);
    hel = hashAdd(gff->sourceHash, words[1], el);
    el->name = hel->name;
    slAddHead(&gff->sourceList, el);
    }
gl->source = hel->name;

if ((hel = hashLookup(gff->featureHash, words[2])) == NULL)
    {
    struct gffFeature *el;
    AllocVar(el);
    hel = hashAdd(gff->featureHash, words[2], el);
    el->name = hel->name;
    slAddHead(&gff->featureList, el);
    }
struct gffFeature *feature = hel->val;
feature->count += 1;

gl->feature = hel->name;

if (!isdigit(words[3][0]) || !isdigit(words[4][0]))
   gffSyntaxError(fileName, lineIx, "col 3 or 4 not a number ");	
gl->start = atoi(words[3])-1 + baseOffset;
gl->end = atoi(words[4]) + baseOffset;
gl->score = atof(words[5]);
gl->strand = words[6][0];
gl->frame = words[7][0];

if (wordCount >= 9)
    {
    char *groupField = words[8];
    if (!gff->typeKnown)
	{
	gff->typeKnown = TRUE;
	gff->isGtf = isGtfGroup(groupField);
	}
    if (gff->isGtf)
	{
	parseGff2End(groupField, gff, gl, fileName, lineIx);
	}
    else
	{
	if (strchr(groupField, ';'))
	    {
	    char *dupeGroup = cloneString(groupField);
	    parseGff2End(dupeGroup, gff, gl, fileName, lineIx);
	    freeMem(dupeGroup);
	    }
	else
	    {
	    char *tnName = gffTnName(gl->seq, trimSpaces(words[8]));
	    if ((hel = hashLookup(gff->groupHash, tnName)) == NULL)
		{
		struct gffGroup *group;
		AllocVar(group);
		hel = hashAdd(gff->groupHash, tnName, group);
		group->name = hel->name;
		group->seq = gl->seq;
		group->source = gl->source;
		slAddHead(&gff->groupList, group);
		}
	    gl->group = hel->name;
	    }
	}
    }
slAddHead(&gff->lineList, gl);
}


void gffFileAdd(struct gffFile *gff, char *fileName, int baseOffset)
/* Create a gffFile structure from a GFF file. */
{
/* Open file and do basic allocations. */
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *words[9];
int lineSize, wordCount;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] != '#')
	{
	wordCount = chopTabs(line, words);
        if (wordCount > 0)
            gffFileAddRow(gff, baseOffset, words, wordCount, lf->fileName, lf->lineIx);
	}
    }
slReverse(&gff->lineList);
slReverse(&gff->seqList);
slReverse(&gff->sourceList);
slReverse(&gff->featureList);
slReverse(&gff->groupList);
slReverse(&gff->geneIdList);
lineFileClose(&lf);
}

struct gffFile *gffFileNew(char *fileName)
/* Create a new gffFile structure. */
{
struct gffFile *gff;
AllocVar(gff);
gff->fileName = cloneString(fileName);
gff->seqHash = newHash(18);
gff->sourceHash = newHash(6);
gff->featureHash = newHash(6);
gff->groupHash = newHash(16);
gff->geneIdHash = newHash(16);
gff->strPool = newHash(20);
return gff;
}

struct gffFile *gffRead(char *fileName)
/* Create a gffFile structure from a GFF file. */
{
struct gffFile *gff = gffFileNew(fileName);
gffFileAdd(gff, fileName, 0);
return gff;
}

static void getGroupBoundaries(struct gffGroup *group)
/* Fill in start, end, strand of group from lines. */
{
struct gffLine *line;
int start = 0x3fffffff;
int end = -start;
line = group->lineList;
group->strand = line->strand;
for (; line != NULL; line = line->next)
    {
    if (start > line->start)
	start = line->start;
    if (end < line->end)
	end = line->end;
    }
group->start = start;
group->end = end;
}

#ifdef UNUSED
static boolean allSameSeq(struct gffGroup *group)
/* Return TRUE if all lines of group are for same chrom */
{
if (group->lineList == NULL || group->lineList->next == NULL)
    return TRUE;
char *seq = group->lineList->seq;
struct gffLine *line;
for (line = group->lineList->next; line != NULL; line = line->next)
    if (!sameString(line->seq, seq))
        return FALSE;
return TRUE;
}
#endif /* UNUSED */

static struct gffGroup *breakGroupBySeq(struct gffGroup *group)
/* Break up a group that has multiple sequences.  Assumes lineList is sorted. */
{
char *curSeq = group->lineList->seq;
struct gffLine *line, *next;
struct gffGroup *brokenList = NULL;
for (line = group->lineList; line != NULL; line = next)
    {
    next = line->next;
    if (next != NULL && !sameString(next->seq, curSeq))
        {
	curSeq = next->seq;
	struct gffGroup *newGroup;
	AllocVar(newGroup);
	newGroup->name = group->name;
	newGroup->seq = curSeq;
	newGroup->source = group->source;
	line->next = NULL;
	newGroup->lineList = next;
	slAddHead(&brokenList, group);
	group = newGroup;
	}
    }
slAddHead(&brokenList, group);
slReverse(&brokenList);
return brokenList;
}

static struct gffGroup *breakMultiSeqGroups(struct gffGroup *oldList)
/* Break up any groups that span multiple chromosomes into one per group.
 * Return reworked list. */
{
struct gffGroup *newList = NULL, *group, *next;

for (group = oldList; group != NULL; group = next)
    {
    next = group->next;
    struct gffGroup *groupList = breakGroupBySeq(group);
    struct gffGroup *newGroup, *newNext;
    for (newGroup = groupList; newGroup != NULL; newGroup = newNext)
	{
	newNext = newGroup->next;
	slAddHead(&newList, newGroup);
	}
    }
slReverse(&newList);
return newList;
}


void gffGroupLines(struct gffFile *gff)
/* Group lines of gff file together, in process mofing
 * gff->lineList to gffGroup->lineList. */
{
struct gffLine *line, *nextLine;
struct hash *groupHash = gff->groupHash;
char *groupName;
struct gffGroup *group;
struct gffLine *ungroupedLines = NULL;

for (line = gff->lineList; line != NULL; line = nextLine)
    {
    nextLine = line->next;
    if ((groupName = line->group) != NULL)
	{
	struct hashEl *hel = hashLookup(groupHash, groupName);
	group = hel->val;
	slAddHead(&group->lineList, line);
	}
    else
	{
	slAddHead(&ungroupedLines, line);
	}
    }

/* Restore ungrouped lines to gff->lineList. */
slReverse(&ungroupedLines);
gff->lineList = ungroupedLines;

/* Restore order of grouped lines. */
for (group = gff->groupList; group != NULL; group = group->next)
    slSort(&group->lineList, gffLineCmp);

/* Look for groups that traverse multiple chromosomes.  Break them apart. */
gff->groupList = breakMultiSeqGroups(gff->groupList);

for (group = gff->groupList; group != NULL; group = group->next)
    getGroupBoundaries(group);
}

void gffOutput(struct gffLine *el, FILE *f, char sep, char lastSep) 
/* Print out GTF.  Separate fields with sep. Follow last field with lastSep. */
{
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->seq);
if (sep == ',') fputc('"',f);
fputc(sep,f);
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->source);
if (sep == ',') fputc('"',f);
fputc(sep,f);
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->feature);
if (sep == ',') fputc('"',f);
fputc(sep,f);
fprintf(f, "%u", el->start+1);
fputc(sep,f);
fprintf(f, "%u", el->end);
fputc(sep,f);
fprintf(f, "%f", el->score);
fputc(sep,f);
if (sep == ',') fputc('"',f);
fprintf(f, "%c", el->strand);
if (sep == ',') fputc('"',f);
fputc(sep,f);
if (sep == ',') fputc('"',f);
fprintf(f, "%c", el->frame);
if (sep == ',') fputc('"',f);
fputc(sep,f);
if (sep == ',') fputc('"',f);
if (el->geneId != NULL)
    fprintf(f, "gene_id %s\"%s%s\"; ",
	    (sep == ',') ? "\\" : "",
	    el->geneId,
	    (sep == ',') ? "\\" : "");
fprintf(f, "transcript_id %s\"%s%s\"; ",
	(sep == ',') ? "\\" : "",
	el->group,
	(sep == ',') ? "\\" : "");
if (el->exonId != NULL)
    fprintf(f, "exon_id %s\"%s%s\"; ",
	    (sep == ',') ? "\\" : "",
	    el->exonId,
	    (sep == ',') ? "\\" : "");
if (sep == ',') fputc('"',f);
fputc(lastSep,f);
}

