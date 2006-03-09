/* vgProbeTrack - build probeExt table in visiGene database. 
 *
 *   This finds sequence for all probes, using the best method available,
 *   which can be given in probe.seq, using the primers to
 *   fetch the probe using isPcr, or using accession or gene to fetch sequence.
 * 
 *   When all the sequences are availabe and ready, then blat is run to 
 *   create psl alignments for use in probe tracks which may also be 
 *   mapped over to human orthologous position using either chains with pslMap (mouse)
 *   or blatz (frog).
 *
 */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "ra.h"
#include "jksql.h"
#include "dystring.h"
#include "portable.h"
#include "fa.h"
#include "probeExt.h"
#include "hdb.h"

/* Variables you can override from command line. */
char *database = "visiGene";
char *sqlPath = ".";
boolean kill = FALSE;

boolean doLock = FALSE;  // TODO delete this

void usage()
/* Explain usage and exit. */
{
errAbort(
  "vgProbeTrack - build probeExt table in visiGene database\n"
  "usage:\n"
  "   vgProbeTrack workingDir\n"
  "\n"
  "workingDir is a directory with space for intermediate and final results.\n"
  "Options:\n"
  "   -database=%s - Specifically set database\n"
  "   -sqlPath=%s - specify location of probeExt.sql, relative to workingDir \n"
  "   -kill - delete probeExt table to start fresh\n"
  , database, sqlPath
  );
}

static struct optionSpec options[] = {
   {"database", OPTION_STRING,},
   {"sqlPath", OPTION_STRING,},
   {"kill", OPTION_BOOLEAN,},
   {NULL, 0},
};


/* --- defining just the fields needed from bac  ---- */

struct bac
/* Information from bac */
    {
    struct bac *next;           /* Next in singly linked list. */
    char *chrom;                /* chrom */
    int chromStart;             /* start */
    int chromEnd;               /* end   */
    char *strand;               /* strand */
    int probe;                  /* probe  */
    };

struct bac *bacLoad(char **row)
/* Load a bac from row fetched with select * from bac
 * from database.  Dispose of this with bacFree(). */
{
struct bac *ret;

AllocVar(ret);
ret->chrom      = cloneString(row[0]);
ret->chromStart =        atoi(row[1]);
ret->chromEnd   =        atoi(row[2]);
ret->strand     = cloneString(row[3]);
ret->probe      =        atoi(row[4]);
return ret;
}

void bacFree(struct bac **pEl)
/* Free a single dynamically allocated bac such as created
 * with bacLoad(). */
{
struct bac *el;

if ((el = *pEl) == NULL) return;
freeMem(el->chrom);
freeMem(el->strand);
freez(pEl);
}

void bacFreeList(struct bac **pList)
/* Free a list of dynamically allocated bac's */
{
struct bac *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    bacFree(&el);
    }
*pList = NULL;
}


struct bac *bacRead(struct sqlConnection *conn, int taxon, char *db)
/* Slurp in the bac rows sorted by chrom */
{
struct bac *list=NULL, *el;
char query[512];
struct sqlResult *sr;
char **row;
safef(query, sizeof(query), 
"select e.chrom, e.chromStart, e.chromEnd, e.strand, p.id"
" from probe p, probeExt pe, bac b, gene g, %s.bacEndPairs e"
" where p.bac = b.id and p.gene = g.id and g.taxon=%d and b.name = e.name"
" and p.id = pe.probe and pe.type='bac' and pe.state='new'"
" order by e.chrom, e.chromStart",
db,taxon);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = bacLoad(row);
    slAddHead(&list,el);
    }
sqlFreeResult(&sr);
slReverse(&list);  
return list;
}

char *checkAndFetchNib(struct dnaSeq *chromSeq, struct bac *bac)
/* fetch nib return string to be freed later. Reports if segment exceeds ends of chromosome. */
{
if (bac->chromStart < 0)
    {
    errAbort("bac error chromStart < 0 : %s %u %u %s %d \n",
        bac->chrom,
        bac->chromStart,
        bac->chromEnd,
        bac->strand,
        bac->probe
        );
    return NULL;
    }
if (bac->chromEnd > chromSeq->size)
    {
    errAbort("bac error chromEnd > chromSeq->size (%lu) : %s %u %u %s %d \n",
        (unsigned long) chromSeq->size,
        bac->chrom,
        bac->chromStart,
        bac->chromEnd,
        bac->strand,
        bac->probe
        );
    return NULL;
    }

return cloneStringZ(chromSeq->dna + bac->chromStart, bac->chromEnd - bac->chromStart);
}

static void populateMissingProbeExt(struct sqlConnection *conn)
/* populate probeExt where missing */
{
struct sqlResult *sr;
char **row;
struct dyString *dy = dyStringNew(0);
struct sqlConnection *conn2 = sqlConnect(database);

dyStringAppend(dy, "select p.id,p.gene,antibody,probeType,fPrimer,rPrimer,p.seq,bac from probe p");
dyStringAppend(dy, " left join probeExt e on e.probe = p.id");
dyStringAppend(dy, " where e.probe is NULL");
sr = sqlGetResult(conn, dy->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    int id = sqlUnsigned(row[0]); 
    int gene = sqlUnsigned(row[1]); 
    int antibody = sqlUnsigned(row[2]); 
    int probeType = sqlUnsigned(row[3]); 
    char *fPrimer = row[4]; 
    char *rPrimer = row[5]; 
    char *seq = row[6]; 
    int bac = sqlUnsigned(row[7]); 

    char *peType = "none";
    int peProbe = id;
    char *peSeq = seq;
    char *tName = "";
    int tStart = 0;
    int tEnd = 0;
    char *tStrand = " ";
    char *peGene = "";
    int bacInfo = 0;
    int seqid = 0;
    int pslid = 0;
    char *state = "new";

    if (isNotEmpty(seq))
    	{
	peType = "probe";
	state = "seq";
	}
    else if (isNotEmpty(fPrimer) && isNotEmpty(rPrimer))
    	{
	peType = "primersMrna";
	}
    else if (isNotEmpty(fPrimer) && isEmpty(rPrimer))
    	{ /* only have fPrimer, it's probably a comment, not dna seq */
	peType = "refSeq";   /* use accession or gene */
	}
    else if (bac > 0)
    	{
	peType = "bac";   /* use bacEndPairs */
	}
    else	    
    	{
	peType = "refSeq";   /* use accession or gene */
	}
	
    dyStringClear(dy);
    dyStringAppend(dy, "insert into probeExt set");
    dyStringPrintf(dy, " id=default,\n");
    dyStringPrintf(dy, " type='%s',\n", peType);
    dyStringPrintf(dy, " probe=%d,\n", peProbe);
    dyStringAppend(dy, " seq='");
    dyStringAppend(dy, peSeq);
    dyStringAppend(dy, "',\n");
    dyStringPrintf(dy, " tName='%s',\n", tName);
    dyStringPrintf(dy, " tStart=%d,\n", tStart);
    dyStringPrintf(dy, " tEnd=%d,\n", tEnd);
    dyStringPrintf(dy, " tStrand='%s',\n", tStrand);
    dyStringPrintf(dy, " state='%s'\n", state);
    verbose(2, "%s\n", dy->string);
    sqlUpdate(conn2, dy->string);
    
    }

dyStringFree(&dy);
    
sqlFreeResult(&sr);

sqlDisconnect(&conn2);

}

static int sqlSaveQuery(struct sqlConnection *conn, char *query, int numCols, char *outPath, boolean isFa)
/* execute query, save the resultset as a tab-separated file */
{
struct sqlResult *sr;
char **row;
char *sep="";
int c = 0;
int count = 0;
FILE *f = mustOpen(outPath,"w");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    sep="";
    if (isFa)
	sep = ">";
    for (c=0;c<numCols;++c)
	{
	fprintf(f,"%s%s",sep,row[c]);
	sep = "\t";
	if (isFa)
	    sep = "\n";
	}
    fprintf(f,"\n");	    
    ++count;
    }
sqlFreeResult(&sr);
carefulClose(&f);
return count;
}


static void processIsPcr(struct sqlConnection *conn)
/* process isPcr results  */
{

/* >NM_010919:371+1088 2 718bp CGCGGATCCAAGGACATCTTGGACCTTCCG CCCAAGCTTGCATGTGCTGCAGCGACTGCG */

struct dyString *dy = dyStringNew(0);
struct lineFile *lf = lineFileOpen("isPcr.fa", TRUE);
int lineSize;
char *line;
char *name;
char *dna;
char *word, *end;
char *tName;
int tStart;
int tEnd;
char *tStrand;
int probeid=0;
boolean more = lineFileNext(lf, &line, &lineSize);
while(more)
    {
    if (line[0] != '>')
	errAbort("unexpected error out of phase\n");
    name = cloneString(line);
    uglyf("name=%s\n",name);
    dyStringClear(dy);
    while(more=lineFileNext(lf, &line, &lineSize))
	{
	if (line[0] == '>')
	    {
	    break;
	    }
	dyStringAppend(dy,line);	    
	}
    dna = cloneString(dy->string);
    word = name+1;
    end = strchr(word,':');
    tName = cloneStringZ(word,end-word); 
    word = end+1;
    end = strchr(word,'+');
    tStrand = "+";
    if (!end)
	{
	end = strchr(word,'-');
	tStrand = "-";
	}
    tStart = atoi(word); 
    word = end+1;
    end = strchr(word,' ');
    tEnd = atoi(word); 
    word = end+1;
    end = strchr(word,' ');
    probeid = atoi(word); 

    dyStringClear(dy);
    dyStringAppend(dy, "update probeExt set");
    dyStringAppend(dy, " seq='");
    dyStringAppend(dy, dna);
    dyStringAppend(dy, "',\n");
    dyStringPrintf(dy, " tName='%s',\n", tName);
    dyStringPrintf(dy, " tStart=%d,\n", tStart);
    dyStringPrintf(dy, " tEnd=%d,\n", tEnd);
    dyStringPrintf(dy, " tStrand='%s',\n", tStrand);
    dyStringPrintf(dy, " state='%s'\n", "seq");
    dyStringPrintf(dy, " where probe=%d\n", probeid);
    dyStringPrintf(dy, " and state='%s'\n", "new");
    verbose(2, "%s\n", dy->string);
    sqlUpdate(conn, dy->string);

    
    freez(&tName);
    freez(&name);
    freez(&dna);
    }
lineFileClose(&lf);

dyStringFree(&dy);
}

static int doBacs(struct sqlConnection *conn, int taxon, char *db)
/* fetch available sequence for bacEndPairs */
{
struct dyString *dy = dyStringNew(0);
struct dnaSeq *chromSeq = NULL;
struct bac *bacs = bacRead(conn, taxon, db);
struct bac *bac = NULL;
char *chrom = cloneString("");
int count = 0;

uglyf("bac list read done.\n");

hSetDb(db);  /* unfortunately this is required for hLoadChrom */
for(bac=bacs;bac;bac=bac->next)
    {
    
    if (differentWord(chrom,bac->chrom))
	{
	verbose(1,"switching to chrom %s\n",bac->chrom);
	dnaSeqFree(&chromSeq); 
	chromSeq = hLoadChrom(bac->chrom);
	freez(&chrom);
	chrom = cloneString(bac->chrom);
	}

    char *dna = checkAndFetchNib(chromSeq, bac);
    if (sameString(bac->strand,"-"))
	{
	reverseComplement(dna,strlen(dna));
	}
    
    dyStringClear(dy);
    dyStringAppend(dy, "update probeExt set");
    dyStringAppend(dy, " seq='");
    dyStringAppend(dy, dna);
    dyStringAppend(dy, "',\n");
    dyStringPrintf(dy, " tName='%s',\n", bac->chrom);
    dyStringPrintf(dy, " tStart=%d,\n", bac->chromStart);
    dyStringPrintf(dy, " tEnd=%d,\n", bac->chromEnd);
    dyStringPrintf(dy, " tStrand='%s',\n", bac->strand);
    dyStringPrintf(dy, " state='%s'\n", "seq");
    dyStringPrintf(dy, " where probe=%d\n", bac->probe);
    dyStringPrintf(dy, " and state='%s'\n", "new");
    //verbose(2, "%s\n", dy->string); // the sql string could be quite large
    sqlUpdate(conn, dy->string);

    ++count; 
    
    verbose(2,"%d finished bac for probe id %d size %d\n", 
	count, bac->probe, bac->chromEnd - bac->chromStart);

    freez(&dna);
    }

freez(&chrom);
dnaSeqFree(&chromSeq);

bacFreeList(&bacs);

dyStringFree(&dy);

return count;  
}

static void doPrimers(struct sqlConnection *conn, int taxon, char *db)
/* get probe seq from primers */
{
int rc = 0;
struct dyString *dy = dyStringNew(0);
char cmdLine[256];
char path1[256];
char path2[256];
FILE *f = NULL;

dyStringClear(dy);
dyStringAppend(dy, "select p.id, p.fPrimer, p.rPrimer from probe p, probeExt e, gene g");
dyStringPrintf(dy, " where p.id = e.probe and g.id = p.gene and g.taxon = %d",taxon);
dyStringAppend(dy, " and e.state = 'new' and e.type='primersMrna'");
rc = sqlSaveQuery(conn, dy->string, 3, "primers.query", FALSE);
uglyf("rc = %d = count of primers for mrna search for taxon %d\n",rc,taxon);

if (rc > 0) /* something to do */
    {

    dyStringClear(dy);
    dyStringPrintf(dy, "select qName from %s.all_mrna",db);
    rc = 0;
    rc = sqlSaveQuery(conn, dy->string, 1, "accFile.txt", FALSE);
    safef(cmdLine,sizeof(cmdLine),"getRna %s accFile.txt mrna.fa",db);
    system("date"); uglyf("cmdLine: [%s]\n",cmdLine); system(cmdLine); system("date");
    
    uglyf("rc = %d = count of mrna for %s\n",rc,db);

    system("date"); system("isPcr mrna.fa primers.query isPcr.fa -out=fa"); system("date");
    system("ls -l");

    processIsPcr(conn);

    unlink("mrna.fa"); unlink("accFile.txt"); unlink("isPcr.fa");

    }
unlink("primers.query");    

/* find any remaining type primersMrna that couldn't be resolved and demote 
 * them to type primersGenome
 */
dyStringClear(dy);
dyStringAppend(dy, "update probeExt e, probe p, gene g set e.type='primersGenome'"); 
dyStringPrintf(dy, " where p.id = e.probe and g.id = p.gene and g.taxon = %d",taxon);
dyStringAppend(dy, " and e.state = 'new' and e.type='primersMrna'");
sqlUpdate(conn, dy->string);



/* get primers for those probes that did not find mrna isPcr matches 
 * and then do them against the genome instead */
dyStringClear(dy);
dyStringAppend(dy, "select p.id, p.fPrimer, p.rPrimer from probe p, probeExt e, gene g");
dyStringPrintf(dy, " where p.id = e.probe and g.id = p.gene and g.taxon = %d",taxon);
dyStringAppend(dy, " and e.state = 'new' and e.type='primersGenome'");
rc = 0;
rc = sqlSaveQuery(conn, dy->string, 3, "primers.query", FALSE);
uglyf("rc = %d = count of primers for genome search for taxon %d\n",rc,taxon);

if (rc > 0) /* something to do */
    {
    safef(path1,sizeof(path1),"/gbdb/%s/%s.2bit",db,db);
    safef(path2,sizeof(path2),"%s/%s.2bit",getCurrentDir(),db);
    uglyf("copy: [%s] to [%s]\n",path1,path2);  copyFile(path1,path2);

    safef(cmdLine,sizeof(cmdLine),
	    "ssh kolossus 'cd %s; isPcr %s.2bit primers.query isPcr.fa -out=fa'",
	    getCurrentDir(),db);
    system("date"); uglyf("cmdLine: [%s]\n",cmdLine); system(cmdLine); system("date");
    safef(path2,sizeof(path2),"%s/%s.2bit",getCurrentDir(),db);
    uglyf("rm %s\n",path2); unlink(path2); system("ls -l");

    processIsPcr(conn);
    
    unlink("mrna.fa"); unlink("accFile.txt"); unlink("isPcr.fa");

    }
unlink("primers.query");    

/* find any remaining type primersGenome that couldn't be resolved and demote 
 * them to type refSeq
 */
dyStringClear(dy);
dyStringAppend(dy, "update probeExt e, probe p, gene g set e.type='refSeq'"); 
dyStringPrintf(dy, " where p.id = e.probe and g.id = p.gene and g.taxon = %d",taxon);
dyStringAppend(dy, " and e.state = 'new' and e.type='primersGenome'");
sqlUpdate(conn, dy->string);

dyStringFree(&dy);
}

static void doBacEndPairs(struct sqlConnection *conn, int taxon, char *db)
/* get probe seq bacEndPairs */
{
struct dyString *dy = dyStringNew(0);
int rc = 0;
/* fetch available sequence for bacEndPairs */
rc = doBacs(conn, taxon, db); uglyf("found seq for %d bacEndPairs\n",rc);

/* find any remaining type bac that couldn't be resolved and demote 
 * them to type refSeq
 */
dyStringClear(dy);
dyStringAppend(dy, "update probeExt e, probe p, gene g set e.type='refSeq'"); 
dyStringPrintf(dy, " where p.id = e.probe and g.id = p.gene and g.taxon = %d",taxon);
dyStringAppend(dy, " and e.state = 'new' and e.type='bac'");
sqlUpdate(conn, dy->string);

dyStringFree(&dy);
}



static void setTName(struct sqlConnection *conn, int taxon, char *db, char *type, char *fld)
/* set probeExt.tName to the desired value to try, 
 *  e.g. gene.refSeq, gene.genbank, mm6.refFlat.name(genoName=gene.name). */
{
struct dyString *dy = dyStringNew(0);
dyStringClear(dy);
dyStringPrintf(dy, 
    "update probeExt e, probe p, gene g"
    " set e.seq = '', e.tName = g.%s"
    " where e.probe = p.id and p.gene = g.id"
    " and e.type = '%s'"
    " and e.state = 'new'"
    " and g.taxon = %d"
    , fld, type, taxon);
sqlUpdate(conn, dy->string);
dyStringFree(&dy);
}

static void setTNameMapped(struct sqlConnection *conn, int taxon, char *db, char *type, char *fld, 
            char *mapTable, char *mapField, char *mapTo)
/* set probeExt.tName to the desired value to try, 
 *  e.g. gene.refSeq, gene.genbank, mm6.refFlat.name(genoName=gene.name). */
{
struct dyString *dy = dyStringNew(0);
dyStringClear(dy);
dyStringPrintf(dy, 
"update probeExt e, probe p, gene g, %s.%s f"
" set e.seq = '', e.tName = f.%s"
" where e.probe = p.id and p.gene = g.id"
" and g.%s = f.%s"
    " and e.type = '%s'"
    " and e.state = 'new'"
    " and g.taxon = %d"
    ,db, mapTable, mapTo, fld, mapField, type, taxon);
sqlUpdate(conn, dy->string);
dyStringFree(&dy);
}

static void processMrnaFa(struct sqlConnection *conn, int taxon, char *type)
/* process isPcr results  */
{

struct dyString *dy = dyStringNew(0);
struct lineFile *lf = lineFileOpen("mrna.fa", TRUE);
int lineSize;
char *line;
char *name;
char *dna;
boolean more = lineFileNext(lf, &line, &lineSize);
while(more)
    {
    if (line[0] != '>')
	errAbort("unexpected error out of phase\n");
    name = cloneString(line+1);
    verbose(2,"name=%s\n",name);
    dyStringClear(dy);
    while(more=lineFileNext(lf, &line, &lineSize))
	{
	if (line[0] == '>')
	    {
	    break;
	    }
	dyStringAppend(dy,line);	    
	}
    dna = cloneString(dy->string);

    dyStringClear(dy);

    dyStringAppend(dy, "update probeExt e, probe p, gene g");
    dyStringAppend(dy, " set e.seq = '");
    dyStringAppend(dy, dna);
    dyStringAppend(dy, "',\n");
    dyStringAppend(dy, " e.state = 'seq',\n");
    dyStringPrintf(dy, " e.tName = '%s'\n",name);  /* so we know where it came from */
    dyStringAppend(dy, " where e.probe = p.id and p.gene = g.id");
    dyStringPrintf(dy, " and g.taxon = %d", taxon);
    dyStringPrintf(dy, " and e.type = '%s' and e.tName = '%s'", type, name);
    dyStringAppend(dy, " and e.seq = ''");
    verbose(2, "%s\n", dy->string);
    sqlUpdate(conn, dy->string);

    freez(&name);
    freez(&dna);
    }
lineFileClose(&lf);

dyStringFree(&dy);
}



static int getAccMrnas(struct sqlConnection *conn, int taxon, char *db, char *type, char *table)
/* get mRNAs for one probeExt acc type */
{
int rc = 0;
struct dyString *dy = dyStringNew(0);
char cmdLine[256];
dyStringClear(dy);
dyStringPrintf(dy, 
    "select distinct e.tName from probeExt e, probe p, gene g, %s.%s m"
    " where e.probe = p.id and p.gene = g.id and e.tName = m.qName"
    " and g.taxon = %d and e.type = '%s' and e.tName <> '' and e.state = 'new'"
    ,db,table,taxon,type);
rc = 0;
rc = sqlSaveQuery(conn, dy->string, 1, "accFile.txt", FALSE);
safef(cmdLine,sizeof(cmdLine),"getRna %s accFile.txt mrna.fa",db);
//system("date"); uglyf("cmdLine: [%s]\n",cmdLine); 
system(cmdLine); 
//system("date");

processMrnaFa(conn, taxon, type);

unlink("mrna.fa"); 
unlink("accFile.txt");

dyStringFree(&dy);
return rc;
}

static void advanceType(struct sqlConnection *conn, int taxon, char *oldType, char *newType)
/* find any remaining type refSeq that couldn't be resolved and demote 
 * them to type genbank
 */
{ 
struct dyString *dy = dyStringNew(0);
dyStringClear(dy);
dyStringPrintf(dy, 
 "update probeExt e, probe p, gene g set e.type='%s', e.tName=''"
 " where p.id = e.probe and g.id = p.gene and g.taxon = %d"
 " and e.state = 'new' and e.type='%s'"
 ,newType,taxon,oldType);
sqlUpdate(conn, dy->string);
dyStringFree(&dy);
}

static int doAccPsls(struct sqlConnection *conn, int taxon, char *db, char *type, char *table)
/* get psls for one probeExt acc type */
{
int rc = 0;
struct dyString *dy = dyStringNew(0);
char outName[256];
dyStringClear(dy);
dyStringPrintf(dy, 
    "select m.matches,m.misMatches,m.repMatches,m.nCount,m.qNumInsert,m.qBaseInsert,"
    "m.tNumInsert,m.tBaseInsert,m.strand,"
    "concat(\"vgPrb_\",p.id),"
    "m.qSize,m.qStart,m.qEnd,m.tName,m.tSize,m.tStart,m.tEnd,m.blockCount,"
    "m.blockSizes,m.qStarts,m.tStarts"    
    " from probeExt e, probe p, gene g, %s.%s m"
    " where e.probe = p.id and p.gene = g.id and e.tName = m.qName"
    " and g.taxon = %d and e.type = '%s' and e.tName <> '' and e.state='seq' and e.seq <> ''"
    " order by m.tName,m.tStart"
    ,db,table,taxon,type);
rc = 0;
safef(outName,sizeof(outName),"%s.psl",type);
rc = sqlSaveQuery(conn, dy->string, 21, outName, FALSE);
dyStringFree(&dy);
return rc;
}

static void doAccessions(struct sqlConnection *conn, int taxon, char *db)
/* get probe seq from primers */
{
int rc = 0;
struct dyString *dy = dyStringNew(0);

/* get refSeq accessions and rna */
setTName(conn, taxon, db, "refSeq", "refSeq");
rc = getAccMrnas(conn, taxon, db, "refSeq", "refSeqAli");
uglyf("rc = %d = count of refSeq mrna for %s\n",rc,db);
advanceType(conn,taxon,"refSeq","genRef");

/* get refSeq-in-gene.genbank accessions and rna */
setTName(conn, taxon, db, "genRef", "genbank");
rc = getAccMrnas(conn, taxon, db, "genRef", "refSeqAli");
uglyf("rc = %d = count of genRef mrna for %s\n",rc,db);
advanceType(conn,taxon,"genRef","genbank");

/* get genbank accessions and rna */
setTName(conn, taxon, db, "genbank", "genbank");
rc = getAccMrnas(conn, taxon, db, "genbank", "all_mrna");
uglyf("rc = %d = count of genbank mrna for %s\n",rc,db);
advanceType(conn,taxon,"genbank","flatRef");

/* get gene.name -> refFlat to refSeq accessions and rna */
setTNameMapped(conn, taxon, db, "flatRef", "name", "refFlat", "geneName", "name");
rc = getAccMrnas(conn, taxon, db, "flatRef", "refSeqAli");
uglyf("rc = %d = count of flatRef mrna for %s\n",rc,db);
advanceType(conn,taxon,"flatRef","flatAll");

/* get gene.name -> refFlat to all_mrna accessions */
setTNameMapped(conn, taxon, db, "flatAll", "name", "refFlat", "geneName", "name");
rc = getAccMrnas(conn, taxon, db, "flatAll", "all_mrna");
uglyf("rc = %d = count of flatAll mrna for %s\n",rc,db);
advanceType(conn,taxon,"flatAll","linkRef");



/* get gene.name -> refLink to refSeq accessions and rna */
setTNameMapped(conn, taxon, db, "linkRef", "name", "refLink", "name", "mrnaAcc");
rc = getAccMrnas(conn, taxon, db, "linkRef", "refSeqAli");
uglyf("rc = %d = count of linkRef mrna for %s\n",rc,db);
advanceType(conn,taxon,"linkRef","linkAll");

/* get gene.name -> refLink to all_mrna accessions */
setTNameMapped(conn, taxon, db, "linkAll", "name", "refLink", "name", "mrnaAcc");
rc = getAccMrnas(conn, taxon, db, "linkAll", "all_mrna");
uglyf("rc = %d = count of linkAll mrna for %s\n",rc,db);
advanceType(conn,taxon,"linkAll","kgAlRef");



/* get gene.name -> kgAlias to refSeq accessions and rna */
setTNameMapped(conn, taxon, db, "kgAlRef", "name", "kgAlias", "alias", "kgId");
rc = getAccMrnas(conn, taxon, db, "kgAlRef", "refSeqAli");
uglyf("rc = %d = count of kgAlRef mrna for %s\n",rc,db);
advanceType(conn,taxon,"kgAlRef","kgAlAll");

/* get gene.name -> kgAlias to all_mrna accessions */
setTNameMapped(conn, taxon, db, "kgAlAll", "name", "kgAlias", "alias", "kgId");
rc = getAccMrnas(conn, taxon, db, "kgAlAll", "all_mrna");
uglyf("rc = %d = count of kgAlAll mrna for %s\n",rc,db);
advanceType(conn,taxon,"kgAlAll","gene");

/* get refSeq psls */
rc = doAccPsls(conn, taxon, db, "refSeq", "refSeqAli");
uglyf("rc = %d = count of refSeq psls for %s\n",rc,db);

/* get genRef psls */
rc = doAccPsls(conn, taxon, db, "genRef", "refSeqAli");
uglyf("rc = %d = count of genRef psls for %s\n",rc,db);

/* get genbank psls */
rc = doAccPsls(conn, taxon, db, "genbank", "all_mrna");
uglyf("rc = %d = count of genbank psls for %s\n",rc,db);

/* get flatRef psls */
rc = doAccPsls(conn, taxon, db, "flatRef", "refSeqAli");
uglyf("rc = %d = count of flatRef psls for %s\n",rc,db);

/* get flatAll psls */
rc = doAccPsls(conn, taxon, db, "flatAll", "all_mrna");
uglyf("rc = %d = count of flatAll psls for %s\n",rc,db);

/* get linkRef psls */
rc = doAccPsls(conn, taxon, db, "linkRef", "refSeqAli");
uglyf("rc = %d = count of linkRef psls for %s\n",rc,db);

/* get linkAll psls */
rc = doAccPsls(conn, taxon, db, "linkAll", "all_mrna");
uglyf("rc = %d = count of linkAll psls for %s\n",rc,db);

/* get kgAlRef psls */
rc = doAccPsls(conn, taxon, db, "kgAlRef", "refSeqAli");
uglyf("rc = %d = count of kgAlRef psls for %s\n",rc,db);

/* get kgAlAll psls */
rc = doAccPsls(conn, taxon, db, "kgAlAll", "all_mrna");
uglyf("rc = %d = count of kgAlRef psls for %s\n",rc,db);


dyStringFree(&dy);
}


static void doBlat(struct sqlConnection *conn, int taxon, char *db)
/* place probe seq from primers, etc with blat */
{
int rc = 0;
char *blatSpec=NULL;
char cmdLine[256];
char path1[256];
char path2[256];
struct dyString *dy = dyStringNew(0);

/* make .ooc and blat on kolossus */

safef(path1,sizeof(path1),"/gbdb/%s/%s.2bit",db,db);
safef(path2,sizeof(path2),"%s/%s.2bit",getCurrentDir(),db);
//restore: 
uglyf("copy: [%s] to [%s]\n",path1,path2);  copyFile(path1,path2);

safef(cmdLine,sizeof(cmdLine),
	"ssh kolossus 'cd %s; blat -makeOoc=11.ooc -tileSize=11 -repMatch=1024 %s.2bit /dev/null /dev/null'",
	getCurrentDir(),db);
//restore: 
system("date"); uglyf("cmdLine: [%s]\n",cmdLine); system(cmdLine); system("date");

/* (will do BACs separately later) */
dyStringClear(dy);
dyStringPrintf(dy, 
    "select concat(\"vgPrb_\",e.probe), e.seq"
    " from probeExt e, probe p, gene g"
    " where e.probe = p.id and p.gene = g.id"
    " and g.taxon = %d"
    " and e.type in ('primersMrna','primersGenome','gene','probe')"
    " and e.seq <> ''"
    " order by e.probe"
    , taxon);
//restore: 
rc = sqlSaveQuery(conn, dy->string, 2, "blat.fa", TRUE);
uglyf("rc = %d = count of sequences for blat, to get psls for taxon %d\n",rc,taxon);

safef(cmdLine,sizeof(cmdLine),
	"ssh kolossus 'cd %s; blat %s.2bit blat.fa -ooc=11.ooc -noHead blat.psl'",
	getCurrentDir(),db);
//restore: 
system("date"); uglyf("cmdLine: [%s]\n",cmdLine); system(cmdLine); system("date");

/* using blat even with -fastMap was way too slow - took over a day,
 * so instead I will make a procedure to write a fake psl for the BACs
 * which you will see called below */

safef(path2,sizeof(path2),"%s.2bit",db);
uglyf("rm %s\n",path2); unlink(path2); 

safef(path2,sizeof(path2),"11.ooc");
uglyf("rm %s\n",path2); unlink(path2); 


/* skip psl header and sort on query name */
safef(cmdLine,sizeof(cmdLine), "sort -k 10,10 blat.psl > blatS.psl");
uglyf("cmdLine=[%s]\n",cmdLine);
system(cmdLine); 

/* keep near best within 5% of the best */
safef(cmdLine,sizeof(cmdLine), 
    "pslCDnaFilter -globalNearBest=0.005 -minId=0.96 -minNonRepSize=20 -minCover=0.50"
    " blatS.psl blatNearBest.psl");
uglyf("cmdLine=[%s]\n",cmdLine);
system(cmdLine); 


unlink("blat.fa");
unlink("blat.psl");
unlink("blatS.psl");

/* create fake psls as blatBAC.psl */
dyStringClear(dy);
dyStringPrintf(dy, 
    "select length(e.seq), 0, 0, 0, 0, 0, 0, 0, e.tStrand, concat('vgPrb_',e.probe), length(e.seq),"
    " 0, length(e.seq), e.tName, ci.size, e.tStart, e.tEnd, 1,"
    " concat(length(e.seq),','), concat(0,','), concat(e.tStart,',')"
    " from probeExt e, %s.chromInfo ci, probe p, gene g"
    " where ci.chrom = e.tName and e.probe=p.id and p.gene=g.id"
    " and e.type = 'bac'"
    " and g.taxon = %d"
    , db, taxon);
//restore: 
rc = sqlSaveQuery(conn, dy->string, 21, "blatBAC.psl", FALSE);
uglyf("rc = %d = count of sequences for blatBAC.psl, for taxon %d\n",rc,taxon);

      

freez(&blatSpec);
dyStringFree(&dy);
}

void static assembleAllFaPsl(struct sqlConnection *conn, int taxon, char *db)
/* assemble final seq (all.fa) and alignments (various.psl) */
{
int rc = 0;
char cmdLine[256];
struct dyString *dy = dyStringNew(0);
dyStringClear(dy);
dyStringPrintf(dy, 
    "select concat(\"vgPrb_\",e.probe) prb, e.seq"
    " from probeExt e, probe p, gene g"
    " where e.probe = p.id and p.gene = g.id"
    " and g.taxon = %d"
    " and e.seq <>  ''"
    " order by prb"
    , taxon);
//restore: 
rc = sqlSaveQuery(conn, dy->string, 2, "vgPrbAll.fa", TRUE);
uglyf("rc = %d = count of sequences for vgPrbAll.fa, to use with psls for taxon %d\n",rc,taxon);

// restore:
/* make final psl */
system(
"cat"
" blatNearBest.psl"
" blatBAC.psl"
" flatAll.psl"
" flatRef.psl"
" genRef.psl"
" genbank.psl"
" kgAlAll.psl"
" kgAlRef.psl"
" linkAll.psl"
" linkRef.psl"
" refSeq.psl"
" > vgPrbAll.psl"
);

uglyf("vgPrbAll.psl assembled for taxon %d\n",taxon);


//unlink("blatNearBest.psl");  
//unlink("blatBAC.psl");  


system("ls -ltr");

dyStringFree(&dy);
}

static void doProbes(struct sqlConnection *conn, int taxon, char *db)
/* get probe seq from primers, bacEndPairs, refSeq, genbank, gene-name */
{

//restore: doPrimers(conn, taxon, db);

//restore: doBacEndPairs(conn, taxon, db);

//restore: doAccessions(conn, taxon, db);

//restore: 
doBlat(conn, taxon, db);   // watch out! some stuff inside here still disabled

//restore: 
assembleAllFaPsl(conn, taxon, db);
printf("OK, DONE.  Go put vgPrbAll.fa in /gbdb/%s/ and use hgLoadPsl and hgLoadSeq \n",db);

}

static void vgProbeTrack()
/* build visiGene probeExt table */
{
struct sqlConnection *conn = sqlConnect(database);
struct probeExt *pE = NULL;

if (kill)
    sqlDropTable(conn, "probeExt");

if (!sqlTableExists(conn, "probeExt"))
    {
    char *sql = NULL;
    char path[256];
    safef(path,sizeof(path),"%s/%s",sqlPath,"probeExt.sql");
    readInGulp(path, &sql, NULL);
    sqlUpdate(conn, sql);
    sqlUpdate(conn, "create index probe on probeExt(probe);");
    sqlUpdate(conn, "create index tName on probeExt(tName(20));");
    }

/* populate probeExt where missing */

if (kill)  // this dependency is convenient for now.
    populateMissingProbeExt(conn);

/* do mouse */
/* get probe seq from primers, bacEndPairs, refSeq, genbank, gene-name */
//restore: 
doProbes(conn,10090,"mm7");  /* taxon 10090 = mouse */

// just to grab frog probe seq fa
//doProbes(conn,8355,"mm7");  /* taxon 8355 = frog */

sqlDisconnect(&conn);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
if (!setCurrentDir(argv[1]))
    usage();
database = optionVal("database", database);
sqlPath = optionVal("sqlPath", sqlPath);
kill = optionExists("kill");
//delete this: visiGeneLoad(argv[1], argv[2], argv[3]);
vgProbeTrack();
return 0;
}
