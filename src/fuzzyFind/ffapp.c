#include "common.h"
#include "memalloc.h"
#include "errabort.h"
#include "dnautil.h"
#include "fa.h"
#include "dnaseq.h"
#include "oldGff.h"
#include "wormdna.h"
#include "fuzzyFind.h"
#include "cheapcgi.h"
#include "htmshell.h"


void debugListAli(struct ffAli *ali, DNA *needle, DNA *hay)
/* Print out ali list for debugging purposes. */
{
int i = 1;
while (ali->left != NULL) ali = ali->left;
while (ali != NULL)
    {
    htmlParagraph("%d needle %d %d haystack %d %d", i, ali->nStart-needle, ali->nEnd-needle,
        ali->hStart-hay, ali->hEnd-hay);
    htmlHorizontalLine();
    ali = ali->right;
    ++i;
    }
}

enum sv
    {
    svBoth,
    svForward,
    svReverse,
    };

void needleInHay(char *rawNeedle, char *rawHay, enum sv strand,
    enum ffStringency stringency, char *needleName, char *hayName,
    int hayNumOffset)
/* Filter input to get rid of non-DNA cruft. Then find best match to needle in
 * haystack and display it. */
{
long needleSize = dnaFilteredSize(rawNeedle);
long haySize = dnaFilteredSize(rawHay);
DNA *needle = needMem(needleSize+1);
DNA *mixedCaseNeedle = needMem(needleSize+1);
DNA *hay = needMem(haySize+1);
DNA *mixedCaseHay = needMem(haySize+1);
DNA *needleEnd = needle + needleSize;
DNA *hayEnd = hay + haySize;
struct ffAli *bestAli;
struct ffAli *forwardAli = NULL;
struct ffAli *reverseAli = NULL;
int forwardScore = -0x7fff;
int reverseScore = -0x7fff;

dnaMixedCaseFilter(rawNeedle, mixedCaseNeedle);
dnaMixedCaseFilter(rawHay, mixedCaseHay);
dnaFilter(mixedCaseNeedle,needle);
dnaFilter(mixedCaseHay, hay);

if (strand == svBoth || strand == svForward)
    {
    forwardAli = ffFind(needle, needleEnd, hay, hayEnd, stringency);
    forwardScore = ffScoreCdna(forwardAli);
    }
if (strand == svBoth || strand == svReverse)
    {
    reverseComplement(needle, needleSize);
    reverseAli = ffFind(needle, needleEnd, hay, hayEnd, stringency);
    reverseScore = ffScoreCdna(reverseAli);
    }
bestAli = forwardAli;
if (reverseScore > forwardScore)
    {
    bestAli = reverseAli;
    }
if (!bestAli)
    {
    errAbort("Couldn't find an allignment between %s and %s",
	needleName, hayName);
    }

memcpy(needle, mixedCaseNeedle,needleSize);
memcpy(hay, mixedCaseHay,haySize);
/* debugListAli(bestAli,needle,hay); */
puts("<H3 ALIGN=CENTER>Alignment Views</H3>");

ffShowAli(bestAli, needleName, needle, 0, hayName, hay, hayNumOffset, 
    bestAli == reverseAli);
ffFreeAli(&forwardAli);
ffFreeAli(&reverseAli);
}

struct cgiChoice stringencyChoices[] =
{
    {"exactly", ffExact},
    {"cDNA",    ffCdna},
    {"tightly", ffTight},
    {"loosely", ffLoose},
};

struct cgiChoice strandChoices[] =
{
    {"both",    svBoth,},
    {"forward", svForward,},
    {"reverse", svReverse,},
};

void directDoMiddle()
/* Grab the form variables from CGI and call routine that does real
 * work on them. */
{
char *rawNeedle = cgiString("needle");
char *rawHay = cgiString("hayStack");
enum ffStringency stringency = cgiOneChoice("stringency", 
    stringencyChoices, ArraySize(stringencyChoices));
enum sv strand = cgiOneChoice("strand", strandChoices, ArraySize(strandChoices));

needleInHay(rawNeedle, rawHay, strand, stringency, "needle", "haystack", 0);
}

void upcExons(struct gffGene *gene)
{
GffExon *exon;
for (exon = gene->exons; exon != NULL; exon = exon->next)
    {
    toUpperN(gene->dna + exon->start, exon->end - exon->start+1);
    }
}

void showInfo(struct wormCdnaInfo *info, struct dnaSeq *cdna)
/* Display some info, with hyperlinks and stuff. */
{
if (info->description != NULL)
    {
    char *gene = info->gene;
    char *cdnaName = cdna->name;

    puts("<H3 ALIGN=CENTER>cDNA Information and Links</H3>");
    printf("<P>%s</P>\n", info->description);
    if (gene)
        {
        printf("Literature on gene: ");
        printf("<A HREF=\"http://www.ncbi.nlm.nih.gov/htbin-post/Entrez/query?form=4&db=m&term=C+elegans+%s&dispmax=50&relentrezdate=No+Limit\">",
            gene);
        printf("%s</A><BR>\n", gene); 
        }
    if (info->product)
        {
        char *encoded = cgiEncode(info->product);
        printf("Literature on product: ");
        printf("<A HREF=\"http://www.ncbi.nlm.nih.gov/htbin-post/Entrez/query?form=4&db=m&term=%s&dispmax=50&relentrezdate=No+Limit\">",
            encoded);
        printf("%s</A><BR>\n", info->product);
        freeMem(encoded);
        }
    if (info->knowStart)
        {
        char protBuf[41];
        dnaTranslateSome(cdna->dna + info->cdsStart-1, protBuf, sizeof(protBuf));
        printf("Translation: ");
        printf("<A HREF=\"../cgi-bin/getgene.exe?geneName=%s&translate=on&utr5=%d\">",
            cdnaName, info->cdsStart);
        printf("%s</A>", protBuf);
        if (strlen(protBuf) == sizeof(protBuf)-1)
            printf("...");
        printf("<BR>\n");
        }
    printf("GenBank accession: %s<BR>\n", cdnaName);
    htmlHorizontalLine();
    }
}

void lookupDoMiddle()
{
char *cdnaName;
char *geneName = cgiString("gene");
struct dnaSeq *cdna;
enum sv strand = svBoth;
enum ffStringency stringency = ffCdna;
char hayStrand = '+';
DNA *dna;

if (cgiVarExists("strand")) {
    strand = cgiOneChoice("strand", strandChoices, ArraySize(strandChoices));
    }
if (cgiVarExists("stringency"))
    {
    stringency = cgiOneChoice("stringency", 
        stringencyChoices, ArraySize(stringencyChoices));
    }
if (cgiVarExists("hayStrand"))
    hayStrand = cgiString("hayStrand")[0];
if ((cdnaName = cgiOptionalString("cDNA")) != NULL)
    {
    struct wormCdnaInfo info;
    if (!wormCdnaSeq(cdnaName, &cdna, &info))
        errAbort("Couldn't find cDNA %s\n", cdnaName);
    showInfo(&info, cdna);
    wormFreeCdnaInfo(&info);
    }
else if ((cdnaName = cgiOptionalString("needleFile")) != NULL)
    {
    cdna = faReadDna(cdnaName);
    cdnaName = "pasted";
    }
else
    {
    errAbort("Can't find cDNA or needleFile in cgi variables.");
    }
if (wormIsChromRange(geneName))
    {
    char *dupeName = cloneString(geneName);
    char *chrom;
    int start, end;
    wormParseChromRange(dupeName, &chrom, &start, &end);
    dna = wormChromPartExonsUpper(chrom, start, end-start);
    if (hayStrand == '-')
        reverseComplement(dna, end-start);
    needleInHay(cdna->dna, dna, strand, stringency, cdnaName, geneName, start);
    freeMem(dupeName);
    freeMem(dna);
    }
else if (wormIsNamelessCluster(geneName))
    {
    dna = wormGetNamelessClusterDna(geneName);
    needleInHay(cdna->dna, dna, strand, stringency, cdnaName, geneName, 0);
    freeMem(dna);
    }
else if (getWormGeneDna(geneName, &dna, TRUE))
    {
    needleInHay(cdna->dna, dna, strand, stringency, cdnaName, geneName, 0);
    }
else
    {
    char *chrom;
    int start, end;
    char hayStrand;

    if (!wormGeneRange(geneName, &chrom, &hayStrand, &start, &end))
        errAbort("Can't find %s", geneName);
    dna = wormChromPartExonsUpper(chrom, start, end-start);
    if (hayStrand == '-')
        reverseComplement(dna, end-start);
    needleInHay(cdna->dna, dna, strand, stringency, cdnaName, geneName, start);
    freeMem(dna);
    }
}

void doMiddle()
/* Decide whether to do middle on data posted, or to
 * look it up from cDNA/gene names. */
{
fprintf(stdout, "<TT>\n");
if (cgiVarExists("gene"))
    lookupDoMiddle();
else
    directDoMiddle();
fprintf(stdout, "</TT>\n");
}

void debugDoMiddle()
{
fprintf(stdout, "<TT>\n");
needleInHay(

"ggcacgagggtatctcaccgactctgccttccatctcaaaccaggacaca"
"cacacactctctctctctctctctctctctctctctgtctctctgtctct"
"ctctctttcgggcatttgtccccagagagtgcctagagacttcacagcct"
"tggccctggaaacccctagacagccgctatgttgccaggcacggctctgg"
"gcactgaggctacagcaatgaaaaaatcagccaagttctctgccttcatg"
"gtgctcacattctaggcagagaaagacagatgatcaacaagtgaaaaaat"
"cataaagctcaggtcatggtgtggcaagtattagagtggagagcgatggg"
"gtggggtgggggcgctgttttatatggggtggtccaaaaatatcttggtg"
"aggtggtgacatttgagtggaaacctggacagcaagaagctagtcgtgct"
"ttggggtcaaaaggactccaaaatttcagttttttaaatggaaaacatgt"
"gtttacccataaacattaaagagcagggaaattag"

,

"gaggcaggcctaggcctgggctcccagcttggggcagcagagcagatccc"
"ttcaagggagaaaccacagatatgccccagcctctccttgatgctgtgag"
"tcaggggtgcttagaaaggctcgtgttcagttccaaatgcccagggtcac"
"cacgaaggaggtgctgccccctcccctgcaccccaagcaacctgcatctg"
"catggccctggagaggccattgctcctgatttccctcaggaaacatggcc"
"agggagctgctgtgagagttttccccgagtccccacctccctgagatgta"
"caatgagggaagggaagaggtatctcaccgacttctgcccttccatctca"
"aacaaagacacacacacactctctctctctctctctctctctctctctct"
"gtctctctgtctctctctctttctggcatttgtccccagagagtgcctag"
"agacttcacagccttggccctggaaacccctagacagccgctatgttgcc"
"aggcacggctctgggcactgaggctacagcaatgaaaaaatcagccaagt"
"tctctgccttcatggtgctcacattctaggcagagaaagacagatgatca"
"acaagtgaaaaaatcataaagctcaggtcatggtgtggcaagtattagag"
"tggagagcgatggggtggggtgggggcgctgttttatatggggtggtcca"
"aaaatatcttggtgaggtggtgacatttgagtggaaacctggacagcaag"
"aagctagtcgtgctttggggtcaaaaggactccaaaatttcagtttttta"
"aatggaaaacatgtgtttacccataaacattaaagagcagggaaattaga"
"actatgtttctggagcacctattatatgtctagcaccatgatggaaaatt"
"catagacatcatttcccaccgccctcttcagaaccctgtgtgtcacatag"
"cattgttttcattttacagatatcaaatgagaaatccatagagattattt"
"cacgcatttaataagatttattgagtattcacctctgaaaacactgtaat"
"aaatcccatacggaccctgccctcctagagcctacagtctgtgacagcga"
"ggagaacggtca",

     svForward, ffCdna, "debugNeedle", "debugHaystack", 0);
fprintf(stdout, "</TT>\n");
}

int main(int argc, char *argv[])
{
dnaUtilOpen();
if (argc == 2)
    {
    //sprintf(envBuf, "QUERY_STRING=%s", argv[1]);
    //putenv(envBuf);
    debugDoMiddle();
    }
else
    htmShell("FuzzyFinder Results", doMiddle, NULL); 
return 0;
}
