/* hc - Human Click processor - gets called when user clicks
 * on something in human tracks display. */
#include "common.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "jksql.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "fuzzyFind.h"
#include "hdb.h"
#include "hgap.h"
#include "agpFrag.h"
#include "psl.h"

char *seqName;		/* Name of sequence we're working on. */
int winStart, winEnd;   /* Bounds of sequence. */

void doGetDna()
/* Get dna in window. */
{
struct dnaSeq *seq = hDnaFromSeq(seqName, winStart, winEnd, dnaMixed);
char name[256];
sprintf(name, "%s:%d-%d", seqName, winStart, winEnd);
printf("<TT><PRE>");
faWriteNext(stdout, name, seq->dna, seq->size);
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
     *development,*cell,*cds,*geneName,*productName;
int seqSize,fileSize;
long fileOffset;
char *date;
char *ext_file;

dyStringAppend(dy,
  "select mrna.type,mrna.direction,"
  	 "source.name,organism.name,library.name,mrnaClone.name,"
         "sex.name,tissue.name,development.name,cell.name,cds.name,"
	 "geneName.name,productName.name,"
	 "seq.size,seq.gb_date,seq.extFile,seq.file_offset,seq.file_size "
  "from mrna,seq,source,organism,library,mrnaClone,sex,tissue,"
        "development,cell,cds,geneName,productName ");
dyStringPrintf(dy,
  "where mrna.acc = '%s' and mrna.id = seq.id ", acc);
dyStringAppend(dy,
    "and mrna.source = source.id and mrna.organism = organism.id "
    "and mrna.library = library.id and mrna.mrnaClone = mrnaClone.id "
    "and mrna.sex = sex.id and mrna.tissue = tissue.id "
    "and mrna.development = development.id and mrna.cell = cell.id "
    "and mrna.cds = cds.id and mrna.geneName = geneName.id "
    "and mrna.productName = productName.id");
sr = sqlMustGetResult(conn, dy->string);
row = sqlNextRow(sr);
type=row[0];direction=row[1];source=row[2];organism=row[3];library=row[4];clone=row[5];
sex=row[6];tissue=row[7];development=row[8];cell=row[9];cds=row[10];geneName=row[11];
productName=row[12];
seqSize = sqlUnsigned(row[13]);
date = row[14];
ext_file = row[15];
fileOffset=sqlUnsigned(row[16]);
fileSize=sqlUnsigned(row[17]);

printf("<H2>Information on %s %s</H2>\n", type, acc);
printf("<B>gene:</B> %s<BR>\n", geneName);
printf("<B>product:</B> %s<BR>\n", productName);
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

void doHgRna(char *acc, boolean isEst)
/* Click on an individual RNA. */
{
struct dnaSeq *rnaSeq;
struct dnaSeq *dnaSeq;
DNA *rna;
int dnaSize,rnaSize;
boolean isRc = FALSE;
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
struct linkedFeatures *lfList = NULL, *lf;

/* Print non-sequence info. */
printRnaSpecs(acc);

/* Print sequence with aligning areas in blue. */

/* Get RNA and DNA sequence.  Save a mixed case copy of DNA, make
 * all lower case for fuzzyFinder. */
rnaSeq = hgRnaSeq(acc);
rnaSize = rnaSeq->size;
rna = rnaSeq->dna;
dnaSeq = hDnaFromSeq(seqName, winStart, winEnd, dnaMixed);

/* Look up alignments in database, convert to ffAli format
 * and display. */
sprintf(query, "select * from %s_mrna_old where qName = '%s' and tStart<%u and tEnd>%u",
    seqName, acc, winEnd, winStart);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct simpleFeature *sfList = NULL, *sf;
    struct psl *psl = pslLoad(row);
    struct ffAli *ffAli;
    
    if (psl->strand[0] == '-')
	{
	isRc = TRUE;
	reverseComplement(rna, rnaSize);
	}
    ffAli = pslToFfAli(psl, rnaSeq, dnaSeq, winStart);
    if (isRc)
	reverseComplement(rna, rnaSize);
    if (ffAli != NULL)
	{
	ffShowAli(ffAli, acc, rna, 0, dnaSeq->name, dnaSeq->dna, winStart, isRc);
	ffFreeAli(&ffAli);
	}
    pslFree(&psl);
    }

/* Cleanup time. */
freeDnaSeq(&rnaSeq);
freeDnaSeq(&dnaSeq);
}

void doHgGold(char *fragName)
/* Click on a fragment of golden path. */
{
struct sqlConnection *conn = hAllocConn();
char query[256];
struct sqlResult *sr;
char **row;
struct agpFrag frag;
struct dnaSeq *seq;
char dnaName[256];

sprintf(query, "select * from %s_gold where frag = '%s'", seqName, fragName);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
agpFragStaticLoad(row, &frag);

printf("Fragment %s bases %d-%d strand %s makes up draft sequence of bases %d-%d of %s<BR>\n",
	frag.frag, frag.fragStart, frag.fragEnd, frag.strand,
	frag.chromStart, frag.chromEnd, frag.chrom);
htmlHorizontalLine();

sprintf(dnaName, "%s:%d-%d", frag.chrom, frag.chromStart, frag.chromEnd);
seq = hDnaFromSeq(frag.chrom, frag.chromStart, frag.chromEnd, dnaMixed);
printf("<TT><PRE>");
faWriteNext(stdout, dnaName, seq->dna, seq->size);
printf("</TT></PRE>");

freeDnaSeq(&seq);
sqlFreeResult(&sr);
hFreeConn(&conn);
}


void doMiddle()
/* Generate body of HTML. */
{
char *group = cgiString("g");
char *item = cgiOptionalString("i");

seqName = cgiString("c");
winStart = cgiInt("l");
winEnd = cgiInt("r");

if (sameWord(group, "getDna"))
    {
    doGetDna();
    }
else if (sameWord(group, "hgMrna") || sameWord(group, "hgMrna2"))
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
	doHgRna(item, FALSE);
	}
    }
else if (sameWord(group, "hgEst"))
    {
    doHgRna(item, TRUE);
    }
else if (sameWord(group, "hgGold"))
    {
    if (item == NULL)
	{
	printf("This track shows the assembly of the draft from fragments");
	}
    else
	{
	doHgGold(item);
	}
    }
else
    {
    errAbort("Sorry, clicking there doesn't do anything yet (%s).", group);
    }
}

int main(int argc, char *argv[])
{
htmShell("Draft Genome Results", doMiddle, NULL);
}
