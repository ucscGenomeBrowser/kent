/* hgc - Human Genome Click processor - gets called when user clicks
 * on something in human tracks display. */
#include "common.h"
#include "hCommon.h"
#include "portable.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "jksql.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "fuzzyFind.h"
#include "seqOut.h"
#include "hdb.h"
#include "hgRelate.h"
#include "psl.h"
#include "bed.h"
#include "agpFrag.h"
#include "ctgPos.h"
#include "clonePos.h"
#include "rmskOut.h"
#include "xenalign.h"
#include "isochores.h"
#include "simpleRepeat.h"
#include "cpgIsland.h"
#include "genePred.h"
#include "pepPred.h"
#include "wabAli.h"
#include "genomicDups.h"
#include "est3.h"
#include "exoFish.h"
#include "rnaGene.h"
#include "stsMarker.h"
#include "stsAlias.h"
#include "mouseSyn.h"
#include "cytoBand.h"
#include "knownMore.h"
#include "snp.h"
#include "softberryHom.h"

char *seqName;		/* Name of sequence we're working on. */
int winStart, winEnd;   /* Bounds of sequence. */
char *database;		/* Name of mySQL database. */

char *entrezScript = "http://www.ncbi.nlm.nih.gov/htbin-post/Entrez/query?form=4";

void printEntrezNucleotideUrl(FILE *f, char *accession)
/* Print URL for Entrez browser on a nucleotide. */
{
fprintf(f, "\"%s&db=n&term=%s\"", entrezScript, accession);
}

void printEntrezProteinUrl(FILE *f, char *accession)
/* Print URL for Entrez browser on a nucleotide. */
{
fprintf(f, "\"%s&db=p&term=%s\"", entrezScript, accession);
}

char *hgcPath()
/* Return path of this CGI script. */
{
return "../cgi-bin/hgc";
}

void hgcAnchorSomewhere(char *group, char *item, char *other, char *chrom)
/* Generate an anchor that calls click processing program with item and other parameters. */
{
printf("<A HREF=\"%s?g=%s&i=%s&c=%s&l=%d&r=%d&db=%s&o=%s\">",
	hgcPath(), group, item, chrom, winStart, winEnd, database, other);
}

void hgcAnchor(char *group, char *item, char *other)
/* Generate an anchor that calls click processing program with item and other parameters. */
{
hgcAnchorSomewhere(group, item, other, seqName);
}

boolean clipToChrom(int *pStart, int *pEnd)
/* Clip start/end coordinates to fit in chromosome. */
{
static int chromSize = -1;

if (chromSize < 0)
   chromSize = hChromSize(seqName);
if (*pStart < 0) *pStart = 0;
if (*pEnd > chromSize) *pEnd = chromSize;
return *pStart < *pEnd;
}

void printCappedSequence(int start, int end, int extra)
/* Print DNA from start to end including extra at either end.
 * Capitalize bits from start to end. */
{
struct dnaSeq *seq;
int s, e, i;
struct cfm cfm;

if (!clipToChrom(&start, &end))
    return;
s = start - extra;
e = end + extra;
clipToChrom(&s, &e);

printf("<P>Here is the sequence around this feature: bases %d to %d of %s.  The bases "
       "that contain the feature itself are in upper case.</P>\n", s, e, seqName);
seq = hDnaFromSeq(seqName, s, e, dnaLower);
toUpperN(seq->dna + (start-s), end - start);
printf("<TT>");
cfmInit(&cfm, 10, 50, TRUE, FALSE, stdout, s);
for (i=0; i<seq->size; ++i)
   {
   cfmOut(&cfm, seq->dna[i], 0);
   }
cfmCleanup(&cfm);
printf("</TT>");
}

void doGetDna()
/* Get dna in window. */
{
struct dnaSeq *seq;
char name[256];
sprintf(name, "%s:%d-%d", seqName, winStart+1, winEnd);
htmlStart(name);
seq = hDnaFromSeq(seqName, winStart, winEnd, dnaMixed);
printf("<TT><PRE>");
faWriteNext(stdout, name, seq->dna, seq->size);
printf("</TT></PRE>");
freeDnaSeq(&seq);
}

void medlineLinkedLine(char *title, char *text, char *search)
/* Produce something that shows up on the browser as
 *     TITLE: value
 * with the value hyperlinked to medline. */
{
char *encoded = cgiEncode(search);

printf("<B>%s:</B> ", title);
if (sameWord(text, "n/a"))
    printf("n/a<BR>\n");
else
    printf("<A HREF=\"%s&db=m&term=%s\" TARGET=_blank>%s</A><BR>", entrezScript, 
        encoded, text);
freeMem(encoded);
}

void appendAuthor(struct dyString *dy, char *gbAuthor, int len)
/* Convert from  Kent,W.J. to Kent WJ and append to dy.
 * gbAuthor gets eaten in the process. */
{
char buf[512];

if (len >= sizeof(buf))
    warn("author %s too long to process", gbAuthor);
else
    {
    memcpy(buf, gbAuthor, len);
    buf[len] = 0;
    stripChar(buf, '.');
    subChar(buf, ',' , ' ');
    dyStringAppend(dy, buf);
    dyStringAppend(dy, " ");
    }
}

void gbToEntrezAuthor(char *authors, struct dyString *dy)
/* Convert from Genbank author format:
 *      Kent,W.J., Haussler,D. and Zahler,A.M.
 * to Entrez search format:
 *      Kent WJ,Haussler D,Zahler AM
 */
{
char buf[256];
char *s = authors, *e;

/* Parse first authors, which will be terminated by '.,' */
while ((e = strstr(s, ".,i ")) != NULL)
    {
    int len = e - s + 1;
    appendAuthor(dy, s, len);
    s += len+2;
    }
if ((e = strstr(s, " and")) != NULL)
    {
    int len = e - s;
    appendAuthor(dy, s, len);
    s += len+4;
    }
if ((s = skipLeadingSpaces(s)) != NULL && s[0] != 0)
    {
    int len = strlen(s);
    appendAuthor(dy, s, len);
    }
}

void printRnaSpecs(char *acc)
/* Print auxiliarry info on RNA. */
{
struct dyString *dy = newDyString(1024);
struct sqlConnection *conn = hgAllocConn();
struct sqlResult *sr;
char **row;
char *type,*direction,*source,*organism,*library,*clone,*sex,*tissue,
     *development,*cell,*cds,*description, *author,*geneName,*productName;
int seqSize,fileSize;
long fileOffset;
char *date;
char *ext_file;

/* This sort of query and having to keep things in sync between
 * the first clause of the select, the from clause, the where
 * clause, and the results in the row ... is really tedious.
 * One of my main motivations for going to a more object
 * based rather than pure relational approach in general,
 * and writing 'autoSql' to help support this.  However
 * the pure relational approach wins for pure search speed,
 * and these RNA fields are searched.  So it looks like
 * the code below stays.  Be really careful when you modify
 * it. */
dyStringAppend(dy,
  "select mrna.type,mrna.direction,"
  	 "source.name,organism.name,library.name,mrnaClone.name,"
         "sex.name,tissue.name,development.name,cell.name,cds.name,"
	 "description.name,author.name,geneName.name,productName.name,"
	 "seq.size,seq.gb_date,seq.extFile,seq.file_offset,seq.file_size "
  "from mrna,seq,source,organism,library,mrnaClone,sex,tissue,"
        "development,cell,cds,description,author,geneName,productName ");
dyStringPrintf(dy,
  "where mrna.acc = '%s' and mrna.id = seq.id ", acc);
dyStringAppend(dy,
    "and mrna.source = source.id and mrna.organism = organism.id "
    "and mrna.library = library.id and mrna.mrnaClone = mrnaClone.id "
    "and mrna.sex = sex.id and mrna.tissue = tissue.id "
    "and mrna.development = development.id and mrna.cell = cell.id "
    "and mrna.cds = cds.id and mrna.description = description.id "
    "and mrna.author = author.id and mrna.geneName = geneName.id "
    "and mrna.productName = productName.id");
sr = sqlMustGetResult(conn, dy->string);
row = sqlNextRow(sr);
if (row != NULL)
    {
    type=row[0];direction=row[1];source=row[2];organism=row[3];library=row[4];clone=row[5];
    sex=row[6];tissue=row[7];development=row[8];cell=row[9];cds=row[10];description=row[11];
    author=row[12];geneName=row[13];productName=row[14];
    seqSize = sqlUnsigned(row[15]);
    date = row[16];
    ext_file = row[17];
    fileOffset=sqlUnsigned(row[18]);
    fileSize=sqlUnsigned(row[19]);

    /* Now we have all the info out of the database and into nicely named
     * local variables.  There's still a few hoops to jump through to 
     * format this prettily on the web with hyperlinks to NCBI. */
    printf("<H2>Information on %s <A HREF=", type);
    printEntrezNucleotideUrl(stdout, acc);
    printf(" TARGET=_blank>%s</A></H2>\n", acc);

    printf("<B>description:</B> %s<BR>\n", description);

    medlineLinkedLine("gene", geneName, geneName);
    medlineLinkedLine("product", productName, productName);
    dyStringClear(dy);
    gbToEntrezAuthor(author, dy);
    medlineLinkedLine("author", author, dy->string);
    printf("<B>organism:</B> %s<BR>\n", organism);
    printf("<B>tissue:</B> %s<BR>\n", tissue);
    printf("<B>development stage:</B> %s<BR>\n", development);
    printf("<B>cell type:</B> %s<BR>\n", cell);
    printf("<B>sex:</B> %s<BR>\n", sex);
    printf("<B>library:</B> %s<BR>\n", library);
    printf("<B>clone:</B> %s<BR>\n", clone);
    if (direction[0] != '0') printf("<B>read direction:</B> %s'<BR>\n", direction);
    printf("<B>cds:</B> %s<BR>\n", cds);
    }
else
    {
    warn("Couldn't find %s in mrna table", acc);
    }

sqlFreeResult(&sr);
freeDyString(&dy);
hgFreeConn(&conn);
}

void doHgRna(char *acc, boolean isEst)
/* Click on an individual RNA. */
{
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
struct linkedFeatures *lfList = NULL, *lf;
char *type = (isEst ? "est" : "mrna");
int start = cgiInt("o");
struct psl *pslList = NULL, *psl;
int aliCount;
char otherString[32];
boolean same;

/* Print non-sequence info. */
htmlStart(acc);

printRnaSpecs(acc);

/* Get alignment info. */
sprintf(query, "select * from all_%s where qName = '%s'",
    type, acc);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row);
    slAddHead(&pslList, psl);
    }
sqlFreeResult(&sr);
slReverse(&pslList);

/* Print alignments. */
htmlHorizontalLine();
printf("<H3>%s/Genomic Alignments</H3>", (isEst ? "EST" : "mRNA"));
aliCount = slCount(pslList);
if (aliCount > 1)
    printf("The alignment you clicked on is first in the table below.<BR>\n");

printf("<TT><PRE>");
printf(" SIZE IDENTITY CHROMOSOME STRAND  START     END       cDNA   START  END  TOTAL\n");
printf("------------------------------------------------------------------------------\n");
for (same = 1; same >= 0; same -= 1)
    {
    for (psl = pslList; psl != NULL; psl = psl->next)
	{
	if (same ^ (psl->tStart != start))
	    {
	    sprintf(otherString, "%d&type=%s", psl->tStart, type);
	    hgcAnchorSomewhere("htcCdnaAli", acc, otherString, psl->tName);
	    printf("%5d  %5.1f%%  %9s     %s %9d %9d  %8s %5d %5d %5d</A>",
		psl->match + psl->misMatch + psl->repMatch + psl->nCount,
		100.0 - pslCalcMilliBad(psl, TRUE) * 0.1,
		skipChr(psl->tName), psl->strand, psl->tStart + 1, psl->tEnd,
		psl->qName, psl->qStart+1, psl->qEnd, psl->qSize);
	    printf("\n");
	    }
	}
    }
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

htmlStart(fragName);
sprintf(query, "select * from %s_gold where frag = '%s'", seqName, fragName);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
agpFragStaticLoad(row, &frag);

printf("Fragment %s bases %d-%d strand %s makes up draft sequence of bases %d-%d of chromosome %s<BR>\n",
	frag.frag, frag.fragStart+1, frag.fragEnd, frag.strand,
	frag.chromStart+1, frag.chromEnd, skipChr(frag.chrom));
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

void doHgGap(char *gapType)
/* Print a teeny bit of info about a gap. */
{
char *words[2];
int wordCount;

htmlStart("Gap in Sequence");
chopByChar(gapType, '_', words, ArraySize(words));
printf("This is a gap between %ss.<BR>", words[0]);
if (sameString(words[1], "no"))
    printf("This gap is not bridged by an mRNA, EST or BAC end pair.\n"); 
else
    printf("This gap is bridged by mRNA, EST or BAC end pairs.\n");
}

void doHgContig(char *ctgName)
/* Click on a contig. */
{
struct sqlConnection *conn = hAllocConn();
char query[256];
struct sqlResult *sr;
char **row;
struct ctgPos *ctg;
int cloneCount;

htmlStart(ctgName);
sprintf(query, "select * from ctgPos where contig = '%s'", ctgName);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
ctg = ctgPosLoad(row);
sqlFreeResult(&sr);
sprintf(query, 
    "select count(*) from clonePos where chrom = '%s' and chromEnd >= %d and chromStart <= %d",
    ctg->chrom, ctg->chromStart, ctg->chromEnd);
cloneCount = sqlQuickNum(conn, query);

printf("Fingerprint Map Contig %s includes %d clones and makes up bases %d-%d of the draft sequence of chromosome %s.<BR>\n",
     ctgName, cloneCount, ctg->chromStart+1, ctg->chromEnd, skipChr(ctg->chrom));
printf("Go back and type '%s' in the position field to bring the entire contig into the browser window.",
     ctgName);

sqlFreeResult(&sr);
hFreeConn(&conn);
}

char *cloneStageName(char *stage)
/* Expand P/D/F. */
{
switch (stage[0])
   {
   case 'P':
      return "predraft (less than 4x coverage shotgun)";
   case 'D':
      return "draft (at least 4x coverage shotgun)";
   case 'F':
      return "finished";
   default:
      return "unknown";
   }
}

void doHgCover(char *cloneName)
/* Respond to click on clone. */
{
struct sqlConnection *conn = hAllocConn();
char query[256];
struct sqlResult *sr;
char **row;
struct clonePos *clone;
int fragCount;

htmlStart(cloneName);
sprintf(query, "select * from clonePos where name = '%s'", cloneName);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
clone = clonePosLoad(row);
sqlFreeResult(&sr);

sprintf(query, 
    "select count(*) from %s_gl where end >= %d and start <= %d and frag like '%s%%'",
    clone->chrom, clone->chromStart, clone->chromEnd, clone->name);
fragCount = sqlQuickNum(conn, query);

printf("<H2>Information on <A HREF=");
printEntrezNucleotideUrl(stdout, cloneName);
printf(" TARGET=_blank>%s</A></H2>\n", cloneName);
printf("<B>status:</B> %s<BR>\n", cloneStageName(clone->stage));
printf("<B>fragments:</B> %d<BR>\n", fragCount);
printf("<B>size:</B> %d bases<BR>\n", clone->seqSize);
printf("<B>chromosome:</B> %s<BR>\n", skipChr(clone->chrom));
printf("<BR>\n");

hgcAnchor("htcCloneSeq", cloneName, "");
printf("Fasta format sequence</A><BR>\n");
hFreeConn(&conn);
}

void doHgClone(char *fragName)
/* Handle click on a clone. */
{
char cloneName[128];
fragToCloneVerName(fragName, cloneName);
doHgCover(cloneName);
}

void htcCloneSeq(char *cloneName)
/* Fetch sequence as fasta file. */
{
struct sqlConnection *conn = hAllocConn();
char query[256];
struct sqlResult *sr;
char **row;
struct clonePos *clone;
struct dnaSeq *seqList, *seq;

htmlStart(cloneName);
sprintf(query, "select * from clonePos where name = '%s'", cloneName);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
clone = clonePosLoad(row);
sqlFreeResult(&sr);

seqList = faReadAllDna(clone->faFile);
printf("<PRE><TT>");
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    faWriteNext(stdout, seq->name, seq->dna, seq->size);
    }
hFreeConn(&conn);
}

void htcCdnaAli(char *acc)
/* Show alignment for accession. */
{
struct tempName indexTn, bodyTn;
FILE *index, *body;
struct dnaSeq *rnaSeq;
struct dnaSeq *dnaSeq;
DNA *rna;
int dnaSize,rnaSize;
boolean isRc = FALSE;
char *type;
int start;
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct psl *psl;
struct ffAli *ffAli, *ff;
int tStart, tEnd, tRcAdjustedStart;
int lastEnd = 0;
int blockCount;
int i;
char title[256];

/* Print start of HTML. */
puts("Content-Type:text/html\n");
puts("<HTML>");
printf("<HEAD>\n<TITLE>%s vs Genomic</TITLE>\n</HEAD>\n\n", acc);

type = cgiString("type");
start = cgiInt("o");

makeTempName(&indexTn, "index", ".html");
makeTempName(&bodyTn, "body", ".html");

/* Look up alignments in database */
sprintf(query, "select * from %s_%s where qName = '%s' and tStart=%d",
    seqName, type, acc, start);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't find alignment for %s at %d", acc, start);
psl = pslLoad(row);
sqlFreeResult(&sr);


/* Get RNA and DNA sequence.  Save a mixed case copy of DNA, make
 * all lower case for fuzzyFinder. */
rnaSeq = hRnaSeq(acc);
rnaSize = rnaSeq->size;
rna = rnaSeq->dna;
tStart = psl->tStart - 100;
if (tStart < 0) tStart = 0;
tEnd  = psl->tEnd + 100;
if (tEnd > psl->tSize) tEnd = psl->tSize;
dnaSeq = hDnaFromSeq(seqName, tStart, tEnd, dnaMixed);
freez(&dnaSeq->name);
dnaSeq->name = cloneString(psl->tName);

/* Start writing body of alignment. */
body = mustOpen(bodyTn.forCgi, "w");
htmStart(body, acc);

/* Convert psl alignment to ffAli. */
tRcAdjustedStart = tStart;
if (psl->strand[0] == '-')
    {
    isRc = TRUE;
    reverseComplement(dnaSeq->dna, dnaSeq->size);
    pslRcBoth(psl);
    tRcAdjustedStart = psl->tSize - tEnd;
    }
ffAli = pslToFfAli(psl, rnaSeq, dnaSeq, tRcAdjustedStart);

/* Write body. */
fprintf(body, "<H2>Alignment of %s and %s:%d-%d</H2>\n", acc, psl->tName, psl->tStart, psl->tEnd);
fprintf(body, "Click on links in the frame to left to navigate through alignment.\n");
blockCount = ffShAliPart(body, ffAli, acc, rna, rnaSize, 0, 
	dnaSeq->name, dnaSeq->dna, dnaSeq->size, tStart, 
	8, FALSE, isRc, FALSE, TRUE, TRUE, TRUE);
fclose(body);
chmod(bodyTn.forCgi, 0666);

/* Write index. */
index = mustOpen(indexTn.forCgi, "w");
htmStart(index, acc);
fprintf(index, "<H3>%s</H3>", acc);
fprintf(index, "<A HREF=\"%s#cDNA\" TARGET=\"body\">cDNA</A><BR>\n", bodyTn.forCgi);
fprintf(index, "<A HREF=\"%s#genomic\" TARGET=\"body\">genomic</A><BR>\n", bodyTn.forCgi);
for (i=1; i<=blockCount; ++i)
    {
    fprintf(index, "<A HREF=\"%s#%d\" TARGET=\"body\">block%d</A><BR>\n",
         bodyTn.forCgi, i, i);
    }
fprintf(index, "<A HREF=\"%s#ali\" TARGET=\"body\">together</A><BR>\n", bodyTn.forCgi);
fclose(index);
chmod(indexTn.forCgi, 0666);

/* Write (to stdout) the main html page containing just the frame info. */
puts("<FRAMESET COLS = \"13%,87% \" >");
printf("  <FRAME SRC=\"%s\" NAME=\"index\" RESIZE>\n", indexTn.forCgi);
printf("  <FRAME SRC=\"%s\" NAME=\"body\" RESIZE>\n", bodyTn.forCgi);
puts("</FRAMESET>");
puts("<NOFRAMES><BODY></BODY></NOFRAMES>");
hFreeConn(&conn);
}

void writeMatches(FILE *f, char *a, char *b, int count)
/* Write a | where a and b agree, a ' ' elsewhere. */
{
int i;
for (i=0; i<count; ++i)
    {
    if (a[i] == b[i])
        fputc('|', f);
    else
        fputc(' ', f);
    }
}

void fetchAndShowWaba(char *table, char *name)
/* Fetch and display waba alignment. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];
int start = cgiInt("o");
struct wabAli *wa;
int qOffset;
char strand = '+';

sprintf(query, "select * from %s where query = '%s' and chrom = '%s' and chromStart = %d",
	table, name, seqName, start);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Sorry, couldn't find alignment of %s at %d of %s in database",
    	name, start, seqName);
wa = wabAliLoad(row);
printf("<PRE><TT>");
qOffset = wa->qStart;
if (wa->strand[0] == '-')
    {
    strand = '-';
    qOffset = wa->qEnd;
    }
xenShowAli(wa->qSym, wa->tSym, wa->hSym, wa->symCount, stdout,
    qOffset, wa->chromStart, strand, '+', 60);
printf("</PRE></TT>");

wabAliFree(&wa);
hFreeConn(&conn);
}

void doHgTet(char *name)
/* Do thing with tet track. */
{
htmlStart("Tetraodon Alignment");
printf("Alignment between tetraodon sequence %s (above) and human chromosome %s (below)\n",
    name, skipChr(seqName));
fetchAndShowWaba("waba_tet", name);
}

void doHgRepeat(char *repeat)
/* Do click on a repeat track. */
{
int offset = cgiInt("o");
htmlStart("Repeat");
if (offset >= 0)
    {
    struct sqlConnection *conn = hAllocConn();
    struct sqlResult *sr;
    char **row;
    struct rmskOut *ro;
    char query[256];
    int start = cgiInt("o");
    sprintf(query, "select * from %s_rmsk where  repName = '%s' and genoName = '%s' and genoStart = %d",
	    seqName, repeat, seqName, start);
    sr = sqlGetResult(conn, query);
    printf("<H3>RepeatMasker Information</H3>\n");
    while ((row = sqlNextRow(sr)) != NULL)
	{
	ro = rmskOutLoad(row);
	printf("<B>Name:</B> %s<BR>\n", ro->repName);
	printf("<B>Family:</B> %s<BR>\n", ro->repFamily);
	printf("<B>Class:</B> %s<BR>\n", ro->repClass);
	printf("<B>SW Score:</B> %d<BR>\n", ro->swScore);
	printf("<B>Divergence:</B> %3.1f%%<BR>\n", 0.1 * ro->milliDiv);
	printf("<B>Deletions:</B>  %3.1f%%<BR>\n", 0.1 * ro->milliDel);
	printf("<B>Insertions:</B> %3.1f%%<BR>\n", 0.1 * ro->milliIns);
	printf("<B>Begin in repeat:</B> %d<BR>\n", ro->repStart);
	printf("<B>End in repeat:</B> %d<BR>\n", ro->repEnd);
	printf("<B>Left in repeat:</B> %d<BR>\n", ro->repLeft);
	printf("<B>Chromosome:</B> %s<BR>\n", skipChr(ro->genoName));
	printf("<B>Begin in chromosome:</B> %d<BR>\n", ro->genoStart);
	printf("<B>End in chromosome:</B> %d<BR>\n", ro->genoEnd);
	printf("<B>Strand:</B> %s<BR>\n", ro->strand);
	printf("<BR>\n");
	}
    hFreeConn(&conn);
    }
else
    {
    if (sameString(repeat, "SINE"))
	printf("This track contains the small interspersed (SINE) class of repeats , which includes ALUs.<BR>\n");
    else if (sameString(repeat, "LINE"))
        printf("This track contains the long interspersed (LINE) class of repeats<BR>\n");
    else if (sameString(repeat, "LTR"))
        printf("This track contains the LTR (long terminal repeat) class of repeats which includes retroposons<BR>\n");
    else
        printf("This track contains the %s class of repeats<BR>\n", repeat);
    printf("Click right on top of an individual repeat for more information on that repeat<BR>\n");
    }
}

void doHgIsochore(char *item)
/* do click on isochore track. */
{
htmlStart("Isochore Info");
printf("<H2>Isochore Information</H2>\n");
if (cgiVarExists("o"))
    {
    struct isochores *iso;
    struct sqlConnection *conn = hAllocConn();
    struct sqlResult *sr;
    char **row;
    char query[256];
    int start = cgiInt("o");
    sprintf(query, "select * from isochores where  name = '%s' and chrom = '%s' and chromStart = %d",
	    item, seqName, start);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	iso = isochoresLoad(row);
	printf("<B>Type:</B> %s<BR>\n", iso->name);
	printf("<B>GC Content:</B> %3.1f%%<BR>\n", 0.1*iso->gcPpt);
	printf("<B>Chromosome:</B> %s<BR>\n", skipChr(iso->chrom));
	printf("<B>Begin in chromosome:</B> %d<BR>\n", iso->chromStart);
	printf("<B>End in chromosome:</B> %d<BR>\n", iso->chromEnd);
	printf("<B>Size:</B> %d<BR>\n", iso->chromEnd - iso->chromStart);
	printf("<BR>\n");
	isochoresFree(&iso);
	}
    hFreeConn(&conn);
    }
htmlHorizontalLine();
puts(
    "<h3>What's an Isochore</h3>"
    "<P>Isochores describe a region of a chromosome where the CG-content is"
    " either higher or lower than the whole genome average (42%).  A CG-rich"
    " isochore is given a dark color, while a CG-poor isochore is a light"
    " color.  </P>"
    ""
    "<P>Isochores were determined by first calculating the CG-content of 100,000bp"
    " windows across the genome.  These windows were either labeled H or L"
    " depending on whether the window contained a higher or lower GC-content"
    " than average.  A two-state HMM was created in which one state represented"
    " GC-rich regions, and the other GC-poor.  It was trained using the first 12"
    " chromosomes. The trained HMM was used to generate traces over all chromosomes."
    " These traces define the boundaries of the isochores,"
    " and their type (GC-rich or AT-rich).</P>"
    );
}

void doSimpleRepeat(char *item)
/* Print info on simple repeat. */
{
htmlStart("Simple Repeat Info");
printf("<H2>Simple Tandem Repeat Information</H2>\n");
if (cgiVarExists("o"))
    {
    struct simpleRepeat *rep;
    struct sqlConnection *conn = hAllocConn();
    struct sqlResult *sr;
    char **row;
    char query[256];
    int start = cgiInt("o");
    sprintf(query, "select * from simpleRepeat where  name = '%s' and chrom = '%s' and chromStart = %d",
	    item, seqName, start);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	rep = simpleRepeatLoad(row);
	printf("<B>Chromosome:</B> %s<BR>\n", skipChr(rep->chrom));
	printf("<B>Begin in chromosome:</B> %d<BR>\n", rep->chromStart);
	printf("<B>End in chromosome:</B> %d<BR>\n", rep->chromEnd);
	printf("<B>Size:</B> %d<BR>\n", rep->chromEnd - rep->chromStart);
	printf("<B>Period:</B> %d<BR>\n", rep->period);
	printf("<B>Copies:</B> %4.1f<BR>\n", rep->copyNum);
	printf("<B>Consensus size:</B> %d<BR>\n", rep->consensusSize);
	printf("<B>Match Percentage:</B> %d%%<BR>\n", rep->perMatch);
	printf("<B>Insert/Delete Percentage:</B> %d%%<BR>\n", rep->perIndel);
	printf("<B>Score:</B> %d<BR>\n", rep->score);
	printf("<B>Entropy:</B> %4.3f<BR>\n", rep->entropy);
	printf("<B>Sequence:</B> %s<BR>\n", rep->sequence);
	printf("<BR>\n");
	simpleRepeatFree(&rep);
	}
    hFreeConn(&conn);
    }
else
    {
    puts("<P>Click directly on a repeat for specific information on that repeat</P>");
    }
htmlHorizontalLine();
puts("<P>The Simple Tandem Repeats were calculated by James Durbin</P>");
}

void printPos(char *chrom, int start, int end)
/* Print position lines. */
{
printf("<B>Chromosome:</B> %s<BR>\n", skipChr(chrom));
printf("<B>Begin in chromosome:</B> %d<BR>\n", start);
printf("<B>End in chromosome:</B> %d<BR>\n", end);
}

void bedPrintPos(struct bed *bed)
/* Print first three fields of a bed type structure in
 * standard format. */
{
printPos(bed->chrom, bed->chromStart, bed->chromEnd);
}

void doCpgIsland(char *item, char *table)
/* Print info on CpG Island. */
{
htmlStart("CpG Island Info");
printf("<H2>CpG Island Info</H2>\n");
puts("<P>CpG islands are associated with genes, particularly housekeeping genes, in vertebrates. "
 "CpG islands are particularly common near transcription start sites, and may be associated "
 "with promoter regions.  Normally a C followed immediately by a G (a CpG) "
 "is rare in vertebrate DNA since the C's in such an arrangement tend to be "
 "methylated.  This methylation helps distinguish the newly synthesized DNA "
 "strand from the parent strand, which aids in the final stages of DNA "
 "proofreading after duplication.  However over evolutionary time methylated "
 "C's tend to turn into T's because of spontanious deamination.  The result "
 "is that CpG's are "
 "relatively rare unless there is selective pressure to keep them or a "
 "region is not methylated for some reason, perhaps having to do with "
 "the regulation of gene expression.  CpG islands "
 "are regions where CpG's are present at significantly higher levels than "
 "is typical for the genome as a whole." 
 "The CpG islands displayed in this browser are all also at least 200 "
 "bases long, and have a GC content of at least 50%.</P>");

htmlHorizontalLine();
if (cgiVarExists("o"))
    {
    struct cpgIsland *island;
    struct sqlConnection *conn = hAllocConn();
    struct sqlResult *sr;
    char **row;
    char query[256];
    int start = cgiInt("o");
    sprintf(query, "select * from %s where  name = '%s' and chrom = '%s' and chromStart = %d",
	    table, item, seqName, start);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	island = cpgIslandLoad(row);
	bedPrintPos((struct bed *)island);
	printf("<B>Size:</B> %d<BR>\n", island->chromEnd - island->chromStart);
	printf("<B>CpG count:</B> %d<BR>\n", island->cpgNum);
	printf("<B>C count plus G count:</B> %d<BR>\n", island->gcNum);
	printf("<B>Percentage CpG:</B> %1.1f%%<BR>\n", island->perCpg);
	printf("<B>Percentage C or G:</B> %1.1f%%<BR>\n", island->perGc);
	printf("<BR>\n");
	htmlHorizontalLine();
	printCappedSequence(island->chromStart, island->chromEnd, 100);
	cpgIslandFree(&island);
	}
    hFreeConn(&conn);
    }
else
    {
    puts("<P>Click directly on a CpG island for specific information on that island</P>");
    }
}

void printLines(FILE *f, char *s, int lineSize)
/* Print s, lineSize characters (or less) per line. */
{
int len = strlen(s);
int start, left;
int oneSize;

printf("<TT><PRE>");
for (start = 0; start < len; start += lineSize)
    {
    oneSize = len - start;
    if (oneSize > lineSize)
        oneSize = lineSize;
    mustWrite(f, s+start, oneSize);
    fputc('\n', f);
    }
if (start != len)
    fputc('\n', f);
printf("</PRE></TT>");
}

void showProteinPrediction(char *geneName, char *table)
/* Fetch and display protein prediction. */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct pepPred *pp = NULL;

if (!sqlTableExists(conn, table))
    {
    warn("Predicted peptide not yet available");
    }
else
    {
    sprintf(query, "select * from %s where name = '%s'", table, geneName);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	pp = pepPredLoad(row);
	}
    sqlFreeResult(&sr);
    if (pp != NULL)
	{
	printf("<H3>Translated Protein</H3>\n");
	printLines(stdout, pp->seq, 50);
	}
    else
        {
	warn("Sorry, currently there is no protein prediction for %s", geneName);
	}
    }
hFreeConn(&conn);
}

void showGenePos(char *name, char *table)
/* Show gene prediction position and other info. */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct genePred *gp;

sprintf(query, "select * from %s where name = '%s'", table, name);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredLoad(row);
    printPos(gp->chrom, gp->txStart, gp->txEnd);
    genePredFree(&gp);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

boolean isGenieGeneName(char *name)
/* Return TRUE if name is in form to be a genie name. */
{
char *s, *e;
int prefixSize;

e = strchr(name, '.');
if (e == NULL)
   return FALSE;
prefixSize = e - name;
if (prefixSize > 3 || prefixSize == 0)
   return FALSE;
s = e+1;
if (!startsWith("ctg", s))
   return FALSE;
e = strchr(name, '-');
if (e == NULL)
   return FALSE;
return TRUE;
}

char *hugoToGenieName(char *hugoName, char *table)
/* Covert from hugo to genie name. */
{
struct sqlConnection *conn = hAllocConn();
char query[256];
static char buf[256], *name;

sprintf(query, "select transId from %s where name = '%s'", table, hugoName);
name = sqlQuickQuery(conn, query, buf, sizeof(buf));
hFreeConn(&conn);
if (name == NULL)
    errAbort("Database inconsistency: couldn't find gene name %s in knownInfo",
    	hugoName);
return name;
}

void geneShowCommon(char *geneName, char *geneTable, char *pepTable)
/* Show parts of gene common to everything */
{
char other[256];
showGenePos(geneName, geneTable);
printf("<H3>Links to sequence:</H3>\n");
printf("<UL>\n");
hgcAnchorSomewhere("htcTranslatedProtein", geneName, pepTable, seqName);
printf("<LI>Translated Protein</A>\n"); 
hgcAnchorSomewhere("htcGeneMrna", geneName, geneTable, seqName);
printf("<LI>Predicted mRNA</A>\n");
hgcAnchorSomewhere("htcGeneInGenome", geneName, geneTable, seqName);
printf("<LI>Genomic Sequence</A>\n");
printf("</UL>\n");
}


void htcTranslatedProtein(char *geneName)
/* Display translated protein. */
{
htmlStart("Protein Translation");
showProteinPrediction(geneName, cgiString("o"));
}

void getCdsInMrna(struct genePred *gp, int *retCdsStart, int *retCdsEnd)
/* Given a gene prediction, figure out the
 * CDS start and end in mRNA coordinates. */
{
int missingStart = 0, missingEnd = 0;
int exonStart, exonEnd, exonSize, exonIx;
int totalSize = 0;

for (exonIx = 0; exonIx < gp->exonCount; ++exonIx)
    {
    exonStart = gp->exonStarts[exonIx];
    exonEnd = gp->exonEnds[exonIx];
    exonSize = exonEnd - exonStart;
    totalSize += exonSize;
    missingStart += exonSize - rangeIntersection(exonStart, exonEnd, gp->cdsStart, exonEnd);
    missingEnd += exonSize - rangeIntersection(exonStart, exonEnd, exonStart, gp->cdsEnd);
    }
*retCdsStart = missingStart;
*retCdsEnd = totalSize - missingEnd;
}

int genePredCdnaSize(struct genePred *gp)
/* Return total size of all exons. */
{
int totalSize = 0;
int exonIx;

for (exonIx = 0; exonIx < gp->exonCount; ++exonIx)
    {
    totalSize += (gp->exonEnds[exonIx] - gp->exonStarts[exonIx]);
    }
return totalSize;
}

struct dnaSeq *getCdnaSeq(struct genePred *gp)
/* Load in cDNA sequence associated with gene prediction. */
{
int txStart = gp->txStart;
struct dnaSeq *genoSeq = hDnaFromSeq(gp->chrom, txStart, gp->txEnd,  dnaLower);
struct dnaSeq *cdnaSeq;
int cdnaSize = genePredCdnaSize(gp);
int cdnaOffset = 0, exonStart, exonSize, exonIx;

AllocVar(cdnaSeq);
cdnaSeq->dna = needMem(cdnaSize+1);
cdnaSeq->size = cdnaSize;
for (exonIx = 0; exonIx < gp->exonCount; ++exonIx)
    {
    exonStart = gp->exonStarts[exonIx];
    exonSize = gp->exonEnds[exonIx] - exonStart;
    memcpy(cdnaSeq->dna + cdnaOffset, genoSeq->dna + (exonStart - txStart), exonSize);
    cdnaOffset += exonSize;
    }
assert(cdnaOffset == cdnaSeq->size);
freeDnaSeq(&genoSeq);
return cdnaSeq;
}

void htcGeneMrna(char *geneName)
/* Display associated cDNA. */
{
char *table = cgiString("o");
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct genePred *gp;
struct dnaSeq *seq;
int cdsStart, cdsEnd;

htmlStart("DNA Near Gene");
sprintf(query, "select * from %s where name = '%s'", table, geneName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredLoad(row);
    seq = getCdnaSeq(gp);
    getCdsInMrna(gp, &cdsStart, &cdsEnd);
    toUpperN(seq->dna + cdsStart, cdsEnd - cdsStart);
    if (gp->strand[0] == '-')
	{
        reverseComplement(seq->dna, seq->size);
	}
    printf("<TT><PRE>");
    faWriteNext(stdout, NULL, seq->dna, seq->size);
    printf("</TT></PRE>");
    genePredFree(&gp);
    freeDnaSeq(&seq);
    }
sqlFreeResult(&sr);
}

void htcGeneInGenome(char *geneName)
/* Put up page that lets user display genomic sequence
 * associated with gene. */
{
htmlStart("Genomic Sequence Near Gene");
printf("<H2>Get Genomic Sequence Near Gene</H2>");
printf("<FORM ACTION=\"%s\">\n\n", hgcPath());
cgiMakeHiddenVar("g", "htcDnaNearGene");
cgiContinueHiddenVar("i");
printf("\n");
cgiContinueHiddenVar("db");
printf("\n");
cgiContinueHiddenVar("c");
printf("\n");
cgiContinueHiddenVar("l");
printf("\n");
cgiContinueHiddenVar("r");
printf("\n");
cgiContinueHiddenVar("o");
printf("\n");
printf("<INPUT TYPE=RADIO NAME=how VALUE = \"tx\" CHECKED>Transcript<BR>");
printf("<INPUT TYPE=RADIO NAME=how VALUE = \"cds\">Coding Region Only<BR>");
printf("<INPUT TYPE=RADIO NAME=how VALUE = \"txPlus\">Transcript + Promoter<BR>");
printf("<INPUT TYPE=RADIO NAME=how VALUE = \"promoter\">Promoter Only<BR>");
printf("Promoter Size: ");
cgiMakeIntVar("promoterSize", 1000, 6);
printf("<BR>");
cgiMakeButton("submit", "submit");
printf("</FORM>");
}

void toUpperExons(int startOffset, struct dnaSeq *seq, struct genePred *gp)
/* Upper case bits of DNA sequence that are exons according to gp. */
{
int s, e, size;
int exonIx;
int seqStart = startOffset, seqEnd = startOffset + seq->size;

if (seqStart < gp->txStart)
    seqStart = gp->txStart;
if (seqEnd > gp->txEnd)
    seqEnd = gp->txEnd;
    
for (exonIx = 0; exonIx < gp->exonCount; ++exonIx)
    {
    s = gp->exonStarts[exonIx];
    e = gp->exonEnds[exonIx];
    if (s < seqStart) s = seqStart;
    if (e > seqEnd) e = seqEnd;
    if ((size = e - s) > 0)
	{
	s -= startOffset;
	if (s < 0 ||  s + size > seq->size)
	   errAbort("Out of range! %d-%d not in %d-%d", s, s+size, 0, size);
	toUpperN(seq->dna + s, size);
	}
    }
}

void htcDnaNearGene(char *geneName)
/* Fetch DNA near a gene. */
{
char *table = cgiString("o");
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row = NULL;
struct genePred *gp = NULL;
struct dnaSeq *seq = NULL;
char *how = cgiString("how");
int start, end, promoSize;
boolean isRev;
char faLine[256];

htmlStart("Predicted mRNA");
sprintf(query, "select * from %s where name = '%s'", table, geneName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredLoad(row);
    isRev = (gp->strand[0] == '-');
    start = gp->txStart;
    end = gp->txEnd;
    promoSize = cgiInt("promoterSize");
    if (sameString(how, "cds"))
        {
	start = gp->cdsStart;
	end = gp->cdsEnd;
	}
    else if (sameString(how, "txPlus"))
        {
	if (isRev)
	    {
	    end += promoSize;
	    }
	else
	    {
	    start -= promoSize;
	    if (start < 0) start = 0;
	    }
	}
    else if (sameString(how, "promoter"))
        {
	if (isRev)
	    {
	    start = gp->txEnd;
	    end = start + promoSize;
	    }
	else
	    {
	    end = gp->txStart;
	    start = end - promoSize;
	    if (start < 0) start = 0;
	    }
	}
    seq = hDnaFromSeq(gp->chrom, start, end, dnaLower);
    toUpperExons(start, seq, gp);
    if (isRev)
        reverseComplement(seq->dna, seq->size);
    printf("<TT><PRE>");
    sprintf(faLine, "%s:%d-%d %s exons in upper case",
    	gp->chrom, start+1, end,
	(isRev ? "(reverse complemented)" : "") );
    faWriteNext(stdout, faLine, seq->dna, seq->size);
    printf("</TT></PRE>");
    genePredFree(&gp);
    freeDnaSeq(&seq);
    }
sqlFreeResult(&sr);
}

void doKnownGene(char *geneName)
/* Handle click on known gene track. */
{
char *transName = geneName;
struct knownMore *km = NULL;
boolean anyMore = FALSE;
boolean upgraded = FALSE;
char *knownTable = "knownInfo";
boolean knownMoreExists = FALSE;

htmlStart("Known Gene");
printf("<H2>Known Gene %s</H2>\n", geneName);
if (hTableExists("knownMore"))
    {
    knownMoreExists = TRUE;
    knownTable = "knownMore";
    }
if (!isGenieGeneName(geneName))
    transName = hugoToGenieName(geneName, knownTable);
else
    geneName = NULL;
    
if (knownMoreExists)
    {
    char query[256];
    struct sqlResult *sr;
    char **row;
    struct sqlConnection *conn = hAllocConn();

    upgraded = TRUE;
    sprintf(query, "select * from knownMore where transId = '%s'", transName);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
       {
       km = knownMoreLoad(row);
       }
    sqlFreeResult(&sr);

    hFreeConn(&conn);
    }

if (km != NULL)
    {
    geneName = km->name;
    if (km->hugoName != NULL && km->hugoName[0] != 0)
	medlineLinkedLine("Name", km->hugoName, km->hugoName);
    if (km->aliases[0] != 0)
	printf("<B>Aliases:</B> %s<BR>\n", km->aliases);
    if (km->omimId != 0)
	{
	printf("<B>OMIM:</B> ");
	printf(
	   "<A HREF = \"http://www.ncbi.nlm.nih.gov/entrez/dispomim.cgi?id=%d\" TARGET=_blank>%d</A><BR>\n", 
	    km->omimId, km->omimId);
	}
    if (km->locusLinkId != 0)
        {
	printf("<B>LocusLink:</B> ");
	printf("<A HREF = \"http://www.ncbi.nlm.nih.gov/LocusLink/LocRpt.cgi?l=%d\" TARGET=_blank>",
		km->locusLinkId);
	printf("%d</A><BR>\n", km->locusLinkId);
	}
    anyMore = TRUE;
    }
if (geneName != NULL) 
    {
    medlineLinkedLine("Symbol", geneName, geneName);
    printf("<B>GeneCards:</B> ");
    printf("<A HREF = \"http://bioinfo.weizmann.ac.il/cards-bin/cardsearch.pl?search=%s\" TARGET=_blank>",
	    geneName);
    printf("%s</A><BR>\n", geneName);
    anyMore = TRUE;
    }

if (anyMore)
    htmlHorizontalLine();

geneShowCommon(transName, "genieKnown", "genieKnownPep");
puts(
   "<P>Known genes are derived from the "
   "<A HREF = \"http://www.ncbi.nlm.nih.gov/LocusLink/\" TARGET=_blank>"
   "RefSeq</A> mRNA database supplemented by other mRNAs with annotated coding regions.  "
   "These mRNAs were mapped to the draft "
   "human genome using <A HREF = \"http://www.cse.ucsc.edu/~kent\" TARGET=_blank>"
   "Jim Kent's</A> PatSpace software, and further processed by "
   "<A HREF = \"http://www.affymetrix.com\" TARGET=_blank>Affymetrix</A> to cope "
   "with sequencing errors in the draft.</P>\n");
if (!upgraded)
    {
    puts(
       "<P>The treatment of known genes in the browser is quite "
       "preliminary.  We hope to provide links into OMIM and LocusLink "
       "in the near future.</P>");
    }
puts(
   "<P>Additional information may be available by clicking on the with the "
   "mRNA associated with this gene in the main browser window.</P>");
}

void doGeniePred(char *geneName)
/* Handle click on Genie prediction track. */
{
htmlStart("Genie Gene Prediction");
printf("<H2>Genie Gene Prediction %s</H2>\n", geneName);
geneShowCommon(geneName, "genieAlt", "genieAltPep");
puts(
   "<P>Genie predictions are based on "
   "<A HREF = \"http://www.affymetrix.com\" TARGET=_blank>Affymetrix's</A> "
   "Genie gene finding software.  Genie is a generalized HMM "
   "which accepts constraints based on mRNA and EST data. </P>"
   "<P>The treatment of predicted genes in the browser is still "
   "preliminary.  In particular gene names are not kept stable "
   "between versions of the draft human genome.");
}

void doEnsPred(char *geneName)
/* HAndle click on Ensembl gene track. */
{
struct sqlConnection *conn = hAllocConn();
htmlStart("Ensembl Prediction");
printf("<H2>Ensembl Prediction %s</H2>\n", geneName);
geneShowCommon(geneName, "ensGene", "ensPep");
if (!sameString(database, "hg3"))
    {
    printf("<P>Visit <A HREF=\"http://www.ensembl.org/perl/transview?transcript=%s\" _TARGET=blank>"
       "Ensembl TransView</A> for more information on this gene prediction.", geneName);      
    }
hFreeConn(&conn);
}

char *getGi(char *ncbiFaHead)
/* Get GI number from NCBI FA format header. */
{
char *s;
static char gi[64];

if (!startsWith("gi|", ncbiFaHead))
    return NULL;
ncbiFaHead += 3;
strncpy(gi, ncbiFaHead, sizeof(gi));
s = strchr(gi, '|');
if (s != NULL) 
    *s = 0;
return trimSpaces(gi);
}

void showHomologies(char *geneName, char *table)
/* Show homology info. */
{
struct sqlConnection *conn = hAllocConn();
char query[256];
struct sqlResult *sr;
char **row;
boolean isFirst = TRUE, gotAny = FALSE;
char *gi;
struct softberryHom hom;


if (sqlTableExists(conn, table))
    {
    sprintf(query, "select * from %s where name = '%s'", table, geneName);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	softberryHomStaticLoad(row, &hom);
	if ((gi = getGi(hom.giString)) == NULL)
	    continue;
	if (isFirst)
	    {
	    htmlHorizontalLine();
	    printf("<H3>Protein Homologies:</H3>\n");
	    isFirst = FALSE;
	    gotAny = TRUE;
	    }
	printf("<A HREF=");
	sprintf(query, "%s[gi]", gi);
	printEntrezProteinUrl(stdout, query);
	printf(" TARGET=_blank>%s</A> %s<BR>", hom.giString, hom.description);
	}
    }
if (gotAny)
    htmlHorizontalLine();
hFreeConn(&conn);
}

void doSoftberryPred(char *geneName)
/* Handle click on Softberry gene track. */
{
htmlStart("Fgenesh++ Gene Prediction");
printf("<H2>Fgenesh++ Gene Prediction %s</H2>\n", geneName);
showHomologies(geneName, "softberryHom");
geneShowCommon(geneName, "softberryGene", "softberryPep");
puts(
   "<P>Fgenesh++ predictions are based on Softberry's gene finding software. "
   "Fgenesh++ uses both HMMs and protein similarity to find genes in a "
   "completely automated manner.  See the "
   "paper \"Ab initio gene finding in </I>Drosophila genomic DNA\", <I>"
   "Genome Research</I> 10(5) 516-522 for more information.</P>"
   "<P>The Fgenesh++ gene predictions were produced by "
   "<A HREF=\"http://www.softberry.com\" TARGET=_blank>Softberry Inc.</A> "
   "Commercial use of these predictions is restricted to viewing in "
   "this browser.  Please contact Softberry Inc. to make arrangements "
   "for further commercial access.");
}


void parseChromPointPos(char *pos, char *retChrom, int *retPos)
/* Parse out chrN:123 into chrN and 123. */
{
char *s, *e;
int len;
e = strchr(pos, ':');
if (e == NULL)
   errAbort("No : in chromosome point position %s", pos);
len = e - pos;
memcpy(retChrom, pos, len);
retChrom[len] = 0;
s = e+1;
*retPos = atoi(s);
}

void doGenomicDups(char *dupName)
/* Handle click on genomic dup track. */
{
struct genomicDups dup;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char oChrom[64];
int oStart;



htmlStart("Genomic Duplications");
printf("<H2>Genomic Duplication</H2>\n");
if (cgiVarExists("o"))
    {
    int start = cgiInt("o");
    parseChromPointPos(dupName, oChrom, &oStart);

    sprintf(query, "select * from genomicDups where chrom = '%s' and chromStart = %d "
		   "and otherChrom = '%s' and otherStart = %d",
		   seqName, start, oChrom, oStart);
    sr = sqlGetResult(conn, query);
    while (row = sqlNextRow(sr))
	{
	genomicDupsStaticLoad(row, &dup);
	printf("<B>First Position:</B> %s:%d-%d<BR>\n",
	   dup.chrom, dup.chromStart, dup.chromEnd);
	printf("<B>Second Position:</B> %s:%d-%d<BR>\n",
	   dup.otherChrom, dup.otherStart, dup.otherEnd);
	printf("<B>Relative orientation:</B> %s<BR>\n", dup.strand);
	printf("<B>Percent identity:</B> %3.1f%%<BR>\n", 0.1*dup.score);
	printf("<B>Size:</B> %d<BR>\n", dup.alignB);
	printf("<B>Bases matching:</B> %d<BR>\n", dup.matchB);
	printf("<B>Bases not matching:</B> %d<BR>\n", dup.mismatchB);
	htmlHorizontalLine();
	}
    }
else
    {
    puts("<P>Click directly on a repeat for specific information on that repeat</P>");
    }
puts("This region was detected as a duplication within the golden path. "
     "Regions with more than 1000 bases of non-RepeatMasked sequence and "
     "greater than 90% pairwise similarity are included in this track. "
     "Duplications of 99% or greater similarity are shown as orange. "
     "Duplications of 98% - 99% similarity are shown as yellow. "
     "Duplications of 90% - 98% similarity are shown as shades of gray. "
     "The data was provided by <A HREF=\"mailto:jab@cwru.edu\">Evan Eichler "
     "and Jeff Bailey</A>.");
hFreeConn(&conn);
}

void doExoFish(char *itemName)
/* Handle click on genomic dup track. */
{
struct exoFish el;
int start = cgiInt("o");
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];

htmlStart("Exofish Ecores");
printf("<H2>Exofish ECORES</A></H2>\n");

sprintf(query, "select * from exoFish where chrom = '%s' and chromStart = %d",
    seqName, start);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    exoFishStaticLoad(row, &el);
    printf("<B>score:</B> %d<BR>\n", el.score);
    bedPrintPos((struct bed *)&el);
    htmlHorizontalLine();
    }

puts("<P>The Exofish track shows regions of homology with the "
     "pufferfish <I>Tetraodon nigroviridis</I>.  This information "
     "was provided by Olivier Jaillon at Genoscope.  For further "
     "information and other Exofish tools please visit the "
     "<A HREF=\"http://www.genoscope.cns.fr/exofish\" TARGET = _blank>"
     "Genoscope Exofish web site</A>, or "
     "email <A HREF=\"mailto:exofish@genoscope.cns.fr\">"
     "exofish@genoscope.cns.fr</A>.  The following paper also describes "
     "exofish:<BR>"
     "'Estimate of human gene number provided by "
     "genome-wide analysis using <I>Tetraodon nigroviridis</I> "
     "DNA sequence' <I>Nature Genetics</I> volume 25 page 235, "
     "June 2000.</P>");
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doEst3(char *itemName)
/* Handle click on EST 3' end track. */
{
struct est3 el;
int start = cgiInt("o");
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];

htmlStart("EST 3' Ends");
printf("<H2>EST 3' Ends</H2>\n");

sprintf(query, "select * from est3 where chrom = '%s' and chromStart = %d",
    seqName, start);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    est3StaticLoad(row, &el);
    printf("<B>EST 3' End Count:</B> %d<BR>\n", el.estCount);
    bedPrintPos((struct bed *)&el);
    printf("<B>strand:</B> %s<BR>\n", el.strand);
    htmlHorizontalLine();
    }

puts("<P>This track shows where clusters of EST 3' ends hit the "
     "genome.  In many cases these represent the 3' ends of genes. "
     "This data was kindly provided by Lukas Wagner and Greg Schuler "
     "at NCBI.  Additional filtering was applied by Jim Kent.</P>");
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doRnaGene(char *itemName)
/* Handle click on EST 3' end track. */
{
struct rnaGene rna;
int start = cgiInt("o");
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];

htmlStart("RNA Genes");
printf("<H2>RNA Gene %s</H2>\n", itemName);

sprintf(query, "select * from rnaGene where chrom = '%s' and chromStart = %d and name = '%s'",
    seqName, start, itemName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    rnaGeneStaticLoad(row, &rna);
    printf("<B>name:</B> %s<BR>\n", rna.name);
    printf("<B>type:</B> %s<BR>\n", rna.type);
    printf("<B>score:</B> %2.1f<BR>\n", rna.fullScore);
    printf("<B>is pseudo-gene:</B> %s<BR>\n", (rna.isPsuedo ? "yes" : "no"));
    printf("<B>program predicted with:</B> %d<BR>\n", rna.source);
    bedPrintPos((struct bed *)&rna);
    printf("<B>strand:</B> %s<BR>\n", rna.strand);
    htmlHorizontalLine();
    }

puts("<P>This track shows where non-protein coding RNA genes and "
     "pseudo-genes are located.  This data was kindly provided by "
     "Sean Eddy at Washington University.</P>");
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doStsMarker(char *item)
/* Respond to click on an STS marker. */
{
struct stsMarker el;
int start = cgiInt("o");
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];
boolean firstTime = TRUE;
struct sqlConnection *conn2 = hAllocConn();
struct sqlResult *sr2;
char **row2;
char *alias;
boolean doAlias = sqlTableExists(conn, "stsAlias");

htmlStart("STS Marker");
printf("<H2>STS Marker %s</H2>\n", item);

if (sameString(item, "NONAME"))
    sprintf(query, "select * from stsMarker where name = '%s' and chromStart = %d", item, start);
else
    sprintf(query, "select * from stsMarker where name = '%s'", item);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    stsMarkerStaticLoad(row, &el);
    if (firstTime)
	{
	boolean firstAlias = TRUE;
	if (doAlias)
	    {
	    printf("<B>name:</B> %s<BR>\n", el.name);
	    sprintf(query, "select alias from stsAlias where trueName = '%s'", 
		    el.name);
	    sr2 = sqlGetResult(conn2, query);
	    while ((row2 = sqlNextRow(sr2)) != NULL)
		 {
		 if (firstAlias)
		     {
		     printf("<B>alias:</B>");
		     firstAlias = FALSE;
		     }
		 printf(" %s", row2[0]);
		 }
	    if (!firstAlias)
		 printf("<BR>\n");
	    }
	printf("<B>STS id:</B> %d<BR>\n", el.identNo);
	if (!sameString(el.ctgAcc, "-"))
	    printf("<B>clone hit:</B> %s<BR>\n", el.ctgAcc);
	if (!sameString(el.otherAcc, "-"))
	    printf("<B>Other clones hit:</B> %s<BR>\n", el.otherAcc);
	if (!sameString(el.genethonChrom, "0"))
	    printf("<B>Genethon:</B> chr%s %f<BR>\n", el.genethonChrom, el.genethonPos);
	if (!sameString(el.marshfieldChrom, "0"))
	    printf("<B>Marshfield:</B> chr%s %f<BR>\n", el.marshfieldChrom, el.marshfieldPos);
	if (!sameString(el.gm99Gb4Chrom, "0"))
	    printf("<B>GeneMap99:</B> chr%s %f<BR>\n", el.gm99Gb4Chrom, el.gm99Gb4Pos);
	if (!sameString(el.shgcG3Chrom, "0"))
	    printf("<B>Stanford G3:</B> chr%s %f<BR>\n", el.shgcG3Chrom, el.shgcG3Pos);
	if (!sameString(el.wiYacChrom, "0"))
	    printf("<B>Whitehead YAC:</B> chr%s %f<BR>\n", el.wiYacChrom, el.wiYacPos);
#ifdef SOON
	if (!sameString(el.shgcTngChrom, "0"))
	    printf("<B>Stanford TNG:</B> chr%s %f<BR>\n", el.shgcTngChrom, el.shgcTngPos);
#endif
        if (!sameString(el.fishChrom, "0"))
	    {
	    printf("<B>FISH:</B> %s.%s - %s.%s<BR>\n", el.fishChrom, 
	        el.beginBand, el.fishChrom, el.endBand);
	    }
	firstTime = FALSE;
	printf("<H3>This marker maps to the following positions on the draft assembly:</H3>\n");
	printf("<BLOCKQUOTE>\n");
	}
    printf("%s\t%d<BR>\n", el.chrom, (el.chromStart+el.chromEnd)>>1);
    }
printf("</BLOCKQUOTE>\n");
htmlHorizontalLine();

puts("<P>This track shows locations of STSs (Sequence Tagged Sites) "
     "along the draft assembly.  Many of these STSs have been mapped "
     "with genetic techiques (in the Genethon and Marshfield maps) or "
     "using radiation hybrid (the Stanford, and GeneMap99 maps) and "
     "YAC mapping (the Whitehead map) techniques.  This track also "
     "shows the approximate position of FISH mapped clones. "
     "Many thanks to the researchers who worked on these "
     "maps, and to Greg Schuler, Arek Kasprzyk, Wonhee Jang, "
     "Terry Furey and Sanja Rogic for helping "
     "process the data. Additional data on the individual maps can be "
     "found at the following links:</P>"
     "<UL>"
     "<LI><A HREF=http://www-shgc.stanford.edu/Mapping/Marker/STSindex.html>"
          "Stanford G3 and TNG</A>"
     "<LI><A HREF=http://www.ncbi.nlm.nih.gov/genemap/>"
          "The GeneMap99 map.</A>"
     "<LI><A HREF=http://carbon.wi.mit.edu:8000/ftp/distribution/human_STS_releases/july97/07-97.INTRO.html#RH>"
          "The Whitehead YAC map</A>"
     "<LI><A HREF=http://research.marshfieldclinic.org/genetics/>"
          "The Marshfield map</A>"
     "<LI><A HREF=ftp://ftp.genethon.fr/pub/Gmap/Nature-1995>"
          "The Genethon map</A>"
     "<LI><A HREF=\"http://www.ncbi.nlm.nih.gov/genome/cyto/\">"
          "The FISH map</A>"
     "</UL>");
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doMouseSyn(char *itemName)
/* Handle click on mouse synteny track. */
{
struct mouseSyn el;
int start = cgiInt("o");
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];

htmlStart("Mouse Synteny");
printf("<H2>Mouse Synteny</H2>\n", itemName);

sprintf(query, "select * from mouseSyn where chrom = '%s' and chromStart = %d",
    seqName, start);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    mouseSynStaticLoad(row, &el);
    printf("<B>mouse chromosome:</B> %s<BR>\n", el.name+6);
    printf("<B>human chromosome:</B> %s<BR>\n", skipChr(el.chrom));
    printf("<B>human starting base:</B> %d<BR>\n", el.chromStart);
    printf("<B>human ending base:</B> %d<BR>\n", el.chromEnd);
    printf("<B>size:</B> %d<BR>\n", el.chromEnd - el.chromStart);
    htmlHorizontalLine();
    }
sqlFreeResult(&sr);
hFreeConn(&conn);

puts("<P>This track syntenous (corresponding) regions between human "
     "and mouse chromosomes.  This track was created by looking for "
     "homology to known mouse genes in the draft assembly.  The data "
     "for this track was kindly provided by "
     "<A HREF=\"mailto:church@ncbi.nlm.nih.gov\">Deanna Church</A> at NCBI. Please "
     "visit <A HREF=\"http://www.ncbi.nlm.nih.gov/Homology/\" TARGET = _blank>"
     "http://www.ncbi.nlm.nih.gov/Homology/</A> for more details.");
}

void doCytoBands(char *itemName)
/* Describe cytological band track. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];
int start = cgiInt("o");
struct cytoBand el;

htmlStart("Chromosome Bands");
printf("<H2>Chromosome Bands</H2>\n", itemName);
sprintf(query, 
	"select * from cytoBand where chrom = '%s' and chromStart = '%d'",
	seqName, start);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    cytoBandStaticLoad(row, &el);
    bedPrintPos((struct bed *)&el);
    htmlHorizontalLine();
    }
sqlFreeResult(&sr);
hFreeConn(&conn);

printf(
 "<P>The chromosome band track represents the approximate "
 "location of bands seen on Geimsa-stained chromosomes at "
 "an 800 band resolution. </P> "
 "<P>Barbara Trask, Vivian Chung and colleagues used fluorescent in-situ "
 "hybridization (FISH) to locate large genomic clones on the chromosomes. "
 "Arek Kasprzyk and Wonhee Jang determined the location "
 "of these clones on the assembled draft human genome"
 "using the clone sequence where possible and in other cases by "
 "fingerprint analysis and other data.  Terry Furey and "
 "David Haussler developed a program which uses a hidden Markov model "
 "(HMM) to integrate the range of bands associated with various "
 "FISHed clones into the most probable arrangement of bands on the "
 "sequence given the assumptions of the model.<P>"
 "<P>For further information please consult the web site "
 "<A HREF=\"http://fishfarm.fhcrc.org/bacresource/index.shtml\">"
 "Resource of FISH-mapped BAC clones</A>.  Please cite the paper "
 "<I>From chromosomal aberrations to genes: Linking the cytogenetic "
 "and sequence maps of the human genome</I> by Cheung et al., which "
 "is currently in preparation."
 );
}

void doSnp(char *group, char *itemName)
/* Put up info on a SNP. */
{
struct snp snp;
int start = cgiInt("o");
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];

htmlStart("Single Nucleotide Polymorphism (SNP)");
printf("<H2>Single Nucleotide Polymorphism (SNP) rs%s</H2>\n", itemName);

sprintf(query, "select * from %s where chrom = '%s' and chromStart = %d and name = '%s'",
    group, seqName, start, itemName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    snpStaticLoad(row, &snp);
    bedPrintPos((struct bed *)&snp);
    }
printf(
  "<P><A HREF=\"http://www.ncbi.nlm.nih.gov/SNP/snp_ref.cgi?type=rs&rs=%s\" TARGET=_blank>", 
  snp.name);
printf("dbSNP link</A></P>\n");
htmlHorizontalLine();
puts("<P>This track shows locations of Single Nucleotide Polymorphisms. "
     "Thanks to the SNP Consortium and NIH for providing this data.</P>");
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doMiddle()
/* Generate body of HTML. */
{
char *group = cgiString("g");
char *item = cgiOptionalString("i");
char title[256];

database = cgiOptionalString("db");
if (database == NULL)
    database = "hg5";
hSetDb(database);
seqName = cgiString("c");
winStart = cgiInt("l");
winEnd = cgiInt("r");

if (sameWord(group, "getDna"))
    {
    doGetDna();
    }
else if (sameWord(group, "hgMrna") || sameWord(group, "hgMrna2"))
    {
    doHgRna(item, FALSE);
    }
else if (sameWord(group, "hgEst") || sameWord(group, "hgIntronEst"))
    {
    doHgRna(item, TRUE);
    }
else if (sameWord(group, "hgContig"))
    {
    doHgContig(item);
    }
else if (sameWord(group, "hgCover"))
    {
    doHgCover(item);
    }
else if (sameWord(group, "hgClone"))
    {
    doHgClone(item);
    }
else if (sameWord(group, "hgGold"))
    {
    doHgGold(item);
    }
else if (sameWord(group, "hgGap"))
    {
    doHgGap(item);
    }
else if (sameWord(group, "hgTet"))
    {
    doHgTet(item);
    }
else if (sameWord(group, "hgRepeat"))
    {
    doHgRepeat(item);
    }
else if (sameWord(group, "hgIsochore"))
    {
    doHgIsochore(item);
    }
else if (sameWord(group, "hgSimpleRepeat"))
    {
    doSimpleRepeat(item);
    }
else if (sameWord(group, "hgCpgIsland"))
    {
    doCpgIsland(item, "cpgIsland");
    }
else if (sameWord(group, "hgCpgIsland2"))
    {
    doCpgIsland(item, "cpgIsland2");
    }
else if (sameWord(group, "genieKnown"))
    {
    doKnownGene(item);
    }
else if (sameWord(group, "genieAlt"))
    {
    doGeniePred(item);
    }
else if (sameWord(group, "hgEnsGene"))
    {
    doEnsPred(item);
    }
else if (sameWord(group, "hgSoftberryGene"))
    {
    doSoftberryPred(item);
    }
else if (sameWord(group, "hgGenomicDups"))
    {
    doGenomicDups(item);
    }
else if (sameWord(group, "hgExoFish"))
    {
    doExoFish(item);
    }
else if (sameWord(group, "hgEst3"))
    {
    doEst3(item);
    }
else if (sameWord(group, "hgRnaGene"))
    {
    doRnaGene(item);
    }
else if (sameWord(group, "hgStsMarker"))
    {
    doStsMarker(item);
    }
else if (sameWord(group, "hgMouseSyn"))
    {
    doMouseSyn(item);
    }
else if (sameWord(group, "hgCytoBands"))
    {
    doCytoBands(item);
    }
else if (sameWord(group, "snpTsc") || sameWord(group, "snpNih"))
    {
    doSnp(group, item);
    }
else if (sameWord(group, "htcCloneSeq"))
    {
    htcCloneSeq(item);
    }
else if (sameWord(group, "htcCdnaAli"))
   {
   htcCdnaAli(item);
   }
else if (sameWord(group, "htcTranslatedProtein"))
   {
   htcTranslatedProtein(item);
   }
else if (sameWord(group, "htcGeneMrna"))
   {
   htcGeneMrna(item);
   }
else if (sameWord(group, "htcGeneInGenome"))
   {
   htcGeneInGenome(item);
   }
else if (sameWord(group, "htcDnaNearGene"))
   {
   htcDnaNearGene(item);
   }
else
   {
   htmlStart(group);
   printf("Sorry, clicking there doesn't do anything yet (%s).", group);
   }
htmlEnd();
}

int main(int argc, char *argv[])
{
dnaUtilOpen();
htmEmptyShell(doMiddle, NULL);
}
