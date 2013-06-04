/* vgLoadMahoney - Convert Paul Gray/Mahoney in situs into something 
 * visiGeneLoad can handle. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "jksql.h"
#include "mahoney.h"
#include "jpegSize.h"
#include "dnautil.h"

/* Globals set from the command line. */
FILE *conflictLog = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "vgLoadMahoney - Convert Paul Gray/Mahoney in situs into something\n"
  "visiGeneLoad can handle\n"
  "usage:\n"
  "   vgLoadMahoney /gbdb/visiGene mm5 input.tab pcr.bed outDir\n"
  "options:\n"
  "   conflictLog=fileName - put info on conflict resolution here\n"
  );
}

static struct optionSpec options[] = {
   {"conflictLog", OPTION_STRING},
   {NULL, 0},
};

void raCommon(FILE *f)
/* Write parts of ra common for whole mounts and slices" */
{
fprintf(f, "%s",
"submissionSource Mahoney Lab\n"
"acknowledgement Thanks to Paul Gray for transferring the images.\n"
"taxon 10090\n"
"strain C57BL\n"
"genotype wild type\n"
"contributor Gray P.A., Fu H., Luo P., Zhao Q., Yu J., Ferrari A., Tenzen T., Yuk D., Tsung E.F., Cai Z., Alberta J.A., Cheng L., Liu Y., Stenman J.M., Valerius M.T., Billings N., Kim H.A., Greenberg M.E., McMahon A.P., Rowitch D.H., Stiles C.D., Ma Q.\n"
"publication Mouse brain organization revealed through direct genome-scale TF expression analysis\n"
"pubUrl http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?cmd=Retrieve&db=pubmed&dopt=Abstract&list_uids=15618518\n"
"year 2004\n"
"journal Science\n"
"journalUrl http://www.sciencemag.org/\n"
"setUrl http://mahoney.chip.org/mahoney/\n"
"itemUrl http://mahoney.chip.org/servlet/Mahoney2.ShowTF?mTF_ID=%s\n"
"fixation 4% paraformaldehyde\n"
"probeColor purple\n"
);
}

void wholeMountRa(char *fileName)
/* Output whole mount ra file. */
{
FILE *f = mustOpen(fileName, "w");

fprintf(f, "%s",
"submitSet mahoneyWhole01\n"
"fullDir ../visiGene/full/inSitu/Mouse/mahoney/wholeMount\n"
"thumbDir ../visiGene/200/inSitu/Mouse/mahoney/wholeMount\n"
"priority 50\n"
"age 10.5\n"
"minAge 10\n"
"maxAge 11\n"
"bodyPart whole\n"
"sliceType whole mount\n"
);

raCommon(f);
carefulClose(&f);
}

void slicesRa(char *fileName)
/* Output slices ra file. */
{
FILE *f = mustOpen(fileName, "w");

fprintf(f, "%s",
"submitSet mahoneySlices01\n"
"fullDir ../visiGene/full/inSitu/Mouse/mahoney/slices\n"
"thumbDir ../visiGene/200/inSitu/Mouse/mahoney/slices\n"
"priority 1500\n"
"bodyPart head\n"
"sliceType transverse\n"
"embedding Cryosection\n"
);

raCommon(f);
carefulClose(&f);
}

char *trim(char *s)
/* Trim spaces and leading question marks. */
{
s = trimSpaces(s);
while (*s == '?')
   ++s;
return s;
}

void trimAll(struct mahoney *mahoneyList)
/* Trim all names in list. */
{
struct mahoney *m;
for (m = mahoneyList; m != NULL; m = m->next)
    {
    m->genbank = trim(m->genbank);
    m->locusId = trim(m->locusId);
    m->geneName = trim(m->geneName);
    m->fPrimer = trim(m->fPrimer);
    m->rPrimer = trim(m->rPrimer);
    }
}


boolean anyGeneId(struct mahoney *m)
/* Return true if any gene ID */
{
if (m->geneName[0] == 0 && m->genbank[0] == 0)
    return FALSE;
else
    return TRUE;
}

void wholeMountTab(struct slName *inList, char *inFull, 
	struct hash *mahoneyHash, char *outFile)
/* Output tab-delimited info on each item in inList. */
{
struct slName *in;
FILE *f = mustOpen(outFile, "w");
fprintf(f, "#fileName\tsubmitId\tgene\tlocusLink\trefSeq\tgenbank\tfPrimer\trPrimer\tpermeablization\timageWidth\timageHeight\n");
for (in = inList; in != NULL; in = in->next)
    {
    char hex[8];
    char mtf[5];
    char treatment;
    int id;
    struct mahoney *m;
    int imageWidth = 0, imageHeight = 0;
    char path[PATH_LEN]; 
    strncpy(mtf, in->name, 4);
    mtf[4] = 0;
    treatment = in->name[4];
    id = atoi(mtf);
    safef(hex, sizeof(hex), "%x", id);
    m = hashFindVal(mahoneyHash, hex);
    if (m == NULL)
        warn("Can't find %d (%s) in hash while processing %s", id, mtf, in->name);
    else
	{
	if (anyGeneId(m))
	    {
	    fprintf(f, "%s\t%d\t", in->name, id);
	    fprintf(f, "%s\t", m->geneName);
	    if (startsWith("NM_", m->genbank))
		{
		fprintf(f, "%s\t", m->locusId);
		fprintf(f, "%s\t%s\t", m->genbank, m->genbank);
		}
	    else
		{
		fprintf(f, "\t");
		fprintf(f, "\t%s\t", m->genbank);
		}
	    fprintf(f, "%s\t", m->fPrimer);
	    fprintf(f, "%s\t", m->rPrimer);
	    if (treatment == 'a')
		fprintf(f, "3 min proteinase K\t");
	    else if (treatment == 'b')
		fprintf(f, "30 min proteinase K\t");
	    else if (treatment == 'd')
		fprintf(f, "3 min proteinase K/30 min proteinase K\t");
	    else if (treatment == 'c' || treatment == '.')
		fprintf(f, "\t"); /* do nothing*/
	    else
		errAbort("Unrecognized treatment %c in %s", treatment, in->name);
	    safef(path, sizeof(path), "%s/%s", inFull, in->name);
	    jpegSize(path,&imageWidth,&imageHeight);
	    fprintf(f, "%d\t", imageWidth);
	    fprintf(f, "%d", imageHeight);
	    fprintf(f, "\n");
	    }
	}
    }
carefulClose(&f);
}

void processWholeMounts(struct hash *mahoneyHash, 
	char *inFull, char *outDir)
/* Output database files for whole mounts. */
{
struct slName *imageFileList = NULL, *imageFile;
char outRa[PATH_LEN], outTab[PATH_LEN];

imageFileList = listDir(inFull, "*.jpg");
safef(outRa, sizeof(outRa), "%s/whole.ra", outDir);
wholeMountRa(outRa);
safef(outTab, sizeof(outRa), "%s/whole.tab", outDir);
wholeMountTab(imageFileList, inFull, mahoneyHash, outTab);
}

void slicesTab(struct slName *inList, char *inFull,
	struct hash *mahoneyHash, char *outFile)
/* Output tab-delimited info on each item in inList. */
{
struct slName *in;
FILE *f = mustOpen(outFile, "w");
char lastStage = 0;
fprintf(f, "#fileName\tsubmitId\tgene\tlocusLink\trefSeq\tgenbank\tfPrimer\trPrimer\tage\tminAge\tmaxAge\tsectionSet\tsectionIx\timageWidth\timageHeight\n");
for (in = inList; in != NULL; in = in->next)
    {
    char hex[8];
    char mtf[5];
    char stage = '1', sliceNo, nextSliceNo = '1';
    int id;
    struct mahoney *m;
    int imageWidth = 0, imageHeight = 0;
    char path[PATH_LEN]; 
    strncpy(mtf, in->name+1, 4);
    mtf[4] = 0;
    id = atoi(mtf);
    safef(hex, sizeof(hex), "%x", id);
    m = hashFindVal(mahoneyHash, hex);
    if (m == NULL)
        warn("Can't find %d (%s) in hash while processing %s", id, mtf, in->name);
    else
        {
	if (anyGeneId(m))
	    {
	    stage = in->name[5];
	    sliceNo = in->name[6];
	    fprintf(f, "%s\t%d\t", in->name, id);
	    fprintf(f, "%s\t", m->geneName);
	    if (startsWith("NM_", m->genbank))
		{
		fprintf(f, "%s\t", m->locusId);
		fprintf(f, "%s\t%s\t", m->genbank, m->genbank);
		}
	    else
		{
		fprintf(f, "\t");
		fprintf(f, "\t%s\t", m->genbank);
		}
	    fprintf(f, "%s\t", m->fPrimer);
	    fprintf(f, "%s\t", m->rPrimer);
	    if (stage == '1')
		fprintf(f, "13.5\t13\t14\t");
	    else if (stage == '2')
		fprintf(f, "19\t19\t20\t");
	    else
		errAbort("Unrecognized stage %c in %s", stage, in->name);
	    fprintf(f, "0\t0\t");
	    safef(path, sizeof(path), "%s/%s", inFull, in->name);
	    jpegSize(path,&imageWidth,&imageHeight);
	    fprintf(f, "%d\t", imageWidth);
	    fprintf(f, "%d", imageHeight);
	    fprintf(f, "\n");
	    }
	}
    }
carefulClose(&f);
}



void processSlices(struct hash *mahoneyHash, 
	char *inFull, char *outDir)
/* Output database files for slices. */
{
struct slName *imageFileList = NULL, *imageFile;
char outRa[PATH_LEN], outTab[PATH_LEN];

imageFileList = listDir(inFull, "*.jpg");
safef(outRa, sizeof(outRa), "%s/slices.ra", outDir);
slicesRa(outRa);
safef(outTab, sizeof(outRa), "%s/slices.tab", outDir);
slicesTab(imageFileList, inFull, mahoneyHash, outTab);
}

void fixPrimers(struct mahoney *el)
/* Filter out bogus looking primers. */
{
if (!isDna(el->fPrimer, strlen(el->fPrimer)))
    {
    static struct hash *uniqHash = NULL;
    if (uniqHash == NULL)
        uniqHash = hashNew(0);
    if (!hashLookup(uniqHash, el->fPrimer))
        {
	hashAdd(uniqHash, el->fPrimer, NULL);
	verbose(1, "Suppressing 'primer' %s\n", el->fPrimer);
	el->fPrimer = cloneString("");
	}
    }
if (!isDna(el->rPrimer, strlen(el->rPrimer)))
    errAbort("Not a primer: %s", el->rPrimer);
}

struct hash *hashMahoneys(struct mahoney *list)
/* Put list of mahoneys into hash keyed by mahoney id. */
{ 
struct hash *hash = hashNew(0);
struct mahoney *el;
for (el = list; el != NULL; el = el->next)
    {
    char hex[8];
    touppers(el->genbank);
    safef(hex, sizeof(hex), "%x", el->mtf);
    hashAdd(hash, hex, el);
    }
return hash;
}

struct mahoney *vgLoadMahoneyList(char *fileName)
/* Load data from tab-separated file (not whitespace separated) */
{
struct mahoney *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[MAHONEY_NUM_COLS];
while (lineFileNextRowTab(lf, row, ArraySize(row)))
    {
    el = mahoneyLoad(row);
    fixPrimers(el);
    slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

struct refSeqInfo
/* Selected info from refLink table. */
    {
    char *refSeq;	/* Not allocated here */
    char *geneName;
    int locusLink;
    };

struct hash *refGeneToNameHash(char *database)
/* Create hash keyed by refSeq accession with refSeqInfo as value */
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
char **row;
struct hash *hash = newHash(17);

sr = sqlGetResult(conn, "NOSQLINJ select mrnaAcc,name,locusLinkId from refLink where name != ''");
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct refSeqInfo *rsi;
    AllocVar(rsi);
    rsi->geneName = cloneString(row[1]);
    rsi->locusLink = atoi(row[2]);
    hashAddSaveName(hash, row[0], rsi, &rsi->refSeq);
    }
sqlDisconnect(&conn);
return hash;
}

struct hash *locusLinkToRefSeq(char *database)
/* Create hash keyed by locusLink with refSeqInfo values. */
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
char **row;
struct hash *hash = newHash(17);

sr = sqlGetResult(conn, "NOSQLINJ select mrnaAcc,name,locusLinkId from refLink where locusLinkId != 0");
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct refSeqInfo *rsi;
    AllocVar(rsi);
    rsi->refSeq = cloneString(row[0]);
    rsi->geneName = cloneString(row[1]);
    rsi->locusLink = atoi(row[2]);
    hashAdd(hash, row[2], rsi);
    }
sqlDisconnect(&conn);
return hash;
}


struct rnaList
/* Holds info on a list of RNA. */
    {
    struct slName *accList;
    };

struct hash *mtfToRnaHash(char *pcrBed)
/* Make hash keyed by numeric part of MTF id with values
 * of rnaList type */
{
struct rnaList *rl;
struct hash *hash = hashNew(0);
struct lineFile *lf = lineFileOpen(pcrBed, TRUE);
char *row[6];

while (lineFileRow(lf, row))
    {
    char *mtf = row[3] + 3;
    char *rna = row[0];
    struct slName *el;
    rl = hashFindVal(hash, mtf);
    if (rl == NULL)
        {
	AllocVar(rl);
	hashAdd(hash, mtf, rl);
	}
    chopSuffix(rna);
    el = slNameNew(rna);
    slAddHead(&rl->accList, el);
    }
lineFileClose(&lf);
return hash;
}

char *findRefSeqInList(struct rnaList *rl, char *preferred)
/* Find refSeq in list, matching preferred if possible. */
{
struct slName *el;

if (preferred != NULL)
    {
    for (el = rl->accList; el != NULL; el = el->next)
	{
	if (sameString(el->name, preferred))
	    return el->name;
	}
    }
for (el = rl->accList; el != NULL; el = el->next)
    {
    if (startsWith("NM_", el->name))
	return el->name;
    }
return NULL;
}

boolean mahoneyNameAgrees(char *mName, char *name)
/* Return TRUE if mahoney name agrees with name. */
{
if (mName == NULL || mName[0] == 0 || name == NULL || name[0] == 0)
    return FALSE;
else
    {
    char *mNameDupe = cloneString(mName);
    char *nameDupe = cloneString(name);
    char *s, *e;
    boolean match = FALSE;

    touppers(mNameDupe);
    touppers(nameDupe);
    stripChar(mNameDupe, ' ');
    stripChar(mNameDupe, '-');
    stripChar(mNameDupe, '.');
    stripChar(nameDupe, ' ');
    stripChar(nameDupe, '-');
    stripChar(mNameDupe, '.');
    verbose(2, "mahoneyNameAgrees %s (%s) %s (%s)", mName, mNameDupe, name, nameDupe);

    s = mNameDupe;
    while (s != NULL && s[0] != 0)
	{
	e = strchr(s, '/');
	if (e != NULL)
	   *e++ = 0;
	if (sameString(s, nameDupe))
	    {
	    match = TRUE;
	    break;
	    }
	s = e;
	}

    verbose(2, "  matches %d\n", match);
    freeMem(mNameDupe);
    freeMem(nameDupe);
    return match;
    }
}

void cleanup(struct mahoney *mahoneyList, char *mouseDb, char *pcrBed)
/* Look up gene names in refLink table where possible and use these instead
 * of embedded names. */
{
struct mahoney *m;
struct hash *refHash = refGeneToNameHash(mouseDb);
struct hash *llHash = locusLinkToRefSeq(mouseDb);
struct hash *pcrHash = mtfToRnaHash(pcrBed);
int refWinsByPcr = 0, llWinsByPcr = 0, noWin = 0, bothWin = 0, noWinNoPcr = 0;
int refWinsByName = 0, llWinsByName = 0;
int refWinsByDefault = 0, llWinsByDefault = 0, noWinByDefault = 0;
int locusLinkFromPcr = 0, genbankFromPcr = 0;

for (m = mahoneyList; m != NULL; m = m->next)
    {
    char mtf[8];
    char buf[9];
    safef(mtf, sizeof(mtf), "%d", m->mtf);
    struct rnaList *rl;
    boolean gotConflict = FALSE;
    rl = hashFindVal(pcrHash, mtf);

    /* Look for conflicts between refSeq and locusLink representations. 
     * Use PCR and names to try to resolve them.  (Ultimately this proves to
     * be futile though).  On unresolved ones axe the locusLink and refSeq
     * id's */
    if (startsWith("NM_", m->genbank) && m->locusId[0] != 0)
        {
	struct refSeqInfo *rsRsi = hashFindVal(refHash, m->genbank);
	struct refSeqInfo *llRsi = hashFindVal(llHash, m->locusId);
	boolean resolved = FALSE;
	if (rsRsi != NULL && llRsi != NULL)
	    {
	    if (rsRsi->locusLink == llRsi->locusLink)
		{
	        ++bothWin;
		resolved = TRUE;
		}
	    else
	        {
		if (conflictLog != NULL)
		    fprintf(conflictLog, 
		    	"MTF #%d locusLink %d (from %s)  doesn't match %s\n",
		    	m->mtf, rsRsi->locusLink, m->genbank, m->locusId);
		}
	    if (!resolved)
		{
		if (rl != NULL)
		    {
		    if (slNameInList(rl->accList, rsRsi->refSeq))
			{
			safef(buf, sizeof(buf), "%d", rsRsi->locusLink);
			m->locusId = cloneString(buf);
			++refWinsByPcr;
			if (conflictLog != NULL)
			    fprintf(conflictLog, "    picking %d by PCR\n",
				rsRsi->locusLink);	
			resolved = TRUE;
			}
		    else if (slNameInList(rl->accList, llRsi->refSeq))
			{
			m->genbank = cloneString(llRsi->refSeq);
			++llWinsByPcr;
			if (conflictLog != NULL)
			    fprintf(conflictLog, "    picking %d by PCR\n",
				llRsi->locusLink);	
			resolved = TRUE;
			}
		    else
			{
			++noWin;
			}
		    }
		}
	    else
	        {
		++noWinNoPcr;
		}
	    if (!resolved)
	        {
		if (mahoneyNameAgrees(m->geneName, rsRsi->geneName))
		    {
		    safef(buf, sizeof(buf), "%d", rsRsi->locusLink);
		    m->locusId = cloneString(buf);
		    ++refWinsByName;
		    if (conflictLog != NULL)
		        fprintf(conflictLog, "    picking %d by name\n",
			    rsRsi->locusLink);	
		    resolved = TRUE;
		    }
		else if (mahoneyNameAgrees(m->geneName, llRsi->geneName))
		    {
		    m->genbank = cloneString(llRsi->refSeq);
		    ++llWinsByName;
		    if (conflictLog != NULL)
		        fprintf(conflictLog, "    picking %d by name\n",
			    llRsi->locusLink);	
		    resolved = TRUE;
		    }
		else
		    {
		    verbose(3, "No resolution MTF%d by name or PCR\n", m->mtf);
		    }
		}
	    }
	else if (rsRsi == NULL && llRsi == NULL)
	    {
	    verbose(3, "refSeq %s, locusLink %s not found in refLink\n", m->genbank, m->locusId);
	    if (conflictLog != NULL)
		fprintf(conflictLog, 
		    "MTF #%d neither locusLink %s nor refSeq %s exist for mouse\n",
		    m->mtf, m->locusId, m->genbank);
	    ++noWinByDefault;
	    }
	else if (llRsi == NULL)
	    {
	    if (conflictLog != NULL)
		{
		fprintf(conflictLog, 
		    "MTF #%d locusLink %d (from %s)  doesn't match %s\n",
		    m->mtf, rsRsi->locusLink, m->genbank, m->locusId);
		fprintf(conflictLog, "    picking %d since %s not in mouse\n",
		    rsRsi->locusLink, m->locusId);	
		}
	    verbose(3, "locusId %s not found in refLink (refSeq %s)\n", m->locusId, m->genbank);
	    safef(buf, sizeof(buf), "%d", rsRsi->locusLink);
	    m->locusId = cloneString(buf);
	    ++refWinsByDefault;
	    resolved = TRUE;
	    }
	else if (rsRsi == NULL)
	    {
	    if (conflictLog != NULL)
		{
		fprintf(conflictLog, 
		    "MTF #%d refSeq %s doesn't exist in mouse\n",
		    m->mtf, m->genbank);
		fprintf(conflictLog, "    replacing with %s from locusLink %s\n",
		    llRsi->refSeq, m->locusId);	
		}
	    verbose(3, "refSeq %s not found in refLink (locusLink %s)\n", m->genbank, m->locusId);
	    m->genbank = cloneString(llRsi->refSeq);
	    ++llWinsByDefault;
	    resolved = TRUE;
	    }
	else
	    {
	    internalErr();
	    }
	}

    /* If no locusLink, try to get it from PCR */
    if (m->locusId == NULL || m->locusId[0] == 0)
        {
	struct rnaList *rl = hashFindVal(pcrHash, mtf);
	if (rl != NULL)
	    {
	    char *refSeq = findRefSeqInList(rl, NULL);
	    if (refSeq != NULL)
		{
		struct refSeqInfo *rsi = hashFindVal(refHash, refSeq);
		if (rsi != NULL)
		    {
		    char buf[8];
		    ++locusLinkFromPcr;
		    safef(buf, sizeof(buf), "%d", rsi->locusLink);
		    verbose(3, "MTF%d from PCR refSeq %s\n", m->mtf, refSeq);
		    if (conflictLog != NULL)
			{
			fprintf(conflictLog, 
			    "MTF #%d  setting locusLink/refseq to %d/%s from PCR\n",
			    m->mtf, rsi->locusLink, refSeq);
			fprintf(conflictLog, "    replacing with %s from locusLink %s\n",
			    rsi->refSeq, m->locusId);	
			}
		    m->locusId = cloneString(buf);
		    m->genbank = refSeq;
		    }
		}
	    }
	}

    /* If have locusLink, use it to update name. */
	{
	struct refSeqInfo *llRsi = hashFindVal(llHash, m->locusId);
	if (llRsi != NULL && llRsi->geneName != NULL && llRsi->geneName[0] != 0)
	    m->geneName = llRsi->geneName;
	}

    /* If no genbank, use PCR to fill in genbank. */
    if (   (m->genbank == NULL || m->genbank[0] == 0) 
    	&& (m->locusId == NULL || m->locusId[0] == 0)
	&& (m->geneName == NULL || m->geneName[0] == 0))
	{
	struct rnaList *rl = hashFindVal(pcrHash, mtf);
	if (rl != NULL)
	    {
	    m->genbank = rl->accList->name;
	    verbose(2, "MTF%d - filling in genbank %s from PCR\n", m->mtf, m->genbank);
	    ++genbankFromPcr;
	    }
	}
    }
verbose(2, "RefSeq conflicts: refWinsByPcr %d, llWinsByPcr %d, noWin %d, noWinNoPcr %d, bothWin %d\n", refWinsByPcr, llWinsByPcr, noWin, noWinNoPcr, bothWin);
verbose(2, "              refWinsByName %d, llWinsByName %d\n", refWinsByName, llWinsByName);
verbose(2, "              refWinsByDefault %d, llWinsByDefault %d, noWinByDefault %d\n", refWinsByDefault, llWinsByDefault, noWinByDefault);
verbose(2, "Filled by PCR: locusLink %d, genbank %d\n", locusLinkFromPcr, genbankFromPcr);
}


void vgLoadMahoney(char *visiGeneDir, char *mouseDb, char *inTab, char *pcrBed, char *outDir)
/* vgLoadMahoney - Convert Paul Gray/Mahoney in situs into something visiGeneLoad can handle. */
{
struct mahoney *mahoneyList = vgLoadMahoneyList(inTab);
struct hash *mHash = hashMahoneys(mahoneyList);
char inFull[PATH_LEN];
char *mahoneyPath = "inSitu/Mouse/mahoney";
char inWholeFull[PATH_LEN], inSlicesFull[PATH_LEN];
safef(inFull, sizeof(inFull), "%s/full", visiGeneDir);
safef(inWholeFull, sizeof(inWholeFull), "%s/%s/%s", inFull, mahoneyPath, "wholeMount");
safef(inSlicesFull, sizeof(inSlicesFull), "%s/%s/%s", inFull, mahoneyPath, "slices");
makeDir(outDir);
trimAll(mahoneyList);
cleanup(mahoneyList, mouseDb, pcrBed);
processWholeMounts(mHash, inWholeFull, outDir);
processSlices(mHash, inSlicesFull, outDir);
}

int main(int argc, char *argv[])
/* Process command line. */
{
dnaUtilOpen();
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
if (optionExists("conflictLog"))
    conflictLog = mustOpen(optionVal("conflictLog", NULL), "w");
vgLoadMahoney(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
