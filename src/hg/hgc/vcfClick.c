/* vcfTrack -- handlers for Variant Call Format data. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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

// Characters we expect to see in |-separated parts of an ##INFO description that specifies
// tabular contents:
#define COL_DESC_WORD_REGEX "[A-Za-z_0-9.-]+"
// Series of |-separated words:
#define COL_DESC_REGEX COL_DESC_WORD_REGEX"(\\|"COL_DESC_WORD_REGEX")+"

// Minimum number of |-separated values for interpreting descriptions and values as tabular:
#define MIN_COLUMN_COUNT 3

static boolean looksTabular(const struct vcfInfoDef *def, struct vcfInfoElement *el)
/* Return TRUE if def->description seems to contain a |-separated description of columns
 * and el's first non-empty string value has the same number of |-separated parts. */
{
if (!def || def->type != vcfInfoString || isEmpty(def->description))
    return FALSE;
if (regexMatch(def->description, COL_DESC_REGEX))
    {
    int descColCount = countChars(def->description, '|') + 1;
    if (descColCount >= MIN_COLUMN_COUNT)
        {
        int j;
        for (j = 0;  j < el->count;  j++)
            {
            char *val = el->values[j].datString;
            if (isEmpty(val))
                continue;
            int elColCount = countChars(val, '|') + 1;
            if (elColCount == descColCount)
                return TRUE;
            }
        }
    }
return FALSE;
}

static void printTabularHeaderRow(const struct vcfInfoDef *def)
/* Parse the column header parts out of def->description and print as table header row;
 * call this only when looksTabular returns TRUE. */
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
    }
else
    errAbort("printTabularHeaderRow: code bug, if looksTabular returns true then "
             "regex should work here");
}

static void printTabularData(struct vcfInfoElement *el)
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
        int colCount = chopByChar(copy, '|', words, ArraySize(words));
        int k;
        for (k = 0;  k < colCount;  k++)
            printf("<TD class='withThinBorder'>%s</TD>", words[k]);
        }
    puts("</TR>");
    }
}


static void vcfInfoDetails(struct vcfRecord *rec)
/* Expand info keys to descriptions, then print out keys and values. */
{
if (rec->infoCount == 0)
    return;
struct vcfFile *vcff = rec->file;
puts("<B>INFO column annotations:</B><BR>");
puts("<TABLE border=0 cellspacing=0 cellpadding=2>");
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
    if (def != NULL)
	printf("&nbsp;&nbsp;</TD><TD>%s", def->description);
    else
	printf("</TD><TD>");
    printf("</TD></TR>\n");
    }
puts("</TABLE>");
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
        printTabularHeaderRow(def);
        printTabularData(el);
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
puts("<B>Genotype info key:</B><BR>");
int i;
for (i = 0;  i < formatCount;  i++)
    {
    if (sameString(formatKeys[i], vcfGtGenotype))
	continue;
    const struct vcfInfoDef *def = vcfInfoDefForGtKey(vcff, formatKeys[i]);
    char *desc = def ? def->description : "<em>not described in VCF header</em>";
    printf("&nbsp;&nbsp;<B>%s:</B> %s<BR>\n", formatKeys[i], desc);
    formatTypes[i] = def ? def->type : vcfInfoString;
    }
hTableStart();
puts("<TR><TH>Sample ID</TH><TH>Genotype</TH><TH>Phased?</TH>");
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
    printf("<TR><TD>%s</TD><TD>%s%c%s</TD><TD>%s</TD>", vcff->genotypeIds[i],
	   hapA, sep, hapB, phasing);
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
// Tally genotypes and alleles for summary:
int refs = 0, alts = 0, unks = 0;
int refRefs = 0, refAlts = 0, altAlts = 0, gtUnk = 0, gtOther = 0, phasedGts = 0;
int i;
for (i = 0;  i < vcff->genotypeCount;  i++)
    {
    struct vcfGenotype *gt = &(rec->genotypes[i]);
    if (gt->isPhased)
	phasedGts++;
    if (gt->hapIxA == 0)
	refs++;
    else if (gt->hapIxA > 0)
	alts++;
    else
	unks++;
    if (!gt->isHaploid)
	{
	if (gt->hapIxB == 0)
	    refs++;
	else if (gt->hapIxB > 0)
	    alts++;
	else
	    unks++;
	if (gt->hapIxA == 0 && gt->hapIxB == 0)
	    refRefs++;
	else if (gt->hapIxA == 1 && gt->hapIxB == 1)
	    altAlts++;
	else if ((gt->hapIxA == 1 && gt->hapIxB == 0) ||
		 (gt->hapIxA == 0 && gt->hapIxB == 1))
	    refAlts++;
	else if (gt->hapIxA < 0 || gt->hapIxB < 0)
	    gtUnk++;
	else
	    gtOther++;
	}
    }
printf("<B>Genotype count:</B> %d", vcff->genotypeCount);
if (differentString(seqName, "chrY"))
    printf(" (%d phased)", phasedGts);
else
    printf(" (haploid)");
puts("<BR>");
int totalAlleles = refs + alts + unks;
double refAf = (double)refs/totalAlleles;
double altAf = (double)alts/totalAlleles;
printf("<B>Alleles:</B> %s: %d (%.3f%%); %s: %d (%.3f%%)",
       displayAls[0], refs, 100*refAf, displayAls[1], alts, 100*altAf);
if (unks > 0)
    printf("; unknown: %d (%.3f%%)", unks, 100 * (double)unks/totalAlleles);
puts("<BR>");
// Should be a better way to detect haploid chromosomes than comparison with "chrY":
if (vcff->genotypeCount > 1 && differentString(seqName, "chrY"))
    {
    printf("<B>Genotypes:</B> %s/%s: %d (%.3f%%); %s/%s: %d (%.3f%%); %s/%s: %d (%.3f%%)",
	   displayAls[0], displayAls[0], refRefs, 100*(double)refRefs/vcff->genotypeCount,
	   displayAls[0], displayAls[1], refAlts, 100*(double)refAlts/vcff->genotypeCount,
	   displayAls[1], displayAls[1], altAlts, 100*(double)altAlts/vcff->genotypeCount);
    if (gtUnk > 0)
	printf("; unknown: %d (%.3f%%)", gtUnk, 100*(double)gtUnk/vcff->genotypeCount);
    if (gtOther > 0)
	printf("; other: %d (%.3f%%)", gtOther, 100*(double)gtOther/vcff->genotypeCount);
    printf("<BR>\n");
    if (rec->alleleCount == 2)
	{
	boolean showHW = cartOrTdbBoolean(cart, tdb, VCF_SHOW_HW_VAR, FALSE);
	if (showHW)
	    printf("<B><A HREF=\"http://en.wikipedia.org/wiki/Hardy%%E2%%80%%93Weinberg_principle\" "
		   "TARGET=_BLANK>Hardy-Weinberg equilibrium</A>:</B> "
		   "P(%s/%s) = %.3f%%; P(%s/%s) = %.3f%%; P(%s/%s) = %.3f%%<BR>",
		   displayAls[0], displayAls[0], 100*refAf*refAf,
		   displayAls[0], displayAls[1], 100*2*refAf*altAf,
		   displayAls[1], displayAls[1], 100*altAf*altAf);
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

static void vcfRecordDetails(struct trackDb *tdb, struct vcfRecord *rec)
/* Display the contents of a single line of VCF, assumed to be from seqName
 * (using seqName instead of rec->chrom because rec->chrom might lack "chr"). */
{
if (isNotEmpty(rec->name) && differentString(rec->name, "."))
    printf("<B>Name:</B> %s<BR>\n", rec->name);
if (sameString(tdb->track, "exacVariants"))
    {
    printf("<b>ExAC:</b> "
           "<a href=\"http://exac.broadinstitute.org/variant/%s-%d-%s-%s\" "
           "target=_blank>%s:%d %s/%s</a><br>\n",
           skipChr(rec->chrom), rec->chromStart+1, rec->alleles[0], rec->alleles[1],
           skipChr(rec->chrom), rec->chromStart+1, rec->alleles[0], rec->alleles[1]);
    }
if (sameString(tdb->track, "gnomadVariants"))
    {
    printf("<b>gnomAD:</b> "
           "<a href=\"http://gnomad.broadinstitute.org/variant/%s-%d-%s-%s\" "
           "target=_blank>%s:%d %s/%s</a><br>\n",
           skipChr(rec->chrom), rec->chromStart+1, rec->alleles[0], rec->alleles[1],
           skipChr(rec->chrom), rec->chromStart+1, rec->alleles[0], rec->alleles[1]);
    }
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
printCustomUrl(tdb, rec->name, TRUE);
boolean hapClustEnabled = cartOrTdbBoolean(cart, tdb, VCF_HAP_ENABLED_VAR, TRUE);
if (hapClustEnabled)
    {
    static char *formName = "vcfCfgHapCenter";
    printf("<FORM NAME=\"%s\" ACTION=\"%s\">\n", formName, hgTracksName());
    cartSaveSession(cart);
    vcfCfgHaplotypeCenter(cart, tdb, tdb->track, FALSE, rec->file, rec->name,
			  seqName, rec->chromStart, formName);
    printf("</FORM>\n");
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
vcfInfoDetails(rec);
pgSnpCodingDetail(rec);
makeDisplayAlleles(rec, showLeftBase, leftBase, 5, FALSE, TRUE, displayAls);
vcfGenotypesDetails(rec, tdb, displayAls);
}

void doVcfDetailsCore(struct trackDb *tdb, char *fileOrUrl, boolean isTabix)
/* Show item details using fileOrUrl. */
{
genericHeader(tdb, NULL);
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
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
if (vcff != NULL)
    {
    struct vcfRecord *rec;
    for (rec = vcff->records;  rec != NULL;  rec = rec->next)
	if (rec->chromStart == start && rec->chromEnd == end) // in pgSnp mode, don't get name
	    vcfRecordDetails(tdb, rec);
    }
else
    printf("Sorry, unable to open %s<BR>\n", fileOrUrl);
printTrackHtml(tdb);
}



void doVcfTabixDetails(struct trackDb *tdb, char *item)
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
doVcfDetailsCore(tdb, fileOrUrl, TRUE);
}




void doVcfDetails(struct trackDb *tdb, char *item)
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
doVcfDetailsCore(tdb, fileOrUrl, FALSE);
}
