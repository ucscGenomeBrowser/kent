/* Handle details pages for wiggle tracks. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "wiggle.h"
#include "cart.h"
#include "hgc.h"
#include "hCommon.h"
#include "hgColors.h"
#include "bigBed.h"
#include "hui.h"
#include "subText.h"

static void bigGenePredLinks(char *track, char *item)
/* output links to genePred driven sequence dumps */
{
printf("<H3>Links to sequence:</H3>\n");
printf("<UL>\n");
puts("<LI>\n");
hgcAnchorSomewhere("htcTranslatedPredMRna", item, "translate", seqName);
printf("Translated Protein</A> from genomic DNA\n");
puts("</LI>\n");

puts("<LI>\n");
hgcAnchorSomewhere("htcGeneMrna", item, track, seqName);
printf("Predicted mRNA</A> \n");
puts("</LI>\n");

puts("<LI>\n");
hgcAnchorSomewhere("htcGeneInGenome", item, track, seqName);
printf("Genomic Sequence</A> from assembly\n");
puts("</LI>\n");
printf("</UL>\n");
}

void printMismatchString(char *a, char *b) 
/* given two strings of same length, print . for every match and for mismatches, the letter of b */
{
int i = 0;
while (TRUE)
    {
    if (a[i]=='\0' || b[i]=='\0')
        break;
    if (a[i]==b[i])
        printf(".");
    else
        printf("%c", (b[i]));
    i++;
    }
}

static void extFieldMismatchCounts(char *val)
/* crispr track: number of mismatches. A comma-sep string of integers */
{
printf("<tr><td>Number of potential off-targets</td>\n");
printf("<td>\n");
char *words[255];
int wordCount = chopByChar(val, ',', words, ArraySize(words));
int i;
printf("<table style='border-style: hidden'><tr>\n");
for (i=0; i<wordCount; i++)
    printf("<td style='border:1px solid #CCCCCC; font-weight: normal; width:auto'><b>%d mismatches:</b><br>%s off-targets</td>", i, words[i]);
printf("</tr></table>\n");
}

static void extFieldCrisprOfftargets(char *val, struct slPair *extraFields)
/* crispr track: locations of off-targets. A |-separated string of coords, including strand and 
 a score
 e.g. chr15;63615585-;71|chr16;8835640+;70 */
{
if (NULL == val)
    {
    printf("<br><table class='bedExtraTbl'>\n");
    printf("<tr><td>Potential Off-targets</td>\n");
    printf("<td>No Off-targets found for this guide</td></tr>\n");
    printf("</table>\n");
    return;
    }
printf("<tr><td>Potential Off-targets</td>\n");

printf("<td>\n");
char *coords[65536];
int coordCount = chopByChar(val, '|', coords, ArraySize(coords));
int i;

struct subText *subList = NULL;
slSafeAddHead(&subList, subTextNew("ig:", "intergenic "));
slSafeAddHead(&subList, subTextNew("ex:", "exon "));
slSafeAddHead(&subList, subTextNew("in:", "intron "));
slSafeAddHead(&subList, subTextNew("|", "-"));

struct sqlConnection *conn = hAllocConn(database);

boolean hasLocus = sqlTableExists(conn, "locusName");

if (coordCount==0)
    puts("Too many off-targets found to display or no off-targets. Please use the Crispor.org link at the top of the page to show all off-targets.\n");
else
    {
    printf("<table style='border-collapse:collapse; font-size:12px; table-layout:fixed'>\n");
    printf("<tr>\n"
           "<th style='width:26em'>Mismatched nucleotides</th>\n"
           "<th style='width:9em'>CFD Score</th>\n");
    if (hasLocus)
           printf("<th style='width:40em'>Locus</th>\n");
    printf("<th style='width:30em'>Position</th></tr>\n");
    }

    
boolean collapsed = FALSE;
for (i=0; i<coordCount; i++)
    {
    if (i>10)
        {
        collapsed = TRUE;
        printf("<tr class='crisprLinkHidden' style='display:none'>\n");
        }
    else
        printf("<tr>\n");

    // parse single coordinate string
    // chr15;63615585-;71 = chrom;startPosStrand;scoreAsInt
    char *parts[3];
    chopByChar(coords[i], ';', parts, 3);
    char* chrom = parts[0];
    char* posStrand = parts[1];
    char* scoreStr = parts[2];

    // get score and strand
    char strand = *(posStrand+strlen(posStrand)-1);
    int pos = atol(posStrand);
    int scoreInt = atoi(scoreStr);
    float score = (float)scoreInt/1000;

    // get the DNA sequence - this is slow! twoBit currently does not cache
    // if the input is not sorted and this list is sorted by off-target score (CFD)
    struct dnaSeq *seq = hDnaFromSeq(database, chrom, pos, pos+23, dnaUpper);
    if (strand=='-')
        reverseComplement(seq->dna, seq->size);
    char *guideSeq = (char*)slPairFindVal(extraFields, "guideSeq");
    // PAM = the last three chars of the off-target
    char *pam = seq->dna+20;
        
    // print sequence + PAM
    printf("<td><tt>");
    printMismatchString(guideSeq, seq->dna);
    printf("&nbsp;%s", pam);
    printf("</tt></td>\n");

    // print score of off-target
    printf("<td>%0.3f</td>", score);

    // print name of this locus
    if (hasLocus)
        {
        struct sqlResult *sr = hRangeQuery(conn, "locusName", chrom, pos, pos+23, NULL, 0);
        char **row;
        row = sqlNextRow(sr);
        if (row != NULL)
            {
            char *desc = row[4];
            char *descLong = subTextString(subList, desc);
            printf("<td>%s</td>", descLong);
            freeMem(descLong);
            }
        sqlFreeResult(&sr);
        }
    
    // print link to location
    printf("<td><a href='%s&db=%s&position=%s%%3A%d-%d'>%s:%d (%c)</a></td>\n", 
        hgTracksPathAndSettings(), database,
        chrom, pos+1, pos+23, chrom, pos+1, strand);

    printf("</tr>\n");
    }
hFreeConn(&conn);
printf("<tr>\n");
if (coordCount!=0)
    printf("</table>\n");
if (collapsed)
    {
    printf("<p><a id='crisprShowAllLink' href='#'>"
        "Show all %d off-targets...</a>\n", coordCount);
    jsOnEventById("click", "crisprShowAllLink", "crisprShowAll(); return false;");
    // inline .js is bad style but why pollute our global .js files for such a rare
    // case? Maybe we should have a generic "collapsible" class, like bootstrap?
    jsInline(
	"function crisprShowAll() {\n"
	"    $('#crisprShowAllLink').hide();\n"
	"    $('.crisprLinkHidden').show();\n"
	"    return false;\n"
    	"}\n"
	);
    }
}

static void detailsTabPrintSpecial(char *name, char *val, struct slPair *extraFields)
/* some extra fields require special printing code, they all start with '_'  */
{
if (sameWord(name, "_mismatchCounts"))
    extFieldMismatchCounts(val);
else if (sameWord(name, "_crisprOfftargets"))
    extFieldCrisprOfftargets(val, extraFields);
}

static void seekAndPrintTable(char *url, off_t offset, struct slPair *extraFields)
/* seek to 0 at url, get headers, then seek to offset, read tab-sep fields and output 
 * (extraFields are needed for some special field handlers) */
{
char *detailsUrl = hReplaceGbdb(replaceChars(url, "$db", database));

// open the URL
struct lineFile *lf = lineFileUdcMayOpen(detailsUrl, TRUE);
if (lf==NULL)
    {
    printf("Error: Could not open the URL referenced in detailsTabUrls, %s", detailsUrl);
    return;
    }

// get the headers
char *headLine = NULL;
int lineSize = 0;
lineFileNext(lf, &headLine, &lineSize);
char *headers[1024];
int headerCount = chopTabs(headLine, headers);

// clone the headers
int i;
for (i=0; i<headerCount; i++)
    headers[i] = cloneString(headers[i]);

lineFileSeek(lf, offset, SEEK_SET);

// read a line
char *detailsLine;
lineFileNext(lf, &detailsLine, &lineSize);
if (!detailsLine || isEmpty(detailsLine))
    return;
char *fields[1024];
int fieldCount = chopTabs(detailsLine, fields);

if (fieldCount!=headerCount)
    {
    printf("Error encountered when reading %s:<br>", detailsUrl);
    printf("The header line of the tab-sep file has a different number of fields compared ");
    printf("with the line pointed to by offset %lld in the bigBed file.<br>", (long long int)offset);
    printf("Number of headers: %d", headerCount);
    printf("Number of fields at offset: %d", fieldCount);
    return;
    }

// print the table for all external extra fields 
printf("<br><table class='bedExtraTbl'>\n");
fieldCount = min(fieldCount, headerCount);
for (i=0; i<fieldCount; i++)
{
    char *name = headers[i];
    char *val  = fields[i];
    
    if (startsWith("_", name))
        detailsTabPrintSpecial(name, val, extraFields);
    else
        {
        printf("<tr><td>%s</td>\n", name);
        printf("<td>%s</td></tr>\n", val);
        }
}
printf("</table>\n");

lineFileClose(&lf);
freez(&detailsUrl);
}

static void printAllExternalExtraFields(struct trackDb *tdb, struct slPair *extraFields)
/* handle the "detailsTabUrls" trackDb setting: 
 * For each field, print a separate html table with all field names and values
 * from the external tab-sep file */

{
char *detailsUrlsStr = trackDbSetting(tdb, "detailsTabUrls");
if (!detailsUrlsStr)
    return;

struct slPair *detailsUrls = slPairListFromString(detailsUrlsStr, TRUE);
if (!detailsUrls)
    {
    printf("Problem when parsing trackDb setting detailsTabUrls<br>\n");
    printf("Expected: a space-separated key=val list, like 'fieldName1=URL1 fieldName2=URL2'<br>\n");
    printf("But got: '%s'<br>", detailsUrlsStr);
    return;
    }

struct slPair *pair;
for (pair = detailsUrls; pair != NULL; pair = pair->next)
    {
    char *fieldName = pair->name;
    char *detailsUrl = pair->val;

    // get extra bigBed field (=the offset) and seek to it
    void *p = slPairFindVal(extraFields, fieldName);
    if (p==NULL)
        {
        printf("Error when parsing trackDb detailsTabUrls statement:<br>\n");
        printf("Cannot find extra bigBed field with name %s\n", fieldName);
        return;
        }
    char *offsetStr = (char*)p;

    if (offsetStr==NULL || sameWord(offsetStr, "0"))
	{
	/* need to show the empty off-targets for crispr tracks */
	if (startsWith("crispr", tdb->track))
	    extFieldCrisprOfftargets(NULL, NULL);
        // empty or "0" value in bigBed means that the lookup should not be performed
        continue;
	}
    off_t offset = atoll(offsetStr);

    seekAndPrintTable(detailsUrl, offset, extraFields);
    }
slPairFreeValsAndList(&detailsUrls);
}

static void bigBedClick(char *fileName, struct trackDb *tdb,
                     char *item, int start, int end, int bedSize)
/* Handle click in generic bigBed track. */
{
boolean showUrl = FALSE;
char *chrom = cartString(cart, "c");

/* Open BigWig file and get interval list. */
struct bbiFile *bbi = bigBedFileOpen(fileName);
struct lm *lm = lmInit(0);
int ivStart = start, ivEnd = end;
if (start == end)
    {
    // item is an insertion; expand the search range from 0 bases to 2 so we catch it:
    ivStart = max(0, start-1);
    ivEnd++;
    }
struct bigBedInterval *bbList = bigBedIntervalQuery(bbi, chrom, ivStart, ivEnd, 0, lm);

/* Get bedSize if it's not already defined. */
if (bedSize == 0)
    {
    bedSize = bbi->definedFieldCount;
    showUrl = TRUE;
    }


char *scoreFilter = cartOrTdbString(cart, tdb, "scoreFilter", NULL);
int minScore = 0;
if (scoreFilter)
    minScore = atoi(scoreFilter);

/* Find particular item in list - matching start, and item if possible. */
boolean found = FALSE;
boolean firstTime = TRUE;
struct bigBedInterval *bb;
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    if (!(bb->start == start && bb->end == end))
	continue;
    if (bedSize > 3)
	{
	char *name = cloneFirstWordByDelimiterNoSkip(bb->rest, '\t');
	boolean match = (isEmpty(name) && isEmpty(item)) || sameOk(name, item);
	freez(&name);
	if (!match)
	    continue;
	}

    found = TRUE;
    if (firstTime)
	printf("<BR>\n");
    int seq1Seq2Fields = 0;
    // check for seq1 and seq2 in columns 7+8 (eg, pairedTagAlign)
    boolean seq1Seq2 = sameOk(trackDbSetting(tdb, BASE_COLOR_USE_SEQUENCE), "seq1Seq2");
    if (seq1Seq2 && bedSize == 6)
	seq1Seq2Fields = 2;
    char *fields[bedSize+seq1Seq2Fields];
    char startBuf[16], endBuf[16];
    char *rest = cloneString(bb->rest);
    int bbFieldCount = bigBedIntervalToRow(bb, chrom, startBuf, endBuf, fields,
                                           bedSize+seq1Seq2Fields);
    if (bbFieldCount != bedSize+seq1Seq2Fields)
        {
        errAbort("Disagreement between trackDb field count (%d) and %s fieldCount (%d)",
		bedSize, fileName, bbFieldCount);
	}
    struct bed *bed = bedLoadN(fields, bedSize);
    if (bedSize >= 6 && scoreFilter && bed->score < minScore)
	continue;
    if (showUrl && (bedSize >= 4))
        printCustomUrl(tdb, item, TRUE);
    bedPrintPos(bed, bedSize, tdb);

    // display seq1 and seq2
    if (seq1Seq2 && bedSize+seq1Seq2Fields == 8)
        printf("<table><tr><th>Sequence 1</th><th>Sequence 2</th></tr>"
	       "<tr><td> %s </td><td> %s </td></tr></table>", fields[6], fields[7]);
    else if (isNotEmpty(rest))
	{
	char *restFields[256];
	int restCount = chopTabs(rest, restFields);
	int restBedFields = bedSize - 3;
	if (restCount > restBedFields)
	    {
            char **extraFields = (restFields + restBedFields);
            int extraFieldCount = restCount - restBedFields;
            int printCount = extraFieldsPrint(tdb,NULL,extraFields, extraFieldCount);

            struct slPair* extraFieldPairs = getExtraFields(tdb, extraFields, extraFieldCount);
            printAllExternalExtraFields(tdb, extraFieldPairs);

            if (printCount == 0)
                {
                int i;
                char label[20];
                safef(label, sizeof(label), "nonBedFieldsLabel");
                printf("<B>%s&nbsp;</B>",
                       trackDbSettingOrDefault(tdb, label, "Non-BED fields:"));
                for (i = restBedFields;  i < restCount;  i++)
                    printf("%s%s", (i > 0 ? "\t" : ""), restFields[i]);
                printf("<BR>\n");
                }
	    }
	if (sameString(tdb->type, "bigGenePred"))
	    bigGenePredLinks(tdb->track, item);
	}


    if (isCustomTrack(tdb->track))
	{
	time_t timep = bbiUpdateTime(bbi);
	printBbiUpdateTime(&timep);
	}

    }

if (!found)
    {
    printf("No item %s starting at %d\n", emptyForNull(item), start);
    }

lmCleanup(&lm);
bbiFileClose(&bbi);
}

void genericBigBedClick(struct sqlConnection *conn, struct trackDb *tdb,
                     char *item, int start, int end, int bedSize)
/* Handle click in generic bigBed track. */
{
char *fileName = bbiNameFromSettingOrTable(tdb, conn, tdb->table);
bigBedClick(fileName, tdb, item, start, end, bedSize);
}

void bigBedCustomClick(struct trackDb *tdb)
/* Display details for BigWig custom tracks. */
{
char *fileName = trackDbSetting(tdb, "bigDataUrl");
char *item = cartOptionalString(cart, "i");
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
bigBedClick(fileName, tdb, item, start, end, 0);
}
