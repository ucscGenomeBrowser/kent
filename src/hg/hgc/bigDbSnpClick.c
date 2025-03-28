/* Show details for bigDbSnp track items. */

/* Copyright (C) 2019 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hgc.h"
#include "bigDbSnp.h"
#include "dbSnpDetails.h"
#include "bPlusTree.h"
#include "htslib/bgzf.h"
#include "soTerm.h"
#include "chromAlias.h"
#include "quickLift.h"

static struct dbSnpDetails *getDetails(struct bigDbSnp *bds, char *detailsFileOrUrl)
/* Seek to the offset for this variant in detailsFileOrUrl, read the line and load as
 * struct dbSnpDetails.  */
{
bits64 offset = bds->_dataOffset;
bits64 len = bds->_dataLen;
char *line = readOneLineMaybeBgzip(detailsFileOrUrl, offset, len);
// Newline must be trimmed or else it messes up parsing of final column if empty!
if (line[len-1] == '\n')
    line[len-1] = '\0';
char *row[DBSNPDETAILS_NUM_COLS+1];
int wordCount = chopTabs(line, row);
if (wordCount != DBSNPDETAILS_NUM_COLS)
    errAbort("dbSnpDetails: expected %d tab-separated words at offset %Ld in %s, got %d",
             DBSNPDETAILS_NUM_COLS, offset, detailsFileOrUrl, wordCount);
return dbSnpDetailsLoad(row);
}

struct slName *getFreqSourceOrder(struct trackDb *tdb, char *rsId, int expectedCount)
/* If tdb has freqSourceOrder*/
{
struct slName *sourceList = NULL;
char *sourceNames = trackDbSetting(tdb, "freqSourceOrder");
if (sourceNames)
    {
    sourceList = slNameListFromComma(sourceNames);
    int settingCount = slCount(sourceList);
    if (settingCount != expectedCount)
        errAbort("bigDbSnp freqSourceCount for %s is %d, "
                 "but trackDb setting freqSourceOrder lists %d sources",
                 rsId, expectedCount, settingCount);
    }
return sourceList;
}

INLINE boolean freqSourceHasData(struct dbSnpDetails *details, int sourceIx)
/* Return TRUE if freqSource sourceIx has any data for this variant. */
{
return details->alleleTotals[sourceIx] > 0;
}

struct alCounts
    {
    int obsCount;   // Number of chromosomes on which allele was observed by a project
    int totalCount; // Number of chromosomes on which project observed some allele of this variant
    };

static struct hash *makePerAlleleCounts(struct dbSnpDetails *details, boolean isRc)
/* Return a hash of allele to array of alCounts (each source's frequency count data). */
{
struct hash *perAlleleCounts = hashNew(0);
int sIx;
for (sIx = 0;  sIx < details->freqSourceCount;  sIx++)
    {
    if (freqSourceHasData(details, sIx))
        {
        int totalCount = details->alleleTotals[sIx];
        char *blob = cloneString(details->alleleCounts[sIx]);
        struct slName *pab, *perAlleleBlobs = slNameListFromString(blob, '|');
        for (pab = perAlleleBlobs;  pab != NULL;  pab = pab->next)
            {
            char *words[3];
            int wordCount = chopByChar(pab->name, ':', words, ArraySize(words));
            if (wordCount != 2)
                errAbort("Malformed allele:count in |-separated '%s': "
                         "expecting two :-separated words but got %d", pab->name, wordCount);
            char *allele = words[0];
            if (isRc)
                reverseComplement(allele, strlen(allele));
            int obsCount = atoi(words[1]);
            struct alCounts *alArray = hashFindVal(perAlleleCounts, allele);
            if (alArray == NULL)
                {
                AllocArray(alArray, details->freqSourceCount);
                hashAdd(perAlleleCounts, allele, alArray);
                }
            alArray[sIx].obsCount = obsCount;
            alArray[sIx].totalCount = totalCount;
            }
        }
    }
return perAlleleCounts;
}

static void printAlleleRow(struct hash *perAlleleCounts, char *allele,
                           struct dbSnpDetails *details, char **perSourceMajorAl)
/* Print the allele and its counts/freqs from each freqSource for allele, if any. */
{
puts("<tr>");
char buf[1024];
char *abbrevAl = isEmpty(allele) ? "-" : bigDbSnpAbbrevAllele(allele, buf, sizeof buf);
printf("<td><span class='inlineCode'>%s</span></td>", abbrevAl);
struct alCounts *alleleCounts = hashFindVal(perAlleleCounts, allele);
int sIx;
for (sIx = 0;  sIx < details->freqSourceCount;  sIx++)
    {
    if (freqSourceHasData(details, sIx))
        {
        struct alCounts *counts = NULL;
        if (alleleCounts)
            counts = &alleleCounts[sIx];
        if (counts != NULL && counts->totalCount != 0)
            {
            boolean isMajor = sameString(perSourceMajorAl[sIx], allele);
            printf("<td>");
            if (isMajor)
                printf("<b>");
            printf("%d/%d (%f)", counts->obsCount, counts->totalCount,
                   ((double)counts->obsCount / (double)counts->totalCount));
            if (isMajor)
                printf("</b>");
            }
        else
            printf("<td>-</td>");
        }
    }
puts("</tr>");
}

static void printAlleleCountsAndFreqs(struct bigDbSnp *bds, struct dbSnpDetails *details,
                                      struct trackDb *tdb)
/* Print a table row containing a table of alleles, counts & frequencies if applicable. */
{
// Allele counts & frequencies
if (bds->freqSourceCount > 0)
    {
    puts("<tr><td colspan=2><b>Allele frequency counts:</b></td></tr>");
    puts("<tr><td colspan=2><table class='stdTbl'>");
    struct slName *sourceList = getFreqSourceOrder(tdb, bds->name, bds->freqSourceCount);
    int sIx;
    if (sourceList)
        {
        puts("<tr><th>Allele</th>");
        struct slName *source = sourceList;
        for (sIx = 0;  sIx < bds->freqSourceCount;  sIx++, source = source->next)
            if (freqSourceHasData(details, sIx))
                printf("<th>%s</th> ", source->name);
        puts("</tr>");
        }
    boolean isRc = (stringIn(bdsRevStrand, bds->ucscNotes) != NULL);
    struct hash *perAlleleCounts = makePerAlleleCounts(details, isRc);
    // Row for reference allele
    printAlleleRow(perAlleleCounts, bds->ref, details, bds->majorAllele);
    hashRemove(perAlleleCounts, bds->ref);
    // Rows for alternate alleles
    int aIx;
    for (aIx = 0;  aIx < bds->altCount;  aIx++)
        {
        char *alt = bds->alts[aIx];
        printAlleleRow(perAlleleCounts, alt, details, bds->majorAllele);
        hashRemove(perAlleleCounts, alt);
        }
    // Rows for frequency alleles not included in ref/alt, if any
    struct hashEl *hel, *helList = hashElListHash(perAlleleCounts);
    for (hel = helList;  hel != NULL;  hel = hel->next)
        {
        char *alt = hel->name;
        printAlleleRow(perAlleleCounts, alt, details, bds->majorAllele);
        }
    puts("</table>");
    puts("</td></tr>");
    }
}

static void printOneSoTerm(struct dbSnpDetails *details, int ix)
/* Print a SO term with link to MISO browser for details->soTerms[ix]. */
{
printf("%s", soTermToMisoLink((enum soTerm)details->soTerms[ix]));
}

static void printOneClinVar(struct dbSnpDetails *details, int ix)
/* Print a link to ClinVar for details->clinVar{Accs,Sigs}[ix]. */
{
printf("<a href='https://www.ncbi.nlm.nih.gov/clinvar/%s' target=_blank>%s</a> (%s)",
       details->clinVarAccs[ix], details->clinVarAccs[ix], details->clinVarSigs[ix]);
}

static void printOneSubmitter(struct dbSnpDetails *details, int ix)
/* Print a link to dbSNP submitter page for details->submitters[ix]. */
{
printf("<a href=\"https://www.ncbi.nlm.nih.gov/SNP/snp_viewTable.cgi?h=%s\" target=_blank>"
       "%s</a>", details->submitters[ix], details->submitters[ix]);
}

static void printOnePub(struct dbSnpDetails *details, int ix)
/* Print a link to Pubmed for details->pubMedIds[ix]. */
{
printf("<a href=\"https://www.ncbi.nlm.nih.gov/pubmed/%d\" target=_blank>PMID%d</a>\n",
       details->pubMedIds[ix], details->pubMedIds[ix]);
}

static void printDetailsRow(struct dbSnpDetails *details, char *label, int itemCount,
                            void (*printOneValue)(struct dbSnpDetails *details, int ix))
/* Print out one row of a details table using callback to print comma-sep list of values. */
{
if (itemCount > 0)
    {
    printf("<tr><td><b>%s:</b></td><td>", label);
    int ix;
    for (ix = 0;  ix < itemCount;  ix++)
        {
        if (ix != 0)
            puts(", ");
        printOneValue(details, ix);
        }
    puts("</td></tr>");
    }
}

static void printDbSnpDetails(struct bigDbSnp *bds, struct dbSnpDetails *details,
                              struct trackDb *tdb)
/* Print out extras from dbSnpDetails file. */
{
printAlleleCountsAndFreqs(bds, details, tdb);
printDetailsRow(details, "Functional effects", details->soTermCount, printOneSoTerm);
printDetailsRow(details, "ClinVar", details->clinVarCount, printOneClinVar);
printDetailsRow(details, "Submitted by", details->submitterCount, printOneSubmitter);
printDetailsRow(details, "Publications in PubMed", details->pubMedIdCount, printOnePub);
}

static void printUcscNotes(char *ucscNotes)
/* Print explanations of ucscNotes items. */
{
if (isNotEmpty(ucscNotes))
    {
    puts("<p>Interesting or anomalous conditions noted by UCSC:</b><br>");
    puts("<ul>");
    boolean isCommonAll = (stringIn(bdsCommonAll, ucscNotes) != NULL);
    boolean isRareAll = (stringIn(bdsRareAll, ucscNotes) != NULL);
    struct slName *note, *noteList = slNameListFromComma(ucscNotes);
    for (note = noteList;  note != NULL;  note = note->next)
        {
        // When commonAll is true, commonSome is also true but not informative,
        // so skip commonSome if commonAll is true.  Likewise for rareAll & rareSome.
        if (! ((isCommonAll && sameString(note->name, bdsCommonSome)) ||
               (isRareAll && sameString(note->name, bdsRareSome))))
            {
            char *desc = bigDbSnpDescribeUcscNote(note->name);
            printf("<li>%s\n", desc ? desc : note->name);
            }
        }
    puts("</ul>");
    }
}

static char *getMinRep(char *ref, char *alt, boolean leftJustify)
/* If ref and alt can be trimmed down to a shorter representation then return that, othw NULL. */
{
char *minRep = NULL;
int refLen = strlen(ref);
int altLen = strlen(alt);
if ((refLen > 1 && altLen > 0) || (altLen > 1 && refLen > 0))
    {
    char *refCpy = cloneStringZ(ref, refLen);
    char *altCpy = cloneStringZ(alt, altLen);
    uint start=0, end=refLen;
    int refCpyLen = refLen, altCpyLen = altLen;
    if (leftJustify)
        trimRefAltLeft(refCpy, altCpy, &start, &end, &refCpyLen, &altCpyLen);
    else
        trimRefAlt(refCpy, altCpy, &start, &end, &refCpyLen, &altCpyLen);
    if (refCpyLen < refLen)
        {
        struct dyString *dy = dyStringNew(0);
        char bufRef[1024], bufAlt[1024];
        if (refCpyLen == 0)
            dyStringPrintf(dy, "ins%s",
                           bigDbSnpAbbrevAllele(altCpy, bufAlt, sizeof bufAlt));
        else if (altCpyLen == 0)
            dyStringPrintf(dy, "del%s",
                           bigDbSnpAbbrevAllele(refCpy, bufRef, sizeof bufRef));
        else
            dyStringPrintf(dy, "del%sins%s",
                           bigDbSnpAbbrevAllele(refCpy, bufRef, sizeof bufRef),
                           bigDbSnpAbbrevAllele(altCpy, bufAlt, sizeof bufAlt));
        minRep = dyStringCannibalize(&dy);
        }
    }
return minRep;
}

static void printOtherMappings(struct bbiFile *bbi, struct bigDbSnp *bds, struct trackDb *tdb)
/* If the variant maps to other genomic sequences (alts/fixes/PAR), link to those positions. */
{
int fieldIx;
struct bptFile *bpt = bigBedOpenExtraIndex(bbi, "name", &fieldIx);
struct lm *lm = lmInit(0);
struct bigBedInterval *bbList = bigBedNameQuery(bbi, bpt, fieldIx, bds->name, lm);
if (slCount(bbList) > 1)
    {
    puts("<p><b>This variant maps to additional locations:</b><br><br>");
    char chromName[bbi->chromBpt->keySize+1];
    int lastChromId = -1;
    struct bigBedInterval *bb;
    for (bb = bbList; bb != NULL; bb = bb->next)
        {
        if (!startsWithWord(bds->name, bb->rest))
            errAbort("Error: bigBedNameQuery search for name '%s' yielded '%s'",
                     bds->name, bb->rest);
        bbiCachedChromLookup(bbi, bb->chromId, lastChromId, chromName, sizeof(chromName));
        char startBuf[16], endBuf[16];
        char *row[BIGDBSNP_NUM_COLS];
        int bbFieldCount = bigBedIntervalToRow(bb, chromName, startBuf, endBuf, row, ArraySize(row));
        if (bbFieldCount != BIGDBSNP_NUM_COLS)
            errAbort("bigDbSnpClick: expected %d columns but got %d", BIGDBSNP_NUM_COLS,
                     bbFieldCount);
        struct bigDbSnp *otherBds = bigDbSnpLoad(row);
        if (differentString(bds->chrom, otherBds->chrom) ||
            bds->chromStart != otherBds->chromStart ||
            bds->chromEnd != otherBds->chromEnd)
            {
            bedPrintPos((struct bed *)otherBds, 3, tdb);
            if (bb->next != NULL)
                puts("<br>");
            }
        }
    puts("</p>");
    }
bptFileDetach(&bpt);
lmCleanup(&lm);
}

struct snp125 *bdsTosnp125(struct bigDbSnp *bds)
/* Copy over the bed6 plus observed fields. */
{
struct snp125 *snp125;
AllocVar(snp125);
snp125->chrom = cloneString(bds->chrom);
snp125->chromStart = bds->chromStart;
snp125->chromEnd = bds->chromEnd;
snp125->name = cloneString(bds->name);
snp125->refUCSC = cloneString(bds->ref);

snp125->strand = cloneString("+");

struct dyString *dy = dyStringNew(0);
int i;
for (i = 0;  i < bds->altCount;  i++)
    {
    char *alt = bds->alts[i];
    if (i > 0)
    dyStringPrintf(dy, "/");
    dyStringPrintf(dy, "%s", alt);
    }
snp125->observed = cloneString(dyStringCannibalize(&dy));

return snp125;
}


void doBigDbSnp(struct trackDb *tdb, char *rsId)
/* Show details for bigDbSnp item. */
{
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
char *fileOrUrl = hReplaceGbdb(trackDbSetting(tdb, "bigDataUrl"));
if (isEmpty(fileOrUrl))
    errAbort("bigDbSnpClick: trackDb is missing bigDataUrl setting");
struct bbiFile *bbi =  bigBedFileOpenAlias(fileOrUrl, chromAliasFindAliases);
boolean found = FALSE;
char *chrom = cartString(cart, "c");
int ivStart = start, ivEnd = end;
if (start == end)
    {
    // item is an insertion; expand the search range from 0 bases to 2 so we catch it:
    ivStart = max(0, start-1);
    ivEnd++;
    }
struct lm *lm = lmInit(0);
char *quickLiftFile = cloneString(trackDbSetting(tdb, "quickLiftUrl"));
struct hash *chainHash = NULL;
struct bigBedInterval *bbList = NULL;
if (quickLiftFile)
    bbList = quickLiftGetIntervals(quickLiftFile, bbi, chrom, ivStart, ivEnd, &chainHash);
else
    bbList = bigBedIntervalQuery(bbi, chrom, ivStart, ivEnd, 0, lm);
struct bigBedInterval *bb;
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    if (!startsWithWord(rsId, bb->rest))
	continue;
    found = TRUE;
    char startBuf[16], endBuf[16];
    char *row[BIGDBSNP_NUM_COLS];
    int bbFieldCount = bigBedIntervalToRow(bb, chrom, startBuf, endBuf, row, ArraySize(row));
    if (bbFieldCount != BIGDBSNP_NUM_COLS)
        errAbort("bigDbSnpClick: expected %d columns but got %d", BIGDBSNP_NUM_COLS, bbFieldCount);
    struct bigDbSnp *bds = bigDbSnpLoad(row);
    struct dbSnpDetails *details = NULL;
    struct slPair *detailsUrls = parseDetailsTablUrls(tdb);
    if (detailsUrls)
        details = getDetails(bds, detailsUrls->val);
    bedPrintPos((struct bed *)bds, 3, tdb);
    puts("<br>");
    puts("<table>");
    char *ref = bds->ref;
    char abbrev[1024];
    char *abbrevRef = isEmpty(ref) ? "-" : bigDbSnpAbbrevAllele(ref, abbrev, sizeof abbrev);
    printf("<tr><td><b>Reference allele:</b></td><td><span class='inlineCode'>%s</span><td></tr>\n",
           abbrevRef);
    printf("<tr><td");
    if (bds->altCount > 1)
        printf(" style='vertical-align:top'");
    printf("><b>Alternate allele%s:</b></td><td>", (bds->altCount > 1 ? "s" : ""));
    if (bds->altCount == 0)
        printf("<em>none</em>");
    else
        {
        int i;
        for (i = 0;  i < bds->altCount;  i++)
            {
            char *alt = bds->alts[i];
            char *abbrevAlt = isEmpty(alt) ? "-" : bigDbSnpAbbrevAllele(alt, abbrev, sizeof abbrev);
            printf("%s<span class='inlineCode'>%s", (i > 0 ? ",<br>\n" : ""), abbrevAlt);
            char *minRepLeft = getMinRep(ref, alt, TRUE);
            if (minRepLeft)
                {
                char *minRepRight = getMinRep(ref, alt, FALSE);
                if (sameString(minRepLeft, minRepRight))
                    printf(" [%s]", minRepLeft);
                else
                    printf(" [%s (left-shifted), %s (right-shifted)]", minRepLeft, minRepRight);
                }
            printf("</span>");
            }
        puts("</td></tr>");
        }
    if (bds->shiftBases > 0)
        printf("<td><b>Uncertainty in indel placement:</b></td><td>%d base%s</td></tr>\n",
               bds->shiftBases, (bds->shiftBases > 1 ? "s" : ""));
    if (details)
        printDbSnpDetails(bds, details, tdb);
    printf("<tr><td><b>Variation class/type:</b></td><td>%s</td></tr>\n",
           bigDbSnpClassToString(bds->class));

    puts("</table>");
    printUcscNotes(bds->ucscNotes);

    struct snp125 *snp125 = bdsTosnp125(bds);
    printSnp153Function(tdb, snp125);

    printOtherMappings(bbi, bds, tdb);
    }
if (!found)
    printf("No item %s starting at %s:%d\n", rsId, chrom, start+1);
lmCleanup(&lm);
bbiFileClose(&bbi);
}
