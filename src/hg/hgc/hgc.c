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
#include "roughAli.h"
#include "exprBed.h"
#include "refLink.h"
#include "browserTable.h"
#include "hgConfig.h"
#include "estPair.h"

#define CHUCK_CODE 0
#define ROGIC_CODE 1
char *seqName;		/* Name of sequence we're working on. */
int winStart, winEnd;   /* Bounds of sequence. */
char *database;		/* Name of mySQL database. */

char *entrezScript = "http://www.ncbi.nlm.nih.gov/htbin-post/Entrez/query?form=4";

#ifdef CHUCK_CODE
struct browserTable *checkDbForTables()
/* Look in the database meta table to get information on which
 *   tables to load that aren't hardcoded into the database. */
{
struct browserTable *tableList = NULL;
struct browserTable *table = NULL;
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
if(!hTableExists("browserTable"))
    return NULL;
sprintf(query, "select * from browserTable order by priority");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    table = browserTableLoad(row);
    slAddHead(&tableList, table);
    }
slReverse(&tableList);
sqlFreeResult(&sr);
hFreeConn(&conn);
return tableList;
}

struct browserTable *getBrowserTableFromList(char *group, struct browserTable *tableList)
/* Look through list to see if that group is defined in meta-table */
{
struct browserTable *table = NULL;
for(table=tableList; table!=NULL; table=table->next)
    {
    if(sameString(group,table->mapName))
	return table;
    }
return NULL;
}

boolean inBrowserTable(char *group, struct browserTable *tableList)
{
struct browserTable *table = getBrowserTableFromList(group,tableList);
if(table == NULL)
    return FALSE;
else
    return TRUE;
}

void doBrowserTable(char *group, struct browserTable *tableList)
{
struct browserTable *table = getBrowserTableFromList(group,tableList);
htmlStart(table->shortLabel);
printf("<h2>Track: %s, Version: %s.</h2>\n",table->shortLabel, table->version);
printf("<p>This track was prepared by:<br>%s\n",table->credit);
if(table->other != NULL)
    printf("<p>%s\n", table->other);
printf("<p>For more information please see <a href=\"%s\">%s</a>.\n", table->url, table->url);

}
#endif /*CHUCK_CODE*/

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

char *chromBand(char *chrom, int pos)
/* Return text string that says what bend pos is on. */
{
struct sqlConnection *conn = hAllocConn();
char query[256];
char buf[64];
static char band[64];

sprintf(query, 
	"select name from cytoBand where chrom = '%s' and chromStart <= %d and chromEnd > %d", 
	chrom, pos, pos);
buf[0] = 0;
sqlQuickQuery(conn, query, buf, sizeof(buf));
sprintf(band, "%s%s", skipChr(chrom), buf);
hFreeConn(&conn);
return band;
}


void printPos(char *chrom, int start, int end, char *strand)
/* Print position lines.  'strand' argument may be null. */
{
printf("<B>Chromosome:</B> %s<BR>\n", skipChr(chrom));
printf("<B>Band:</B> %s<BR>\n", chromBand(chrom, (start + end)/2));
printf("<B>Begin in chromosome:</B> %d<BR>\n", start+1);
printf("<B>End in chromosome:</B> %d<BR>\n", end);
if (strand != NULL)
    printf("<B>Strand:</B> %s<BR>\n", strand);
else
    strand = "?";
printf("<A HREF=\"%s?o=%d&g=getDna&i=mixed&c=%s&l=%d&r=%d&strand=%s&db=%s\">"
      "View DNA for this feature</A><BR>\n",  hgcPath(),
      start, chrom, start, end, strand, database);
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
printf("<TT><PRE>");
cfmInit(&cfm, 10, 50, TRUE, FALSE, stdout, s);
for (i=0; i<seq->size; ++i)
   {
   cfmOut(&cfm, seq->dna[i], 0);
   }
cfmCleanup(&cfm);
printf("</PRE></TT>");
}

void doGetDna()
/* Get dna in window. */
{
struct dnaSeq *seq;
char name[256];
char *strand = cgiOptionalString("strand");

if (strand == NULL || strand[0] == '?')
    strand = "";
sprintf(name, "%s:%d-%d %s", seqName, winStart+1, winEnd, strand);
htmlStart(name);
seq = hDnaFromSeq(seqName, winStart, winEnd, dnaMixed);
if (strand[0] == '-')
    reverseComplement(seq->dna, seq->size);
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
     *development,*cell,*cds,*description, *author,*geneName,
     *date,*productName;
int seqSize,fileSize;
long fileOffset;
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
    printf("<B>date:</B> %s<BR>\n", date);
    }
else
    {
    warn("Couldn't find %s in mrna table", acc);
    }

sqlFreeResult(&sr);
freeDyString(&dy);
hgFreeConn(&conn);
}



#ifdef ROGIC_CODE

void printEstPairInfo(char *name)
/* print information about 5' - 3' EST pairs */
{
struct sqlConnection *conn = hgAllocConn();
struct sqlResult *sr;
char query[256]; 
char **row;
struct estPair *ep = NULL;

char *s = cgiOptionalString("pix");

sprintf(query, "select * from estPair where mrnaClone='%s'", name);
sr = sqlGetResult(conn, query);
 if((row = sqlNextRow(sr)) != NULL)
   {
     ep = estPairLoad(row);
     printf("<H2>Information on 5' - 3' EST pair from the clone %s </H2>\n\n", name);
     printf("<B>chromosome:</B> %s<BR>\n", ep->chrom); 
     printf("<B>Start position in chromosome :</B> %u<BR>\n", ep->chromStart); 
     printf("<B>End position in chromosome :</B> %u<BR>\n", ep->chromEnd);
     printf("<B>5' accession:</B> <A HREF=\"http://genome-test.cse.ucsc.edu/cgi-bin/hgc?o=%u&t=%u&g=hgEst&i=%s&c=%s&l=%d&r=%d&db=%s&pix=%s\"> %s</A><BR>\n", ep->start5, ep->end5, ep->acc5, ep->chrom, winStart, winEnd, database, s, ep->acc5); 
     printf("<B>Start position of 5' est in chromosome :</B> %u<BR>\n", ep->start5); 
     printf("<B>End position of 5' est in chromosome :</B> %u<BR>\n", ep->end5); 
     printf("<B>3' accession:</B> <A HREF=\"http://genome-test.cse.ucsc.edu/cgi-bin/hgc?o=%u&t=%u&g=hgEst&i=%s&c=%s&l=%d&r=%d&db=%s&pix=%s\"> %s</A><BR>\n", ep->start3, ep->end3, ep->acc3, ep->chrom, winStart, winEnd, database, s, ep->acc3);  
     printf("<B>Start position of 3' est in chromosome :</B> %u<BR>\n", ep->start3); 
     printf("<B>End position of 3' est in chromosome :</B> %u<BR>\n", ep->end3);
   }
 else
   {
     warn("Couldn't find %s in mrna table", name);
   }   
sqlFreeResult(&sr);
hgFreeConn(&conn);
}
#endif /* ROGIC_CODE */

void printAlignments(struct psl *pslList, 
	int startFirst, char *hgcCommand, char *typeName, char *itemIn)
/* Print list of mRNA alignments. */
{
struct psl *psl;
int aliCount = slCount(pslList);
boolean same;
char otherString[512];

if (aliCount > 1)
    printf("The alignment you clicked on is first in the table below.<BR>\n");

printf("<TT><PRE>");
printf(" SIZE IDENTITY CHROMOSOME STRAND  START     END       cDNA   START  END  TOTAL\n");
printf("------------------------------------------------------------------------------\n");
for (same = 1; same >= 0; same -= 1)
    {
    for (psl = pslList; psl != NULL; psl = psl->next)
	{
	if (same ^ (psl->tStart != startFirst))
	    {
	    sprintf(otherString, "%d&type=%s", psl->tStart, typeName);
	    hgcAnchorSomewhere(hgcCommand, itemIn, otherString, psl->tName);
	    printf("%5d  %5.1f%%  %9s     %s %9d %9d  %8s %5d %5d %5d</A>",
		psl->match + psl->misMatch + psl->repMatch,
		100.0 - pslCalcMilliBad(psl, TRUE) * 0.1,
		skipChr(psl->tName), psl->strand, psl->tStart + 1, psl->tEnd,
		psl->qName, psl->qStart+1, psl->qEnd, psl->qSize);
	    printf("\n");
	    }
	}
    }
printf("</TT></PRE>");
}

#ifdef ROGIC_CODE

void doHgEstPair(char *name)
/* Click on EST pair */
{
htmlStart(name);
printEstPairInfo(name);
}

#endif /* ROGIC_CODE */

void doHgRna(char *acc, boolean isEst)
/* Click on an individual RNA. */
{
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
char *type = (isEst ? "est" : "mrna");
int start = cgiInt("o");
struct psl *pslList = NULL, *psl;

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

htmlHorizontalLine();
printf("<H3>%s/Genomic Alignments</H3>", (isEst ? "EST" : "mRNA"));

printAlignments(pslList, start, "htcCdnaAli", type, acc);
}

void parseSs(char *ss, char **retPslName, char **retFaName, char **retQName)
/* Parse space separated 'ss' item. */
{
static char buf[512*2];
int wordCount;
char *words[4];
strcpy(buf, ss);
wordCount = chopLine(buf, words);
if (wordCount != 3)
    errAbort("Expecting 3 words in ss item");
*retPslName = words[0];
*retFaName = words[1];
*retQName = words[2];
}

void doUserPsl(char *item)
/* Process click on user-defined alignment. */
{
int start = cgiInt("o");
struct lineFile *lf;
struct psl *pslList = NULL, *psl;
char *pslName, *faName, *qName;
char *encItem = cgiEncode(item);
enum gfType qt, tt;
char *words[4];
int wordCount;

htmlStart("BLAT Search Alignments");
printf("<H2>BLAT Search Alignments</H2>\n");
printf("<H3>Click over a line to see detailed letter by letter display</H3>");
parseSs(item, &pslName, &faName, &qName);
pslxFileOpen(pslName, &qt, &tt, &lf);
while ((psl = pslNext(lf)) != NULL)
    {
    if (sameString(psl->qName, qName))
        {
	slAddHead(&pslList, psl);
	}
    else
        {
	pslFree(&psl);
	}
    }
slReverse(&pslList);
lineFileClose(&lf);
printAlignments(pslList, start, "htcUserAli", "user", encItem);
pslFreeList(&pslList);
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
int start = cgiInt("o");

htmlStart(fragName);
sprintf(query, "select * from %s_gold where frag = '%s' and chromStart = %d", 
	seqName, fragName, start+1);
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
wordCount = chopByChar(gapType, '_', words, ArraySize(words));
if (wordCount == 1)
    wordCount = chopByChar(gapType, ' ', words, ArraySize(words));
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

int showGfAlignment(struct psl *psl, bioSeq *oSeq, FILE *f, enum gfType qType)
/* Show protein/DNA alignment. */
{
struct dnaSeq *dnaSeq = NULL;
boolean tIsRc = (psl->strand[1] == '-');
boolean qIsRc = (psl->strand[0] == '-');
boolean isProt = (qType == gftProt);
int tStart = psl->tStart, tEnd = psl->tEnd;
int qStart = 0;
int mulFactor = (isProt ? 3 : 1);


/* Load dna sequence. */
dnaSeq = hDnaFromSeq(seqName, tStart, tEnd, dnaMixed);
freez(&dnaSeq->name);
dnaSeq->name = cloneString(psl->tName);
if (tIsRc)
    {
    int temp;
    reverseComplement(dnaSeq->dna, dnaSeq->size);
    temp = psl->tSize - tEnd;
    tEnd = psl->tSize - tStart;
    tStart = temp;
    }
if (qIsRc)
    {
    reverseComplement(oSeq->dna, oSeq->size);
    qStart = oSeq->size;
    }

fprintf(f, "<TT><PRE>");
fprintf(f, "<H4><A NAME=cDNA></A>%s%s</H4>\n", psl->qName, (qIsRc  ? " (reverse complemented)" : ""));
/* Display query sequence. */
    {
    struct cfm cfm;
    char *colorFlags = needMem(oSeq->size);
    int i,j;

    for (i=0; i<psl->blockCount; ++i)
	{
	int qs = psl->qStarts[i];
	int ts = psl->tStarts[i] - tStart;
	int sz = psl->blockSizes[i]-1;
	colorFlags[qs] = socBrightBlue;
	colorFlags[qs+sz] = socBrightBlue;
	if (isProt)
	    {
	    for (j=1; j<sz; ++j)
		{
		AA aa = oSeq->dna[qs+j];
		DNA *codon = &dnaSeq->dna[ts + 3*j];
		AA trans = lookupCodon(codon);
		if (trans != 'X' && trans == aa)
		    colorFlags[qs+j] = socBlue;
		}
	    }
	else
	    {
	    for (j=1; j<sz; ++j)
	        {
		if (oSeq->dna[qs+j] == dnaSeq->dna[ts+j])
		    colorFlags[qs+j] = socBlue;
		}
	    }
	}
    cfmInit(&cfm, 10, 60, TRUE, qIsRc, f, qStart);
    for (i=0; i<oSeq->size; ++i)
	cfmOut(&cfm, oSeq->dna[i], seqOutColorLookup[colorFlags[i]]);
    cfmCleanup(&cfm);
    freez(&colorFlags);
    htmHorizontalLine(f);
    }

fprintf(f, "<H4><A NAME=genomic></A>Genomic %s %s:</H4>\n", 
    psl->tName, (tIsRc ? "(reverse strand)" : ""));
/* Display DNA sequence. */
    {
    struct cfm cfm;
    char *colorFlags = needMem(dnaSeq->size);
    int i,j;
    int curBlock = 0;
    int anchorCount = 0;

    for (i=0; i<psl->blockCount; ++i)
	{
	int qs = psl->qStarts[i];
	int ts = psl->tStarts[i] - tStart;
	int sz = psl->blockSizes[i];
	if (isProt)
	    {
	    for (j=0; j<sz; ++j)
		{
		AA aa = oSeq->dna[qs+j];
		int codonStart = ts + 3*j;
		DNA *codon = &dnaSeq->dna[codonStart];
		AA trans = lookupCodon(codon);
		if (trans != 'X' && trans == aa)
		    {
		    colorFlags[codonStart] = socBlue;
		    colorFlags[codonStart+1] = socBlue;
		    colorFlags[codonStart+2] = socBlue;
		    }
		}
	    }
	else
	    {
	    for (j=1; j<sz; ++j)
	        {
		if (oSeq->dna[qs+j] == dnaSeq->dna[ts+j])
		    colorFlags[ts+j] = socBlue;
		}
	    }
	colorFlags[ts] = socBrightBlue;
	colorFlags[ts+sz*mulFactor-1] = socBrightBlue;
	}
    cfmInit(&cfm, 10, 60, TRUE, tIsRc, f, psl->tStart);
    for (i=0; i<dnaSeq->size; ++i)
	{
	/* Put down "anchor" on first match position in haystack
	 * so user can hop here with a click on the needle. */
	if (curBlock < psl->blockCount && psl->tStarts[curBlock] == (i + tStart) )
	     {
	     fprintf(f, "<A NAME=%d></A>", ++curBlock);
	     }
	cfmOut(&cfm, dnaSeq->dna[i], seqOutColorLookup[colorFlags[i]]);
	}
    cfmCleanup(&cfm);
    freez(&colorFlags);
    htmHorizontalLine(f);
    }

/* Display side by side. */
fprintf(f, "<H4><A NAME=ali></A>Side by Side Alignment</H4>\n");
    {
    struct baf baf;
    int i,j;

    bafInit(&baf, oSeq->dna, qStart, qIsRc,
    	dnaSeq->dna, psl->tStart, tIsRc, f, 60, isProt);
    for (i=0; i<psl->blockCount; ++i)
	{
	int qs = psl->qStarts[i];
	int ts = psl->tStarts[i] - tStart;
	int sz = psl->blockSizes[i];
	bafSetPos(&baf, qs, ts);
	bafStartLine(&baf);
	if (isProt)
	    {
	    for (j=0; j<sz; ++j)
		{
		AA aa = oSeq->dna[qs+j];
		int codonStart = ts + 3*j;
		DNA *codon = &dnaSeq->dna[codonStart];
		bafOut(&baf, ' ', codon[0]);
		bafOut(&baf, aa, codon[1]);
		bafOut(&baf, ' ', codon[2]);
		}
	    }
	else
	    {
	    for (j=0; j<sz; ++j)
		{
		bafOut(&baf, oSeq->dna[qs+j], dnaSeq->dna[ts+j]);
		}
	    }
	bafFlushLine(&baf);
	}
    }
fprintf(f, "</TT></PRE>");
if (qIsRc)
    reverseComplement(oSeq->dna, oSeq->size);
return psl->blockCount;
}


int showDnaAlignment(struct psl *psl, struct dnaSeq *rnaSeq, FILE *body)
/* Show alignment for accession. */
{
struct dnaSeq *dnaSeq;
DNA *rna;
int dnaSize,rnaSize;
boolean isRc = FALSE;
struct ffAli *ffAli, *ff;
int tStart, tEnd, tRcAdjustedStart;
int lastEnd = 0;
int blockCount;
char title[256];


/* Get RNA and DNA sequence.  Save a mixed case copy of DNA, make
 * all lower case for fuzzyFinder. */
rna = rnaSeq->dna;
rnaSize = rnaSeq->size;
tStart = psl->tStart - 100;
if (tStart < 0) tStart = 0;
tEnd  = psl->tEnd + 100;
if (tEnd > psl->tSize) tEnd = psl->tSize;
dnaSeq = hDnaFromSeq(seqName, tStart, tEnd, dnaMixed);
freez(&dnaSeq->name);
dnaSeq->name = cloneString(psl->tName);

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
fprintf(body, "<H2>Alignment of %s and %s:%d-%d</H2>\n", psl->qName, psl->tName, psl->tStart, psl->tEnd);
fprintf(body, "Click on links in the frame to left to navigate through alignment.\n");
blockCount = ffShAliPart(body, ffAli, psl->qName, rna, rnaSize, 0, 
	dnaSeq->name, dnaSeq->dna, dnaSeq->size, tStart, 
	8, FALSE, isRc, FALSE, TRUE, TRUE, TRUE);
return blockCount;
}

void showSomeAlignment(struct psl *psl, bioSeq *oSeq, enum gfType qType)
/* Display protein or DNA alignment in a frame. */
{
int blockCount;
struct tempName indexTn, bodyTn;
FILE *index, *body;
char title[256];
int i;

makeTempName(&indexTn, "index", ".html");
makeTempName(&bodyTn, "body", ".html");

/* Writing body of alignment. */
body = mustOpen(bodyTn.forCgi, "w");
htmStart(body, psl->qName);
if (qType == gftRna || qType == gftDna)
    blockCount = showDnaAlignment(psl, oSeq, body);
else 
    blockCount = showGfAlignment(psl, oSeq, body, qType);
fclose(body);
chmod(bodyTn.forCgi, 0666);

/* Write index. */
index = mustOpen(indexTn.forCgi, "w");
htmStart(index, psl->qName);
fprintf(index, "<H3>Alignment of %s</H3>", psl->qName);
fprintf(index, "<A HREF=\"%s#cDNA\" TARGET=\"body\">%s</A><BR>\n", bodyTn.forCgi, psl->qName);
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
}


void htcCdnaAli(char *acc)
/* Show alignment for accession. */
{
char query[256];
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
struct psl *psl;
struct dnaSeq *rnaSeq;
char *type;
int start;

/* Print start of HTML. */
puts("Content-Type:text/html\n");
printf("<HEAD>\n<TITLE>%s vs Genomic</TITLE>\n</HEAD>\n\n", acc);
puts("<HTML>");

/* Get some environment vars. */
type = cgiString("type");
start = cgiInt("o");

/* Look up alignments in database */
conn = hAllocConn();
sprintf(query, "select * from %s_%s where qName = '%s' and tStart=%d",
    seqName, type, acc, start);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't find alignment for %s at %d", acc, start);
psl = pslLoad(row);
sqlFreeResult(&sr);
hFreeConn(&conn);

rnaSeq = hRnaSeq(acc);
showSomeAlignment(psl, rnaSeq, gftDna);
}

void htcUserAli(char *fileNames)
/* Show alignment for accession. */
{
char *pslName, *faName, *qName;
struct lineFile *lf;
bioSeq *oSeqList = NULL, *oSeq = NULL;
struct psl *psl;
int start;
enum gfType tt, qt;
boolean isProt;

/* Print start of HTML. */
puts("Content-Type:text/html\n");
printf("<HEAD>\n<TITLE>User Sequence vs Genomic</TITLE>\n</HEAD>\n\n");
puts("<HTML>");

start = cgiInt("o");
parseSs(fileNames, &pslName, &faName, &qName);
pslxFileOpen(pslName, &qt, &tt, &lf);
isProt = (qt == gftProt);
while ((psl = pslNext(lf)) != NULL)
    {
    if (sameString(psl->tName, seqName) && psl->tStart == start && sameString(psl->qName, qName))
        break;
    pslFree(&psl);
    }
lineFileClose(&lf);
if (psl == NULL)
    errAbort("Couldn't find alignment at %s:%d", seqName, start);
oSeqList = faReadAllSeq(faName, !isProt);
for (oSeq = oSeqList; oSeq != NULL; oSeq = oSeq->next)
    {
    if (sameString(oSeq->name, qName))
         break;
    }
if (oSeq == NULL)  errAbort("%s is in %s but not in %s. Internal error.", qName, pslName, faName);
showSomeAlignment(psl, oSeq, qt);
}

char *blatMouseTable()
/* Return name of blatMouse table. */
{
static char buf[64];
sprintf(buf, "%s_blatMouse", seqName);
if (hTableExists(buf))
    return buf;
else
    return "blatMouse";
}

void htcBlatMouse(char *readName, char *table)
/* Show alignment for accession. */
{
char *pslName, *faName, *qName;
struct lineFile *lf;
bioSeq *oSeqList = NULL, *oSeq = NULL;
struct psl *psl;
int start;
enum gfType tt, qt;
boolean isProt;
struct sqlResult *sr;
struct sqlConnection *conn = hAllocConn();
struct dnaSeq *seq;
char query[256], **row;

/* Print start of HTML. */
puts("Content-Type:text/html\n");
printf("<HEAD>\n<TITLE>Mouse Read %s</TITLE>\n</HEAD>\n\n", readName);
puts("<HTML>");

start = cgiInt("o");
sprintf(query, "select * from %s where qName = '%s' and tName = '%s' and tStart=%d",
    table, readName, seqName, start);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't find alignment for %s at %d", readName, start);
psl = pslLoad(row);
sqlFreeResult(&sr);
hFreeConn(&conn);
seq = hExtSeq(readName);
showSomeAlignment(psl, seq, gftDnaX);
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
	printPos(seqName, ro->genoStart, ro->genoEnd, ro->strand);
	htmlHorizontalLine();
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
    htmlHorizontalLine();
    }
puts(
  "This track was created by Arian Smit's RepeatMasker program "
  "which uses the RepBase library of repeats from the Genetic "
  "Information Research Institute.  RepBase is described in "
  "J. Jurka,  RepBase Update, <I>Trends in Genetics</I> 16:418-420, 2000");
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
puts("<P>The Simple Tandem Repeats were located with the program "
     "<A HREF=\"http://c3.biomath.mssm.edu/trf.html\">Tandem Repeat Finder</A> "
     "by G. Benson</P>");
}

void bedPrintPos(struct bed *bed)
/* Print first three fields of a bed type structure in
 * standard format. */
{
printPos(bed->chrom, bed->chromStart, bed->chromEnd, NULL);
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
    printPos(gp->chrom, gp->txStart, gp->txEnd, gp->strand);
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

void geneShowPosAndLinks(char *geneName, char *pepName, char *geneTable, char *pepTable,
	char *pepClick, char *mrnaClick, char *genomicClick, char *mrnaDescription)
/* Show parts of gene common to everything */
{
char other[256];
showGenePos(geneName, geneTable);
printf("<H3>Links to sequence:</H3>\n");
printf("<UL>\n");
if (pepTable != NULL && hTableExists(pepTable))
    {
    hgcAnchorSomewhere(pepClick, pepName, pepTable, seqName);
    printf("<LI>Translated Protein</A>\n"); 
    }
hgcAnchorSomewhere(mrnaClick, geneName, geneTable, seqName);
printf("<LI>%s</A>\n", mrnaDescription);
hgcAnchorSomewhere(genomicClick, geneName, geneTable, seqName);
printf("<LI>Genomic Sequence</A>\n");
printf("</UL>\n");
}


void geneShowCommon(char *geneName, char *geneTable, char *pepTable)
/* Show parts of gene common to everything */
{
geneShowPosAndLinks(geneName, geneName, geneTable, pepTable, "htcTranslatedProtein",
	"htcGeneMrna", "htcGeneInGenome", "Predicted mRNA");
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

void htcRefMrna(char *geneName)
/* Display mRNA associated with a refSeq gene. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];

htmlStart("RefSeq mRNA");
sprintf(query, "select seq from refMrna where name = '%s'", geneName);
sr = sqlGetResult(conn, query);
printf("<PRE><TT>");
while ((row = sqlNextRow(sr)) != NULL)
    {
    faWriteNext(stdout, NULL, row[0], strlen(row[0]));
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
   "<P>Additional information may be available by clicking on the "
   "mRNA associated with this gene in the main browser window.</P>");
}

void doRefGene(char *rnaName)
/* Process click on a known RefSeq gene. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];
struct refLink *rl;
struct genePred *gp;

htmlStart("Known Gene");
sprintf(query, "select * from refLink where mrnaAcc = '%s'", rnaName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't find %s in refLink table - database inconsistency.", rnaName);
rl = refLinkLoad(row);
sqlFreeResult(&sr);
printf("<H2>Known Gene %s</H2>\n", rl->name);
    
printf("<B>RefSeq:</B> <A HREF=");
printEntrezNucleotideUrl(stdout, rl->mrnaAcc);
printf(" TARGET=_blank>%s</A><BR>", rl->mrnaAcc);
if (rl->omimId != 0)
    {
    printf("<B>OMIM:</B> ");
    printf(
       "<A HREF = \"http://www.ncbi.nlm.nih.gov/entrez/dispomim.cgi?id=%d\" TARGET=_blank>%d</A><BR>\n", 
	rl->omimId, rl->omimId);
    }
if (rl->locusLinkId != 0)
    {
    printf("<B>LocusLink:</B> ");
    printf("<A HREF = \"http://www.ncbi.nlm.nih.gov/LocusLink/LocRpt.cgi?l=%d\" TARGET=_blank>",
	    rl->locusLinkId);
    printf("%d</A><BR>\n", rl->locusLinkId);
    }

medlineLinkedLine("PubMed on Gene", rl->name, rl->name);
if (rl->product[0] != 0)
    medlineLinkedLine("PubMed on Product", rl->product, rl->product);
printf("<B>GeneCards:</B> ");
printf("<A HREF = \"http://bioinfo.weizmann.ac.il/cards-bin/cardsearch.pl?search=%s\" TARGET=_blank>",
	rl->name);
printf("%s</A><BR>\n", rl->name);
htmlHorizontalLine();

geneShowPosAndLinks(rl->mrnaAcc, rl->protAcc, "refGene", "refPep", "htcTranslatedProtein",
	"htcRefMrna", "htcGeneInGenome", "mRNA Sequence");

puts(
   "<P>Known genes are derived from the "
   "<A HREF = \"http://www.ncbi.nlm.nih.gov/LocusLink/refseq.html\" TARGET=_blank>"
   "RefSeq</A> mRNA database. "
   "These mRNAs were mapped to the draft "
   "human genome using <A HREF = \"http://www.cse.ucsc.edu/~kent\" TARGET=_blank>"
   "Jim Kent's</A> BLAT software.</P>\n");
puts(
   "<P>Additional information may be available by clicking on the "
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

void doSanger22(char *geneName)
/* Handle click on Sanger 22 track. */
{
struct sqlConnection *conn = hAllocConn();
htmlStart("Sanger Chromosome 22 Annotation");
printf("<H2>Sanger Chromosome 22 Annotation %s</H2>\n", geneName);
geneShowCommon(geneName, "sanger22", NULL);
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
/* Handle click on exoFish track. */
{
struct exoFish el;
int start = cgiInt("o");
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];
struct browserTable *table = NULL;
struct browserTable *tableList = checkDbForTables();
table = getBrowserTableFromList("hgExoFish", tableList);

htmlStart("Exofish Ecores");
printf("<H2>Exofish ECORES</A></H2>\n");
if(table != NULL)
    printf("<b>version:</b> %s<br>%s<br><br>\n", table->version,table->other);

sprintf(query, "select * from exoFish where chrom = '%s' and chromStart = %d",
    seqName, start);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    exoFishStaticLoad(row, &el);
    if (!sameString(el.name, "."))
         {
	 printf("<B>Exofish ID:</B> "
	        "<A HREF=\"http://www.genoscope.cns.fr/proxy/cgi-bin/exofish.cgi?Id=%s&Mode=Id\">"
		"%s</A><BR>\n", 
		 el.name, el.name);
	 }
    printf("<B>score:</B> %d<BR>\n", el.score);
    bedPrintPos((struct bed *)&el);
    htmlHorizontalLine();
    if (!sameString(el.name, "."))
        {
	}
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
browserTableFreeList(&tableList);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doExoMouse(char *itemName)
/* Handle click on exoMouse track. */
{
struct roughAli el;
int start = cgiInt("o");
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];

htmlStart("Exonerate Mouse");
printf("<H2>Exonerate Mouse</A></H2>\n");

sprintf(query, "select * from exoMouse where chrom = '%s' and chromStart = %d and name = '%s'",
    seqName, start, itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    roughAliStaticLoad(row, &el);
    printf("<B>score:</B> %d<BR>\n", el.score);
    bedPrintPos((struct bed *)&el);
    htmlHorizontalLine();
    }

puts("<P>The Exonerate mouse shows regions of homology with the "
     "mouse based on Exonerate alignments of mouse random reads "
     "with the human genome.  The data for this track was kindly provided by "
     "Guy Slater, Michele Clamp, and Ewan Birney at "
     "<A HREF=\"http://www.ensembl.org\" TARGET=_blank>Ensembl</A>.");
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void htcExtSeq(char *item)
/* Print out DNA from some external but indexed .fa file. */
{
struct dnaSeq *seq;
htmlStart(item);
seq = hExtSeq(item);
printf("<PRE><TT>");
faWriteNext(stdout, item, seq->dna, seq->size);
printf("</PRE></TT>");
freeDnaSeq(&seq);
}

void doBlatMouse(char *itemName, char *tableName)
/* Handle click on blatMouse track. */
{
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
int start = cgiInt("o");
struct psl *pslList = NULL, *psl;
struct dnaSeq *seq;
char *tiNum = strrchr(itemName, '|');

/* Print heading info including link to NCBI. */
if (tiNum == NULL) 
    tiNum = itemName;
else
    ++tiNum;
htmlStart(itemName);
printf("<H1>Information on Mouse Read %s</H1>", itemName);

/* Print links to NCBI and to sequence. */
printf("Link to ");
printf("<A HREF=\"http://www.ncbi.nlm.nih.gov/Traces/trace.cgi?val=%s\" TARGET=_blank>", tiNum);
printf("NCBI Trace Repository for %s\n</A><BR>\n", itemName);
printf("Get ");
printf("<A HREF=\"%s?g=htcExtSeq&c=%s&l=%d&r=%d&i=%s&db=%s\">",
      hgcPath(), seqName, winStart, winEnd, itemName, database);
printf("DNA for this read</A><BR>\n");

/* Print info about mate pair. */
if (sqlTableExists(conn, "mouseTraceInfo"))
    {
    char buf[256];
    char *templateId;
    boolean gotMate = FALSE;
    sprintf(query, "select templateId from mouseTraceInfo where ti = '%s'", itemName);
    templateId = sqlQuickQuery(conn, query, buf, sizeof(buf));
    if (templateId != NULL)
        {
	sprintf(query, "select ti from mouseTraceInfo where templateId = '%s'", templateId);
	sr = sqlGetResult(conn, query);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    char *ti = row[0];
	    if (!sameString(ti, itemName))
	        {
		printf("Get ");
		printf("<A HREF=\"%s?g=htcExtSeq&c=%s&l=%d&r=%d&i=%s&db=%s\">",
		      hgcPath(), seqName, winStart, winEnd, ti, database);
		printf("DNA for read on other end of plasmid</A><BR>\n");
		gotMate = TRUE;
		}
	    }
	sqlFreeResult(&sr);
	}
    if (!gotMate)
	printf("No read from other end of plasmid in database.<BR>\n");
    }

/* Get alignment info and print. */
printf("<H2>Alignments</H2>\n");
sprintf(query, "select * from %s where qName = '%s'", tableName, itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row);
    slAddHead(&pslList, psl);
    }
sqlFreeResult(&sr);
slReverse(&pslList);
printAlignments(pslList, start, "htcBlatMouse", tableName, itemName);

printf("<H2>Methods</H2>\n");
puts(
 "<P>This track displays alignments of 16 million mouse whole genome shotgun "
 "reads (3x coverage) vs. the draft human genome.  The alignments were done "
 "with BLAT in translated protein mode requiring two perfect 4-mer hits "
 "within 100 amino acids of each other to trigger an alignment.  The human "
 "genome was masked with RepeatMasker and Tandem Repeat Finder before "
 "running BLAT.  Places where more than 150 reads aligned were filtered out "
 "as were alignments of greater than 95% base identity (to avoid human "
 "contaminants in the mouse data). </P>");
printf("<H2>Data Release Policy</H2>\n");
puts(
 "These data are made available before scientific publication with the "
 "following understanding:<BR>"
 "1.The data may be freely downloaded, used in analyses, and "
 "  repackaged in databases.<BR>"
 "2.Users are free to use the data in scientific papers analyzing "
 "  particular genes and regions, provided that providers of this data "
 "  (the Mouse Sequencing Consortium) are properly acknowledged.<BR>"
 "3.The Centers producing the data reserve the right to publish the initial "
 "  large-scale analyses of the dataset-including large-scale identification "
 "  of regions of evolutionary conservation and large-scale "
 "  genomic assembly. Large-scale refers to regions with size on the order of "
 "  a chromosome (that is, 30 Mb or more). <BR>"
 "4.Any redistribution of the data should carry this notice.<BR>" );
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
struct browserTable *table = NULL;
struct browserTable *tableList = checkDbForTables();
table = getBrowserTableFromList("hgRnaGene", tableList);

htmlStart("RNA Genes");

printf("<H2>RNA Gene %s</H2>\n", itemName);
if(table != NULL)
    printf("<b>version:</b> %s<br>%s<br><br>\n", table->version,table->other);
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
    printf("<B>program predicted with:</B> %s<BR>\n", rna.source);
    bedPrintPos((struct bed *)&rna);
    printf("<B>strand:</B> %s<BR>\n", rna.strand);
    htmlHorizontalLine();
    }

puts("<P>This track shows where non-protein coding RNA genes and "
     "pseudo-genes are located.  This data was kindly provided by "
     "Sean Eddy at Washington University.</P>");
sqlFreeResult(&sr);
browserTableFreeList(&tableList);
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
printf("<H2>Mouse Synteny</H2>\n");

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
printf("<H2>Chromosome Bands</H2>\n");
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
#ifdef ROGIC_CODE

void doMgcMrna(char *acc)
/* Redirects to genbank record */
{

  printf("Content-Type: text/html\n\n<HTML><BODY><SCRIPT>\n");
  printf("location.replace('http://www.ncbi.nlm.nih.gov/htbin-post/Entrez/query?form=4&db=n&term=%s');\n",acc); 
  printf("</SCRIPT> <NOSCRIPT> No JavaScript support. Click <b><a href=\"http://www.ncbi.nlm.nih.gov/htbin-post/Entrez/query?form=4&db=n&term=%s\">continue</a></b> for the requested GenBank report. </NOSCRIPT>\n",acc); 
}

#endif /* ROGIC_CODE */
#ifdef CHUCK_CODE

void chuckHtmlStart(char *title) 
/* Prints the header appropriate for the title
 * passed in. Links html to chucks stylesheet for 
 * easier maintaince 
 */
{
printf("Content-Type: text/html\n\n<HTML><HEAD>\n");
printf("<LINK REL=STYLESHEET TYPE=\"text/css\" href=\"http://genome-test.cse.ucsc.edu/style/blueStyle.css\" title=\"Chuck Style\">\n");
printf("<title>%s</title>\n</head><body bgcolor=\"#f3f3ff\">",title);
}

void chuckHtmlContactInfo()
/* Writes out Chuck's email so people bother Chuck instead of Jim */
{
puts("<br><br><font size=-2><i>If you have comments and/or suggestions please email "
     "<a href=\"mailto:sugnet@cse.ucsc.edu\">sugnet@cse.ucsc.edu</a>.\n");
}

void makeCheckBox(char *name, boolean isChecked)
/* Create a checkbox with the given name in the given state. */
{
printf("<INPUT TYPE=CHECKBOX NAME=\"%s\" VALUE=on%s>", name,
    (isChecked ? " CHECKED" : "") );

}


struct rgbColor getColorForExprBed(float val, float max, boolean RG_COLOR_SCHEME)
/* Return the correct color for a given score */
{
float absVal = fabs(val);
struct rgbColor color; 
int colorIndex = 0;
if(absVal > 100) 
    {
    color.g = color.r = color.b = 128;
    return(color);
    }
if(absVal > max) 
    absVal = max;

if (max == 0) 
    errAbort("ERROR: hgc::getColorForExprBed() maxDeviation can't be zero\n"); 
colorIndex = (int)(absVal * 255/max);
if(RG_COLOR_SCHEME) 
    {
    if(val > 0) 
	{
	color.r = colorIndex; 
	color.g = 0;
	color.b = 0;
	}
    else 
	{
	color.r = 0;
	color.g = colorIndex;
	color.b = 0;
	}
    }
else
    {
    if(val > 0) 
	{
	color.r = colorIndex; 
	color.g = 0;
	color.b = 0;
	}
    else 
	{
	color.r = 0;
	color.g = 0;
	color.b = colorIndex;
	}
    }
return color;
}

void abbr(char *s, char *fluff)
/* Cut out fluff from s. */
{
int len;
s = strstr(s, fluff);
if (s != NULL)
   {
   len = strlen(fluff);
   strcpy(s, s+len);
   }
}

char *abbrevExprBedName(char *name)
{
static char abbrev[32];
char *ret;
strncpy(abbrev, name, sizeof(abbrev));
abbr(abbrev, "LINK_Em:");
ret = strstr(abbrev, "_");
ret++;
return ret;
}

void printTableHeaderName(char *name, char *clickName) 
/* creates a table to display a name vertically,
 * basically creates a column of letters 
 */
{
int i, length;
char *header = cloneString(name);
header = cloneString(abbrevExprBedName(header));
subChar(header,'_',' ');
length = strlen(header);

printf("<table border=0 cellspacing=0 cellpadding=0>\n");
for(i = 0; i < length; i++)
    {
    if(header[i] == ' ') 
	printf("<tr><td align=center>&nbsp</td></tr>\n");
    else
	{
	printf("<tr><td align=center>");
	if(sameString(name,clickName)) 
	    {
	    printf("<font color=blue>");
	    }
	printf("%c", header[i]);
	if(sameString(name, clickName)) 
	    {
	    printf("</font>");
	    }
	printf("</tr></td>");
	}
    }
printf("</table>\n");
freez(&header);
}

void printRosettaReference() 
{
puts(
     "<table border=0 width=600><tr><td>\n"
     "<p>Expression data from <a href=\"http://www.rii.com\">Rosetta Inpharmatics</a>. "
     "See the paper \"<a href=\"http://www.rii.com/tech/pubs/nature_shoemaker.htm\"> "
     "Experimental Annotation of the Human Genome Using Microarray Technology</a>\" "
     "[<a href=\"http://www.ncbi.nlm.nih.gov:80/entrez/query.fcgi?cmd=Retrieve&db=PubMed&list_uids=11237012&dopt=Abstract\">medline</a>] "
     "<i>Nature</i> vol 409 pp 922-7 for more information. Rosetta created DNA probes for "
     "each exon as described by the Sanger center for the October 2000 draft of the genome. "
     "Exons are labeled according whether they are predicted (pe) or true (te) exons, the "
     "relative position in the genome, and the contig name. The probes presented here correspond to those contained "
     "in window range seen on the Genome Browser, the exon probe selected is highlighted "
     "in blue.</p><br></br>"
     "<p>This display below is an average of many data points to see the detailed data "
     "select the checkboxes for the experiments of interest and hit the submit value below. "
     "It is possible to view both the browser and the expression summary and detailed plots "
     "at the same time by clicking the <b>Show Browser in different Frame</b> check box. This "
     "will create two frames which are aware of eachother, one displaying the browser and the other "
     "the detailed expression data.<br><br>\n"
     "</td></tr></table>\n"
     );
}

void exprBedPrintTable(struct exprBed *expList, char *itemName)
/* prints out a table from the data present in the exprBed */
{
int i,featureCount=0, currentRow=0,square=10;
struct exprBed *exp = NULL;
char buff[32];
if(expList == NULL) 
    errAbort("No exprBeds were selected.\n");

featureCount = slCount(expList);

/* time to write out some html, first the table and header */
printf("<basefont size=-1>\n");
printf("<table cellspacing=0 border=0 cellpadding=0 >\n");
printf("<tr>\n");
printf("<th align=center>Hybridization</th>\n");
printf("<th align=center colspan=%d valign=top>Exons</th>\n",featureCount);
printf("</tr>\n<tr><td>&nbsp</td>\n");
for(exp = expList; exp != NULL; exp = exp->next)
    {
    printf("<td valign=top align=center>\n");
    printTableHeaderName(exp->name, itemName);
    printf("</td>");
    }
printf("</tr>\n");
/* for each experiment write out the name and then all of the values */
for(i = 0; i < expList->numExp; i++) 
    {
    zeroBytes(buff,32);
    sprintf(buff,"e%d",i);
    printf("<tr>\n");
    printf("<td align=left>");
    makeCheckBox(buff,FALSE);
    printf(" %s</td>\n",expList->hybes[i]);
    for(exp = expList;exp != NULL; exp = exp->next)
	{
	/* use the background colors to creat patterns */
	struct rgbColor rgb = getColorForExprBed(exp->scores[i], 1.0, TRUE);
	printf("<td height=%d width=%d bgcolor=\"#%.2X%.2X%.2X\">&nbsp</td>\n", square, square, rgb.r, rgb.g, rgb.b);
	}
    printf("</tr>\n");
    }
printf("</table>");
printf("</basefont>\n");
}

struct exprBed *selectExprBedFromDB(char *table, char *chrom, int start, int end)
/* select all of the exprBed (expression data) from by chromosome name, position in 
   chomosome and table name */
{
char query[256];
struct exprBed *expList = NULL, *exp = NULL;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
sprintf(query, 
	"select * from %s where chrom = '%s' and chromStart<%u and chromEnd>%u order by chromStart",
	table, chrom, end, start);
sr = sqlGetResult(conn,query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    exp = exprBedLoad(row);
    slAddHead(&expList, exp);
    }

slReverse(&expList);
sqlFreeResult(&sr);
hFreeConn(&conn);
return expList;
}

void doGetExprBed(char *tableName, char *itemName)
/* Print out a colored table of the expression band track. */
{
struct exprBed *expList=NULL;
char buff[64];
char *type;
char *maxIntensity[] = { "100", "20", "15", "10", "5" ,"4","3","2","1" };
char *thisFrame = cgiOptionalString("tf");
char *otherFrame = cgiOptionalString("of");
type = abbrevExprBedName(itemName);
type = strstr(type, "_");
type++;

chuckHtmlStart("Rosetta Expression Data Requested");
printf("<h2>Rosetta Expression Data For: %s %d-%d</h2>\n", seqName, winStart, winEnd);
printRosettaReference();
expList = selectExprBedFromDB(tableName, seqName, winStart, winEnd);
puts("<table width=\"100%\" cellpadding=0 cellspacing=0><tr><td>\n");
printf("<form action=\"../cgi-bin/rosChr22VisCGI\" method=get>\n");
exprBedPrintTable(expList, itemName);
printf("</td></tr><tr><td><br>\n");

puts("<table width=\"100%\" cellpadding=0 cellspacing=0>\n"
     "<tr><th align=left><h3>Plot Options:</h3></th></tr><tr><td><p><br>");
makeCheckBox("f", FALSE);
printf(" Show browser in different Frame<br>\n");

zeroBytes(buff,64);
sprintf(buff,"%d",winStart);
cgiMakeHiddenVar("winStart", buff);
zeroBytes(buff,64);
sprintf(buff,"%d",winEnd);
cgiMakeHiddenVar("t",type);
cgiMakeHiddenVar("winEnd", buff);
cgiMakeHiddenVar("db",database); 
if(thisFrame != NULL)
    cgiMakeHiddenVar("of",thisFrame);
if(otherFrame != NULL)
    cgiMakeHiddenVar("tf",otherFrame);
printf("<br>\n");
cgiMakeDropList("mi",maxIntensity, 9, "20");
printf(" Maximum Itensity value to allow.\n");
printf("</td></tr><tr><td align=center><br>\n");
printf("<b>Press Here to View Detailed Plots</b><br><input type=submit name=Submit value=submit>\n");
printf("<br><br><br><b>Clear Values</b><br><input type=reset name=Reset></form>\n");
printf("</td></tr></table></td></tr></table>\n");
chuckHtmlContactInfo();
}



#endif /*CHUCK_CODE*/

void doMiddle()
/* Generate body of HTML. */
{
char *group = cgiString("g");
char *item = cgiOptionalString("i");
char title[256];
struct browserTable *tableList = NULL;
database = cgiOptionalString("db");
if (database == NULL)
    database = "hg6";

hDefaultConnect(); 	/* set up default connection settings */
hSetDb(database);
seqName = cgiString("c");
winStart = cgiInt("l");
winEnd = cgiInt("r");
tableList = checkDbForTables();
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
#ifdef ROGIC_CODE
else if (sameWord(group, "hgEstPairs"))
    {
    doHgEstPair(item);
    }
#endif /*ROGIC_CODE*/
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
else if (sameWord(group, "hgRefGene"))
    {
    doRefGene(item);
    }
else if (sameWord(group, "genieKnown"))
    {
    doKnownGene(item);
    }
else if (sameWord(group, "hgSanger22"))
    {
    doSanger22(item);
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
else if (sameWord(group, "hgExoMouse"))
    {
    doExoMouse(item);
    }
else if (sameWord(group, "hgBlatMouse"))
    {
    doBlatMouse(item, blatMouseTable());
    }
else if (sameWord(group, "hgMus7of8"))
    {
    doBlatMouse(item, "mus7of8");
    }
else if (sameWord(group, "hgMusPairOf4"))
    {
    doBlatMouse(item, "musPairOf4");
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
else if (sameWord(group, "hgUserPsl"))
    {
    doUserPsl(item);
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
else if (sameWord(group, "htcUserAli"))
   {
   htcUserAli(item);
   }
else if (sameWord(group, "htcBlatMouse"))
   {
   htcBlatMouse(item, cgiString("type"));
   }
else if (sameWord(group, "htcExtSeq"))
   {
   htcExtSeq(item);
   }
else if (sameWord(group, "htcTranslatedProtein"))
   {
   htcTranslatedProtein(item);
   }
else if (sameWord(group, "htcGeneMrna"))
   {
   htcGeneMrna(item);
   }
else if (sameWord(group, "htcRefMrna"))
   {
   htcRefMrna(item);
   }
else if (sameWord(group, "htcGeneInGenome"))
   {
   htcGeneInGenome(item);
   }
else if (sameWord(group, "htcDnaNearGene"))
   {
   htcDnaNearGene(item);
   }
#ifdef CHUCK_CODE
else if (sameWord(group, "hgExprBed"))
    {
    doGetExprBed("exprBed", item);
    } 
else if (sameWord(group, "rosettaPe"))
    {
    doGetExprBed("rosettaPe", item);
    }
else if (sameWord(group, "rosettaTe"))
    {
    doGetExprBed("rosettaTe", item);
    }

#endif /*CHUCK_CODE*/
#ifdef ROGIC_CODE
 else if (sameWord(group, "mgc_mrna"))
   {
     doMgcMrna(item);
   }
#endif /*ROGIC_CODE*/
else
   {
   if(inBrowserTable(group,tableList))
       doBrowserTable(group, tableList);
   else 
       {
       htmlStart(group);
       printf("Sorry, clicking there doesn't do anything yet (%s).", group);
       }
   }
browserTableFreeList(&tableList);
htmlEnd();
}

int main(int argc, char *argv[])
{
dnaUtilOpen();
cgiSpoof(&argc,argv);
htmEmptyShell(doMiddle, NULL);
return 0;
}
