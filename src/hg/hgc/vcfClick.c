/* vcfTrack -- handlers for Variant Call Format data. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "dystring.h"
#include "errCatch.h"
#include "hCommon.h"
#include "hdb.h"
#include "hgc.h"
#include "htmshell.h"
#include "jsHelper.h"
#include "pgSnp.h"
#include "regexHelper.h"
#include "trashDir.h"
#include "knetUdc.h"
#include "udc.h"
#include "vcf.h"
#include "vcfUi.h"
#include "trackHub.h"
#include "featureBits.h"

#define NA "<em>n/a</em>"

static void printKeysWithDescriptions(struct vcfFile *vcff, int wordCount, char **words,
				      struct vcfInfoDef *infoDefs, boolean stripToSymbol)
/* Given an array of keys, print out a list of values with descriptions if descriptions are
 * available.  If stripToSymbol, when searching infoDefs, pick the actual key out of
 * <>'s and other extraneous stuff (e.g. "(C)<DEL>" --> "DEL"). */
{
int i;
for (i = 0;  i < wordCount; i++)
    {
    if (i > 0)
	printf(", ");
    char *displayKey = words[i];
    char *descKey = displayKey;
    if (stripToSymbol)
        {
        char *p = strchr(displayKey, '<');
        if (p)
            {
            descKey = cloneString(p+1);
            p = strchr(descKey, '>');
            if (p)
                *p = '\0';
            }
        }
    char *description = NULL;
    struct vcfInfoDef *def;
    for (def = infoDefs;  def != NULL;  def = def->next)
        if (sameString(descKey, def->key))
            {
            description = def->description;
            break;
            }
    char *htmlKey = htmlEncode(displayKey);
    if (description)
	printf("%s (%s)", htmlKey, description);
    else
	printf("%s", htmlKey);
    }
printf("<BR>\n");
}

static void vcfAltAlleleDetails(struct vcfRecord *rec, char **displayAls)
/* If VCF header specifies any symbolic alternate alleles, pull in descriptions. */
{
printf("<B>Alternate allele(s):</B> ");
if (rec->alleleCount < 2 || sameString(rec->alleles[1], "."))
    {
    printf(NA"<BR>\n");
    return;
    }
struct vcfFile *vcff = rec->file;
printKeysWithDescriptions(vcff, rec->alleleCount-1, &(displayAls[1]), vcff->altDefs, TRUE);
}

static void vcfQualDetails(struct vcfRecord *rec)
/* If VCF header specifies a quality/confidence score (not "."), print it out. */
{
printf("<B>Quality/confidence score:</B> %s<BR>\n", sameString(rec->qual, ".") ? NA : rec->qual);
}

static void vcfFilterDetails(struct vcfRecord *rec)
/* If VCF header specifies any filters, pull in descriptions. */
{
if (rec->filterCount == 0 || sameString(rec->filters[0], "."))
    printf("<B>Filter:</B> "NA"<BR>\n");
else if (rec->filterCount == 1 && sameString(rec->filters[0], "PASS"))
    printf("<B>Filter:</B> PASS<BR>\n");
else
    {
    printf("<B>Filter failures:</B> ");
    printf("<font style='font-weight: bold; color: #FF0000;'>\n");
    struct vcfFile *vcff = rec->file;
    printKeysWithDescriptions(vcff, rec->filterCount, rec->filters, vcff->filterDefs, FALSE);
    printf("</font>\n");
    }
}



static int printTabularHeaderRow(const struct vcfInfoDef *def)
/* Parse the column header parts out of def->description and print as table header row;
 * call this only when looksTabular returns TRUE.
 * Returns the number of columns in the header */
{
regmatch_t substrArr[PATH_LEN];
if (regexMatchSubstr(def->description, COL_DESC_REGEX, substrArr, ArraySize(substrArr)))
    {
    puts("<TR>");
    // Make a copy of the part of def->description that matches the regex,
    // then chop by '|' and print out header column tags:
    int matchSize = substrArr[0].rm_eo - substrArr[0].rm_so;
    char copy[matchSize+1];
    safencpy(copy, sizeof(copy), def->description + substrArr[0].rm_so, matchSize);
    // Turn '_' into ' ' so description words can wrap inside headers, saving some space
    subChar(copy, '_', ' ');
    char *words[PATH_LEN];
    int descColCount = chopByChar(copy, '|', words, ArraySize(words));
    int i;
    for (i = 0;  i < descColCount; i++)
        printf("<TH class='withThinBorder'>%s</TH>", words[i]);
    puts("</TR>");
    return descColCount;
    }
else
    errAbort("printTabularHeaderRow: code bug, if looksTabular returns true then "
             "regex should work here");
return -1;
}

static void printTabularData(struct vcfInfoElement *el, int headerCount)
/* Print a row for each value in el, separating columns by '|'. */
{
int j;
for (j = 0;  j < el->count;  j++)
    {
    puts("<TR>");
    char *val = el->values[j].datString;
    if (!isEmpty(val))
        {
        int len = strlen(val);
        char copy[len+1];
        safencpy(copy, sizeof(copy), val, len);
        char *words[PATH_LEN];
        chopByChar(copy, '|', words, ArraySize(words));
        int k;
        // printTabularHeaderRow strips off (but still prints!) a trailing '|'
        // because of the regex, so enforce that here too so the rows after
        // the header don't get all out of whack
        for (k = 0;  k < headerCount;  k++)
            printf("<TD class='withThinBorder'>%s</TD>", words[k]);
        }
    puts("</TR>");
    }
}


static void vcfInfoDetails(struct vcfRecord *rec, char *trackName, int recordCount)
/* Expand info keys to descriptions, then print out keys and values. */
{
if (rec->infoCount == 0)
    return;
struct vcfFile *vcff = rec->file;
puts("<table>"); // wrapper table for collapsible section
char infoId[32];
safef(infoId, sizeof(infoId), "infoFields%d", recordCount);
jsBeginCollapsibleSectionFontSize(cart, trackName, infoId, "INFO column annotations:", FALSE, "medium");
puts("<TABLE class=\"stdTbl\">\n");
int i;
for (i = 0;  i < rec->infoCount;  i++)
    {
    struct vcfInfoElement *el = &(rec->infoElements[i]);
    const struct vcfInfoDef *def = vcfInfoDefForKey(vcff, el->key);
    printf("<TR valign='top'><TD align=\"right\"><B>%s:</B></TD><TD>",
           el->key);
    int j;
    enum vcfInfoType type = def ? def->type : vcfInfoString;
    if (type == vcfInfoFlag && el->count == 0)
	printf("Yes"); // no values, so we can't call vcfPrintDatum...
    // However, if this is older VCF, type vcfInfoFlag might have a value.
    if (looksTabular(def, el))
        {
        // Make a special display below
        printf("<em>see below</em>");
        }
    else
        {
        for (j = 0;  j < el->count;  j++)
            {
            if (j > 0)
                printf(", ");
            if (el->missingData[j])
                printf(".");
            else
                vcfPrintDatum(stdout, el->values[j], type);
            }
        }
    if (def != NULL && !looksTabular(def, el))
	printf("&nbsp;&nbsp;</TD><TD>%s", def->description);
    else
	printf("</TD><TD>");
    printf("</TD></TR>\n");
    }
puts("</TABLE>");
jsEndCollapsibleSection();
puts("</table>"); // close the wrapper around the collapsible section
// Now show the tabular fields, if any
for (i = 0;  i < rec->infoCount;  i++)
    {
    struct vcfInfoElement *el = &(rec->infoElements[i]);
    const struct vcfInfoDef *def = vcfInfoDefForKey(vcff, el->key);
    if (looksTabular(def, el))
        {
        puts("<BR>");
        printf("<B>%s</B>: %s<BR>\n", el->key, def->description);
        puts("<TABLE class='stdTbl'>");
        int headerCount = printTabularHeaderRow(def);
        printTabularData(el, headerCount);
        puts("</TABLE>");
        }
    }
}

static void vcfGenotypeTable(struct vcfRecord *rec, char *track, char **displayAls)
/* Put the table containing details about each genotype into a collapsible section. */
{
static struct dyString *tmp1 = NULL;
if (tmp1 == NULL)
    tmp1 = dyStringNew(0);
jsBeginCollapsibleSection(cart, track, "genotypes", "Detailed genotypes", FALSE);
dyStringClear(tmp1);
dyStringAppend(tmp1, rec->format);
struct vcfFile *vcff = rec->file;
enum vcfInfoType formatTypes[256];
char *formatKeys[256];
int formatCount = chopString(tmp1->string, ":", formatKeys, ArraySize(formatKeys));
boolean firstInfo = TRUE;
int i;
for (i = 0;  i < formatCount;  i++)
    {
    if (sameString(formatKeys[i], vcfGtGenotype))
	continue;
    if (firstInfo)
        {
        puts("<B>Genotype info key:</B><BR>");
        firstInfo = FALSE;
        }
    const struct vcfInfoDef *def = vcfInfoDefForGtKey(vcff, formatKeys[i]);
    char *desc = def ? def->description : "<em>not described in VCF header</em>";
    printf("&nbsp;&nbsp;<B>%s:</B> %s<BR>\n", formatKeys[i], desc);
    formatTypes[i] = def ? def->type : vcfInfoString;
    }
hTableStart();
boolean isDiploid = sameString(vcfHaplotypeOrSample(cart), "Haplotype");
puts("<TR><TH>Sample ID</TH><TH>Genotype</TH>");
if (isDiploid)
    puts("<TH>Phased?</TH>");
for (i = 0;  i < formatCount;  i++)
    {
    if (sameString(formatKeys[i], vcfGtGenotype))
	continue;
    printf("<TH>%s</TH>", formatKeys[i]);
    }
puts("</TR>\n");
for (i = 0;  i < vcff->genotypeCount;  i++)
    {
    struct vcfGenotype *gt = &(rec->genotypes[i]);
    char *hapA = ".", *hapB = ".";
    if (gt->hapIxA >= 0)
	hapA = displayAls[(unsigned char)gt->hapIxA];
    if (gt->isHaploid)
	hapB = "";
    else if (gt->hapIxB >= 0)
	hapB = displayAls[(unsigned char)gt->hapIxB];
    char sep = gt->isHaploid ? ' ' : gt->isPhased ? '|' : '/';
    char *phasing = gt->isHaploid ? NA : gt->isPhased ? "Y" : "n";
    printf("<TR><TD>%s</TD><TD>%s%c%s</TD>", vcff->genotypeIds[i],
	   hapA, sep, hapB);
    if (isDiploid)
        printf("<TD>%s</TD>", phasing);
    int j;
    for (j = 0;  j < gt->infoCount;  j++)
	{
	if (sameString(formatKeys[j], vcfGtGenotype))
	    continue;
	printf("<TD>");
	struct vcfInfoElement *el = &(gt->infoElements[j]);
	int k;
	for (k = 0;  k < el->count;  k++)
	    {
	    if (k > 0)
		printf(", ");
	    if (el->missingData[k])
		printf(".");
	    else
		vcfPrintDatum(stdout, el->values[k], formatTypes[j]);
	    }
	printf("</TD>");
	}
    puts("</TR>");
    }
hTableEnd();
jsEndCollapsibleSection();
}

static void ignoreEm(char *format, va_list args)
/* Ignore warnings from genotype parsing -- when there's one, there
 * are usually hundreds more just like it. */
{
}

static void vcfGenotypesDetails(struct vcfRecord *rec, struct trackDb *tdb, char **displayAls)
/* Print summary of allele and genotype frequency, plus collapsible section
 * with table of genotype details. */
{
struct vcfFile *vcff = rec->file;
if (vcff->genotypeCount == 0)
    return;
// Wrapper table for collapsible section:
puts("<TABLE>");
pushWarnHandler(ignoreEm);
vcfParseGenotypes(rec);
popWarnHandler();
int *gtCounts = NULL, *alCounts = NULL;;
int phasedGts = 0, diploidCount = 0;
vcfCountGenotypes(rec, &gtCounts, &alCounts, &phasedGts, &diploidCount);
boolean isDiploid = sameString(vcfHaplotypeOrSample(cart), "Haplotype");
if (isDiploid)
    {
    printf("<B>Genotype count:</B> %d", vcff->genotypeCount);
    if (diploidCount == 0)
        printf(" (haploid)");
    else if (diploidCount != vcff->genotypeCount)
        printf(" (%d phased, %d diploid, %d haploid)", phasedGts, diploidCount,
               vcff->genotypeCount - diploidCount);
    else
        printf(" (%d phased)", phasedGts);
    }
else
    printf("<B>Sample count:</B> %d", vcff->genotypeCount);

puts("<BR>");
int totalAlleles = vcff->genotypeCount + diploidCount;
double refAf = (double)alCounts[0]/totalAlleles;
printf("<B>Alleles:</B> %s: %d (%.3f%%)", displayAls[0], alCounts[0], 100*refAf);
int i;
for (i = 1;  i < rec->alleleCount;  i++)
    {
    double altAf = (double)alCounts[i]/totalAlleles;
    printf("; %s: %d (%.3f%%)", displayAls[i], alCounts[i], 100*altAf);
    }
if (alCounts[rec->alleleCount] > 0)
    printf("; unknown: %d (%.3f%%)", alCounts[rec->alleleCount],
           100 * (double)alCounts[rec->alleleCount]/totalAlleles);
puts("<BR>");
if (vcff->genotypeCount > 1 && diploidCount > 0)
    {
    printf("<B>Genotypes:</B> %s/%s: %d (%.3f%%)",
	   displayAls[0], displayAls[0], gtCounts[0], 100*(double)gtCounts[0]/diploidCount);
    for (i = 1;  i < rec->alleleCount + 1;  i++)
        {
        int j;
        for (j = 0;  j <= i;  j++)
            {
            int gtIx = vcfGenotypeIndex(j, i);
            if (gtCounts[gtIx] > 0)
                {
                char *alJ = (j == rec->alleleCount) ? "?" : displayAls[j];
                char *alI = (i == rec->alleleCount) ? "?" : displayAls[i];
                printf("; %s/%s: %d (%.3f%%)", alJ, alI, gtCounts[gtIx],
                       100*(double)gtCounts[gtIx]/diploidCount);
                }
            }
        }
    printf("<BR>\n");
    if (rec->alleleCount == 2)
	{
	boolean showHW = cartOrTdbBoolean(cart, tdb, VCF_SHOW_HW_VAR, FALSE);
	if (showHW)
            {
            double altAf = (double)alCounts[1]/totalAlleles;
	    printf("<B><A HREF=\"http://en.wikipedia.org/wiki/Hardy%%E2%%80%%93Weinberg_principle\" "
		   "TARGET=_BLANK>Hardy-Weinberg equilibrium</A>:</B> "
		   "P(%s/%s) = %.3f%%; P(%s/%s) = %.3f%%; P(%s/%s) = %.3f%%<BR>",
		   displayAls[0], displayAls[0], 100*refAf*refAf,
		   displayAls[0], displayAls[1], 100*2*refAf*altAf,
		   displayAls[1], displayAls[1], 100*altAf*altAf);
            }
	}
    }
puts("<BR>");
vcfGenotypeTable(rec, tdb->track, displayAls);
puts("</TABLE>");
}

static void pgSnpCodingDetail(struct vcfRecord *rec)
/* Translate rec into pgSnp (with proper chrom name) and call Belinda's
 * coding effect predictor from pgSnp details. */
{
char *genePredTable = "knownGene";
if (hTableExists(database, genePredTable))
    {
    struct pgSnp *pgs = pgSnpFromVcfRecord(rec);
    if (!sameString(rec->chrom, seqName))
	// rec->chrom might be missing "chr" prefix:
	pgs->chrom = seqName;
    printSeqCodDisplay(database, pgs, genePredTable);
    }
}

static void abbreviateLongSeq(char *seqIn, int endLength, boolean showLength, struct dyString *dy)
/* If seqIn is longer than 2*endLength plus abbreviation fudge, abbreviate it
 * to its first endLength bases, ellipsis that says how many bases are skipped,
 * and its last endLength bases; add result to dy. */
{
int threshold = 2*endLength + 30;
int seqInLen = strlen(seqIn);
if (seqInLen > threshold)
    {
    dyStringAppendN(dy, seqIn, endLength);
    dyStringAppend(dy, "...");
    if (showLength)
	{
	int skippedLen = seqInLen-2*endLength;
	dyStringPrintf(dy, "<%d bases>...", skippedLen);
	}
    dyStringAppend(dy, seqIn+seqInLen-endLength);
    }
else
    dyStringAppend(dy, seqIn);
}

static void makeDisplayAlleles(struct vcfRecord *rec, boolean showLeftBase, char leftBase,
			       int endLength, boolean showLength, boolean encodeHtml,
			       char **displayAls)
/* If necessary, show the left base that we trimmed and/or abbreviate long sequences. */
{
struct dyString *dy = dyStringNew(128);
int i;
for (i = 0;  i < rec->alleleCount; i++)
    {
    dyStringClear(dy);
    if (showLeftBase)
	dyStringPrintf(dy, "(%c)", leftBase);
    abbreviateLongSeq(rec->alleles[i], endLength, showLength, dy);
    if (encodeHtml)
	displayAls[i] = htmlEncode(dy->string);
    else
	displayAls[i] = cloneString(dy->string);
    }
}

static void vcfRecordDetails(struct trackDb *tdb, struct vcfRecord *rec, int recordCount)
/* Display the contents of a single line of VCF, assumed to be from seqName
 * (using seqName instead of rec->chrom because rec->chrom might lack "chr"). */
{
if (isNotEmpty(rec->name) && differentString(rec->name, "."))
    printf("<B>Name:</B> %s<BR>\n", rec->name);
// Add some special URL substitution variables for ExAC/GnomAD-style links
struct slPair *substFields = slPairNew("ref", rec->alleles[0]);
substFields->next = slPairNew("firstAlt", rec->alleles[1]);
char posString[64];
safef(posString, sizeof posString, "%d", rec->chromStart+1);
substFields->next->next = slPairNew("pos", posString);
char *label = rec->name;
if ((isEmpty(rec->name) || sameString(rec->name, ".")) &&
    (startsWith("exac", tdb->track) || startsWith("gnomad", tdb->track)))
    {
    struct dyString *dyLabel = dyStringCreate("%s-%s-%s-%s", skipChr(rec->chrom), posString,
                                              rec->alleles[0], rec->alleles[1]);
    label = dyStringCannibalize(&dyLabel);
    }
printCustomUrlWithFields(tdb, rec->name, label, TRUE, substFields);
// Since these are variants, if it looks like a dbSNP or dbVar ID, provide a link:
if (regexMatch(rec->name, "^rs[0-9]+$"))
    {
    printf("<B>dbSNP:</B> ");
    printDbSnpRsUrl(rec->name, "%s", rec->name);
    puts("<BR>");
    }
else if (regexMatch(rec->name, "^[en]ss?v[0-9]+$"))
    {
    printf("<B>dbVar:</B> ");
    printf("<A HREF=\"https://www.ncbi.nlm.nih.gov/dbvar/variants/%s/\" "
	   "TARGET=_BLANK>%s</A><BR>\n", rec->name, rec->name);
    }
boolean hapClustEnabled = cartOrTdbBoolean(cart, tdb, VCF_HAP_ENABLED_VAR, TRUE);
if (hapClustEnabled && rec->file != NULL && rec->file->genotypeCount > 1 && differentString(tdb->type, "vcfPhasedTrio"))
    {
    char *hapMethod = cartOrTdbString(cart, tdb, VCF_HAP_METHOD_VAR, VCF_DEFAULT_HAP_METHOD);
    char *hapOrSample = vcfHaplotypeOrSample(cart);
    if (sameString(hapMethod, VCF_HAP_METHOD_CENTER_WEIGHTED))
        {
        static char *formName = "vcfCfgHapCenter";
        printf("<FORM NAME=\"%s\" ACTION=\"%s\">\n", formName, hgTracksName());
        cartSaveSession(cart);
        printf("<TABLE cellpadding=0><TR><TD colspan=2>"
               "<B>%s sorting order:</B> ", hapOrSample);
        vcfCfgHaplotypeCenter(cart, tdb, tdb->track, FALSE, rec->file, rec->name,
                              seqName, rec->chromStart, formName);
        printf("</TABLE></FORM>\n");
        }
    }
char leftBase = rec->alleles[0][0];
unsigned int vcfStart = vcfRecordTrimIndelLeftBase(rec);
boolean showLeftBase = (rec->chromStart == vcfStart+1);
(void)vcfRecordTrimAllelesRight(rec);
char *displayAls[rec->alleleCount];
makeDisplayAlleles(rec, showLeftBase, leftBase, 20, TRUE, FALSE, displayAls);
printPosOnChrom(seqName, rec->chromStart, rec->chromEnd, NULL, FALSE, rec->name);
printf("<B>Reference allele:</B> %s<BR>\n", displayAls[0]);
vcfAltAlleleDetails(rec, displayAls);
vcfQualDetails(rec);
vcfFilterDetails(rec);
vcfInfoDetails(rec, tdb->track, recordCount);
pgSnpCodingDetail(rec);
makeDisplayAlleles(rec, showLeftBase, leftBase, 5, FALSE, TRUE, displayAls);
vcfGenotypesDetails(rec, tdb, displayAls);
}

void doVcfDetailsCore(struct trackDb *tdb, char *fileOrUrl, boolean isTabix, struct featureBits **pFbList, int rgnStart, int rgnEnd)
/* Show item details using fileOrUrl. */
{
if (!pFbList)
    genericHeader(tdb, NULL);
int start;
int end;
if (pFbList)
    {
    start = rgnStart;
    end = rgnEnd;
    }
else
    {
    start = cartInt(cart, "o");
    end = cartInt(cart, "t");
    }


int vcfMaxErr = -1;
struct vcfFile *vcff = NULL;
/* protect against temporary network or parsing error */
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    if (isTabix)
        {
        char *indexUrl = trackDbSetting(tdb, "bigDataIndex");
	vcff = vcfTabixFileAndIndexMayOpen(fileOrUrl, indexUrl, seqName, start, end, vcfMaxErr, -1);
        }
    else
	vcff = vcfFileMayOpen(fileOrUrl, seqName, start, end, vcfMaxErr, -1, TRUE);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    if (isNotEmpty(errCatch->message->string))
	warn("%s", errCatch->message->string);
    }
errCatchFree(&errCatch);

struct featureBits *fbList = NULL, *fb;
if (vcff != NULL)
    {
    struct vcfRecord *rec;
    // use the recordCount to make up unique id strings for html collapsible INFO section later
    int recordCount = 0;
    for (rec = vcff->records;  rec != NULL;  rec = rec->next)
        {
        if (pFbList)
	    {
            AllocVar(fb);
            fb->chrom = rec->chrom;
            fb->start = rec->chromStart;
            fb->end = rec->chromEnd;
            fb->strand = '+';
            slAddHead(&fbList, fb);
	    }
	else
	    {
	    if (rec->chromStart == start && rec->chromEnd == end) // in pgSnp mode, don't get name
		vcfRecordDetails(tdb, rec, recordCount);
	    }
	recordCount++;
        }
    }
else
    {
    if (!pFbList)
        printf("Sorry, unable to open %s<BR>\n", fileOrUrl);
    }
if (pFbList)
    {
    slReverse(&fbList); 
    *pFbList = fbList;
    }
else
    {
    printTrackHtml(tdb);
    }
}



void doVcfTabixDetailsExt(struct trackDb *tdb, char *item, struct featureBits **pFbList, int start, int end)
/* Show details of an alignment from a VCF file compressed and indexed by tabix. */
{
knetUdcInstall();
if (udcCacheTimeout() < 300)
    udcSetCacheTimeout(300);
struct sqlConnection *conn = NULL;

if (!trackHubDatabase(database))
    conn = hAllocConnTrack(database, tdb);

char *fileOrUrl = bbiNameFromSettingOrTableChrom(tdb, conn, tdb->table, seqName);
hFreeConn(&conn);
doVcfDetailsCore(tdb, fileOrUrl, TRUE, pFbList, start, end);
}

void doVcfTabixDetails(struct trackDb *tdb, char *item)
/* Show details of an alignment from a VCF file compressed and indexed by tabix. */
{
doVcfTabixDetailsExt(tdb, item, NULL, 0, 0);
}




void doVcfDetailsExt(struct trackDb *tdb, char *item, struct featureBits **pFbList, int start, int end)
/* Show details of an alignment from an uncompressed VCF file. */
{
struct customTrack *ct = lookupCt(tdb->track);
struct sqlConnection *conn = NULL;
char *table = tdb->table;
if (ct)
    {
    conn = hAllocConn(CUSTOM_TRASH);
    table = ct->dbTableName;
    }
else
    conn = hAllocConnTrack(database, tdb);
char *fileOrUrl = bbiNameFromSettingOrTableChrom(tdb, conn, table, seqName);
hFreeConn(&conn);
doVcfDetailsCore(tdb, fileOrUrl, FALSE, pFbList, start, end);
}

void doVcfDetails(struct trackDb *tdb, char *item)
/* Show details of an alignment from an uncompressed VCF file. */
{
doVcfDetailsExt(tdb, item, NULL, 0, 0);
}

