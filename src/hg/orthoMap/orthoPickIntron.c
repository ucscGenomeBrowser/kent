/** orthoPickIntron.c - Pick an intron for RACE PCR based off of the orthoEvaluate
    records loaded.
    - Coding potenttial (Using Victor Solovyev's bestorf repeatedly.)
    - Overlap with native mRNAs/ESTs
    NOTE: THIS PROGRAM UNDER DEVLOPMENT. NOT FOR PRODUCTION USE!
*/
#include "common.h"
#include "hdb.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"
#include "bed.h"
#include "borf.h"
#include "dnaseq.h"
#include "fa.h"
#include "portable.h"
#include "dystring.h"
#include "altGraphX.h"
#include "bits.h"
#include "dnautil.h"
#include "orthoEval.h"

static char const rcsid[] = "$Id: orthoPickIntron.c,v 1.2 2003/06/24 05:53:49 sugnet Exp $";

struct intronEv
/** Data about one intron. */
{
    struct intronEv *next; /* Next in list. */
    struct orthoEval *ev;  /* Parent orthoEval structure. */
    char *chrom;           /* Chromosome. */
    unsigned int           /* Exon starts and stops on chromsome. */
        e1S, e1E, 
	e2S, e2E;
    char *agxName;         /* Name of best fitting native agx record. Memory not owned here. */
    int support;           /* Number of mRNAs supporting this list. */
    int orientation;       /* Orientation of intron. */
    boolean inCodInt;         /* TRUE if in coding region, FALSE otherwise. */
    float borfScore;       /* Score for ev from borf program. */
};

static boolean doHappyDots;   /* output activity dots? */

static struct optionSpec optionSpecs[] = 
/* Our acceptable options to be called with. */
{
    {"help", OPTION_BOOLEAN},
    {"db", OPTION_STRING},
    {"orthoEvalFile", OPTION_STRING},
    {"bedOutFile", OPTION_STRING},
    {"orthoBedOut", OPTION_STRING},
    {"orthoEvalOut", OPTION_STRING},
    {"htmlFile", OPTION_STRING},
    {"htmlFrameFile", OPTION_STRING},
    {"scoreFile", OPTION_STRING},
    {"numPicks", OPTION_INT},
    {NULL, 0}
};

static char *optionDescripts[] = 
/* Description of our options for usage summary. */
{
    "Display this message.",
    "Database (i.e. hg15) that bedFile corresponds to.",
    "File to input orthoEval records from.",
    "File to output final bed picks for track.",
    "File to output mapped orthoBeds that are used.",
    "File to output evaluation records.",
    "File for html output.",
    "[optional default: frame.html] File for html frames.",
    "[optional default: not created] Print scores for each intron. orient,support,borf,coding",
    "[optional default: 100] Number of introns to pick."
};

void usage()
/** Print usage and quit. */
{
int i=0;
char *version = cloneString((char*)rcsid);
char *tmp = strstr(version, "orthoPickIntron.c,v ");
if(tmp != NULL)
    version = tmp + strlen("orthoPickIntron.c,v ");
tmp = strrchr(version, 'E');
if(tmp != NULL)
    (*tmp) = '\0';
warn("orthoPickIntron - Pick best intron from orthoEval.\n"
     "   (version: %s)", version );
for(i=0; i<ArraySize(optionSpecs) -1; i++)
    fprintf(stderr, "  -%s -- %s\n", optionSpecs[i].name, optionDescripts[i]);
errAbort("\nusage:\n"
	 "orthoPickIntron -db=hg15 -orthoEvalFile=orthoEval.tab -bedOutFile=tmp.bed -orthoBedOut=tmp.ortho.bed \\\n"
	 "  -htmlFile=tmp.html -htmlFrameFile=opi.html\n"
	 "  NOTE: THIS PROGRAM UNDER DEVLOPMENT. NOT FOR PRODUCTION USE!\n");
}

void occassionalDot()
/* Write out a dot every 20 times this is called. */
{
static int dotMod = 100;
static int dot = 100;
if (doHappyDots && (--dot <= 0))
    {
    putc('.', stdout);
    fflush(stdout);
    dot = dotMod;
    }
}

struct intronEv *intronIvForEv(struct orthoEval *ev, int intron)
/* Return an intronEv record for a particular intron. */
{
struct intronEv *iv = NULL;
struct bed *bed = NULL;
assert(ev);
assert(ev->orthoBed);
assert(intron < ev->orthoBed->blockCount - 1);
bed = ev->orthoBed;
AllocVar(iv);
iv->ev = ev;
iv->chrom = bed->chrom;

iv->e1S = bed->chromStart + bed->chromStarts[intron];
iv->e1E = iv->e1S + bed->blockSizes[intron];
iv->e2S = bed->chromStart + bed->chromStarts[intron+1];
iv->e2E = iv->e2S + bed->blockSizes[intron+1];

iv->agxName = ev->agxNames[intron];
iv->support = ev->support[intron];
iv->orientation = ev->orientation[intron];
iv->inCodInt = ev->inCodInt[intron];
iv->borfScore = ev->borf->score;
return iv;
}

int scoreForIntronEv(const struct intronEv *iv)
/* Score function based on native support, orientation, and coding. */
{
static FILE *out = NULL;
int score = 0;
char *scoreFile = optionVal("scoreFile", NULL);
if(out == NULL && scoreFile != NULL)
    out = mustOpen(scoreFile, "w");
if(out != NULL)
    fprintf(out, "%d\t%d\t%f\t%d\n", iv->orientation, iv->support, iv->borfScore, iv->inCodInt);
if(iv->orientation != 0)
    score += 5;
score += iv->support *2;
score += (float) iv->borfScore/5;
if(iv->inCodInt)
    score = score +2;
return score;
}

boolean checkMgcPicks(struct intronEv *iv)
/* Try to look up an mgc pick in this area. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
int rowOffset = 0;
char *mgcTable = optionVal("mgcTable", "chuckMgcPicked");
char *mgcGenes = optionVal("mgcGenes", "chuckMgcGenes");
char *mgcAList = optionVal("mgcAList", "chuckMgcAList");
boolean foundSome = FALSE;
sr = hRangeQuery(conn, mgcTable, iv->chrom, iv->e1S, iv->e2E, NULL, &rowOffset); 
if(sqlNextRow(sr) != NULL)
    foundSome = TRUE;
sqlFreeResult(&sr);
if(foundSome == FALSE)
    {
    sr = hRangeQuery(conn, mgcGenes, iv->chrom, iv->e1S, iv->e2E, NULL, &rowOffset); 
    if(sqlNextRow(sr) != NULL)
	foundSome = TRUE;
    sqlFreeResult(&sr);
    }
if(foundSome == FALSE)
    {
    sr = hRangeQuery(conn, mgcAList, iv->chrom, iv->e1S, iv->e2E, NULL, &rowOffset); 
    if(sqlNextRow(sr) != NULL)
	foundSome = TRUE;
    sqlFreeResult(&sr);
    }

hFreeConn(&conn);
return foundSome;
}

boolean isOverlappedByRefSeq(struct intronEv *iv)
/* Return TRUE if there is a refSeq overlapping. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
int rowOffset = 0;
char *refSeqTable = "refGene";
boolean foundSome = FALSE;
sr = hRangeQuery(conn, refSeqTable, iv->chrom, iv->e1S, iv->e2E, NULL, &rowOffset); 
if(sqlNextRow(sr) != NULL)
    foundSome = TRUE;
sqlFreeResult(&sr);
hFreeConn(&conn);
return foundSome;
}

int intronEvalCmp(const void *va, const void *vb)
/* Compare to sort based score function. */
{
const struct intronEv *a = *((struct intronEv **)va);
const struct intronEv *b = *((struct intronEv **)vb);
float aScore = scoreForIntronEv(a);
float bScore = scoreForIntronEv(b);
return bScore - aScore; /* reverse to get largest values first. */
}

boolean isUniqueCoordAndAgx(struct intronEv *iv, struct hash *posHash, struct hash *agxHash)
/* Return TRUE if iv isn't in posHash and agxHash.
   Return FALSE otherwise. */
{
static char key[1024];
boolean unique = TRUE;

safef(key, sizeof(key), "%s-%d-%d-%d-%d", iv->chrom, iv->e1S, iv->e1E, iv->e2S, iv->e2E);
/* Unique location (don't pick same intron twice. */
if(hashFindVal(posHash, key) == NULL)
    hashAdd(posHash, key, iv);
else 
    unique = FALSE;

/* Unique loci, don't pick from same overall loci if possible. */
safef(key, sizeof(key), "%s", iv->agxName);
if(hashFindVal(agxHash, key) == NULL)
    hashAdd(agxHash, key, iv);
else
    unique = FALSE;

/* Definitely don't pick from same mRNA. */
safef(key, sizeof(key), "%s", iv->ev->orthoBedName);
if(hashFindVal(agxHash, key) == NULL)
    hashAdd(agxHash, key, iv);
else
    unique = FALSE;

if(unique)
    unique = !checkMgcPicks(iv);

return unique;
}

struct bed *bedForIv(struct intronEv *iv)
/* Make a bed to represent a intronEv structure. */
{
struct bed *bed = NULL;
AllocVar(bed);
bed->chrom = cloneString(iv->chrom);
bed->chromStart = bed->thickStart = iv->e1S;
bed->chromEnd = bed->thickEnd = iv->e2E;
bed->name = cloneString(iv->ev->orthoBed->name);
if(iv->orientation == 1)
    safef(bed->strand, sizeof(bed->strand), "+");
else if(iv->orientation == -1)
    safef(bed->strand, sizeof(bed->strand), "-");
else 
    safef(bed->strand, sizeof(bed->strand), "%s", iv->ev->orthoBed->strand);
bed->blockCount = 2;
AllocArray(bed->chromStarts, 2);
bed->chromStarts[0] = iv->e1S - bed->chromStart;
bed->chromStarts[1] = iv->e2S - bed->chromStart;
AllocArray(bed->blockSizes, 2);
bed->blockSizes[0] = iv->e1E - iv->e1S;
bed->blockSizes[1] = iv->e2E - iv->e2S;
bed->score = scoreForIntronEv(iv);
return bed;
}

void writeOutFrames(FILE *htmlOut, char *fileName, char *db)
{
fprintf(htmlOut, "<html><head><title>Introns and flanking exons for RACE PCR</title></head>\n"
	"<frameset cols=\"18%,82%\">\n"
	"<frame name=\"_list\" src=\"./%s\">\n"
	"<frame name=\"browser\" src=\"http://mgc.cse.ucsc.edu/cgi-bin/hgTracks?db=%s&position=chrX:151171710-151173193&"
	"hgt.customText=http://www.soe.ucsc.edu/~sugnet/tmp/mgcIntron.bed\">\n"
	"</frameset>\n"
	"</html>\n", fileName, db);
}

void pickIntrons()
{
char *htmlFileName=NULL, *htmlFrameFileName=NULL;
char *bedFileName=NULL, *orthoBedFileName=NULL;
FILE *htmlOut=NULL, *htmlFrameOut=NULL;
FILE *bedOut=NULL, *orthoBedOut=NULL;
char *orthoEvalFile = NULL;
char *db = NULL;
struct orthoEval *ev=NULL, *evList = NULL;
struct intronEv *iv=NULL, *ivList = NULL;
int maxPicks = optionInt("numPicks", 100);
int i=0;
boolean isRefSeq=FALSE;
struct hash *posHash = newHash(12), *agxHash = newHash(12);
struct bed *bed = NULL;

htmlFileName = optionVal("htmlFile", NULL);
htmlFrameFileName = optionVal("htmlFrameFile", "frame.html");
orthoEvalFile = optionVal("orthoEvalFile", NULL);
db = optionVal("db", NULL);
bedFileName = optionVal("bedOutFile", NULL);
orthoBedFileName = optionVal("orthoBedOut", NULL);
if(htmlFileName == NULL || orthoEvalFile == NULL || db == NULL || 
   bedFileName == NULL || orthoBedFileName == NULL )
    errAbort("Missing parameters. Use -help for usage.");

hSetDb(db);
warn("Loading orthoEvals.");
evList = orthoEvalLoadAll(orthoEvalFile);
warn("Creating intron records");
for(ev = evList; ev != NULL; ev = ev->next)
    {
    for(i=0; i<ev->numIntrons; i++)
	{
	occassionalDot();
	iv = intronIvForEv(ev, i);
	slAddHead(&ivList, iv);
	}
    }
warn("\nDone");
warn("Sorting");
slSort(&ivList, intronEvalCmp);
warn("Done.");
htmlOut = mustOpen(htmlFileName, "w");
bedOut = mustOpen(bedFileName, "w");
htmlFrameOut = mustOpen(htmlFrameFileName, "w");
orthoBedOut = mustOpen(orthoBedFileName, "w");
writeOutFrames(htmlFrameOut, htmlFileName, db);
fprintf(htmlOut, "<html><body><table border=1><tr><th>Mouse Acc.</th><th>Score</th><th>RefSeq</th></tr>\n");
warn("Writing out");
for(iv = ivList; iv != NULL && maxPicks > 0; iv = iv->next)
    {
    if(isUniqueCoordAndAgx(iv, posHash, agxHash))
	{
	bed = bedForIv(iv);
	isRefSeq = isOverlappedByRefSeq(iv);
	fprintf(htmlOut, "<tr><td><a target=\"browser\" "
		"href=\"http://mgc.cse.ucsc.edu/cgi-bin/hgTracks?db=hg15&position=%s:%d-%d\"> "
		"%s </a></td><td>%d</td><td>%s</td></tr>\n", 
		bed->chrom, bed->chromStart-40, bed->chromEnd+50, bed->name, bed->score, isRefSeq ? "yes" : "no");
	bedTabOutN(bed, 12, bedOut);
	bedTabOutN(iv->ev->orthoBed, 12, orthoBedOut);
	bedFree(&bed);
	maxPicks--;
	}
    }
fprintf(htmlOut, "</table></body></html>\n");
carefulClose(&bedOut);
carefulClose(&htmlOut);
carefulClose(&htmlFrameOut);
carefulClose(&orthoBedOut);
warn("Done.");
hashFree(&posHash);
hashFree(&agxHash);
}

int main(int argc, char *argv[])
{
doHappyDots = isatty(1);
if(argc == 1)
    usage();
optionInit(&argc, argv, optionSpecs);
pickIntrons();
return 0;
}
