/* loadMahoney - Convert Paul Gray/Mahoney in situs into something genePixLoad can handle. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "jksql.h"
#include "mahoney.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "loadMahoney - Convert Paul Gray/Mahoney in situs into something genePixLoad can handle\n"
  "usage:\n"
  "   loadMahoney /gbdb/genePix mm5 input.tab pcr.bed outDir\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void raCommon(FILE *f)
/* Write parts of ra common for whole mounts and slices" */
{
fprintf(f, "%s",
"taxon 10090\n"
"imageType RNA in situ\n"
"contributor Gray P.A., Fu H., Luo P., Zhao Q., Yu J., Ferrari A., Tenzen T., Yuk D., Tsung E.F., Cai Z., Alberta J.A., Cheng L., Liu Y., Stenman J.M., Valerius M.T., Billings N., Kim H.A., Greenberg M.E., McMahon A.P., Rowitch D.H., Stiles C.D., Ma Q.\n"
"publication Mouse brain organization revealed through direct genome-scale TF expression analysis\n"
"pubUrl http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?cmd=Retrieve&db=pubmed&dopt=Abstract&list_uids=15618518\n"
"journal Science\n"
"journalUrl http://www.sciencemag.org/\n"
"setUrl http://mahoney.chip.org/mahoney/\n"
"itemUrl http://mahoney.chip.org/servlet/Mahoney2.ShowTF?mTF_ID=%s\n"
"probeColor purple\n"
);
}

void wholeMountRa(char *fileName)
/* Output whole mount ra file. */
{
FILE *f = mustOpen(fileName, "w");

fprintf(f, "%s",
"fullDir /gbdb/genePix/full/inSitu/Mouse/mahoney/wholeMount\n"
"screenDir /gbdb/genePix/700/inSitu/Mouse/mahoney/wholeMount\n"
"thumbDir /gbdb/genePix/200/inSitu/Mouse/mahoney/wholeMount\n"
"priority 10\n"
"isEmbryo 1\n"
"age 10.5\n"
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
"fullDir /gbdb/genePix/full/inSitu/Mouse/mahoney/slices\n"
"screenDir /gbdb/genePix/700/inSitu/Mouse/mahoney/slices\n"
"thumbDir /gbdb/genePix/200/inSitu/Mouse/mahoney/slices\n"
"priority 100\n"
"bodyPart brain\n"
"sliceType transverse\n"
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

void wholeMountTab(struct slName *inList, struct hash *mahoneyHash,
	char *outFile)
/* Output tab-delimited info on each item in inList. */
{
struct slName *in;
FILE *f = mustOpen(outFile, "w");
fprintf(f, "#fileName\tsubmitId\tgene\tlocusLink\trefSeq\tgenbank\tfPrimer\trPrimer\ttreatment\n");
for (in = inList; in != NULL; in = in->next)
    {
    char hex[8];
    char mtf[5];
    char treatment;
    int id;
    struct mahoney *m;
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
		fprintf(f, "3 min proteinase K");
	    else if (treatment == 'b')
		fprintf(f, "30 min proteinase K");
	    else if (treatment == 'd')
		fprintf(f, "3 min proteinase K/30 min proteinase K");
	    else if (treatment == 'c' || treatment == '.')
		/* do nothing*/ ;
	    else
		errAbort("Unrecognized treatment %c in %s", treatment, in->name);
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
wholeMountTab(imageFileList, mahoneyHash, outTab);
}

void slicesTab(struct slName *inList, struct hash *mahoneyHash,
	char *outFile)
/* Output tab-delimited info on each item in inList. */
{
struct slName *in;
FILE *f = mustOpen(outFile, "w");
char lastStage = 0;
fprintf(f, "#fileName\tsubmitId\tgene\tlocusLink\trefSeq\tgenbank\tfPrimer\trPrimer\tisEmbryo\tage\tsectionSet\tsectionIx\n");
for (in = inList; in != NULL; in = in->next)
    {
    char hex[8];
    char mtf[5];
    char stage = '1', sliceNo, nextSliceNo = '1';
    int id;
    struct mahoney *m;
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
		fprintf(f, "1\t13.5\t");
	    else if (stage == '2')
		fprintf(f, "0\t0\t");
	    else
		errAbort("Unrecognized stage %c in %s", stage, in->name);
	    fprintf(f, "0\t0\n");
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
slicesTab(imageFileList, mahoneyHash, outTab);
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

struct mahoney *loadMahoneyList(char *fileName)
/* Load data from tab-separated file (not whitespace separated) */
{
struct mahoney *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[MAHONEY_NUM_COLS];
while (lineFileNextRowTab(lf, row, ArraySize(row)))
    {
    el = mahoneyLoad(row);
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

sr = sqlGetResult(conn, "select mrnaAcc,name,locusLinkId from refLink where name != ''");
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

sr = sqlGetResult(conn, "select mrnaAcc,name,locusLinkId from refLink where locusLinkId != 0");
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


#ifdef OLD
void cleanup(struct mahoney *mahoneyList, char *mouseDb, char *pcrBed)
/* Look up gene names in refLink table where possible and use these instead
 * of embedded names. */
{
struct mahoney *m;
struct hash *refHash = refGeneToNameHash(mouseDb);
struct hash *llHash = locusLinkToRefSeq(mouseDb);
struct hash *pcrHash = mtfToRnaHash(pcrBed);
int perfectRefMatch = 0;
int mPcrMatch = 0;
int lPcrMatch = 0;
int mPcrNameMatch = 0;
int lPcrNameMatch = 0;
int mlRefMatch = 0;
int mNameMRefMatch = 0;
int mNameLMatch = 0;
int mmMatch = 0;
int lmMatch = 0;
int llMatch = 0;

for (m = mahoneyList; m != NULL; m = m->next)
    {
    char *finalRefSeq = NULL;
    char *pcrRefSeq = NULL;
    char *lRefSeq = NULL;
    char *mRefSeq = NULL;
    char *finalName = NULL;
    char *pcrName = NULL;
    char *lName = NULL;
    char *mName = NULL;
    char *mRefSeqName = NULL;
    char mtf[8];

    safef(mtf, sizeof(mtf), "%d", m->mtf);
    struct rnaList *rl = hashFindVal(pcrHash, mtf);
    if (startsWith("NM_", m->genbank))
	{
	struct refSeqInfo *rsi = hashFindVal(refHash, m->genbank);
        mRefSeq = m->genbank;
	if (rsi != NULL)
	    if (rsi->geneName != NULL && rsi->geneName[0] != 0)
		mRefSeqName = rsi->geneName;
	}
    if (m->geneName[0] != 0)
        mName = m->geneName;
    if (m->locusId[0] != 0)
	{
	struct refSeqInfo *rsi = hashFindVal(llHash, m->locusId);
	if (rsi != NULL)
	    {
	    lRefSeq = rsi->refSeq;
	    if (rsi->geneName != NULL && rsi->geneName[0] != 0)
	       lName = rsi->geneName;
	    }
	}
    if (rl != NULL)
	{
        pcrRefSeq = findRefSeqInList(rl, mRefSeq);
	if (pcrRefSeq != NULL)
	    {
	    struct refSeqInfo *rsi = hashFindVal(refHash, pcrRefSeq);
	    if (rsi != NULL)
		if (rsi->geneName != NULL && rsi->geneName[0] != 0)
		    pcrName = rsi->geneName;
	    }
	}
    verbose(3, "MTF %d, locusId %s, mRefSeq %s, lRefSeq %s, pcrRefSeq %s, mName %s, lName %s, pcrName %s\n", m->mtf, m->locusId, mRefSeq, lRefSeq, pcrRefSeq, mName, lName, pcrName);

    if (pcrRefSeq != NULL)	/* The one we get from PCR we consider most trustworthy */
        {
	if (mRefSeq != NULL && lRefSeq != NULL && sameString(pcrRefSeq, mRefSeq) && sameString(pcrRefSeq, lRefSeq))
	    {
	    ++perfectRefMatch;
	    finalRefSeq = pcrRefSeq;
	    finalName = pcrName;
	    }
	else if (mRefSeq != NULL && sameString(mRefSeq, pcrRefSeq))
	    {
	    ++mPcrMatch;
	    finalRefSeq = pcrRefSeq;
	    finalName = pcrName;
	    }
	else if (lRefSeq != NULL && sameString(lRefSeq, pcrRefSeq))
	    {
	    ++lPcrMatch;
	    finalRefSeq = pcrRefSeq;
	    finalName = pcrName;
	    }
	else if (mahoneyNameAgrees(mName, pcrName))
	    {
	    ++mPcrNameMatch;
	    finalRefSeq = pcrRefSeq;
	    finalName = pcrName;
	    }
	else if (lName != NULL && pcrName != NULL && sameWord(lName, pcrName))
	    {
	    ++lPcrNameMatch;
	    finalRefSeq = pcrRefSeq;
	    finalName = pcrName;
	    }
	else
	    {
	    verbose(2, "conflicted MTF A %d, locusId %s, mRefSeq %s, lRefSeq %s, pcrRefSeq %s, mName %s, lName %s, pcrName %s\n", m->mtf, m->locusId, mRefSeq, lRefSeq, pcrRefSeq, mName, lName, pcrName);

	    }
	}
    if (finalRefSeq == NULL)
        {
	if (mRefSeq != NULL && lRefSeq != NULL)
	    {
	    if (sameString(mRefSeq, lRefSeq))
	        {
		++mlRefMatch;
		finalRefSeq = lRefSeq;
		if (lName != NULL)
		    finalName = lName;
		else
		    finalName = mName;
		}
	    else if (mahoneyNameAgrees(mName, mRefSeqName))
		{
		++mNameMRefMatch;
		finalRefSeq = mRefSeq;
		finalName = mRefSeqName;
		}
	    else if (mahoneyNameAgrees(mName, lName))
	        {
		++mNameLMatch;
		finalRefSeq = lRefSeq;
		finalName = lName;
		}
	    else
	        {
		verbose(2, "conflicted MTF B %d, locusId %s, mRefSeq %s, lRefSeq %s, pcrRefSeq %s, mName %s, lName %s, pcrName %s\n", m->mtf, m->locusId, mRefSeq, lRefSeq, pcrRefSeq, mName, lName, pcrName);
		}
	    }
	}
    if (finalRefSeq == NULL)
        {
	if (mRefSeq != NULL)
	    {
	    if (mahoneyNameAgrees(mName, mRefSeqName))
	        {
		++mmMatch;
		finalRefSeq = mRefSeq;
		finalName = mRefSeqName;
		}
	    else
	        {
		verbose(2, "conflicted MTF C %d, locusId %s, mRefSeq %s, lRefSeq %s, pcrRefSeq %s, mName %s, lName %s, pcrName %s\n", m->mtf, m->locusId, mRefSeq, lRefSeq, pcrRefSeq, mName, lName, pcrName);
		}
	    }
	}
    if (finalRefSeq == NULL)
        {
	if (lRefSeq != NULL)
	    {
	    verbose(3, "lRefSeq ok, mName %s, lName %s, mRefSeq %s, pcrRefSeq %s\n", mName, lName, mRefSeq, pcrRefSeq);
	    if (mahoneyNameAgrees(mName, lName))
	        {
		++lmMatch;
		finalRefSeq = lRefSeq;
		finalName = lName;
		}
	    else if (mRefSeq == NULL && pcrRefSeq == NULL && (mName == NULL || mName[0] == 0))
	        {
		verbose(3, "got llMatch\n");
		++llMatch;
		finalRefSeq = lRefSeq;
		finalName = lName;
		}
	    else
	        {
		verbose(2, "conflicted MTF D %d, locusId %s, mRefSeq %s, lRefSeq %s, pcrRefSeq %s, mName %s, lName %s, pcrName %s\n", m->mtf, m->locusId, mRefSeq, lRefSeq, pcrRefSeq, mName, lName, pcrName);
		}
	    }
	}
    if (finalRefSeq == NULL && pcrRefSeq != NULL)
        {
	finalRefSeq = pcrRefSeq;
	finalName = pcrName;
	}

    if (finalName != NULL && finalName[0] != 0)
	m->geneName = finalName;
    if (finalRefSeq != NULL && finalRefSeq[0] != 0)
        {
	struct refSeqInfo *rsi = hashFindVal(refHash, finalRefSeq);
	if (rsi != NULL && rsi->locusLink != 0)
	    {
	    char buf[9];
	    safef(buf, sizeof(buf), "%d", rsi->locusLink);
	    if (!sameString(m->locusId, buf))
		m->locusId = cloneString(buf);
	    }
	m->genbank = finalRefSeq;
	}
    if (m->geneName != NULL && m->geneName[0] != 0)
        m->geneName[0] = toupper(m->geneName[0]);
    verbose(3, "   geneName %s, genbank %s\n", m->geneName, m->genbank);



    if (startsWith("NM_", m->genbank))
        {
	struct refSeqInfo *rsi = hashFindVal(refHash, m->genbank);
	if (rsi != NULL)
	    {
	    if (m->locusId[0] != 0)
	        {
		int locusId = atoi(m->locusId);
		if (rsi->locusLink != locusId)
		    {
		    warn("locusId mismatch on %s (%s) refLink=%d, clone.tab=%d", 
		    	m->genbank, m->geneName, rsi->locusLink, locusId);
		    }
		}
	    if (rsi->geneName[0] != 0 && m->geneName[0] != 0)
	        {
		if (!sameWord(rsi->geneName, m->geneName))
		    {
		    warn("gene name mismatch on %s refLink=%s, clone.tab=%s", 
		    	m->genbank, rsi->geneName, m->geneName);
		    }
		}
	    }
	}
    }
}
#endif /* OLD */

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
	    else if (rl != NULL)
		{
		if (slNameInList(rl->accList, rsRsi->refSeq))
		    {
		    safef(buf, sizeof(buf), "%d", rsRsi->locusLink);
		    m->locusId = cloneString(buf);
		    ++refWinsByPcr;
		    resolved = TRUE;
		    }
		else if (slNameInList(rl->accList, llRsi->refSeq))
		    {
		    m->genbank = cloneString(llRsi->refSeq);
		    ++llWinsByPcr;
		    resolved = TRUE;
		    }
		else
		    {
		    ++noWin;
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
		    resolved = TRUE;
		    }
		else if (mahoneyNameAgrees(m->geneName, llRsi->geneName))
		    {
		    m->genbank = cloneString(llRsi->refSeq);
		    ++llWinsByName;
		    resolved = TRUE;
		    }
		else
		    verbose(3, "No resolution MTF%d by name or PCR\n", m->mtf);
		}
	    }
	else if (rsRsi == NULL && llRsi == NULL)
	    {
	    verbose(3, "refSeq %s, locusLink %s not found in refLink\n", m->genbank, m->locusId);
	    ++noWinByDefault;
	    }
	else if (llRsi == NULL)
	    {
	    verbose(3, "locusId %s not found in refLink (refSeq %s)\n", m->locusId, m->genbank);
	    safef(buf, sizeof(buf), "%d", rsRsi->locusLink);
	    m->locusId = cloneString(buf);
	    ++refWinsByDefault;
	    resolved = TRUE;
	    }
	else if (rsRsi == NULL)
	    {
	    verbose(3, "refSeq %s not found in refLink (locusLink %s)\n", m->genbank, m->locusId);
	    m->genbank = cloneString(llRsi->refSeq);
	    ++llWinsByDefault;
	    resolved = TRUE;
	    }
	else
	    {
	    internalErr();
	    }
	if (!resolved)
	    {
	    m->locusId = "";
	    m->genbank = "";
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


void loadMahoney(char *genePixDir, char *mouseDb, char *inTab, char *pcrBed, char *outDir)
/* loadMahoney - Convert Paul Gray/Mahoney in situs into something genePixLoad can handle. */
{
struct mahoney *mahoneyList = loadMahoneyList(inTab);
struct hash *mHash = hashMahoneys(mahoneyList);
char inFull[PATH_LEN];
char *mahoneyPath = "inSitu/Mouse/mahoney";
char inWholeFull[PATH_LEN], inSlicesFull[PATH_LEN];
safef(inFull, sizeof(inFull), "%s/full", genePixDir);
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
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
loadMahoney(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
