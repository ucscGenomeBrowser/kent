/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "gff.h"
#include "obscure.h"

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
    freeHash(&gff->exonHash);
    slFreeList(&gff->lineList);
    slFreeList(&gff->seqList);
    slFreeList(&gff->sourceList);
    slFreeList(&gff->featureList);
    slFreeList(&gff->geneIdList);
    gffGroupFreeList(&gff->groupList);
    freez(pGff);
    }
}

int gffLineCmp(const void *va, const void *vb)
/* Compare two gffLines. */
{
const struct gffLine *a = *((struct gffLine **)va);
const struct gffLine *b = *((struct gffLine **)vb);
int diff;

diff = strcmp(a->seq, b->seq);
if (diff == 0)
    diff = a->start - b->start;
return diff;
}


void gffSyntaxError(struct lineFile *lf)
/* Complain about syntax error in GFF file. */
{
errAbort("Bad line %d of %s:\n%s",
    lf->lineIx, lf->fileName, lf->buf + lf->lineStart);
}

static char *gffTnName(char *seqName, char *groupName)
/* Make name that encorperates seq and group names.... */
{
static char nameBuf[512];
char *s;
if (startsWith("gene-", groupName))
    groupName += 5;
if (startsWith("cc_", groupName))
    groupName += 3;
sprintf(nameBuf, "%s.%s", seqName, groupName);
return nameBuf;
}

static int countChars(char *s, char c)
/* Count number of times c occurs in s. */
{
int count = 0;
char a;

while ((a = *s++) != 0)
    {
    if (a == c)
        ++count;
    }
return count;
}

static boolean isGtfGroup(char *group)
/* Return TRUE if group field looks like GTF */
{
if (strstr(group, "gene_id") == NULL)
    return FALSE;
if (countChars(group, '"') >= 2)
    return TRUE;
if (strstr(group, "transcript_id") != NULL)
    return TRUE;
return FALSE;
}

static void readQuotedString(struct lineFile *lf, char *in, char *out, char **retNext)
/* Parse quoted string and abort on error. */
{
if (!parseQuotedString(in, out, retNext))
    errAbort("Line %d of %s\n", lf->lineIx, lf->fileName);
}

static void parseGtfEnd(char *s, struct gffFile *gff, struct gffLine *gl, struct lineFile *lf)
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
   if (s[0] == 0 || s == NULL)
       errAbort("Unpaired type/val on end of gtf line %d of %s", lf->lineIx, lf->fileName);
   if (s[0] == '"' || s[0] == '\'')
       {
       val = s;
       readQuotedString(lf, s, val, &s);
       }
   else
       {
       int len;
       val = nextWord(&s);
       len = strlen(val) - 1;
       if (val[len] == ';')
	   {
	   val[len] = 0;
           gotSemi = TRUE;
	   }
       }
   if (s != NULL && !gotSemi)
      {
      s = strchr(s, ';');
      if (s != NULL)
         ++s;
      }
   if (sameString("gene_id", type))
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
   else if (sameString("transcript_id", type))
       {
       struct gffGroup *gg;
       if ((hel = hashLookup(gff->groupHash, val)) == NULL)
	   {
	   AllocVar(gg);
           hel = hashAdd(gff->groupHash, val, gg);
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
   else if (sameString("exon_id", type))
       {
       if ((hel = hashLookup(gff->exonHash, val)) == NULL)
	   hel = hashAdd(gff->exonHash, val, NULL);
       gl->exonId = hel->val;
       }
   else if (sameString("exon_number", type))
       {
       if (!isdigit(val[0]))
           errAbort("Expecting number after exon_number, got %s line %d of %s", val, lf->lineIx, lf->fileName);
       gl->exonNumber = atoi(val);
       }
   }
if (gl->group == NULL)
    {
    if (gl->geneId == NULL)
        warn("No gene_id or transcript_id line %d of %s", lf->lineIx, lf->fileName);
    }
}

void gffFileAdd(struct gffFile *gff, char *fileName, int baseOffset)
/* Create a gffFile structure from a GFF file. */
{
/* Open file and do basic allocations. */
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
int lineSize;
boolean typeKnown = FALSE;
boolean isGtf = FALSE;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] != '#')
	{
	int wordCount;
	char *words[9];
	struct hashEl *hel;
	struct gffLine *gl;
	wordCount = chopTabs(line, words);
	if (wordCount < 8)
	    gffSyntaxError(lf);
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
	gl->feature = hel->name;

	if (!isdigit(words[3][0]) || !isdigit(words[4][0]))
	   gffSyntaxError(lf);	
	gl->start = atoi(words[3])-1 + baseOffset;
	gl->end = atoi(words[4]) + baseOffset;
	gl->score = atof(words[5]);
	gl->strand = words[6][0];
	gl->frame = words[7][0];

	if (wordCount >= 9)
	    {
	    if (!typeKnown)
	        {
		typeKnown = TRUE;
		isGtf = isGtfGroup(words[8]);
		}
	    if (isGtf)
	        {
		parseGtfEnd(words[8], gff, gl, lf);
		}
	    else
	        {
		char *tnName = gffTnName(gl->seq, words[8]);
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
	slAddHead(&gff->lineList, gl);
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
gff->seqHash = newHash(6);
gff->sourceHash = newHash(6);
gff->featureHash = newHash(6);
gff->groupHash = newHash(12);
gff->geneIdHash = newHash(12);
gff->exonHash = newHash(16);
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

/* Restore order of grouped lines and fill in start and end. */
for (group = gff->groupList; group != NULL; group = group->next)
    {
    slSort(&group->lineList, gffLineCmp);
    getGroupBoundaries(group);
    }
}

