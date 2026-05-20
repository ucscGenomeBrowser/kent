/* blat - /blat endpoint: run a BLAT against an assembly's gfServer and
 * return PSL hits as JSON (or PSL text).  This is the API-callable twin
 * of hgBlat?output=json; hgBlat's CGI interface and behavior remain unchanged.
 *
 * NOTE: Much of the alignment logic here (server lookup, sequence filtering,
 * gfAlign* calls, temp-file round-trip) is derived from hgBlat.c.  If you
 * fix a bug or change behaviour there, check whether this file needs the
 * same fix.  See also the reciprocal note in hgBlat.c. */

#include "dataApi.h"
#include "blatServers.h"
#include "fa.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "psl.h"
#include "trashDir.h"
#include "genoFind.h"
#include "trackHub.h"
#include "hubConnect.h"
#include "hdb.h"
#include "fuzzyFind.h"
#include "botDelay.h"

/* Default BLAT bottleneck fraction -- 10x the hubApi-wide default of 0.03.
 * Overridable in hg.conf with hubApi.blatDelayFraction=<value>. */
#define blatDelayFractionDefault 0.3

/* Server lookup is provided by findBlatServer() in hg/lib/blatServers.c,
 * which is also used by hgBlat.c. */

/* Validated query-type token derived from the user's "type" arg. */
struct blatType
    {
    boolean isTx;       /* translated query */
    boolean isTxTx;     /* both query and target translated (dna vs dnax) */
    boolean txTxBoth;   /* dnax query -- search both strands */
    boolean qIsProt;
    enum gfType qType;
    enum gfType tType;
    };

static void parseTypeArg(char *type, struct dnaSeq *firstSeq, struct blatType *bt)
/* Translate the URL path subcommand into gfType flags.  Caller passes the
 * lowercase token taken from /blat/<type>.  "guess" means infer from the
 * first sequence (matches hgBlat's "BLAT's guess"). */
{
ZeroVar(bt);
if (isEmpty(type))
    apiErrAbort(err400, err400Msg,
        "/blat requires a query-type subcommand: "
        "/blat/dna, /blat/protein, /blat/transRna, /blat/transDna, or /blat/guess");
else if (sameWord(type, "dna"))
    ;  /* defaults */
else if (sameWord(type, "protein"))
    {
    bt->isTx = TRUE;
    bt->qIsProt = TRUE;
    }
else if (sameWord(type, "transRna"))
    {
    bt->isTx = TRUE;
    bt->isTxTx = TRUE;
    }
else if (sameWord(type, "transDna"))
    {
    bt->isTx = TRUE;
    bt->isTxTx = TRUE;
    bt->txTxBoth = TRUE;
    }
else if (sameWord(type, "guess"))
    {
    if (firstSeq != NULL)
        {
        bt->isTx = !seqIsDna(firstSeq);
        bt->qIsProt = bt->isTx;
        }
    }
else
    apiErrAbort(err400, err400Msg,
        "do not recognize endpoint function: '/blat/%s' "
        "(use dna, protein, transRna, transDna, or guess)", type);
if (bt->isTx)
    {
    if (bt->isTxTx)
        {
        bt->qType = gftDnaX;
        bt->tType = gftDnaX;
        }
    else
        {
        bt->qType = gftProt;
        bt->tType = gftDnaX;
        }
    }
else
    {
    bt->qType = gftDna;
    bt->tType = gftDna;
    }
}

static void filterSequences(struct dnaSeq *seqList, struct blatType *bt)
/* Apply the same per-seq filtering hgBlat does before submitting to the server. */
{
struct dnaSeq *seq;
if (bt->isTx && !bt->isTxTx)
    {
    for (seq = seqList; seq != NULL; seq = seq->next)
        {
        seq->size = aaFilteredSize(seq->dna);
        aaFilter(seq->dna, seq->dna);
        toUpperN(seq->dna, seq->size);
        }
    }
else
    {
    for (seq = seqList; seq != NULL; seq = seq->next)
        {
        seq->size = dnaFilteredSize(seq->dna);
        dnaFilter(seq->dna, seq->dna);
        toLowerN(seq->dna, seq->size);
        subChar(seq->dna, 'u', 't');
        }
    }
if (seqList != NULL && seqList->name[0] == 0)
    {
    freeMem(seqList->name);
    seqList->name = cloneString("YourSeq");
    }
}

static void writePslOutput(struct psl *pslList, struct blatType *bt)
/* PSL text output path (output=psl). */
{
hPrintDisable();
puts("Content-Type:text/plain\n");
pslxWriteHead(stdout, bt->qType, bt->tType);
struct psl *psl;
int n = 0;
for (psl = pslList; psl != NULL && n < maxItemsOutput; psl = psl->next, ++n)
    pslTabOut(psl, stdout);
}

static void writeLegacyJsonOutput(struct psl *pslList, char *db)
/* Byte-for-byte the same JSON shape hgBlat?output=json emits: a top-level
 * object with "track":"blat", "genome", a "fields" header array, and "blat"
 * as an array of arrays (one row per PSL).
 * Triggered by format=hgblat or jsonOutputArrays=1. */
{
hPrintDisable();
puts("Content-Type:text/plain\n");
pslWriteAllJson(pslList, stdout, db, TRUE);
}

static void writePslAsObject(struct jsonWrite *jw, struct psl *psl)
/* Write one PSL hit as a JSON object with named keys. */
{
int b;
jsonWriteObjectStart(jw, NULL);
jsonWriteNumber(jw, "matches", psl->match);
jsonWriteNumber(jw, "misMatches", psl->misMatch);
jsonWriteNumber(jw, "repMatches", psl->repMatch);
jsonWriteNumber(jw, "nCount", psl->nCount);
jsonWriteNumber(jw, "qNumInsert", psl->qNumInsert);
jsonWriteNumber(jw, "qBaseInsert", psl->qBaseInsert);
jsonWriteNumber(jw, "tNumInsert", psl->tNumInsert);
jsonWriteNumber(jw, "tBaseInsert", psl->tBaseInsert);
jsonWriteStringf(jw, "strand", "%s", psl->strand);
jsonWriteString(jw, "qName", psl->qName);
jsonWriteNumber(jw, "qSize", psl->qSize);
jsonWriteNumber(jw, "qStart", psl->qStart);
jsonWriteNumber(jw, "qEnd", psl->qEnd);
jsonWriteString(jw, "tName", psl->tName);
jsonWriteNumber(jw, "tSize", psl->tSize);
jsonWriteNumber(jw, "tStart", psl->tStart);
jsonWriteNumber(jw, "tEnd", psl->tEnd);
jsonWriteNumber(jw, "blockCount", psl->blockCount);
jsonWriteListStart(jw, "blockSizes");
for (b = 0; b < psl->blockCount; ++b)
    jsonWriteNumber(jw, NULL, psl->blockSizes[b]);
jsonWriteListEnd(jw);
jsonWriteListStart(jw, "qStarts");
for (b = 0; b < psl->blockCount; ++b)
    jsonWriteNumber(jw, NULL, psl->qStarts[b]);
jsonWriteListEnd(jw);
jsonWriteListStart(jw, "tStarts");
for (b = 0; b < psl->blockCount; ++b)
    jsonWriteNumber(jw, NULL, psl->tStarts[b]);
jsonWriteListEnd(jw);
jsonWriteObjectEnd(jw);
}

static void writePslAsArray(struct jsonWrite *jw, struct psl *psl)
/* Write one PSL hit as a JSON array (jsonOutputArrays mode).
 * Block arrays are integer arrays, matching hubApi conventions. */
{
int b;
jsonWriteListStart(jw, NULL);
jsonWriteNumber(jw, NULL, psl->match);
jsonWriteNumber(jw, NULL, psl->misMatch);
jsonWriteNumber(jw, NULL, psl->repMatch);
jsonWriteNumber(jw, NULL, psl->nCount);
jsonWriteNumber(jw, NULL, psl->qNumInsert);
jsonWriteNumber(jw, NULL, psl->qBaseInsert);
jsonWriteNumber(jw, NULL, psl->tNumInsert);
jsonWriteNumber(jw, NULL, psl->tBaseInsert);
jsonWriteString(jw, NULL, psl->strand);
jsonWriteString(jw, NULL, psl->qName);
jsonWriteNumber(jw, NULL, psl->qSize);
jsonWriteNumber(jw, NULL, psl->qStart);
jsonWriteNumber(jw, NULL, psl->qEnd);
jsonWriteString(jw, NULL, psl->tName);
jsonWriteNumber(jw, NULL, psl->tSize);
jsonWriteNumber(jw, NULL, psl->tStart);
jsonWriteNumber(jw, NULL, psl->tEnd);
jsonWriteNumber(jw, NULL, psl->blockCount);
jsonWriteListStart(jw, NULL);
for (b = 0; b < psl->blockCount; ++b)
    jsonWriteNumber(jw, NULL, psl->blockSizes[b]);
jsonWriteListEnd(jw);
jsonWriteListStart(jw, NULL);
for (b = 0; b < psl->blockCount; ++b)
    jsonWriteNumber(jw, NULL, psl->qStarts[b]);
jsonWriteListEnd(jw);
jsonWriteListStart(jw, NULL);
for (b = 0; b < psl->blockCount; ++b)
    jsonWriteNumber(jw, NULL, psl->tStarts[b]);
jsonWriteListEnd(jw);
jsonWriteListEnd(jw);
}

static char *pslFieldNames[] = {
    "matches", "misMatches", "repMatches", "nCount",
    "qNumInsert", "qBaseInsert", "tNumInsert", "tBaseInsert",
    "strand", "qName", "qSize", "qStart", "qEnd",
    "tName", "tSize", "tStart", "tEnd",
    "blockCount", "blockSizes", "qStarts", "tStarts",
    NULL
};

static void writeJsonOutput(struct psl *pslList, char *db, char *hubUrl,
    char *type, struct blatType *bt)
/* JSON output path -- standard hubApi envelope plus a blat[] array.
 * When jsonOutputArrays is set, each hit is an array and a "fields" key
 * lists the column names (matching getData's jsonOutputArrays behaviour).
 * Otherwise each hit is a named-key object. */
{
struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "genome", db);
if (isNotEmpty(hubUrl))
    jsonWriteString(jw, "hubUrl", hubUrl);
if (isNotEmpty(type))
    jsonWriteString(jw, "type", type);
jsonWriteString(jw, "qType",
    bt->qType == gftProt ? "protein" : (bt->qType == gftDnaX ? "dnax" : "dna"));
jsonWriteString(jw, "tType",
    bt->tType == gftDnaX ? "dnax" : "dna");

if (jsonOutputArrays)
    {
    jsonWriteListStart(jw, "fields");
    char **fp;
    for (fp = pslFieldNames; *fp != NULL; ++fp)
        jsonWriteString(jw, NULL, *fp);
    jsonWriteListEnd(jw);
    }

jsonWriteListStart(jw, "blat");
struct psl *psl;
long long count = 0;
for (psl = pslList; psl != NULL && count < maxItemsOutput; psl = psl->next, ++count)
    {
    if (jsonOutputArrays)
        writePslAsArray(jw, psl);
    else
        writePslAsObject(jw, psl);
    }
jsonWriteListEnd(jw);
itemsReturned = count;
jsonWriteNumber(jw, "itemsReturned", (long long)count);
apiFinishOutput(0, NULL, jw);
}

void apiBlat(char *words[MAX_PATH_INFO])
/* '/blat' endpoint: run a BLAT alignment of userSeq against the requested
 * assembly's gfServer and return PSL hits as JSON. */
{
char *extraArgs = verifyLegalArgs(argBlat);
if (extraArgs)
    apiErrAbort(err400, err400Msg,
        "extraneous arguments found for function /blat '%s'", extraArgs);

/* /blat is gated on an apiKey -- both for attribution and to anchor the
 * bot-bottleneck on something more stable than IP.  Known programmatic
 * clients (IGV, rtracklayer, etc.) identified via bottleneck.except IPs or
 * noCaptchaAgent. user-agent patterns in hg.conf are exempt.
 * For everyone else, an apiKey must be present; validity was already
 * checked in main(). */
char *apiKey = cgiOptionalString(argApiKey);
if (isEmpty(apiKey) && !botException() && !botExceptionUserAgent())
    apiErrAbort(err403, err403Msg,
        "/blat requires an '%s' URL parameter. "
        "Generate one under My Data > My Track Hubs > Hub Development: API Key, "
        "then add it to this API call as apiKey=xxxxx. "
        "Contact us if you need assistance.", argApiKey);

/* Apply a per-BLAT bottleneck penalty on top of the global hubApi delay
 * already paid in main().  Bottleneck key is the apiKey (see botDelay.c),
 * so heavy users throttle themselves instead of starving everyone. */
char *blatDelayStr = cfgOptionDefault("hubApi.blatDelayFraction", NULL);
double blatDelayFraction = blatDelayStr ? atof(blatDelayStr) : blatDelayFractionDefault;
int extraDelay = hgBotDelayTimeFrac(blatDelayFraction);
if (extraDelay > 0)
    sleep1000(extraDelay);

/* Query-type subcommand comes from the URL path, like /getData/track.
 * Apache rewrites /blat/<type>?... onto PATH_INFO=/blat/<type>, so
 * words[1] holds the user-supplied subcommand. */
char *type = words[1];

char *genome = cgiOptionalString(argGenome);
char *userSeq = cgiOptionalString(argUserSeq);
char *format = cgiOptionalString(argFormat);
char *hubUrl = cgiOptionalString(argHubUrl);

if (isEmpty(genome))
    apiErrAbort(err400, err400Msg,
        "/blat requires '%s=<assembly>'", argGenome);
if (isEmpty(userSeq))
    apiErrAbort(err400, err400Msg,
        "/blat requires '%s=<sequence>' (FASTA or raw)", argUserSeq);

/* Cap output volume.  maxItemsOutput is shared with the rest of hubApi
 * and is already initialized from the URL by hubApi.c. */

/* If a hubUrl is given, attach the hub so trackHubDatabase() recognizes it. */
if (isNotEmpty(hubUrl))
    {
    (void) errCatchTrackHubOpen(hubUrl);
    }

/* Parse the user's sequence so we can autodetect type when needed. */
struct dnaSeq *seqList = faSeqListFromMemTextRaw(cloneString(userSeq));
if (seqList == NULL)
    apiErrAbort(err400, err400Msg, "no parseable sequence in '%s'", argUserSeq);

struct blatType bt;
parseTypeArg(type, seqList, &bt);
filterSequences(seqList, &bt);

struct blatServerParams *st = findBlatServer(genome, bt.isTx);
if (st == NULL)
    apiErrAbort(err400, err400Msg,
        "no %s BLAT server configured for genome='%s'",
        bt.isTx ? "translated" : "DNA", genome);

/* Run alignments into a temp pslx, then read it back to drive output.
 * Mirrors hgBlat's strategy so we benefit from the same gfOutputPsl path. */
struct tempName pslTn;
trashDirFile(&pslTn, "apiBlat", "apiBlat", ".pslx");

int maxSeqCount = 25;
char *optionMaxSeqCount = cfgOptionDefault("hgBlat.maxSequenceCount", NULL);
if (isNotEmpty(optionMaxSeqCount))
    maxSeqCount = atoi(optionMaxSeqCount);

FILE *f = NULL;
struct errCatch *ec = errCatchNew();
if (errCatchStart(ec))
    {
    f = mustOpen(pslTn.forCgi, "w");
    struct gfOutput *gvo = gfOutputPsl(0, bt.qIsProt, FALSE, f, FALSE, TRUE);
    pslxWriteHead(f, bt.qType, bt.tType);

    struct gfConnection *conn = gfConnect(st->host, st->port,
        trackHubDatabaseToGenome(st->db), st->genomeDataDir);
    struct hash *tFileCache = gfFileCacheNew();
    int minMatch = 0;  /* let gfServer decide; matches hgBlat allResults path */

    struct dnaSeq *seq;
    int singleMax = bt.isTx ? 10000 : 75000;
    int totalMax  = singleMax * 2.5;
    int total = 0;
    int seqCount = 0;
    for (seq = seqList; seq != NULL; seq = seq->next)
        {
        if (++seqCount > maxSeqCount)
            break;
        if (seq->size <= 0 || seq->size > singleMax)
            continue;
        total += seq->size;
        if (total > totalMax)
            break;
        if (bt.isTx)
            {
            if (bt.isTxTx)
                {
                gfAlignTransTrans(conn, st->nibDir, seq, FALSE, 5, tFileCache, gvo,
                    !bt.txTxBoth);
                if (bt.txTxBoth)
                    {
                    reverseComplement(seq->dna, seq->size);
                    gfAlignTransTrans(conn, st->nibDir, seq, TRUE, 5, tFileCache, gvo,
                        FALSE);
                    }
                }
            else
                gfAlignTrans(conn, st->nibDir, seq, 5, tFileCache, gvo);
            }
        else
            {
            gfAlignStrand(conn, st->nibDir, seq, FALSE, minMatch, tFileCache, gvo);
            reverseComplement(seq->dna, seq->size);
            gfAlignStrand(conn, st->nibDir, seq, TRUE, minMatch, tFileCache, gvo);
            }
        gfOutputQuery(gvo, f);
        }
    carefulClose(&f);
    f = NULL;
    gfFileCacheFree(&tFileCache);
    gfDisconnect(&conn);
    }
errCatchEnd(ec);
if (ec->gotError)
    {
    if (f != NULL)
        carefulClose(&f);
    remove(pslTn.forCgi);
    apiErrAbort(err500, err500Msg, "BLAT server error: %s", ec->message->string);
    }
errCatchFree(&ec);

struct lineFile *lf = pslFileOpen(pslTn.forCgi);
struct psl *pslList = NULL, *psl;
while ((psl = pslNext(lf)) != NULL)
    slAddHead(&pslList, psl);
lineFileClose(&lf);
slReverse(&pslList);

if (sameWordOk(format, "text") || sameWordOk(format, "psl"))
    writePslOutput(pslList, &bt);
else if (sameWordOk(format, "hgblat"))
    writeLegacyJsonOutput(pslList, st->db);
else
    writeJsonOutput(pslList, st->db, hubUrl, type, &bt);
}
