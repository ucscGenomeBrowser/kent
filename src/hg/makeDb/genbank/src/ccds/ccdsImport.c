/* ccdsImport - convert NCBI CCDS accession file */

#include "common.h"
#include "options.h"
#include "ccdsAccessions.h"
#include "genePred.h"
#include "sqlNum.h"
#include "ccdsInfo.h"
#include "gbFileOps.h"

static char const rcsid[] = "$Id: ccdsImport.c,v 1.1 2005/02/25 01:18:40 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "ccdsImport - convert NCBI CCDS accession file\n"
  "\n"
  "Usage:\n"
  "   ccdsImport [options] accFile genePredOut ccdsInfoOut\n"
  "   Output files compressed if they end in .gz.\n"
  "Options:\n"
  "\n"
  );
}

int chopStringCp(char *in, char *sep, char **outArrayPtr[])
/* Copy a string and chop it as with chopString.  This allocates a single
 * memory block containing both the pointer array and the string.  The in
 * string is not modified. */
{
int n = chopString(in, sep, NULL, 0);
char **arr = needMem((n*sizeof(char*)) + strlen(in)+1);
char *str = (char*)(arr+n);
strcpy(str, in);
chopString(str, sep, arr, n);
*outArrayPtr = arr;
return n;
}

void parseAccLists(struct ccdsAccessions *ccdsRec)
/* parse the mrna and protein acc lists */
{
int n;

ccdsRec->ncbiAccCnt = chopStringCp(ccdsRec->ncbi_mrna, ";", &ccdsRec->ncbiMRnaLst);
n = chopStringCp(ccdsRec->ncbi_prot, ";", &ccdsRec->ncbiProtLst);
if (n != ccdsRec->ncbiAccCnt)
    errAbort("%s: number of ncbi_mrna entries doesn't match ncbi_prot", ccdsRec->ccds);
ccdsRec->hinxtonAccCnt = chopStringCp(ccdsRec->hinxton_mrna, ";", &ccdsRec->hinxtonMRnaLst);
n = chopStringCp(ccdsRec->hinxton_prot, ";", &ccdsRec->hinxtonProtLst);
if (n != ccdsRec->hinxtonAccCnt)
    errAbort("%s: number of hinxton_mrna entries doesn't match hinxton_prot", ccdsRec->ccds);
}

struct exonCoords parseCcdsExon(char *exonStr, char* errDesc)
/* parse a exon string from a ccds record */
{
struct exonCoords exonCoords;
char *sep;

exonStr = trimSpaces(exonStr);
sep = strchr(exonStr, '-');
if (sep == NULL)
    errAbort("invalid ccds cds_loc for %s", errDesc);
*sep++ = '\0';

exonCoords.start = sqlUnsigned(exonStr);
exonCoords.end = sqlUnsigned(sep);
return exonCoords;
}

void parseCcdsExons(struct ccdsAccessions *ccdsRec)
/* convert exon list to exonStarts and exonEnds arrays
 * [989020-989022, 989430-989924] */
{
char *exonsStrBuf = cloneString(ccdsRec->cds_loc);
char *exonsStr = exonsStrBuf;
int exonsStrLen = strlen(exonsStr);
int iExon;
char **exonLocs;

/* drop [ and ] from string */
if ((exonsStr[0] != '[') || (exonsStr[exonsStrLen-1] != ']'))
    errAbort("invalid ccds cds_loc for %s", ccdsRec->ccds);
exonsStr[exonsStrLen-1] = '\0';
exonsStr++;

ccdsRec->numExons = chopStringCp(exonsStr, ",", &exonLocs);
ccdsRec->exons = needMem(ccdsRec->numExons*sizeof(struct exonCoords));
for (iExon = 0; iExon < ccdsRec->numExons; iExon++)
    ccdsRec->exons[iExon] = parseCcdsExon(exonLocs[iExon], ccdsRec->ccds);
freeMem(exonsStrBuf);
freeMem(exonLocs);
}

struct ccdsAccessions* loadCcds(char *accFile)
/* load CCDS accession entries from the the file. Drop non-CCDS entries. */
{
struct lineFile *lf = gzLineFileOpen(accFile);
char *row[CCDSACCESSIONS_NUM_COLS];
struct ccdsAccessions *ccdsList = NULL;

while (lineFileNextRowTab(lf, row, CCDSACCESSIONS_NUM_COLS))
    {
    struct ccdsAccessions *ccdsRec = ccdsAccessionsLoad(row);
    if (ccdsRec->ccds[0] == '\0')
        ccdsAccessionsFree(&ccdsRec);
    else
        {
        slAddHead(&ccdsList, ccdsRec);
        parseCcdsExons(ccdsRec);
        parseAccLists(ccdsRec);
        }
    }
slReverse(&ccdsList);
return ccdsList;
}

char *parseChrom(char *chromAcc)
/* parse an NCBI chromosome accession (NC_000001.8).
 * Note: currentlly human only. */
{
int chrNum;
char chrom[32];
if (sscanf(chromAcc, "NC_%d.8", &chrNum) != 1)
    errAbort("can't parse NCBI chromosome: %s", chromAcc);
if (chrNum <= 22)
    safef(chrom, sizeof(chrom), "chr%d", chrNum);
else if (chrNum == 23)
    strcpy(chrom, "chrX");
else if (chrNum == 24)
    strcpy(chrom, "chrY");
else
    errAbort("can't parse NCBI chromosome: %s", chromAcc);
return cloneString(chrom);
}

void computeFrames(struct genePred *gp)
/* compute from for the genePred.  */
{
int iExon, iBase = 0;
gp->exonFrames = needMem(gp->exonCount*sizeof(unsigned*));
if (gp->strand[0] == '+')
    {
    for (iExon = 0; iExon < gp->exonCount; iExon++)
        {
        gp->exonFrames[iExon] = iBase % 3;
        iBase += (gp->exonEnds[iExon] - gp->exonStarts[iExon]);
        }
    }
else
    {
    for (iExon = gp->exonCount-1; iExon >= 0; iExon--)
        {
        gp->exonFrames[iExon] = iBase % 3;
        iBase += (gp->exonEnds[iExon] - gp->exonStarts[iExon]);
        }
    }
}

struct genePred* ccdsToGenePred(struct ccdsAccessions *ccdsRec)
/* convert a CCDS accession record to a genePred */
{
struct genePred* gp;
int i;
AllocVar(gp);

gp->name = cloneString(ccdsRec->ccds);
gp->name2 = cloneString("");
gp->chrom = parseChrom(ccdsRec->chromosome);
gp->strand[0] = ccdsRec->cds_strand[0];
gp->txStart = ccdsRec->cds_from;
gp->txEnd = ccdsRec->cds_to+1;
gp->cdsStart = ccdsRec->cds_from;
gp->cdsEnd = ccdsRec->cds_to+1;
gp->exonCount = ccdsRec->numExons;
gp->exonStarts = needMem(ccdsRec->numExons*sizeof(unsigned*));
gp->exonEnds = needMem(ccdsRec->numExons*sizeof(unsigned*));
for (i = 0; i < gp->exonCount; i++)
    {
    gp->exonStarts[i] = ccdsRec->exons[i].start;
    gp->exonEnds[i] = ccdsRec->exons[i].end+1;
    }
gp->optFields = genePredAllFlds;
gp->cdsStartStat = cdsComplete;
gp->cdsEndStat = cdsComplete;

computeFrames(gp);
return gp;
}

void ccdsToInfoRec(struct ccdsAccessions *ccdsRec, FILE *infoFh, char srcDb,
                   char *mrna, char *prot)
/* output a single ccdsInfo record */
{
struct ccdsInfo info;
ZeroVar(&info);
safef(info.ccds, sizeof(info.ccds), "%s", ccdsRec->ccds);
info.srcDb[0] = srcDb;
safef(info.mrnaAcc, sizeof(info.mrnaAcc), "%s", mrna);
safef(info.protAcc, sizeof(info.protAcc), "%s", prot);
ccdsInfoTabOut(&info, infoFh);
}

void ccdsToInfo(struct ccdsAccessions *ccdsRec, FILE *infoFh)
/* create ccdsInfo records from a ccdsAccessions record */
{
int numNcbi, numHinxton, n, i;
char **ncbiMrnas, **ncbiProts, **hinxtonMrnas, **hinxtonProts;

numNcbi = chopStringCp(ccdsRec->ncbi_mrna, ";", &ncbiMrnas);
n = chopStringCp(ccdsRec->ncbi_prot, ";", &ncbiProts);
if (n != numNcbi)
    errAbort("%s: number of ncbi_mrna entries doesn't match ncbi_prot", ccdsRec->ccds);
numHinxton = chopStringCp(ccdsRec->hinxton_mrna, ";", &hinxtonMrnas);
n = chopStringCp(ccdsRec->hinxton_prot, ";", &hinxtonProts);
if (n != numHinxton)
    errAbort("%s: number of hinxton_mrna entries doesn't match hinxton_prot", ccdsRec->ccds);

for (i = 0; i < numNcbi; i++)
    ccdsToInfoRec(ccdsRec, infoFh, 'N', ncbiMrnas[i], ncbiProts[i]);
for (i = 0; i < numHinxton; i++)
    ccdsToInfoRec(ccdsRec, infoFh, 'H', hinxtonMrnas[i], hinxtonProts[i]);
freeMem(ncbiMrnas);
freeMem(ncbiProts);
freeMem(hinxtonMrnas);
freeMem(hinxtonProts);
}

void ccdsImportRec(struct ccdsAccessions *ccdsRec, FILE *gpFh, FILE *infoFh)
/* process a ccds record */
{
struct genePred* gp = ccdsToGenePred(ccdsRec);
genePredTabOut(gp, gpFh);
genePredFree(&gp);

ccdsToInfo(ccdsRec, infoFh);
}

void ccdsImport(char *accFile, char *genePredFile, char *ccdsInfoFile)
/* convert NCBI CCDS accession file */
{
struct ccdsAccessions *ccdsRec, *ccdsList = loadCcds(accFile);
FILE *gpFh = mustOpen(genePredFile, "w");
FILE *infoFh = mustOpen(ccdsInfoFile, "w");

for (ccdsRec = ccdsList; ccdsRec != NULL; ccdsRec = ccdsRec->next)
    ccdsImportRec(ccdsRec, gpFh, infoFh);

carefulClose(&gpFh);
carefulClose(&infoFh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage();
ccdsImport(argv[1], argv[2], argv[3]);
return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

