/* vgProbeTrack - build vgPrb% permanent id tables in visiGene database, 
 *   also updates $db.vg{All}Probes tables
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
#include "hdb.h"

#include "vgPrb.h"

/* Variables you can override from command line. */
char *database = "visiGene";
char *sqlPath = ".";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "vgProbeTrack - build permanent-Id vgPrb* tables in visiGene database\n"
  " and update assembly-specific tables vgProbes and vgAllProbes\n"
  "usage:\n"
  "   vgProbeTrack <COMMAND> {workingDir db} {optional params}\n"
  "\n"
  "Commands:\n"
  "   INIT - WARNING! drops and creates all tables (except vgPrb is not dropped). \n"
  "   POP  - populate vgPrb to catch probes for newly added submission sets. \n"
  "   SEQ workingDir db - find sequence using assembly db. \n"
  "   ALI workingDir db - find or blat any needed alignments for vgProbes track. \n"
  "   EXT workingDir db - add any needed seq and extfile records for vgProbes track. \n"
  "   PSLMAP  workingDir db fromDb - pslMap using chains fromDb to db for vgAllProbes track. \n"
  "   REMAP   workingDir db fromDb track fa - import db.track psls of fromDb using fa fasta file for vgAllProbes track. \n"
  "   SELFMAP workingDir db - migrate self vgProbes to vgAllProbes track. \n"
  "   EXTALL workingDir db - add any needed seq and extfile records for vgAllProbes track. \n"
  "\n"
  "workingDir is a directory with space for intermediate and final results.\n"
  "db is the target assembly to use.\n"
  "Options:\n"
  "   -database=%s - Specifically set database (default visiGene)\n"
  "   -sqlPath=%s - specify location of vgPrb.sql, relative to workingDir \n"
  , database, sqlPath
  );
}

static struct optionSpec options[] = {
   {"database", OPTION_STRING,},
   {"sqlPath", OPTION_STRING,},
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
"select e.chrom, e.chromStart, e.chromEnd, e.strand, pe.id"
" from probe p, vgPrbMap m, vgPrb pe, bac b, %s.bacEndPairs e"
" where p.bac = b.id and p.id = m.probe and pe.id = m.vgPrb"
" and pe.taxon=%d and b.name = e.name"
" and pe.type='bac' and pe.state='new'"
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

static int findVgPrbBySeq(struct sqlConnection *conn, char *seq, int taxon)
/* find vgPrb.id by seq given or return 0 if not found */
{
char *fmt = "select id from vgPrb where seq = '%s' and taxon=%d";
int size = strlen(fmt)+strlen(seq)+4;
char *sql = needMem(size);
safef(sql,size,fmt,seq,taxon);
return sqlQuickNum(conn,sql);
freez(&sql);
}

static void populateMissingVgPrb(struct sqlConnection *conn)
/* populate vgPrb where missing, usually after new records added to visiGene */
{
struct sqlResult *sr;
char **row;
struct dyString *dy = dyStringNew(0);
struct sqlConnection *conn2 = sqlConnect(database);
struct sqlConnection *conn3 = sqlConnect(database);
int probeCount=0, vgPrbCount=0;

dyStringAppend(dy, 
"select p.id,p.gene,antibody,probeType,fPrimer,rPrimer,p.seq,bac,g.taxon"
" from probe p join gene g"
" left join vgPrbMap m on m.probe = p.id"
" where g.id = p.gene"
"   and m.probe is NULL");
sr = sqlGetResult(conn, dy->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    int id = sqlUnsigned(row[0]); 
    /* int gene = sqlUnsigned(row[1]); */
    /* int antibody = sqlUnsigned(row[2]); */
    /* int probeType = sqlUnsigned(row[3]); */
    char *fPrimer = row[4]; 
    char *rPrimer = row[5]; 
    char *seq = row[6]; 
    int bac = sqlUnsigned(row[7]); 
    int taxon = sqlUnsigned(row[8]); 

    char *peType = "none";
    int peProbe = id;
    char *peSeq = seq;
    char *tName = "";
    int tStart = 0;
    int tEnd = 0;
    char *tStrand = " ";
    /*
    char *peGene = "";
    int bacInfo = 0;
    int seqid = 0;
    int pslid = 0;
    */
    char *state = "new";
    char *db = "";
    int vgPrb = 0;

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

    if (!sameString(peSeq,""))
	{
	vgPrb = findVgPrbBySeq(conn3,peSeq,taxon);
	}

    if (vgPrb == 0)
	{
	dyStringClear(dy);
	dyStringAppend(dy, "insert into vgPrb set");
	dyStringPrintf(dy, " id=default,\n");
	dyStringPrintf(dy, " type='%s',\n", peType);
	dyStringAppend(dy, " seq='");
	dyStringAppend(dy, peSeq);
	dyStringAppend(dy, "',\n");
	dyStringPrintf(dy, " tName='%s',\n", tName);
	dyStringPrintf(dy, " tStart=%d,\n", tStart);
	dyStringPrintf(dy, " tEnd=%d,\n", tEnd);
	dyStringPrintf(dy, " tStrand='%s',\n", tStrand);
	dyStringPrintf(dy, " db='%s',\n", db);
	dyStringPrintf(dy, " taxon='%d',\n", taxon);
	dyStringPrintf(dy, " state='%s'\n", state);
	verbose(2, "%s\n", dy->string);
	sqlUpdate(conn2, dy->string);
	vgPrb = sqlLastAutoId(conn2);
	vgPrbCount++;
	}
	
    dyStringClear(dy);
    dyStringAppend(dy, "insert into vgPrbMap set");
    dyStringPrintf(dy, " probe=%d,\n", peProbe);
    dyStringPrintf(dy, " vgPrb=%d \n", vgPrb);
    verbose(2, "%s\n", dy->string);
    sqlUpdate(conn2, dy->string);

    probeCount++;
	
    }

verbose(1, "# new probe records found = %d, # new vgPrb records added = %d\n", probeCount, vgPrbCount);

dyStringFree(&dy);
    
sqlFreeResult(&sr);

sqlDisconnect(&conn3);
sqlDisconnect(&conn2);

}

static void processIsPcr(struct sqlConnection *conn, int taxon, char *db)
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
int probeid=0;  /* really a vgPrb id */
boolean more = lineFileNext(lf, &line, &lineSize);
while(more)
    {
    if (line[0] != '>')
	errAbort("unexpected error out of phase\n");
    name = cloneString(line);
    verbose(1,"name=%s\n",name);
    dyStringClear(dy);
    while((more=lineFileNext(lf, &line, &lineSize)))
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
    dyStringPrintf(dy, "select count(*) from vgPrb where id=%d and state='new'",probeid);
    if (sqlQuickNum(conn,dy->string)>0)
	{
	/* record exists and hasn't already been updated */

	int vgPrb = findVgPrbBySeq(conn,dna,taxon);
	
	if (vgPrb == 0)
	    {
	    dyStringClear(dy);
	    dyStringAppend(dy, "update vgPrb set");
	    dyStringAppend(dy, " seq='");
	    dyStringAppend(dy, dna);
	    dyStringAppend(dy, "',\n");
	    dyStringPrintf(dy, " tName='%s',\n", tName);
	    dyStringPrintf(dy, " tStart=%d,\n", tStart);
	    dyStringPrintf(dy, " tEnd=%d,\n", tEnd);
	    dyStringPrintf(dy, " tStrand='%s',\n", tStrand);
	    dyStringPrintf(dy, " db='%s',\n", db);
	    dyStringPrintf(dy, " state='%s'\n", "seq");
	    dyStringPrintf(dy, " where id=%d\n", probeid);
	    dyStringPrintf(dy, " and state='%s'\n", "new");
	    verbose(2, "%s\n", dy->string);
	    sqlUpdate(conn, dy->string);
	    }
	else  /* probe seq already exists */ 
	    { 
	    /* just re-map the probe table recs to it */
	    dyStringClear(dy);
	    dyStringPrintf(dy, "update vgPrbMap set vgPrb=%d where vgPrb=%d",vgPrb,probeid);
	    sqlUpdate(conn, dy->string);
	    /* and delete it from vgPrb */
	    dyStringClear(dy);
	    dyStringPrintf(dy, "delete from vgPrb where id=%d",probeid);
	    sqlUpdate(conn, dy->string);
	    }
	}
    
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

verbose(1,"bac list read done.\n");

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
    dyStringPrintf(dy, "select count(*) from vgPrb where id=%d and state='new'",bac->probe);
    if (sqlQuickNum(conn,dy->string)>0)
	{
	/* record exists and hasn't already been updated */

	int vgPrb = findVgPrbBySeq(conn,dna,taxon);
	
	if (vgPrb == 0)
	    {
	    dyStringClear(dy);
	    dyStringAppend(dy, "update vgPrb set");
	    dyStringAppend(dy, " seq='");
	    dyStringAppend(dy, dna);
	    dyStringAppend(dy, "',\n");
	    dyStringPrintf(dy, " tName='%s',\n", bac->chrom);
	    dyStringPrintf(dy, " tStart=%d,\n", bac->chromStart);
	    dyStringPrintf(dy, " tEnd=%d,\n", bac->chromEnd);
	    dyStringPrintf(dy, " tStrand='%s',\n", bac->strand);
	    dyStringPrintf(dy, " db='%s',\n", db);
	    dyStringPrintf(dy, " state='%s'\n", "seq");
	    dyStringPrintf(dy, " where id=%d\n", bac->probe);
	    dyStringPrintf(dy, " and state='%s'\n", "new");
	    //verbose(2, "%s\n", dy->string); // the sql string could be quite large
	    sqlUpdate(conn, dy->string);
	    }
	else  /* probe seq already exists */ 
	    { 
	    /* just re-map the probe table recs to it */
	    dyStringClear(dy);
	    dyStringPrintf(dy, "update vgPrbMap set vgPrb=%d where vgPrb=%d",vgPrb,bac->probe);
	    sqlUpdate(conn, dy->string);
	    /* and delete it from vgPrb */
	    dyStringClear(dy);
	    dyStringPrintf(dy, "delete from vgPrb where id=%d",bac->probe);
	    sqlUpdate(conn, dy->string);
	    }
	    
	++count; 
	
    
	verbose(2,"%d finished bac for probe id %d size %d\n", 
	    count, bac->probe, bac->chromEnd - bac->chromStart);
	}

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

dyStringClear(dy);
dyStringAppend(dy, "select e.id, p.fPrimer, p.rPrimer from probe p, vgPrbMap m, vgPrb e, gene g");
dyStringPrintf(dy, " where p.id = m.probe and m.vgPrb = e.id and g.id = p.gene and g.taxon = %d",taxon);
dyStringAppend(dy, " and e.state = 'new' and e.type='primersMrna'");
rc = sqlSaveQuery(conn, dy->string, "primers.query", FALSE);
verbose(1,"rc = %d = count of primers for mrna search for taxon %d\n",rc,taxon);

if (rc > 0) /* something to do */
    {

    dyStringClear(dy);
    dyStringPrintf(dy, "select qName from %s.all_mrna",db);
    rc = 0;
    rc = sqlSaveQuery(conn, dy->string, "accFile.txt", FALSE);
    safef(cmdLine,sizeof(cmdLine),"getRna %s accFile.txt mrna.fa",db);
    system("date"); verbose(1,"cmdLine: [%s]\n",cmdLine); system(cmdLine); system("date");
    
    verbose(1,"rc = %d = count of mrna for %s\n",rc,db);

    system("date"); system("isPcr mrna.fa primers.query isPcr.fa -out=fa"); system("date");
    system("ls -l");

    processIsPcr(conn,taxon,db);

    unlink("mrna.fa"); unlink("accFile.txt"); unlink("isPcr.fa");

    }
unlink("primers.query");    

/* find any remaining type primersMrna that couldn't be resolved and demote 
 * them to type primersGenome
 */
dyStringClear(dy);
dyStringAppend(dy, "update vgPrb set type='primersGenome'"); 
dyStringPrintf(dy, " where taxon = %d",taxon);
dyStringAppend(dy, " and state = 'new' and type='primersMrna'");
sqlUpdate(conn, dy->string);



/* get primers for those probes that did not find mrna isPcr matches 
 * and then do them against the genome instead */
dyStringClear(dy);
dyStringAppend(dy, "select e.id, p.fPrimer, p.rPrimer from probe p, vgPrbMap m, vgPrb e, gene g");
dyStringPrintf(dy, " where p.id = m.probe and m.vgPrb = e.id and g.id = p.gene and g.taxon = %d",taxon);
dyStringAppend(dy, " and e.state = 'new' and e.type='primersGenome'");
rc = 0;
rc = sqlSaveQuery(conn, dy->string, "primers.query", FALSE);
verbose(1,"rc = %d = count of primers for genome search for taxon %d\n",rc,taxon);

if (rc > 0) /* something to do */
    {
    safef(path1,sizeof(path1),"/gbdb/%s/%s.2bit",db,db);
    safef(path2,sizeof(path2),"%s/%s.2bit",getCurrentDir(),db);
    verbose(1,"copy: [%s] to [%s]\n",path1,path2);  copyFile(path1,path2);

    safef(cmdLine,sizeof(cmdLine),
	    "ssh kolossus 'cd %s; isPcr %s.2bit primers.query isPcr.fa -out=fa'",
	    getCurrentDir(),db);
    system("date"); verbose(1,"cmdLine: [%s]\n",cmdLine); system(cmdLine); system("date");
    safef(path2,sizeof(path2),"%s/%s.2bit",getCurrentDir(),db);
    verbose(1,"rm %s\n",path2); unlink(path2); system("ls -l");

    processIsPcr(conn,taxon,db);
    
    unlink("mrna.fa"); unlink("accFile.txt"); unlink("isPcr.fa");

    }
unlink("primers.query");    

/* find any remaining type primersGenome that couldn't be resolved and demote 
 * them to type refSeq
 */
dyStringClear(dy);
dyStringAppend(dy, "update vgPrb set type='refSeq'"); 
dyStringPrintf(dy, " where taxon = %d",taxon);
dyStringAppend(dy, " and state = 'new' and type='primersGenome'");
sqlUpdate(conn, dy->string);

dyStringFree(&dy);
}

static void doBacEndPairs(struct sqlConnection *conn, int taxon, char *db)
/* get probe seq bacEndPairs */
{
struct dyString *dy = dyStringNew(0);
int rc = 0;
/* fetch available sequence for bacEndPairs */
rc = doBacs(conn, taxon, db); verbose(1,"found seq for %d bacEndPairs\n",rc);

/* find any remaining type bac that couldn't be resolved and demote 
 * them to type refSeq
 */
dyStringClear(dy);

dyStringAppend(dy, "update vgPrb set type='refSeq'"); 
dyStringPrintf(dy, " where taxon = %d",taxon);
dyStringAppend(dy, " and state = 'new' and type='bac'");

sqlUpdate(conn, dy->string);

dyStringFree(&dy);
}



static void setTName(struct sqlConnection *conn, int taxon, char *db, char *type, char *fld)
/* set vgPrb.tName to the desired value to try, 
 *  e.g. gene.refSeq, gene.genbank, mm6.refFlat.name(genoName=gene.name). */
{
struct dyString *dy = dyStringNew(0);
dyStringClear(dy);
dyStringPrintf(dy, 
    "update vgPrb e, vgPrbMap m, probe p, gene g"
    " set e.seq = '', e.tName = g.%s"
    " where e.id = m.vgPrb and m.probe = p.id and p.gene = g.id"
    " and e.type = '%s'"
    " and e.state = 'new'"
    " and e.taxon = %d"
    , fld, type, taxon);
sqlUpdate(conn, dy->string);
dyStringFree(&dy);
}

static void setTNameMapped(struct sqlConnection *conn, int taxon, char *db, char *type, char *fld, 
            char *mapTable, char *mapField, char *mapTo)
/* set vgPrb.tName to the desired value to try, 
 *  e.g. gene.refSeq, gene.genbank, mm6.refFlat.name(genoName=gene.name). */
{
struct dyString *dy = dyStringNew(0);
dyStringClear(dy);
dyStringPrintf(dy, 
"update vgPrb e, vgPrbMap m, probe p, gene g, %s.%s f"
" set e.seq = '', e.tName = f.%s"
" where e.id = m.vgPrb and m.probe = p.id and p.gene = g.id"
" and g.%s = f.%s"
    " and e.type = '%s'"
    " and e.state = 'new'"
    " and g.taxon = %d"
    ,db, mapTable, mapTo, fld, mapField, type, taxon);
sqlUpdate(conn, dy->string);
dyStringFree(&dy);
}

static void processMrnaFa(struct sqlConnection *conn, int taxon, char *type, char *db)
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
    while((more=lineFileNext(lf, &line, &lineSize)))
	{
	if (line[0] == '>')
	    {
	    break;
	    }
	dyStringAppend(dy,line);	    
	}
    dna = cloneString(dy->string);

    while(1)
	{
	int oldProbe = 0;
	dyStringClear(dy);
	dyStringPrintf(dy, "select id from vgPrb "
	   "where taxon=%d and type='%s' and tName='%s' and state='new'",taxon,type,name);
	oldProbe = sqlQuickNum(conn,dy->string);
	if (oldProbe==0)
	    break;       /* no more records match */
	    
	/* record exists and hasn't already been updated */
	
	int vgPrb = findVgPrbBySeq(conn,dna,taxon);
	
	if (vgPrb == 0)
	    {
	    dyStringClear(dy);
	    dyStringAppend(dy, "update vgPrb set");
	    dyStringAppend(dy, " seq = '");
	    dyStringAppend(dy, dna);
	    dyStringAppend(dy, "',\n");
	    dyStringPrintf(dy, " db = '%s',\n", db);
	    dyStringAppend(dy, " state = 'seq'\n");
	    dyStringPrintf(dy, " where id=%d\n", oldProbe);
	    dyStringPrintf(dy, " and state='%s'\n", "new");
	    verbose(2, "%s\n", dy->string);
	    sqlUpdate(conn, dy->string);
	    }
	else  /* probe seq already exists */ 
	    { 
	    /* just re-map the probe table recs to it */
	    dyStringClear(dy);
	    dyStringPrintf(dy, "update vgPrbMap set vgPrb=%d where vgPrb=%d",vgPrb,oldProbe);
	    sqlUpdate(conn, dy->string);
	    /* and delete it from vgPrb */
	    dyStringClear(dy);
	    dyStringPrintf(dy, "delete from vgPrb where id=%d",oldProbe);
	    sqlUpdate(conn, dy->string);
	    }
	    
	}    

    freez(&name);
    freez(&dna);
    }
lineFileClose(&lf);

dyStringFree(&dy);
}



static int getAccMrnas(struct sqlConnection *conn, int taxon, char *db, char *type, char *table)
/* get mRNAs for one vgPrb acc type */
{
int rc = 0;
struct dyString *dy = dyStringNew(0);
char cmdLine[256];
dyStringClear(dy);
dyStringPrintf(dy, 
    "select distinct e.tName from vgPrb e, %s.%s m"
    " where e.tName = m.qName"
    " and e.taxon = %d and e.type = '%s' and e.tName <> '' and e.state = 'new'"
    ,db,table,taxon,type);
rc = 0;
rc = sqlSaveQuery(conn, dy->string, "accFile.txt", FALSE);
safef(cmdLine,sizeof(cmdLine),"getRna %s accFile.txt mrna.fa",db);
//system("date"); verbose(1,"cmdLine: [%s]\n",cmdLine); 
system(cmdLine); 
//system("date");

processMrnaFa(conn, taxon, type, db);

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
 "update vgPrb set type='%s', tName=''"
 " where taxon = %d and state = 'new' and type='%s'"
 ,newType,taxon,oldType);
sqlUpdate(conn, dy->string);
dyStringFree(&dy);
}

static void doAccessionsSeq(struct sqlConnection *conn, int taxon, char *db)
/* get probe seq from Accessions */
{
int rc = 0;
struct dyString *dy = dyStringNew(0);

/* get refSeq accessions and rna */
setTName(conn, taxon, db, "refSeq", "refSeq");
rc = getAccMrnas(conn, taxon, db, "refSeq", "refSeqAli");
verbose(1,"rc = %d = count of refSeq mrna for %s\n",rc,db);
advanceType(conn,taxon,"refSeq","genRef");

/* get refSeq-in-gene.genbank accessions and rna */
setTName(conn, taxon, db, "genRef", "genbank");
rc = getAccMrnas(conn, taxon, db, "genRef", "refSeqAli");
verbose(1,"rc = %d = count of genRef mrna for %s\n",rc,db);
advanceType(conn,taxon,"genRef","genbank");

/* get genbank accessions and rna */
setTName(conn, taxon, db, "genbank", "genbank");
rc = getAccMrnas(conn, taxon, db, "genbank", "all_mrna");
verbose(1,"rc = %d = count of genbank mrna for %s\n",rc,db);
advanceType(conn,taxon,"genbank","flatRef");

/* get gene.name -> refFlat to refSeq accessions and rna */
setTNameMapped(conn, taxon, db, "flatRef", "name", "refFlat", "geneName", "name");
rc = getAccMrnas(conn, taxon, db, "flatRef", "refSeqAli");
verbose(1,"rc = %d = count of flatRef mrna for %s\n",rc,db);
advanceType(conn,taxon,"flatRef","flatAll");

/* get gene.name -> refFlat to all_mrna accessions */
setTNameMapped(conn, taxon, db, "flatAll", "name", "refFlat", "geneName", "name");
rc = getAccMrnas(conn, taxon, db, "flatAll", "all_mrna");
verbose(1,"rc = %d = count of flatAll mrna for %s\n",rc,db);
advanceType(conn,taxon,"flatAll","linkRef");



/* get gene.name -> refLink to refSeq accessions and rna */
setTNameMapped(conn, taxon, db, "linkRef", "name", "refLink", "name", "mrnaAcc");
rc = getAccMrnas(conn, taxon, db, "linkRef", "refSeqAli");
verbose(1,"rc = %d = count of linkRef mrna for %s\n",rc,db);
advanceType(conn,taxon,"linkRef","linkAll");

/* get gene.name -> refLink to all_mrna accessions */
setTNameMapped(conn, taxon, db, "linkAll", "name", "refLink", "name", "mrnaAcc");
rc = getAccMrnas(conn, taxon, db, "linkAll", "all_mrna");
verbose(1,"rc = %d = count of linkAll mrna for %s\n",rc,db);
advanceType(conn,taxon,"linkAll","kgAlRef");



/* get gene.name -> kgAlias to refSeq accessions and rna */
setTNameMapped(conn, taxon, db, "kgAlRef", "name", "kgAlias", "alias", "kgId");
rc = getAccMrnas(conn, taxon, db, "kgAlRef", "refSeqAli");
verbose(1,"rc = %d = count of kgAlRef mrna for %s\n",rc,db);
advanceType(conn,taxon,"kgAlRef","kgAlAll");

/* get gene.name -> kgAlias to all_mrna accessions */
setTNameMapped(conn, taxon, db, "kgAlAll", "name", "kgAlias", "alias", "kgId");
rc = getAccMrnas(conn, taxon, db, "kgAlAll", "all_mrna");
verbose(1,"rc = %d = count of kgAlAll mrna for %s\n",rc,db);
advanceType(conn,taxon,"kgAlAll","gene");

dyStringFree(&dy);
}

static void initTable(struct sqlConnection *conn, char *table, boolean nuke)
/* build tables */
{
char *sql = NULL;
char path[256];
if (nuke)
    sqlDropTable(conn, table);
if (!sqlTableExists(conn, table))
    {
    safef(path,sizeof(path),"%s/%s.sql",sqlPath,table);
    readInGulp(path, &sql, NULL);
    sqlUpdate(conn, sql);
    }
}


static void populateMissingVgPrbAli(struct sqlConnection *conn, int taxon, char *db, char *table)
/* populate vgPrbAli for db */
{
struct dyString *dy = dyStringNew(0);
dyStringClear(dy);
dyStringPrintf(dy,
"insert into %s"
" select distinct '%s', e.id, 'new' from vgPrb e"
" left join %s a on e.id = a.vgPrb and a.db = '%s'"
" where a.vgPrb is NULL "
" and e.taxon = %d"
    ,table,db,table,db,taxon);
sqlUpdate(conn, dy->string);
dyStringFree(&dy);
}


static void updateVgPrbAli(struct sqlConnection *conn, char *db, char *table, char *track)
/* update vgPrbAli from vgProbes track for db */
{
struct dyString *dy = dyStringNew(0);
char dbTrk[256];
safef(dbTrk,sizeof(dbTrk),"%s.%s",db,track);
if (!sqlTableExists(conn, dbTrk))
    {
    struct sqlConnection *conn2 = sqlConnect(db);
    verbose(1,"FYI: Table %s does not exist\n",dbTrk);
    initTable(conn2, track, FALSE);
    sqlDisconnect(&conn2);
    }
dyStringClear(dy);
dyStringPrintf(dy,
"update %s a, %s.%s v"
" set a.status = 'ali'"
" where v.qName = concat('vgPrb_',a.vgPrb)"
" and a.db = '%s'"
    ,table,db,track,db);
sqlUpdate(conn, dy->string);
dyStringFree(&dy);
}


static void markNoneVgPrbAli(struct sqlConnection *conn, int fromTaxon, char *db, char *table)
/* mark records in vgPrbAli that did not find any alignment for db */
{
struct dyString *dy = dyStringNew(0);
dyStringClear(dy);
dyStringPrintf(dy,
"update %s a, vgPrb e"
" set a.status = 'none'"
" where a.status = 'new'"
" and a.db = '%s' and e.taxon = %d"
" and a.vgPrb = e.id"
    ,table,db,fromTaxon);
sqlUpdate(conn, dy->string);
dyStringFree(&dy);
}



static int doAccPsls(struct sqlConnection *conn, int taxon, char *db, char *type, char *table)
/* get psls for one vgPrb acc type */
{
int rc = 0;
struct dyString *dy = dyStringNew(0);
char outName[256];
dyStringClear(dy);
dyStringPrintf(dy, 
    "select m.matches,m.misMatches,m.repMatches,m.nCount,m.qNumInsert,m.qBaseInsert,"
    "m.tNumInsert,m.tBaseInsert,m.strand,"
    "concat(\"vgPrb_\",e.id),"
    "m.qSize,m.qStart,m.qEnd,m.tName,m.tSize,m.tStart,m.tEnd,m.blockCount,"
    "m.blockSizes,m.qStarts,m.tStarts"    
    " from vgPrb e, vgPrbAli a, %s.%s m"
    " where e.id = a.vgPrb and a.db = '%s' and a.status='new' and e.tName = m.qName"
    " and e.taxon = %d and e.type = '%s' and e.tName <> '' and e.state='seq' and e.seq <> ''"
    " order by m.tName,m.tStart"
    ,db,table,db,taxon,type);
rc = 0;
safef(outName,sizeof(outName),"%s.psl",type);
rc = sqlSaveQuery(conn, dy->string, outName, FALSE);
dyStringFree(&dy);
return rc;
}

static int dumpPslTable(struct sqlConnection *conn, char *db, char *table)
/* dump psls for db.table to table.psl in current (working) dir */
{
int rc = 0;
struct dyString *dy = dyStringNew(0);
char outName[256];
dyStringClear(dy);
dyStringPrintf(dy, 
    "select matches,misMatches,repMatches,nCount,qNumInsert,qBaseInsert,"
    "tNumInsert,tBaseInsert,strand,"
    "qName,qSize,qStart,qEnd,tName,tSize,tStart,tEnd,"
    "blockCount,blockSizes,qStarts,tStarts"    
    " from %s.%s"
    " order by tName,tStart"
    ,db,table);
rc = 0;
safef(outName,sizeof(outName),"%s.psl",table);
rc = sqlSaveQuery(conn, dy->string, outName, FALSE);
dyStringFree(&dy);
return rc;
}

static void doAccessionsAli(struct sqlConnection *conn, int taxon, char *db)
/* get probe alignments from Accessions */
{
int rc = 0;

/* get refSeq psls */
rc = doAccPsls(conn, taxon, db, "refSeq", "refSeqAli");
verbose(1,"rc = %d = count of refSeq psls for %s\n",rc,db);

/* get genRef psls */
rc = doAccPsls(conn, taxon, db, "genRef", "refSeqAli");
verbose(1,"rc = %d = count of genRef psls for %s\n",rc,db);

/* get genbank psls */
rc = doAccPsls(conn, taxon, db, "genbank", "all_mrna");
verbose(1,"rc = %d = count of genbank psls for %s\n",rc,db);

/* get flatRef psls */
rc = doAccPsls(conn, taxon, db, "flatRef", "refSeqAli");
verbose(1,"rc = %d = count of flatRef psls for %s\n",rc,db);

/* get flatAll psls */
rc = doAccPsls(conn, taxon, db, "flatAll", "all_mrna");
verbose(1,"rc = %d = count of flatAll psls for %s\n",rc,db);

/* get linkRef psls */
rc = doAccPsls(conn, taxon, db, "linkRef", "refSeqAli");
verbose(1,"rc = %d = count of linkRef psls for %s\n",rc,db);

/* get linkAll psls */
rc = doAccPsls(conn, taxon, db, "linkAll", "all_mrna");
verbose(1,"rc = %d = count of linkAll psls for %s\n",rc,db);

/* get kgAlRef psls */
rc = doAccPsls(conn, taxon, db, "kgAlRef", "refSeqAli");
verbose(1,"rc = %d = count of kgAlRef psls for %s\n",rc,db);

/* get kgAlAll psls */
rc = doAccPsls(conn, taxon, db, "kgAlAll", "all_mrna");
verbose(1,"rc = %d = count of kgAlRef psls for %s\n",rc,db);

}

static void doFakeBacAli(struct sqlConnection *conn, int taxon, char *db)
/* place probe seq from primers, etc with blat */
{
int rc = 0;
struct dyString *dy = dyStringNew(0);
/* create fake psls as blatBAC.psl */
dyStringClear(dy);
dyStringPrintf(dy, 
    "select length(e.seq), 0, 0, 0, 0, 0, 0, 0, e.tStrand, concat('vgPrb_',e.id), length(e.seq),"
    " 0, length(e.seq), e.tName, ci.size, e.tStart, e.tEnd, 1,"
    " concat(length(e.seq),','), concat(0,','), concat(e.tStart,',')"
    " from vgPrb e, %s.chromInfo ci, vgPrbAli a"
    " where ci.chrom = e.tName and e.id = a.vgPrb"
    " and e.type = 'bac'"
    " and e.taxon = %d"
    " and a.db = '%s' and a.status='new'"
    , db, taxon, db);
//restore: 
rc = sqlSaveQuery(conn, dy->string, "fakeBAC.psl", FALSE);
verbose(1,"rc = %d = count of sequences for fakeBAC.psl, for taxon %d\n",rc,taxon);
dyStringFree(&dy);
}

static void doBlat(struct sqlConnection *conn, int taxon, char *db)
/* place probe seq from non-BAC with blat that have no alignments yet */
{
int rc = 0;
char *blatSpec=NULL;
char cmdLine[256];
char path1[256];
char path2[256];
struct dyString *dy = dyStringNew(0);

/* (non-BACs needing alignment) */
dyStringClear(dy);
dyStringPrintf(dy, 
    "select concat(\"vgPrb_\",e.id), e.seq"
    " from vgPrb e, vgPrbAli a"
    " where e.id = a.vgPrb"
    " and a.db = '%s'"
    " and a.status = 'new'"
    " and e.taxon = %d"
    " and e.type <> 'bac'"
    " and e.seq <> ''"
    " order by e.id"
    , db, taxon);
//restore: 
rc = sqlSaveQuery(conn, dy->string, "blat.fa", TRUE);
verbose(1,"rc = %d = count of sequences for blat, to get psls for taxon %d\n",rc,taxon);

if (rc == 0) 
    {
    unlink("blat.fa");
    system("rm -f blatNearBest.psl; touch blatNearBest.psl");  /* make empty file */
    return;
    }

/* make .ooc and blat on kolossus */

safef(path1,sizeof(path1),"/gbdb/%s/%s.2bit",db,db);
safef(path2,sizeof(path2),"%s/%s.2bit",getCurrentDir(),db);
//restore: 
verbose(1,"copy: [%s] to [%s]\n",path1,path2);  copyFile(path1,path2);

safef(cmdLine,sizeof(cmdLine),
"ssh kolossus 'cd %s; blat -makeOoc=11.ooc -tileSize=11"
" -repMatch=1024 %s.2bit /dev/null /dev/null'",
    getCurrentDir(),db);
//restore: 
system("date"); verbose(1,"cmdLine: [%s]\n",cmdLine); system(cmdLine); system("date");

safef(cmdLine,sizeof(cmdLine),
	"ssh kolossus 'cd %s; blat %s.2bit blat.fa -ooc=11.ooc -noHead blat.psl'",
	getCurrentDir(),db);
//restore: 
system("date"); verbose(1,"cmdLine: [%s]\n",cmdLine); system(cmdLine); system("date");

/* using blat even with -fastMap was way too slow - took over a day,
 * so instead I will make a procedure to write a fake psl for the BACs
 * which you will see called below */

safef(path2,sizeof(path2),"%s.2bit",db);
verbose(1,"rm %s\n",path2); unlink(path2); 

safef(path2,sizeof(path2),"11.ooc");
verbose(1,"rm %s\n",path2); unlink(path2); 


/* skip psl header and sort on query name */
safef(cmdLine,sizeof(cmdLine), "sort -k 10,10 blat.psl > blatS.psl");
verbose(1,"cmdLine=[%s]\n",cmdLine);
system(cmdLine); 

/* keep near best within 5% of the best */
safef(cmdLine,sizeof(cmdLine), 
    "pslCDnaFilter -globalNearBest=0.005 -minId=0.96 -minNonRepSize=20 -minCover=0.50"
    " blatS.psl blatNearBest.psl");
verbose(1,"cmdLine=[%s]\n",cmdLine);
system(cmdLine); 

unlink("blat.fa");
unlink("blat.psl");
unlink("blatS.psl");

freez(&blatSpec);
dyStringFree(&dy);
}



void static assembleAllPsl(struct sqlConnection *conn, int taxon, char *db)
/* assemble NonBlat.psl from variouls psl alignments */
{
// " blatNearBest.psl" not included
/* make final psl */
system(
"cat"
" fakeBAC.psl"
" flatAll.psl"
" flatRef.psl"
" genRef.psl"
" genbank.psl"
" kgAlAll.psl"
" kgAlRef.psl"
" linkAll.psl"
" linkRef.psl"
" refSeq.psl"
" > vgPrbNonBlat.psl"
);

verbose(1,"vgPrbNonBlat.psl assembled for taxon %d\n",taxon);


//unlink("blatNearBest.psl");  
//unlink("fakeBAC.psl");  

//system("ls -ltr");

}


static void rollupPsl(char *pslName, char *table, struct sqlConnection *conn, char *db)
{
char cmd[256];
char dbTbl[256];

if (fileSize(pslName)==0)
    return;

safef(dbTbl,sizeof(dbTbl),"%s.%s",db,table);
if (!sqlTableExists(conn, dbTbl))
    {
    verbose(1,"FYI: Table %s does not exist\n",dbTbl);
    safef(cmd,sizeof(cmd),"rm -f %s.psl; touch %s.psl",table,table); /* make empty file */
    verbose(1,"%s\n",cmd); system(cmd);
    }
else
    {
    dumpPslTable(conn, db, table);
    }

safef(cmd,sizeof(cmd),"cat %s %s.psl | sort -u | sort -k 10,10 > %sNew.psl", pslName, table, table);
verbose(1,"%s\n",cmd); system(cmd);

safef(cmd,sizeof(cmd),"hgLoadPsl %s %sNew.psl -table=%s",db,table,table);
verbose(1,"%s\n",cmd); system(cmd);

safef(cmd,sizeof(cmd),"rm %s %s.psl %sNew.psl",pslName,table,table);
verbose(1,"%s\n",cmd); 
//system(cmd);

}


static int findTaxon(struct sqlConnection *conn, char *db)
/* return the taxon for the given db, or abort */
{
char sql[256];
int taxon = 0;
char *fmt = "select ncbi_taxa_id from go.species "
"where concat(genus,' ',species) = '%s'";
safef(sql,sizeof(sql),fmt,hScientificName(db));
taxon = sqlQuickNum(conn, sql);
if (taxon == 0) 
    {
    if (sameString(db,"nibb"))   /* we don't really have this frog as an assembly */
	taxon = 8355;    /* Xenopus laevis - African clawed frog */
    /* can put more here in future */    
    }
if (taxon == 0)
   errAbort("unknown taxon for db = %s, unable to continue until its taxon is defined.",db);
return taxon;
}


static void makeFakeProbeSeq(struct sqlConnection *conn, char *db)
/* get probe seq from primers, bacEndPairs, refSeq, genbank, gene-name */
{

int taxon = findTaxon(conn,db);

//restore: 
doPrimers(conn, taxon, db);

//restore: 
doBacEndPairs(conn, taxon, db);

//restore: 
doAccessionsSeq(conn, taxon, db);

}

/*  keep around, but dangerous in the wrong hands ;)
static void init(struct sqlConnection *conn)
/ * build tables - for the first time * /
{
if (!sqlTableExists(conn, "vgPrb"))
    {
    initTable(conn, "vgPrb", FALSE);  / * this most important table should never be nuked automatically * /
    sqlUpdate(conn, "create index tName on vgPrb(tName(20));");
    sqlUpdate(conn, "create index seq on vgPrb(seq(40));");
    }

initTable(conn, "vgPrbMap", TRUE);
sqlUpdate(conn, "create index probe on vgPrbMap(probe);");
sqlUpdate(conn, "create index vgPrb on vgPrbMap(vgPrb);");

initTable(conn, "vgPrbAli", TRUE);
initTable(conn, "vgPrbAliAll", TRUE);

}
*/


static void doAlignments(struct sqlConnection *conn, char *db)
{
int taxon = findTaxon(conn,db);

populateMissingVgPrbAli(conn, taxon, db, "vgPrbAli");

updateVgPrbAli(conn, db, "vgPrbAli","vgProbes");

doAccessionsAli(conn, taxon, db);

doFakeBacAli(conn, taxon, db);

assembleAllPsl(conn, taxon, db);

rollupPsl("vgPrbNonBlat.psl", "vgProbes", conn, db);

updateVgPrbAli(conn, db, "vgPrbAli","vgProbes");

doBlat(conn, taxon, db);   

rollupPsl("blatNearBest.psl", "vgProbes", conn, db);

updateVgPrbAli(conn, db, "vgPrbAli","vgProbes");

markNoneVgPrbAli(conn, taxon, db, "vgPrbAli");

}


static void getPslMapAli(struct sqlConnection *conn, 
    char *db, int fromTaxon, char *fromDb, boolean isBac)
{
int rc = 0;
struct dyString *dy = dyStringNew(0);
char outName[256];
/* get {non-}bac $db.vgProbes not yet aligned */
dyStringClear(dy);
dyStringPrintf(dy, 
    "select m.matches,m.misMatches,m.repMatches,m.nCount,m.qNumInsert,m.qBaseInsert,"
    "m.tNumInsert,m.tBaseInsert,m.strand,"
    "m.qName,m.qSize,m.qStart,m.qEnd,m.tName,m.tSize,m.tStart,m.tEnd,m.blockCount,"
    "m.blockSizes,m.qStarts,m.tStarts"    
    " from vgPrb e, vgPrbAliAll a, %s.vgProbes m"
    " where e.id = a.vgPrb and a.db = '%s' and a.status='new'"
    " and m.qName = concat(\"vgPrb_\",e.id)"
    " and e.taxon = %d and e.type %s 'bac' and e.state='seq' and e.seq <> ''"
    " order by m.tName,m.tStart"
    ,fromDb,db,fromTaxon, isBac ? "=" : "<>");
rc = 0;
safef(outName,sizeof(outName), isBac ? "bac.psl" : "nonBac.psl");
rc = sqlSaveQuery(conn, dy->string, outName, FALSE);
verbose(1,"Count of %s Psls found for pslMap: %d\n", isBac ? "bac" : "nonBac", rc);
}


static void getPslMapFa(struct sqlConnection *conn, 
    char *db, int fromTaxon)
{
int rc = 0;
struct dyString *dy = dyStringNew(0);
/* get .fa for pslRecalcMatch use */
dyStringClear(dy);
dyStringPrintf(dy, 
    "select concat(\"vgPrb_\",e.id), e.seq"
    " from vgPrb e, vgPrbAliAll a"
    " where e.id = a.vgPrb"
    " and a.db = '%s'"
    " and a.status = 'new'"
    " and e.taxon = %d"
    " and e.seq <> ''"
    " order by e.id"
    , db, fromTaxon);
//restore: 
rc = sqlSaveQuery(conn, dy->string, "pslMap.fa", TRUE);
verbose(1,"rc = %d = count of sequences for pslMap for taxon %d\n",rc,fromTaxon);
}

static void doPslMapAli(struct sqlConnection *conn, 
    int taxon, char *db, 
    int fromTaxon, char *fromDb)
{
char cmd[256];

struct dyString *dy = dyStringNew(0);
char path[256];
char toDb[12];

safef(toDb,sizeof(toDb),"%s", db);
toDb[0]=toupper(toDb[0]);

safef(path,sizeof(path),"/cluster/data/%s/nib", db);
if (!fileExists(path))
    errAbort("unable to locate nib dir %s",path);
    
safef(path,sizeof(path),"/gbdb/%s/liftOver/%sTo%s.over.chain.gz", fromDb, fromDb, toDb);
if (!fileExists(path))
    errAbort("unable to locate chain file %s",path);

/* get non-bac $db.vgProbes not yet aligned */
getPslMapAli(conn, db, fromTaxon, fromDb, FALSE);
/* get bac $db.vgProbes not yet aligned */
getPslMapAli(conn, db, fromTaxon, fromDb, TRUE);
/* get .fa for pslRecalcMatch use */
getPslMapFa(conn, db, fromTaxon);

/* non-bac */
safef(cmd,sizeof(cmd),
"zcat %s | pslMap -chainMapFile -swapMap  nonBac.psl stdin stdout "
"|  sort -k 14,14 -k 16,16n > unscoredNB.psl"
,path);
verbose(1,"%s\n",cmd); system(cmd);

safef(cmd,sizeof(cmd),
"pslRecalcMatch unscoredNB.psl /cluster/data/%s/nib" 
" pslMap.fa nonBac.psl"
,db);
verbose(1,"%s\n",cmd); system(cmd);

/* bac */
safef(cmd,sizeof(cmd),
"zcat %s | pslMap -chainMapFile -swapMap  bac.psl stdin stdout "
"|  sort -k 14,14 -k 16,16n > unscoredB.psl"
,path);
verbose(1,"%s\n",cmd); system(cmd);

safef(cmd,sizeof(cmd),
"pslRecalcMatch unscoredB.psl /cluster/data/%s/nib" 
" pslMap.fa bacTemp.psl"
,db);
verbose(1,"%s\n",cmd); system(cmd);

safef(cmd,sizeof(cmd),
"pslCDnaFilter -globalNearBest=0.00001 -minCover=0.05"
" bacTemp.psl bac.psl");
verbose(1,"%s\n",cmd); system(cmd);

safef(cmd,sizeof(cmd),"cat bac.psl nonBac.psl > vgPrbPslMap.psl");
verbose(1,"%s\n",cmd); system(cmd);

dyStringFree(&dy);

}

static void doAlignmentsPslMap(struct sqlConnection *conn, char *db, char *fromDb)
{
int taxon = findTaxon(conn,db);
int fromTaxon = findTaxon(conn,fromDb);

populateMissingVgPrbAli(conn, fromTaxon, db, "vgPrbAliAll");

updateVgPrbAli(conn, db, "vgPrbAliAll","vgAllProbes");

doPslMapAli(conn, taxon, db, fromTaxon, fromDb);

rollupPsl("vgPrbPslMap.psl", "vgAllProbes", conn, db);

updateVgPrbAli(conn, db, "vgPrbAliAll","vgAllProbes");

markNoneVgPrbAli(conn, fromTaxon, db, "vgPrbAliAll");

}

static void doReMapAli(struct sqlConnection *conn, 
    int taxon, char *db, 
    int fromTaxon, char *fromDb,
    char *track, char *fasta
    )
/* re-map anything in track specified that is not aligned, 
      nor even attempted yet, using specified fasta file. */
{
char cmd[256];

int rc = 0;
struct dyString *dy = dyStringNew(0);
char dbTrk[256];

safef(dbTrk,sizeof(dbTrk),"%s.%s",db,track);
if (!sqlTableExists(conn, dbTrk))
    errAbort("Track %s does not exist\n",dbTrk);

if (!fileExists(fasta))
    errAbort("Unable to locate fasta file %s",fasta);

if (sqlTableExists(conn, "vgRemapTemp"))
    {
    sqlUpdate(conn, "drop table vgRemapTemp;");
    }

safef(cmd,sizeof(cmd),
"hgPepPred %s generic vgRemapTemp %s "
,database,fasta);
verbose(1,"%s\n",cmd); system(cmd);

sqlUpdate(conn, "create index seq on vgRemapTemp(seq(40));");

/* get remapped psl probes not yet aligned */
dyStringClear(dy);
dyStringPrintf(dy, 
    "select m.matches,m.misMatches,m.repMatches,m.nCount,m.qNumInsert,m.qBaseInsert,"
    "m.tNumInsert,m.tBaseInsert,m.strand,"
    "concat('vgPrb_',e.id),m.qSize,m.qStart,m.qEnd,m.tName,m.tSize,m.tStart,m.tEnd,m.blockCount,"
    "m.blockSizes,m.qStarts,m.tStarts"    
    " from vgPrb e, vgPrbAliAll a, %s.%s m, vgRemapTemp n"
    " where e.id = a.vgPrb and a.db = '%s' and a.status='new'"
    " and m.qName = n.name and n.seq = lower(e.seq)"
    " and e.taxon = %d and e.state='seq' and e.seq <> ''"
    " order by m.tName,m.tStart"
    ,db,track,db,fromTaxon);
rc = 0;
rc = sqlSaveQuery(conn, dy->string, "vgPrbReMap.psl", FALSE);
verbose(1,"Count of Psls found for reMap: %d\n", rc);

sqlUpdate(conn, "drop table vgRemapTemp;");

dyStringFree(&dy);

}


static void doAlignmentsReMap(
    struct sqlConnection *conn, 
    char *db, char *fromDb, char *track, char *fasta)
/* re-map anything in track specified that is not aligned, 
      nor even attempted yet, using specified fasta file. */
{
int taxon = findTaxon(conn,db);
int fromTaxon = findTaxon(conn,fromDb);

populateMissingVgPrbAli(conn, fromTaxon, db, "vgPrbAliAll");

updateVgPrbAli(conn, db, "vgPrbAliAll","vgAllProbes");

doReMapAli(conn, taxon, db, fromTaxon, fromDb, track, fasta);

rollupPsl("vgPrbReMap.psl", "vgAllProbes", conn, db);

updateVgPrbAli(conn, db, "vgPrbAliAll","vgAllProbes");

markNoneVgPrbAli(conn, fromTaxon, db, "vgPrbAliAll");

}

static void doSelfMapAli(struct sqlConnection *conn, 
    int taxon, char *db)
{
char cmd[256];

/* get non-bac $db.vgProbes not yet aligned */
getPslMapAli(conn, db, taxon, db, FALSE);
/* get bac $db.vgProbes not yet aligned */
getPslMapAli(conn, db, taxon, db, TRUE);

safef(cmd,sizeof(cmd),"cat bac.psl nonBac.psl > vgPrbSelfMap.psl");
verbose(1,"%s\n",cmd); system(cmd);

}

static void doAlignmentsSelfMap(
    struct sqlConnection *conn, char *db)
/* copy anything in vgProbes but not in vgAllProbes to vgAllProbes */
{
int taxon = findTaxon(conn,db);

populateMissingVgPrbAli(conn, taxon, db, "vgPrbAliAll");

updateVgPrbAli(conn, db, "vgPrbAliAll","vgAllProbes");

doSelfMapAli(conn, taxon, db);

rollupPsl("vgPrbSelfMap.psl", "vgAllProbes", conn, db);

updateVgPrbAli(conn, db, "vgPrbAliAll","vgAllProbes");

markNoneVgPrbAli(conn, taxon, db, "vgPrbAliAll");

}


static void doSeqAndExtFile(struct sqlConnection *conn, char *db, char *table)
{
int rc = 0;
char cmd[256];
char path[256];
char bedPath[256];
char gbdbPath[256];
char *fname=NULL;
struct dyString *dy = dyStringNew(0);
dyStringClear(dy);
dyStringPrintf(dy, 
"select distinct concat('vgPrb_',e.id), e.seq"
" from vgPrb e join %s.%s v"
" left join %s.seq s on s.acc = v.qName"
" where concat('vgPrb_',e.id) = v.qName"
" and s.acc is NULL"
" order by e.id"
    , db, table, db);
rc = sqlSaveQuery(conn, dy->string, "vgPrbExt.fa", TRUE);
verbose(1,"rc = %d = count of sequences for vgPrbExt.fa, to use with %s track %s\n",rc,db,table);
if (rc > 0)  /* can set any desired minimum */
    {
    safef(bedPath,sizeof(bedPath),"/cluster/data/%s/bed/visiGene/",db);
    if (!fileExists(bedPath))
	{
	safef(cmd,sizeof(cmd),"mkdir %s",bedPath);
	verbose(1,"%s\n",cmd); system(cmd);
	}
    
    safef(gbdbPath,sizeof(gbdbPath),"/gbdb/%s/visiGene/",db);
    if (!fileExists(gbdbPath))
	{
	safef(cmd,sizeof(cmd),"mkdir %s",gbdbPath);
    	verbose(1,"%s\n",cmd); system(cmd);
	}
   
    while(1)
	{
	int i=0;
	safef(path,sizeof(path),"%svgPrbExt_AAAAAA.fa",bedPath);
        char *c = rStringIn("AAAAAA",path);
        srand( (unsigned)time( NULL ) );
        for(i=0;i<6;++i)
            {
            *c++ += (int) 26 * (rand() / (RAND_MAX + 1.0));
            }
	if (!fileExists(path))
	    break;
	}

    
    safef(cmd,sizeof(cmd),"cp vgPrbExt.fa %s",path);
    verbose(1,"%s\n",cmd); system(cmd);
    
    fname = rStringIn("/", path);
    ++fname;
    
    safef(cmd,sizeof(cmd),"ln -s %s %s%s",path,gbdbPath,fname);
    verbose(1,"%s\n",cmd); system(cmd);
    
    safef(cmd,sizeof(cmd),"hgLoadSeq %s %s%s", db, gbdbPath,fname);
    verbose(1,"%s\n",cmd); system(cmd);
    }

dyStringFree(&dy);
}


int main(int argc, char *argv[])
/* Process command line. */
{
struct sqlConnection *conn = NULL;
char *command = NULL;
optionInit(&argc, argv, options);
database = optionVal("database", database);
sqlPath = optionVal("sqlPath", sqlPath);
if (argc < 2)
    usage();
command = argv[1];
if (argc >= 3)
    setCurrentDir(argv[2]);
conn = sqlConnect(database);
if (sameWord(command,"INIT"))
    {
    if (argc != 2)
	usage();
    errAbort("INIT is probably too dangerous. DO NOT USE.");
    /*	    
    init(conn);	    
    */
    }
else if (sameWord(command,"POP"))
    {
    if (argc != 2)
	usage();
    /* populate vgPrb where missing */
    populateMissingVgPrb(conn);
    }
else if (sameWord(command,"SEQ"))
    {
    if (argc != 4)
	usage();
    /* make fake probe sequences */
    makeFakeProbeSeq(conn,argv[3]);
    }
else if (sameWord(command,"ALI"))
    {
    if (argc != 4)
	usage();
    /* blat anything left that is not aligned, 
      nor even attempted */
    doAlignments(conn,argv[3]);
    }
else if (sameWord(command,"EXT"))
    {
    if (argc != 4)
	usage();
    /* update seq and extfile as necessary */
    doSeqAndExtFile(conn,argv[3],"vgProbes");
    }
else if (sameWord(command,"PSLMAP"))
    {
    if (argc != 5)
	usage();
    /* pslMap anything left that is not aligned, 
      nor even attempted */
    doAlignmentsPslMap(conn,argv[3],argv[4]);
    }
else if (sameWord(command,"REMAP"))
    {
    if (argc != 7)
	usage();
    /* re-map anything in track specified that is not aligned, 
      nor even attempted yet, using specified fasta file. */
    doAlignmentsReMap(conn,argv[3],argv[4],argv[5],argv[6]);
    }
else if (sameWord(command,"SELFMAP"))
    {
    if (argc != 4)
	usage();
    /* re-map anything in track specified that is not aligned, 
      nor even attempted yet, using specified fasta file. */
    doAlignmentsSelfMap(conn,argv[3]);
    }
else if (sameWord(command,"EXTALL"))
    {
    if (argc != 4)
	usage();
    /* update seq and extfile as necessary */
    doSeqAndExtFile(conn,argv[3],"vgAllProbes");
    }
else
    usage();
sqlDisconnect(&conn);
return 0;
}
