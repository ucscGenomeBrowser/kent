/* htc - Human Track Click processor - gets called when user clicks
 * on something in human tracks display. */
#include "common.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "jksql.h"
#include "hgap.h"
#include "ens.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "fuzzyFind.h"

char *seqName;		/* Name of sequence we're working on. */
int winStart, winEnd;   /* Bounds of sequence. */

void doEnsTranscript(char *txName)
/* Process click on EnsEMBL transcript.  Get DNA and put exons in upper case. */
{
struct ensGene *gene;
struct ensTranscript *tx;
int tStart, tEnd;
struct dnaSeq *seq;
DNA *dna;
struct dlNode *node;
struct ensExon *exon;
int orientation;
struct contigTree *contig;

printf("<H2>Transcript %s</H2>", txName);
ensGetGeneAndTranscript(txName, &gene, &tx);
ensTranscriptBounds(tx, &tStart, &tEnd);
seq = ensDnaInBacRange(seqName, tStart, tEnd, dnaLower);
dna = seq->dna;
for (node = tx->exonList->head; node->next != NULL; node = node->next)
    {
    int eStart,eEnd;
    exon = node->val;
    orientation = exon->orientation;
    contig = exon->contig;
    eStart = ensBrowserCoordinates(contig, exon->seqStart);
    eEnd = ensBrowserCoordinates(contig, exon->seqEnd);
    toUpperN(dna + eStart - tStart, eEnd-eStart);
    }
if (orientation < 0)
    reverseComplement(seq->dna, seq->size);
printf("<TT><PRE>");
faWriteNext(stdout, txName, seq->dna, seq->size);
freeDnaSeq(&seq);
printf("</TT></PRE>");
ensFreeGene(&gene);
}

void doEnsClone(char *cloneName)
/* Process click on EnsEMBL clone. */
{
int size = ensBacBrowserLength(cloneName);
struct dnaSeq *seq = ensDnaInBacRange(cloneName, 0, size, dnaMixed);
printf("<TT><PRE>");
faWriteNext(stdout, cloneName, seq->dna, seq->size);
printf("</TT></PRE>");
freeDnaSeq(&seq);
}

void doEnsContig(char *contigName)
/* Process click on EnsEMBL contig. */
{
struct contigTree *contig = ensGetContig(contigName);
struct dnaSeq *seq = ensDnaInBacRange(contig->parent->id, contig->browserOffset, 
	contig->browserOffset + contig->browserLength, dnaMixed);
printf("<TT><PRE>");
faWriteNext(stdout, contigName, seq->dna, seq->size);
printf("</TT></PRE>");
freeDnaSeq(&seq);
}

void doEnsFeat(char *featTypeString)
/* Process click on EnsEMBL feature track. */
{
int featType = atoi(featTypeString);
struct ensAnalysis **anTable;
int anCount;
ensGetAnalysisTable(&anTable, &anCount);
if (featType >= 0 && featType <= anCount)
    {
    struct ensAnalysis *ea = anTable[featType];
    printf("<H2>Description of %s Features</H2>", ea->shortName);
    printf("<TT><PRE>");
    printf("The boxes in the track you clicked on represent the following feature type\n");
    if (ea->db)
	printf("Database: %s\n", ea->db);
    if (ea->dbVersion)
	printf("Version: %s\n", ea->dbVersion);
    if (ea->program)
	printf("Program: %s\n", ea->program);
    if (ea->programVersion)
	printf("Version: %s\n", ea->programVersion);
    if (ea->gffSource)
	printf("GFF Source: %s\n", ea->gffSource);
    if (ea->gffFeature)
	printf("GFF Feature: %s\n", ea->gffFeature);
    }
}

void doGetDna()
/* Get dna in window. */
{
struct dnaSeq *seq = ensDnaInBacRange(seqName, winStart, winEnd, dnaMixed);
printf("<TT><PRE>");
faWriteNext(stdout, seq->name, seq->dna, seq->size);
printf("</TT></PRE>");
freeDnaSeq(&seq);
}

void printRnaSpecs(char *acc)
/* Print auxiliarry info on RNA. */
{
struct dyString *dy = newDyString(1024);
struct sqlConnection *conn = hgAllocConn();
struct sqlResult *sr;
char **row;
char *type,*direction,*source,*organism,*library,*clone,*sex,*tissue,
     *development,*cell,*cds,*gene_name,*product_name;
int seqSize,fileSize;
long fileOffset;
char *date;
char *ext_file;

dyStringAppend(dy,
  "select mrna.type,mrna.direction,"
  	 "source.name,organism.name,library.name,est_clone.name,"
         "sex.name,tissue.name,development.name,cell.name,cds.name,"
	 "gene_name.name,product_name.name,"
	 "seq.size,seq.gb_date,seq.ext_file,seq.file_offset,seq.file_size "
  "from mrna,seq,source,organism,library,est_clone,sex,tissue,"
        "development,cell,cds,gene_name,product_name ");
dyStringPrintf(dy,
  "where mrna.acc = '%s' and mrna.id = seq.id ", acc);
dyStringAppend(dy,
    "and mrna.source = source.id and mrna.organism = organism.id "
    "and mrna.library = library.id and mrna.clone = est_clone.id "
    "and mrna.sex = sex.id and mrna.tissue = tissue.id "
    "and mrna.development = development.id and mrna.cell = cell.id "
    "and mrna.cds = cds.id and mrna.gene_name = gene_name.id "
    "and mrna.product_name = product_name.id");
sr = sqlMustGetResult(conn, dy->string);
row = sqlNextRow(sr);
type=row[0];direction=row[1];source=row[2];organism=row[3];library=row[4];clone=row[5];
sex=row[6];tissue=row[7];development=row[8];cell=row[9];cds=row[10];gene_name=row[11];
product_name=row[12];
seqSize = sqlUnsigned(row[13]);
date = row[14];
ext_file = row[15];
fileOffset=sqlUnsigned(row[16]);
fileSize=sqlUnsigned(row[17]);

printf("<H2>Information on %s %s</H2>\n", type, acc);
printf("<B>gene:</B> %s<BR>\n", gene_name);
printf("<B>product:</B> %s<BR>\n", product_name);
printf("<B>organism:</B> %s<BR>\n", organism);
printf("<B>tissue:</B> %s<BR>\n", tissue);
printf("<B>development stage:</B> %s<BR>\n", development);
printf("<B>cell type:</B> %s<BR>\n", cell);
printf("<B>sex:</B> %s<BR>\n", sex);
printf("<B>library:</B> %s<BR>\n", library);
printf("<B>clone:</B> %s<BR>\n", clone);
if (direction[0] != '0') printf("<B>read direction:</B> %s'<BR>\n", direction);
printf("<B>cds:</B> %s<BR>\n", cds);

sqlFreeResult(&sr);
freeDyString(&dy);
hgFreeConn(&conn);
}

void doHgRna(char *acc)
/* Click on an individual RNA. */
{
struct dnaSeq *rnaSeq;
struct dnaSeq *dnaSeq;
DNA *mixedCaseDna, *dna, *rna;
int dnaSize,rnaSize;
boolean isRc;
struct ffAli *ffAli;

/* Print non-sequence info. */
printRnaSpecs(acc);

/* Get RNA and DNA sequence.  Save a mixed case copy of DNA, make
 * all lower case for fuzzyFinder. */
rnaSeq = hgRnaSeq(acc);
rnaSize = rnaSeq->size;
rna = rnaSeq->dna;
// ugly ok to here.
dnaSeq = ensDnaInBacRange(seqName, winStart, winEnd, dnaMixed);
// ugly crashes by here.
dnaSize = dnaSeq->size;
dna = dnaSeq->dna;
mixedCaseDna = needLargeMem(dnaSize);
memcpy(mixedCaseDna, dnaSeq->dna, dnaSize);
tolowers(dna);

/* Find in lower case, display in mixed case. */
if (!ffFindEitherStrand(rna, dna, ffCdna, &ffAli, &isRc))
    errAbort("Couldn't align %s and %s.", rnaSeq->name, dnaSeq->name);
memcpy(dna, mixedCaseDna, dnaSize);
ffShowAli(ffAli, acc, rna, 0, dnaSeq->name, dna, winStart, isRc);

/* Cleanup time. */
ffFreeAli(&ffAli);
freeMem(mixedCaseDna);
freeDnaSeq(&rnaSeq);
freeDnaSeq(&dnaSeq);
}

void doMiddle()
/* Generate body of HTML. */
{
char *group = cgiString("g");
char *item = cgiOptionalString("i");

seqName = cgiString("c");
winStart = cgiInt("l");
winEnd = cgiInt("r");

if (sameWord(group, "ensTx"))
    {
    doEnsTranscript(item);
    }
else if (sameWord(group, "ensClone"))
    {
    doEnsClone(item);
    }
else if (sameWord(group, "ensContig"))
    {
    doEnsContig(item);
    }
else if (sameWord(group, "ensFeat"))
    {
    doEnsFeat(item);
    }
else if (sameWord(group, "getDna"))
    {
    doGetDna();
    }
else if (sameWord(group, "hgMrna"))
    {
    if (item == NULL)
	{
	printf("This track is composed of human mRNAs that were individually "
	       "deposited in GenBank.  Most of these are high quality full "
	       "length mRNAs.<BR>\n"
	       "Click on an individual EST to get specific information "
	       "about that EST such as it's sequence, the library it "
	       "came from, etc.");
	}
    else
	{
	doHgRna(item);
	}
    }
else if (sameWord(group, "hgEstI"))
    {
    if (item == NULL)
	{
	printf("This track is composed of human ESTs that appear to have "
	       "introns (and thus are almost certainly derived from mRNA "
	       "rather than genomic contamination).<BR>\n"
	       "Click on an individual EST to get specific information "
	       "about that EST such as it's sequence, the library it "
	       "came from, etc.");
	}
    else
	{
	doHgRna(item);
	}
    }
else if (sameWord(group, "hgEstNoI"))
    {
    if (item == NULL)
	{
	printf("This track is composed of human ESTs that appear to have "
	       "no introns.  Many of these are probably the result of "
	       "genomic DNA contiminating the EST preparations.  Others "
	       "may be long exons in the 3' UTR.  A few may even be "
	       "long coding exons.<BR>\n"
	       "Click on an individual EST to get specific information "
	       "about that EST such as it's sequence, the library it "
	       "came from, etc.");
	}
    else
	{
	doHgRna(item);
	}
    }
else
    {
    errAbort("Sorry, clicking there doesn't do anything yet (%s).", group);
    }
}

int main(int argc, char *argv[])
{
htmShell("Human Genome Click Results", doMiddle, NULL);
}
