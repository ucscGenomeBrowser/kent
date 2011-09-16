/* vcfTrack -- handlers for Variant Call Format data. */

#ifdef USE_TABIX

#include "common.h"
#include "dystring.h"
#include "errCatch.h"
#include "hCommon.h"
#include "hdb.h"
#include "hgc.h"
#include "htmshell.h"
#include "jsHelper.h"
#if (defined USE_TABIX && defined KNETFILE_HOOKS)
#include "knetUdc.h"
#include "udc.h"
#endif//def USE_TABIX && KNETFILE_HOOKS
#include "pgSnp.h"
#include "trashDir.h"
#include "vcf.h"
#include "vcfUi.h"

#define NA "<em>n/a</em>"

static void printKeysWithDescriptions(struct vcfFile *vcff, int wordCount, char **words,
				      struct vcfInfoDef *infoDefs)
/* Given an array of keys, print out a list of values with
 * descriptions if descriptions are available. */
{
int i;
for (i = 0;  i < wordCount; i++)
    {
    if (i > 0)
	printf(", ");
    char *key = words[i];
    const struct vcfInfoDef *def = vcfInfoDefForKey(vcff, key);
    if (def != NULL)
	printf("%s (%s)", htmlEncode(key), def->description);
    else
	printf("%s", htmlEncode(key));
    }
printf("<BR>\n");
}

static void vcfAltAlleleDetails(struct vcfRecord *rec)
/* If VCF header specifies any symbolic alternate alleles, pull in descriptions. */
{
printf("<B>Alternate allele(s):</B> ");
if (rec->alleleCount < 2 || sameString(rec->alleles[1], "."))
    {
    printf(NA"<BR>\n");
    return;
    }
struct vcfFile *vcff = rec->file;
printKeysWithDescriptions(vcff, rec->alleleCount-1, &(rec->alleles[1]), vcff->altDefs);
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
    printKeysWithDescriptions(vcff, rec->filterCount, rec->filters, vcff->filterDefs);
    printf("</font>\n");
    }
}

static void vcfInfoDetails(struct vcfRecord *rec)
/* Expand info keys to descriptions, then print out keys and values. */
{
if (rec->infoCount == 0)
    return;
struct vcfFile *vcff = rec->file;
puts("<B>INFO column annotations:</B><BR>");
puts("<TABLE border=0 cellspacing=0 cellpadding=0>");
int i;
for (i = 0;  i < rec->infoCount;  i++)
    {
    struct vcfInfoElement *el = &(rec->infoElements[i]);
    const struct vcfInfoDef *def = vcfInfoDefForKey(vcff, el->key);
    if (def == NULL)
	continue;
    printf("<TR><TD align=\"right\"><B>%s:</B></TD><TD>&nbsp;", el->key);
    int j;
    enum vcfInfoType type = def->type;
    if (type == vcfInfoFlag && el->count == 0)
	printf("Yes"); // no values, so we can't call vcfPrintDatum...
    // However, if this is older VCF, type vcfInfoFlag might have a value.
    for (j = 0;  j < el->count;  j++)
	{
	if (j > 0)
	    printf(", ");
	vcfPrintDatum(stdout, el->values[j], type);
	}
    if (def != NULL)
	printf("</TD><TD>&nbsp;%s", def->description);
    else
	printf("</TD><TD>");
    printf("</TD></TR>\n");
    }
puts("</TABLE>");
}

static void ignoreEm(char *format, va_list args)
/* Ignore warnings from genotype parsing -- when there's one, there
 * are usually hundreds more just like it. */
{
}

static void vcfGenotypesDetails(struct vcfRecord *rec, char *track)
/* Print genotypes in some kind of table... */
{
struct vcfFile *vcff = rec->file;
if (vcff->genotypeCount == 0)
    return;
static struct dyString *tmp1 = NULL;
if (tmp1 == NULL)
    tmp1 = dyStringNew(0);
pushWarnHandler(ignoreEm);
vcfParseGenotypes(rec);
popWarnHandler();
// Tally genotypes and alleles for summary:
int refs = 0, alts = 0, refRefs = 0, refAlts = 0, altAlts = 0, gtOther = 0, phasedGts = 0;
int i;
for (i = 0;  i < vcff->genotypeCount;  i++)
    {
    struct vcfGenotype *gt = &(rec->genotypes[i]);
    if (gt->isPhased)
	phasedGts++;
    if (gt->hapIxA == 0)
	refs++;
    else
	alts++;
    if (!gt->isHaploid)
	{
	if (gt->hapIxB == 0)
	    refs++;
	else
	    alts++;
	if (gt->hapIxA == 0 && gt->hapIxB == 0)
	    refRefs++;
	else if (gt->hapIxA == 1 && gt->hapIxB == 1)
	    altAlts++;
	else if ((gt->hapIxA == 1 && gt->hapIxB == 0) ||
		 (gt->hapIxA == 0 && gt->hapIxB == 1))
	    refAlts++;
	else
	    gtOther++;
	}
    }
printf("<B>Genotype count:</B> %d (%d phased)<BR>\n", vcff->genotypeCount, phasedGts);
double refAf = (double)refs/(2*vcff->genotypeCount);
double altAf = (double)alts/(2*vcff->genotypeCount);
printf("<B>Alleles:</B> %s: %d (%.3f%%); %s: %d (%.3f%%)<BR>\n",
       rec->alleles[0], refs, 100*refAf,  rec->alleles[1], alts, 100*altAf);
if (vcff->genotypeCount > 1)
    {
    printf("<B>Genotypes:</B> %s/%s: %d (%.3f%%); %s/%s: %d (%.3f%%); %s/%s: %d (%.3f%%)",
	   rec->alleles[0], rec->alleles[0], refRefs, 100*(double)refRefs/vcff->genotypeCount,
	   rec->alleles[0], rec->alleles[1], refAlts, 100*(double)refAlts/vcff->genotypeCount,
	   rec->alleles[1], rec->alleles[1], altAlts, 100*(double)altAlts/vcff->genotypeCount);
    if (gtOther > 0)
	printf("; other: %d (%.3f)", gtOther, (double)gtOther/vcff->genotypeCount);
    printf("<BR>\n");
    if (rec->alleleCount == 2)
	printf("<B>Hardy-Weinberg equilibrium:</B> "
	       "P(%s/%s) = %.3f%%; P(%s/%s) = %.3f%%; P(%s/%s) = %.3f%%<BR>",
	       rec->alleles[0], rec->alleles[0], 100*refAf*refAf,
	       rec->alleles[0], rec->alleles[1], 100*2*refAf*altAf,
	       rec->alleles[1], rec->alleles[1], 100*altAf*altAf);
    }
jsBeginCollapsibleSection(cart, track, "genotypes", "Detailed genotypes", FALSE);
dyStringClear(tmp1);
dyStringAppend(tmp1, rec->format);
enum vcfInfoType formatTypes[256];
char *formatKeys[256];
int formatCount = chopString(tmp1->string, ":", formatKeys, ArraySize(formatKeys));
puts("<B>Genotype info key:</B><BR>");
for (i = 0;  i < formatCount;  i++)
    {
    if (sameString(formatKeys[i], vcfGtGenotype))
	continue;
    const struct vcfInfoDef *def = vcfInfoDefForGtKey(vcff, formatKeys[i]);
    char *desc = def ? def->description : "<em>not described in VCF header</em>";
    printf("&nbsp;&nbsp;<B>%s:</B> %s<BR>\n", formatKeys[i], desc);
    formatTypes[i] = def->type;
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
    char *hapA = rec->alleles[gt->hapIxA];
    char *hapB = gt->isHaploid ? NA : rec->alleles[gt->hapIxB];
    char sep = gt->isPhased ? '|' : '/';
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
	    vcfPrintDatum(stdout, el->values[k], formatTypes[j]);
	    }
	printf("</TD>");
	}
    puts("</TR>");
    }
hTableEnd();
jsEndCollapsibleSection();
}

static void pgSnpCodingDetail(struct vcfRecord *rec)
/* Translate rec into pgSnp (with proper chrom name) and call Belinda's
 * coding effect predictor from pgSnp details. */
{
if (hTableExists(database, "knownGene"))
    {
    struct pgSnp *pgs = pgSnpFromVcfRecord(rec);
    if (!sameString(rec->chrom, seqName))
	// rec->chrom might be missing "chr" prefix:
	pgs->chrom = seqName;
    printSeqCodDisplay(database, pgs);
    }
}

static void vcfRecordDetails(struct trackDb *tdb, struct vcfRecord *rec)
/* Display the contents of a single line of VCF, assumed to be from seqName
 * (using seqName instead of rec->chrom because rec->chrom might lack "chr"). */
{
printf("<B>Name:</B> %s<BR>\n", rec->name);
printCustomUrl(tdb, rec->name, TRUE);
static char *formName = "vcfCfgHapCenter";
printf("<FORM NAME=\"%s\" ACTION=\"%s\">\n", formName, hgTracksName());
vcfCfgHaplotypeCenter(cart, tdb, rec->file, rec->name, seqName, rec->chromStart, formName);
printf("</FORM>\n");
printPosOnChrom(seqName, rec->chromStart, rec->chromEnd, NULL, FALSE, rec->name);
printf("<B>Reference allele:</B> %s<BR>\n", rec->alleles[0]);
vcfAltAlleleDetails(rec);
vcfQualDetails(rec);
vcfFilterDetails(rec);
vcfInfoDetails(rec);
pgSnpCodingDetail(rec);
// Wrapper table for collapsible section:
puts("<TABLE>");
vcfGenotypesDetails(rec, tdb->track);
puts("</TABLE>");
}

void doVcfTabixDetails(struct trackDb *tdb, char *item)
/* Show details of an alignment from a VCF file compressed and indexed by tabix. */
{
#if (defined USE_TABIX && defined KNETFILE_HOOKS)
knetUdcInstall();
if (udcCacheTimeout() < 300)
    udcSetCacheTimeout(300);
#endif//def USE_TABIX && KNETFILE_HOOKS
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
struct sqlConnection *conn = hAllocConnTrack(database, tdb);
// TODO: will need to handle per-chrom files like bam, maybe fold bamFileNameFromTable into this::
char *fileOrUrl = bbiNameFromSettingOrTable(tdb, conn, tdb->table);
hFreeConn(&conn);
int vcfMaxErr = -1;
struct vcfFile *vcff = NULL;
/* protect against temporary network error */
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    vcff = vcfTabixFileMayOpen(fileOrUrl, seqName, start, end, vcfMaxErr);
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
}


#endif // no USE_TABIX
