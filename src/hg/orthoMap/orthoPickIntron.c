/** \page orthoPickIntron.c Pick an intron for exploratory PCR using mouse transcript data.

<h2>Overview:</h2> <p>Mouse mRNAs and ESTs were summarized in a more
compact format and were mapped to orthologous locations in the human
genome using blastz nets and chains created by Jim Kent. Individual
introns and flanking exons were chosen as candidates for exploratory
PCR by the number of mouse transcripts containing the intron, The
objective is to take advantage of the mouse ESTs and mRNAs to
supplement the human ones in looking for novel genes.

<h2>Details:</h2> 
<p><b>Objective:</b>

Construct a summary of all mRNA and EST data in mouse and map them to
human genome via whole genome alignments. Select mouse mapped
transcripts that are not overlapped by native human transcripts for
exploratory PCR.

<h2>Algorithm:</h2>
<ul>

<li>Mouse mRNAs and cDNAs are clustered on the mouse genome and
a summary of the cluster (altGraphX) is generated.</li>

<li>Mouse clusters are mapped via Jim Kent's <a
href="http://mgc.cse.ucsc.edu/cgi-bin/hgTrackUi?&c=chr14&g=mouseNet">
net/chain mapping</a> of the mouse and human genomes to the human
genome. By using the net/chain mapping we can take into account more
sequence than just the individual mRNAs and also take into account
synteny</li>


<li>Each intron and flanking exons are evaluated for the number of mouse
mRNAs and ESTs that mapped over, as well as overlap with exons predicted
by gene finding programs.</li>

<li>Individual introns are selected and the loci are remembered so
that only one intron is chosen from each loci. Also, no intron is 
chosen if it is overlapped by any transcript evidence. </li>

</ul>
</table>
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
#include "rbTree.h"
static char const rcsid[] = "$Id: orthoPickIntron.c,v 1.8 2008/09/03 19:20:52 markd Exp $";

struct intronEv
/** Data about one intron. */
{
    struct intronEv *next; /**< Next in list. */
    struct orthoEval *ev;  /**< Parent orthoEval structure. */
    char *chrom;           /**< Chromosome. */
    unsigned int           /** Exon starts and stops on chromsome. */
        e1S, e1E, 
	e2S, e2E;
    char *agxName;         /**< Name of best fitting native agx record. Memory not owned here. */
    int support;           /**< Number of mRNAs supporting this list. */
    int orientation;       /**< Orientation of intron. */
    boolean inCodInt;      /**< TRUE if in coding region, FALSE otherwise. */
    float borfScore;       /**< Score for ev from borf program. */
};

static boolean doHappyDots;   /* output activity dots? */

static struct optionSpec optionSpecs[] = 
/** Our acceptable options to be called with. */
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
/** Description of our options for usage summary. */
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
	 "  -htmlFile=tmp.html -htmlFrameFile=opi.html\n");
}

void occassionalDot()
/** Write out a dot every 20 times this is called. */
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

/* int bedCmp(void *va, void *vb) */
/* { */
/* struct bed *a = va; */
/* struct bed *b = vb; */
/* int diff = strcmp(a->chrom, b->chrom); */
/* if(diff == 0) */
/*     { */
/*     if (diff == 0) */
/* 	diff = a->chromStart - b->chromStart; */
/*     if(diff == 0)  */
/* 	diff = a->chromEnd - b->chromEnd; */
/*     } */
/* return diff; */
/* } */

struct intronEv *intronIvForEv(struct orthoEval *ev, int intron)
/** Return an intronEv record for a particular intron. */
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
iv->support = ev->basesOverlap;
iv->orientation = ev->orientation[intron];
iv->inCodInt = ev->inCodInt[intron];
if(sameString(ev->borf->name, "dummy"))
    iv->borfScore = (5*ev->orthoBed->score) + (ev->borf->score * 100);
else
    iv->borfScore = ev->borf->score;
return iv;
}

int scoreForIntronEv(const struct intronEv *iv)
/** Score function based on native support, orientation, and coding. */
{
static FILE *out = NULL;
int score = 0;
char *scoreFile = optionVal("scoreFile", NULL);
if(out == NULL && scoreFile != NULL)
    out = mustOpen(scoreFile, "w");
if(out != NULL)
    fprintf(out, "%d\t%d\t%f\t%d\n", iv->orientation, iv->support, iv->borfScore, iv->inCodInt);
if(iv->orientation != 0)
    score += 200;
// score += iv->support *2;
score += (float) iv->borfScore;
if(iv->inCodInt)
    score = score +2;
return score;
}

boolean checkMgcPicks(char *db, struct intronEv *iv)
/** Try to look up an mgc pick in this area. */
{
struct sqlConnection *conn = hAllocConn(db);
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

boolean coordOverlappedByTable(char *db, char *chrom, int start, int end, char *table)
/** Return TRUE if there is a record overlapping. */
{
struct sqlConnection *conn = hAllocConn(db);
struct sqlResult *sr = NULL;
char **row;
int rowOffset = 0;
boolean foundSome = FALSE;
sr = hRangeQuery(conn, table, chrom, start, end, NULL, &rowOffset); 
if(sqlNextRow(sr) != NULL)
    foundSome = TRUE;
sqlFreeResult(&sr);
hFreeConn(&conn);
return foundSome;
}

boolean isOverlappedByTable(char *db, struct intronEv *iv, char *table)
/** Return TRUE if there is a record overlapping. */
{
return coordOverlappedByTable(db, iv->chrom, iv->e1S, iv->e2E, table);
}

boolean isOverlappedByRefSeq(char *db, struct intronEv *iv)
{
return isOverlappedByTable(db, iv, "refGene");
}

boolean isOverlappedByMgcBad(char *db, struct intronEv *iv)
{
return isOverlappedByTable(db, iv, "chuckMgcBad");
}

boolean isOverlappedByEst(char *db, struct intronEv *iv)
{
return isOverlappedByTable(db, iv, "est");
}

boolean isOverlappedByMRna(char *db, struct intronEv *iv)
{
return isOverlappedByTable(db, iv, "mrna");
}

int intronEvalCmp(const void *va, const void *vb)
/** Compare to sort based score function. */
{
const struct intronEv *a = *((struct intronEv **)va);
const struct intronEv *b = *((struct intronEv **)vb);
float aScore = scoreForIntronEv(a);
float bScore = scoreForIntronEv(b);
return bScore - aScore; /* reverse to get largest values first. */
}

boolean bedUniqueInTree(struct rbTree *bedTree, struct intronEv *iv)
{
static struct bed searchLo, searchHi; 
struct slRef *refList = NULL, *ref = NULL;
searchLo.chrom = iv->chrom;
searchLo.chromStart =iv->e1S;
searchLo.chromEnd = iv->e2E;

/* searchHi.chrom = iv->chrom; */
/* searchHi.chromStart = iv->e2E; */
/* searchHi.chromEnd = iv->e2E; */
refList = rbTreeItemsInRange(bedTree, &searchLo, &searchLo);
if(refList == NULL)
    return TRUE;
else
    slFreeList(&refList);
return FALSE;
}

int bedRangeCmp(void *va, void *vb)
{
struct bed *a = va;
struct bed *b = vb;
int diff = strcmp(a->chrom, b->chrom);
if(diff == 0)
    {
    if(a->chromEnd <= b->chromStart)
	diff = -1;
    else if(a->chromStart >= b->chromEnd)
	diff = 1;
    else 
	diff = 0;
    }
return diff;
}

boolean isUniqueCoordAndAgx(char *db, struct intronEv *iv, struct hash *posHash, struct hash *agxHash)
/** Return TRUE if iv isn't in posHash and agxHash.
   Return FALSE otherwise. */
{
static char key[1024];
static struct rbTree *bedTree = NULL;
boolean unique = TRUE;
struct bed *bed = NULL;
if(bedTree == NULL)
    bedTree = rbTreeNew(bedRangeCmp);
/* Unique location (don't pick same intron twice. */
if(bedUniqueInTree(bedTree, iv))
    {
    AllocVar(bed);
    bed->chrom = cloneString(iv->chrom);
    bed->chromStart = iv->e1S;
    bed->chromEnd = iv->e2E;
    rbTreeAdd(bedTree, bed);
    }
else 
    unique = FALSE;

/* Unique loci, don't pick from same overall loci if possible. */
safef(key, sizeof(key), "%s", iv->agxName);
if(hashFindVal(agxHash, key) == NULL)
    hashAdd(agxHash, key, iv);
else
    unique = FALSE;


/* Definitely don't pick from same mRNA. */
chopSuffix(iv->ev->orthoBedName);
safef(key, sizeof(key), "%s", iv->ev->orthoBedName);
if(hashFindVal(agxHash, key) == NULL)
    hashAdd(agxHash, key, iv);
else
    unique = FALSE;

if(unique)
    unique = !checkMgcPicks(db, iv);

return unique;
}

struct bed *bedForIv(struct intronEv *iv)
/** Make a bed to represent a intronEv structure. */
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

void writeOutFrames(FILE *htmlOut, char *fileName, char *db, char *bedFile, char *pos)
/** Write out frame for htmls. */
{
fprintf(htmlOut, "<html><head><title>Introns and flanking exons for exploratory PCR</title></head>\n"
	"<frameset cols=\"18%,82%\">\n"
	"<frame name=\"_list\" src=\"./%s\">\n"
	"<frame name=\"browser\" src=\"http://mgc.cse.ucsc.edu/cgi-bin/hgTracks?db=%s&position=%s&"
	"hgt.customText=http://www.soe.ucsc.edu/~sugnet/mgc/%s\">\n"
	"</frameset>\n"
	"</html>\n", fileName, db, pos, bedFile);
}

void pickIntrons()
/** Top level routine, actually picks the introns. */
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
boolean isRefSeq=FALSE, isMgcBad=FALSE;
struct hash *posHash = newHash(12), *agxHash = newHash(12);
struct bed *bed = NULL;
char buff[256];

htmlFileName = optionVal("htmlFile", NULL);
htmlFrameFileName = optionVal("htmlFrameFile", "frame.html");
orthoEvalFile = optionVal("orthoEvalFile", NULL);
db = optionVal("db", NULL);
bedFileName = optionVal("bedOutFile", NULL);
orthoBedFileName = optionVal("orthoBedOut", NULL);
if(htmlFileName == NULL || orthoEvalFile == NULL || db == NULL || 
   bedFileName == NULL || orthoBedFileName == NULL )
    errAbort("Missing parameters. Use -help for usage.");

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
i=0;
fprintf(htmlOut, "<html><body><table border=1><tr><th>Num</th><th>Mouse Acc.</th><th>Score</th><th>TS Pick</th></tr>\n");
warn("Filtering");
safef(buff, sizeof(buff), "tmp");
for(iv = ivList; iv != NULL && maxPicks > 0; iv = iv->next)
    {
    if(isUniqueCoordAndAgx(db, iv, posHash, agxHash) && iv->support == 0 && !isOverlappedByRefSeq(db, iv) &&
       ! isOverlappedByEst(db, iv) && ! isOverlappedByMRna(db, iv))
	{
	boolean twinScan = (coordOverlappedByTable(db, iv->chrom, iv->e1S, iv->e1E, "mgcTSExpPcr") &&
			    coordOverlappedByTable(db, iv->chrom, iv->e2S, iv->e2E, "mgcTSExpPcr"));
	bed = bedForIv(iv);
	if(sameString(buff, "tmp"))
	    safef(buff, sizeof(buff), "%s:%d-%d", bed->chrom, bed->chromStart-50, bed->chromEnd+50);
//	isMgcBad = isOverlappedByMgcBad(iv);
	fprintf(htmlOut, "<tr><td>%d</td><td><a target=\"browser\" "
		"href=\"http://mgc.cse.ucsc.edu/cgi-bin/hgTracks?db=hg15&position=%s:%d-%d\"> "
		"%s </a></td><td>%d</td><td>%s</td></tr>\n", 
		++i,bed->chrom, bed->chromStart-50, bed->chromEnd+50, bed->name, bed->score, 
		twinScan ? "yes" : "no");

	bedTabOutN(bed, 12, bedOut);
	bedTabOutN(iv->ev->orthoBed, 12, orthoBedOut);
	bedFree(&bed);
	maxPicks--;
	}
    }
writeOutFrames(htmlFrameOut, htmlFileName, db, bedFileName, buff);
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
/** Main, everybodies favorite function. */
{
doHappyDots = isatty(1);
if(argc == 1)
    usage();
optionInit(&argc, argv, optionSpecs);
pickIntrons();
return 0;
}
