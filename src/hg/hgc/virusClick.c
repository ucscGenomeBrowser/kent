/* virusClick - hgc code for a prototype virus browser on the h1n1 genome */

#include "common.h"
#include "hgc.h"
#include "virusClick.h"
#include "hCommon.h"
#include "linefile.h"
#include "dystring.h"
#include "lsSnpPdbChimera.h"
#include "jksql.h"
#include "net.h"
#include "trashDir.h"
#include "htmshell.h"

static void h1n1DownloadPdb(char *item, char *pdbUrl, struct tempName *tmpPdb)
/* uncompress PDB to trash */
{
int inFd = netOpenHttpExt(pdbUrl, "GET", TRUE);
int inFdRedir = 0;
char *pdbUrlRedir = NULL;
if (!netSkipHttpHeaderLinesHandlingRedirect(inFd, pdbUrl, &inFdRedir, &pdbUrlRedir))
    errAbort("Unable to access predicted 3D structure file: %s", pdbUrl);
if (pdbUrlRedir != NULL)
    {
    close(inFd);
    inFd = inFdRedir;
    freez(&pdbUrlRedir);
    }

trashDirFile(tmpPdb, "hgct", item, ".pdb");
FILE *outFh = mustOpen(tmpPdb->forCgi, "w");
struct lineFile *inLf = lineFileDecompressFd(pdbUrl, TRUE, inFd);
char *line;
while (lineFileNext(inLf, &line, NULL))
    {
    fputs(line, outFh);
    fputc('\n', outFh);
    }
lineFileClose(&inLf);
carefulClose(&outFh);
}

static void h1n1MkChimeraxTrashFullUrl(struct tempName *tmpPdb, char *fullUrl, int fullUrlSize)
/* generate full URL for a chimerax file decompressed to trash */
{
// FIXME: this is not generate (URL generation will not work on all web servers).
// if this is kept, should address this.
char *serverName = getenv("SERVER_NAME");
char *serverPort = getenv("SERVER_PORT");
char *scriptName = getenv("SCRIPT_NAME");
if ((serverName != NULL) && (serverPort != NULL) && (scriptName != NULL))
    {
    // remote url
    safef(fullUrl, fullUrlSize, "http://%s", serverName);
    if (!sameString(serverPort, "80"))
        {
        safecat(fullUrl, fullUrlSize, ":");
        safecat(fullUrl, fullUrlSize, serverPort);
        }
    safecat(fullUrl, fullUrlSize, scriptName);
    char *p = strrchr(fullUrl, '/');
    if (p != NULL)
        *++p = '\0';  // drop cgi name, keeping directory
    safecat(fullUrl, fullUrlSize, tmpPdb->forHtml);
    }
else
    {
    // local url
    safef(fullUrl, fullUrlSize, "file:///%s/%s", getCurrentDir(), tmpPdb->forHtml);
    }
}

static char *mkChimeraPyScript(char *scriptFile)
/* create chimera python script from commands in scriptFile */
{
struct dyString *buf = dyStringNew(0);
dyStringAppend(buf, "from chimera import runCommand\n");
struct lineFile *lf = lineFileOpen(scriptFile, TRUE);
char *line;
while (lineFileNextReal(lf, &line))
    dyStringPrintf(buf, "runCommand(\"%s\")\n", line);

lineFileClose(&lf);
return dyStringCannibalize(&buf);
}

static void mkChimerax(char *item, char *pdbUrl, char *scriptFile, struct tempName *chimerax)
/* generate a chimerax file for downloading h1n1 PDB structure */
{
// chimera doesn't handle compressed files via URL, so uncompress into the trash
struct tempName tmpPdb;
char usePdbUrl[PATH_LEN];
if (endsWith(pdbUrl, ".gz") || endsWith(pdbUrl, ".Z"))
    {
    h1n1DownloadPdb(item, pdbUrl, &tmpPdb);
    h1n1MkChimeraxTrashFullUrl(&tmpPdb, usePdbUrl, sizeof(usePdbUrl));
    }
else
    safecpy(usePdbUrl, sizeof(usePdbUrl), pdbUrl);
char *pyScript = NULL;
if (scriptFile != NULL)
    pyScript = mkChimeraPyScript(scriptFile);
lsSnpPdbChimeraGenericLink(usePdbUrl, pyScript, "hgct", item, chimerax);
freeMem(pyScript);
}

static void tempNameFromPrefix(struct tempName *name, struct tempName *prefix, char *suffix)
/* build a tempName based on an existing tempName and a suffix */
{
safecpy(name->forCgi, sizeof(name->forCgi), prefix->forCgi);
safecat(name->forCgi, sizeof(name->forCgi), suffix);
safecpy(name->forHtml, sizeof(name->forHtml), prefix->forHtml);
safecat(name->forHtml, sizeof(name->forHtml), suffix);
}

/* location and url of h1n1 modeling directory */
static char *h1n1StructDir = "/projects/compbio/experiments/protein-predict/h1n1";
static char *h1n1StructUrl = "http://users.soe.ucsc.edu/~karplus/h1n1";

static boolean getH1n1Model(char *gene, char *pdbUrl)
/* Find model PDB file URL, return false if does not exist.  URL is
 * return in pdbUrl if it is not NULL */
{
char relPath[PATH_LEN], pdbPath[PATH_LEN];
if (sameString(gene, "HA"))
    safef(relPath, sizeof(relPath), "%s/%s", gene, "consensus.uncleaved.D18-G519.pdb");
else if (sameString(gene, "NA"))
    safef(relPath, sizeof(relPath), "%s/%s", gene, "consensus.tetramer.V83-I467.pdb");
else 
    return FALSE;

if (pdbUrl != NULL)
    safef(pdbUrl, PATH_LEN, "%s/%s", h1n1StructUrl, relPath);
safef(pdbPath, sizeof(pdbPath), "%s/%s", h1n1StructDir, relPath);
return fileExists(pdbPath);
}

static void mkH1n1StructData(char *gene, char *idFile,
                             struct tempName *imageFile, struct tempName *chimeraScript)
/* generate 3D structure files; idFile can be NULL */
{
struct tempName prefix;
trashDirFile(&prefix, "hgct", gene, "tmp");

// dynamic_highlight.pl knows locations of model files
char idArg[PATH_LEN], logFile[PATH_LEN], cmd[2*PATH_LEN];
idArg[0] = '\0';
if (idFile != NULL)
    safef(idArg, sizeof(idArg), "--ids %s", idFile);
safef(logFile, sizeof(logFile), "%s.log", prefix.forCgi);
safef(cmd, sizeof(cmd), "%s/dynamic_highlight.pl --rasmol --chimera --protein %s --consensus 0602 %s --base %s >%s 2>&1",
      h1n1StructDir, gene, idArg, prefix.forCgi, logFile);
if (system(cmd) != 0)
    errAbort("creation of 3D structure highlight files failed: %s", cmd);

// output names are all predefined by script relative to prefix
tempNameFromPrefix(imageFile, &prefix, "_highlight.jpg");
tempNameFromPrefix(chimeraScript, &prefix, "_highlight.cmd");
}

static void showProtH1n1(char *item)
{
char query2[256];
struct sqlResult *sr2;
char **row2;
struct sqlConnection *conn2 = hAllocConn(database);

char *subjId, *dnaSeqId;
char *aaSeqId= NULL;
char *gene=NULL;

char cond_str[256];
char *predFN;
char *homologID;
char *SCOPdomain;
char *chain;
char goodSCOPdomain[40];
int  first = 1;
float  eValue;
char *chp;
int homologCount;
int gotPDBFile = 0;

safef(query2, sizeof(query2),
	"select subjId, dnaSeqId, aaSeqId, gene from gisaidXref where dnaSeqId='%s'", item);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
if (row2 != NULL)
    {
    subjId = strdup(row2[0]);
    dnaSeqId = strdup(row2[1]);
    aaSeqId  = strdup(row2[2]);
    gene     = strdup(row2[3]);
    }
else
    {
    errAbort("%s not found.", item);
    }
sqlFreeResult(&sr2);

printf("<H3>Protein Structure Analysis and Prediction</H3>");

printf("<B>Comparison to 1918 Flu Virus:</B> ");
printf("<A HREF=\"%s/%s/%s/1918_%s.mutate", h1n1StructUrl, gene, aaSeqId, aaSeqId);
printf("\" TARGET=_blank>%s</A><BR>\n", aaSeqId);

printf("<B>Comparison to A H1N1 gene %s concensus:</B> ", gene);
printf("<A HREF=\"%s/%s/%s/consensus_%s.mutate", h1n1StructUrl, gene, aaSeqId, aaSeqId);
printf("\" TARGET=_blank>%s</A><BR>\n", aaSeqId);

//printf("<BR><B>3D structure prediction:</B> ");

printf("<BR><B>3D Structure Prediction (PDB file):</B> ");
char pdbUrl[PATH_LEN];
safef(pdbUrl, sizeof(pdbUrl), "%s/%s/decoys/%s.try1-opt3.pdb.gz", h1n1StructUrl, item, item);


// Modeller stuff
char modelPdbUrl[PATH_LEN];
if (getH1n1Model(gene, modelPdbUrl))
    {
    char *selectFile = cartOptionalString(cart, gisaidAaSeqList);
    struct tempName imageFile, chimeraScript, chimerax;
    mkH1n1StructData(gene, selectFile, &imageFile, &chimeraScript);
    mkChimerax(gene, modelPdbUrl, chimeraScript.forCgi, &chimerax);
    printf("<A HREF=\"%s\" TARGET=_blank>%s</A>, view with <A HREF=\"%s\">Chimera</A><BR>\n", modelPdbUrl, gene, chimerax.forHtml);
    printf("<TABLE>\n");
    printf("<TR>\n");
    printf("<TD ALIGN=\"center\"><img src=\"%s\"></TD>", imageFile.forHtml);
    printf("</TR>\n");
    printf("</TABLE>\n");
    }
return;

gotPDBFile = 0;
safef(cond_str, sizeof(cond_str), "proteinID='%s' and evalue <1.0e-5;", item);

printf("<TABLE>\n");
printf("<TR><TD ALIGN=\"center\">Front</TD>\n");
printf("<TD ALIGN=\"center\">Top</TD>\n");
printf("<TD ALIGN=\"center\">Side</TD>\n");
printf("</TR>\n");
printf("<TR>\n");
printf("<TD ALIGN=\"center\"><img src=\"%s/%s/%s.undertaker-align.view1_200.jpg\"></TD>", h1n1StructUrl, item, item);
printf("<TD ALIGN=\"center\"><img src=\"%s/%s/%s.undertaker-align.view2_200.jpg\"></TD>", h1n1StructUrl, item, item);
printf("<TD ALIGN=\"center\"><img src=\"%s/%s/%s.undertaker-align.view3_200.jpg\"></TD>", h1n1StructUrl, item, item);
printf("</TR>\n");
printf("<TR>\n");
printf("<TD ALIGN=\"center\"><A HREF=\"%s/%s/%s.undertaker-align.view1_500.jpg\">500x500</A></TD>",
	h1n1StructUrl, item, item);
printf("<TD ALIGN=\"center\"><A HREF=\"%s/%s/%s.undertaker-align.view2_500.jpg\">500x500</A></TD>",
	h1n1StructUrl, item, item);
printf("<TD ALIGN=\"center\"><A HREF=\"%s/%s/%s.undertaker-align.view3_500.jpg\">500x500</A></TD>",
	h1n1StructUrl, item, item);
printf("</TR>\n");
printf("</TABLE>\n");

printf("<BR><B>Detailed results of SAM-T02:</B> ");
printf("<A HREF=\"%s/%s/summary.html", h1n1StructUrl, item);
printf("\" TARGET=_blank>%s</A><BR>\n", item);

/* by pass the following additional processing for now, until two necessary tables are built */
hFreeConn(&conn2);
return;

if (sqlGetField(database, "protHomolog", "proteinID", cond_str) != NULL)
    {
    safef(cond_str, sizeof(cond_str), "proteinID='%s'", item);
    predFN = sqlGetField(database, "protPredFile", "predFileName", cond_str);
    if (predFN != NULL)
	{
	printf("<A HREF=\"../SARS/%s/", item);
	/* printf("%s.t2k.undertaker-align.pdb\">%s</A><BR>\n", item,item); */
	printf("%s\">%s</A><BR>\n", predFN,item);
	gotPDBFile = 1;
	}
    }
if (!gotPDBFile)
    {
    printf("No high confidence level structure prediction available for this sequence.");
    printf("<BR>\n");
    }
printf("<B>3D Structure of Close Homologs:</B> ");
homologCount = 0;
strcpy(goodSCOPdomain, "dummy");

conn2= hAllocConn(database);
safef(query2, sizeof(query2),
	"select homologID,eValue,SCOPdomain,chain from sc1.protHomolog where proteinID='%s' and evalue <= 0.01;",
	item);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
if (row2 != NULL)
    {
    while (row2 != NULL)
	{
	homologID = row2[0];
	sscanf(row2[1], "%e", &eValue);
	SCOPdomain = row2[2];
	chp = SCOPdomain+strlen(SCOPdomain)-1;
	while (*chp != '.') chp--;
	*chp = '\0';
	chain = row2[3];
	if (eValue <= 1.0e-10)
	    strcpy(goodSCOPdomain, SCOPdomain);
	else
	    {
	    if (strcmp(goodSCOPdomain,SCOPdomain) != 0)
		goto skip;
	    else
		if (eValue > 0.1) goto skip;
	    }
	if (first)
	    first = 0;
	else
	    printf(", ");

	printf("<A HREF=\"http://www.rcsb.org/pdb/cgi/explore.cgi?job=graphics&pdbId=%s",
	       homologID);
	if (strlen(chain) >= 1)
	    printf("\"TARGET=_blank>%s(chain %s)</A>", homologID, chain);
	else
	    printf("\"TARGET=_blank>%s</A>", homologID);
	homologCount++;

	skip:
	row2 = sqlNextRow(sr2);
	}
    }
hFreeConn(&conn2);
sqlFreeResult(&sr2);
if (homologCount == 0)
    printf("None<BR>\n");

printf("<BR><B>Details:</B> ");
printf("<A HREF=\"../SARS/%s/summary.html", item);
printf("\" TARGET=_blank>%s</A><BR>\n", item);

htmlHorizontalLine();
}

void showSAM_h1n1(char *item)
{
char query2[256];
struct sqlResult *sr2;
char **row2;
struct sqlConnection *conn2 = hAllocConn(database);
char cond_str[256];
char *predFN;
char *homologID;
char *SCOPdomain;
char *chain;
char goodSCOPdomain[40];
int  first = 1;
float  eValue;
char *chp;
int homologCount;
int gotPDBFile = 0;

printf("<H3>Protein Structure Analysis and Prediction by ");
printf("<A HREF=\"http://www.soe.ucsc.edu/research/compbio/SAM_T02/sam-t02-faq.html\"");
printf(" TARGET=_blank>SAM-T02</A></H3>\n");

printf("<B>Multiple Alignment:</B> ");
printf("<A HREF=\"%s/%s/summary.html#alignment", h1n1StructUrl, item);
printf("\" TARGET=_blank>%s</A><BR>\n", item);

printf("<B>Secondary Structure Predictions:</B> ");
printf("<A HREF=\"%s/%s/summary.html#secondary-structure", h1n1StructUrl, item);
printf("\" TARGET=_blank>%s</A><BR>\n", item);

printf("<B>3D Structure Prediction (PDB file):</B> ");
char pdbUrl[PATH_LEN];
safef(pdbUrl, sizeof(pdbUrl), "%s/%s/decoys/%s.try1-opt3.pdb.gz", h1n1StructUrl, item, item);
struct tempName chimerax;
mkChimerax(item, pdbUrl, NULL, &chimerax);

printf("<A HREF=\"%s\" TARGET=_blank>%s</A>, view with <A HREF=\"%s\">Chimera</A><BR>\n", pdbUrl, item, chimerax.forHtml);

gotPDBFile = 0;
safef(cond_str, sizeof(cond_str), "proteinID='%s' and evalue <1.0e-5;", item);

printf("<TABLE>\n");
printf("<TR><TD ALIGN=\"center\">Front</TD>\n");
printf("<TD ALIGN=\"center\">Top</TD>\n");
printf("<TD ALIGN=\"center\">Side</TD>\n");
printf("</TR>\n");
printf("<TR>\n");
printf("<TD ALIGN=\"center\"><img src=\"%s/%s/%s.undertaker-align.view1_200.jpg\"></TD>", h1n1StructUrl, item, item);
printf("<TD ALIGN=\"center\"><img src=\"%s/%s/%s.undertaker-align.view2_200.jpg\"></TD>", h1n1StructUrl, item, item);
printf("<TD ALIGN=\"center\"><img src=\"%s/%s/%s.undertaker-align.view3_200.jpg\"></TD>", h1n1StructUrl, item, item);
printf("</TR>\n");
printf("<TR>\n");
printf("<TD ALIGN=\"center\"><A HREF=\"%s/%s/%s.undertaker-align.view1_500.jpg\">500x500</A></TD>",
	h1n1StructUrl, item, item);
printf("<TD ALIGN=\"center\"><A HREF=\"%s/%s/%s.undertaker-align.view2_500.jpg\">500x500</A></TD>",
	h1n1StructUrl, item, item);
printf("<TD ALIGN=\"center\"><A HREF=\"%s/%s/%s.undertaker-align.view3_500.jpg\">500x500</A></TD>",
	h1n1StructUrl, item, item);
printf("</TR>\n");
printf("</TABLE>\n");

printf("<BR><B>Detailed results of SAM-T02:</B> ");
printf("<A HREF=\"%s/%s/summary.html", h1n1StructUrl, item);
printf("\" TARGET=_blank>%s</A><BR>\n", item);

/* by pass the following additional processing for now, until two necessary tables are built */
hFreeConn(&conn2);
return;

if (sqlGetField(database, "protHomolog", "proteinID", cond_str) != NULL)
    {
    safef(cond_str, sizeof(cond_str), "proteinID='%s'", item);
    predFN = sqlGetField(database, "protPredFile", "predFileName", cond_str);
    if (predFN != NULL)
	{
	printf("<A HREF=\"../SARS/%s/", item);
	/* printf("%s.t2k.undertaker-align.pdb\">%s</A><BR>\n", item,item); */
	printf("%s\">%s</A><BR>\n", predFN,item);
	gotPDBFile = 1;
	}
    }
if (!gotPDBFile)
    {
    printf("No high confidence level structure prediction available for this sequence.");
    printf("<BR>\n");
    }
printf("<B>3D Structure of Close Homologs:</B> ");
homologCount = 0;
strcpy(goodSCOPdomain, "dummy");

conn2= hAllocConn(database);
safef(query2, sizeof(query2),
	"select homologID,eValue,SCOPdomain,chain from sc1.protHomolog where proteinID='%s' and evalue <= 0.01;",
	item);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
if (row2 != NULL)
    {
    while (row2 != NULL)
	{
	homologID = row2[0];
	sscanf(row2[1], "%e", &eValue);
	SCOPdomain = row2[2];
	chp = SCOPdomain+strlen(SCOPdomain)-1;
	while (*chp != '.') chp--;
	*chp = '\0';
	chain = row2[3];
	if (eValue <= 1.0e-10)
	    strcpy(goodSCOPdomain, SCOPdomain);
	else
	    {
	    if (strcmp(goodSCOPdomain,SCOPdomain) != 0)
		goto skip;
	    else
		if (eValue > 0.1) goto skip;
	    }
	if (first)
	    first = 0;
	else
	    printf(", ");

	printf("<A HREF=\"http://www.rcsb.org/pdb/cgi/explore.cgi?job=graphics&pdbId=%s",
	       homologID);
	if (strlen(chain) >= 1)
	    printf("\"TARGET=_blank>%s(chain %s)</A>", homologID, chain);
	else
	    printf("\"TARGET=_blank>%s</A>", homologID);
	homologCount++;

	skip:
	row2 = sqlNextRow(sr2);
	}
    }
hFreeConn(&conn2);
sqlFreeResult(&sr2);
if (homologCount == 0)
    printf("None<BR>\n");

printf("<BR><B>Details:</B> ");
printf("<A HREF=\"../SARS/%s/summary.html", item);
printf("\" TARGET=_blank>%s</A><BR>\n", item);

htmlHorizontalLine();
}

void doH1n1Seq(struct trackDb *tdb, char *item)
/* Show extra info for H1N1 Seq  Annotations track. */
{
struct sqlConnection *conn  = hAllocConn(database);
struct sqlResult *sr;
char query[256];
char **row;
genericHeader(tdb, item);

/*pslList = getAlignments(conn, track, item);
printAlignmentsSimple(pslList, start, "h1n1Seq", track, item);
*/
sprintf(query, "select * from h1n1SeqXref where seqId = '%s'", item);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    char *seqId, *geneSymbol, *strain;

    seqId = row[0];
    geneSymbol = row[1];
    strain = row[2];

    printf("<B>Sequence ID: %s</B> <BR>", seqId);
    printf("<B>Gene: %s</B> <BR>", geneSymbol);
    printf("<B>Strain: %s</B> <BR>", strain);
    }
htmlHorizontalLine();
//showSAM_h1n1(item);
showProtH1n1(item);

htmlHorizontalLine();
printTrackHtml(tdb);

sqlFreeResult(&sr);
hFreeConn(&conn);
}

