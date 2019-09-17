/* annoGratorGpVar -- integrate pgSNP or VCF with gene pred and make gpFx predictions */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "annoGratorGpVar.h"
#include "annoStreamDbPslPlus.h"
#include "fa.h"
#include "genbank.h"
#include "genePred.h"
#include "hdb.h"
#include "hgHgvs.h"
#include "pgSnp.h"
#include "vcf.h"
#include "variant.h"
#include "gpFx.h"
#include "seqWindow.h"
#include "trackHub.h"
#include "twoBit.h"
#include "variantProjector.h"
#include "annoGratorQuery.h"

struct annoGratorGpVar
{
    struct annoGrator grator;	// external annoGrator/annoStreamer interface
    struct lm *lm;		// localmem scratch storage
    struct dyString *dyScratch;	// dyString for local temporary use
    struct annoGratorGpVarFuncFilter *funcFilter; // Which categories of effect should we output?
    enum annoGratorOverlap gpVarOverlapRule;	  // Should we set RJFail if no overlap?
    struct seqWindow *gSeqWin;  // means of fetching genomic sequence for HGVS term generation
    boolean hgvsMakeG;          // Generate genomic (g.) HGVS terms only if this is set
    boolean hgvsMakeCN;         // Generate transcript (n. / c.) HGVS terms only if this is set
    boolean hgvsMakeP;          // Generate protein (p.) HGVS terms only if this is set
    boolean hgvsAddParensToP;   // Add parentheses around predicted protein changes (strict HGVS)
    boolean hgvsBreakDelIns;    // Include deleted sequence (not only ins) e.g. delGGinsAT
    boolean sourceHasFrames;    // True if transcript annotations include exonFrames column
    boolean sourceIsBigGenePred;// True if transcript annotations are from bigGenePred
    boolean sourceIsPslPlus;    // True if transcript annotations are PSL+CDS+seq info
    boolean protLookupInited;   // For looking up protein accessions in genePred-derived HGVS p.
    char *protLookupTable;      // "
    struct hash *protLookupHash;// "

    struct variant *(*variantFromRow)(struct annoGratorGpVar *self, struct annoRow *row,
				      char *refAllele);
    // Translate row from whatever format it is (pgSnp or VCF) into generic variant.
    };


static char *aggvAutoSqlStringStart =
"table genePredWithSO"
"\"genePred with Sequence Ontology annotation\""
"(";

static char *aggvAutoSqlStringEnd =
"string  allele;             \"Allele used to predict functional effect\""
"string  transcript;         \"ID of affected transcript\""
"uint    soNumber;           \"Sequence Ontology Number \""
"uint    detailType;         \"gpFx detail type (1=codingChange, 2=nonCodingExon, 3=intron)\""
"string  detail0;            \"detail column (per detailType) 0\""
"string  detail1;            \"detail column (per detailType) 1\""
"string  detail2;            \"detail column (per detailType) 2\""
"string  detail3;            \"detail column (per detailType) 3\""
"string  detail4;            \"detail column (per detailType) 4\""
"string  detail5;            \"detail column (per detailType) 5\""
"string  detail6;            \"detail column (per detailType) 6\""
"string  detail7;            \"detail column (per detailType) 7\""
"string  detail8;            \"detail column (per detailType) 8\""
"string  detail9;            \"detail column (per detailType) 9\""
"string  detail10;           \"detail column (per detailType) 10\""
"string  hgvsG;              \"HGVS g. term\""
"string  hgvsCN;             \"HGVS c. or n. term\""
"string  hgvsP;              \"HGVS p. term\""
")";

struct asObject *annoGpVarAsObj(struct asObject *sourceAsObj)
// Return asObject describing fields of internal source plus the fields we add here.
{
struct dyString *gpPlusGpFx = dyStringCreate("%s", aggvAutoSqlStringStart);
// Translate each column back into autoSql text for combining with new output fields:
struct asColumn *col;
for (col = sourceAsObj->columnList;  col != NULL;  col = col->next)
    {
    if (col->fixedSize)
	dyStringPrintf(gpPlusGpFx, "%s[%d]\t%s;\t\"%s\"",
		       col->lowType->name, col->fixedSize, col->name, col->comment);
    else if (col->isArray || col->isList)
	{
	if (col->linkedSizeName)
	    dyStringPrintf(gpPlusGpFx, "%s[%s]\t%s;\t\"%s\"",
			   col->lowType->name, col->linkedSizeName, col->name, col->comment);
	else
	    errAbort("Neither col->fixedSize nor col->linkedSizeName given for "
		     "asObj %s column '%s'",
		     sourceAsObj->name, col->name);
	}
    else
	dyStringPrintf(gpPlusGpFx, "%s\t%s;\t\"%s\"", col->lowType->name, col->name, col->comment);
    }
dyStringAppend(gpPlusGpFx, aggvAutoSqlStringEnd);
struct asObject *asObj = asParseText(gpPlusGpFx->string);
dyStringFree(&gpPlusGpFx);
return asObj;
}

static boolean passesFilter(struct annoGratorGpVar *self, struct gpFx *gpFx)
/* Based on type of effect, should we print this one? */
{
struct annoGratorGpVarFuncFilter *filt = self->funcFilter;
enum soTerm term = gpFx->soNumber;
if (term == NMD_transcript_variant)
    // This one gets special treatment because gpFx->detailType might still be codingChange:
    return filt->nmdTranscript;
if (filt->intron && (term == intron_variant || term == complex_transcript_variant))
    return TRUE;
if (filt->upDownstream && (term == upstream_gene_variant || term == downstream_gene_variant))
    return TRUE;
if (filt->exonLoss && (term == exon_loss_variant))
    return TRUE;
if ((filt->exonLoss || filt->cdsNonSyn) && term == transcript_ablation)
    return TRUE;
if (filt->utr && (term == _5_prime_UTR_variant || term == _3_prime_UTR_variant))
    return TRUE;
if (filt->cdsSyn && term == synonymous_variant)
    return TRUE;
if (filt->cdsNonSyn && term != synonymous_variant
    && (gpFx->detailType == codingChange || term == complex_transcript_variant))
    return TRUE;
if (filt->nonCodingExon && term == non_coding_transcript_exon_variant)
    return TRUE;
if (filt->splice && (term == splice_donor_variant || term == splice_acceptor_variant ||
		     term == splice_region_variant))
    return TRUE;
if (filt->noVariation && term == no_sequence_alteration)
    return TRUE;
return FALSE;
}

static char *blankIfNull(char *input)
{
if (input == NULL)
    return "";

return input;
}

static char *uintToString(struct lm *lm, uint num)
{
char buffer[10];

safef(buffer,sizeof buffer, "%d", num);
return lmCloneString(lm, buffer);
}

static void aggvStringifyGpFx(char **words, struct gpFx *effect, struct lm *lm)
// turn gpFx structure into array of words
{
int count = 0;

words[count++] = lmCloneString(lm, effect->gAllele);
words[count++] = lmCloneString(lm, blankIfNull(effect->transcript));
words[count++] = uintToString(lm, effect->soNumber);
words[count++] = uintToString(lm, effect->detailType);
int gpFxNumCols = 4;

if (effect->detailType == intron)
    {
    words[count++] = uintToString(lm, effect->details.intron.intronNumber);
    words[count++] = uintToString(lm, effect->details.intron.intronCount);
    }
else if (effect->detailType == nonCodingExon)
    {
    words[count++] = uintToString(lm, effect->details.nonCodingExon.exonNumber);
    words[count++] = uintToString(lm, effect->details.nonCodingExon.exonCount);
    words[count++] = uintToString(lm, effect->details.nonCodingExon.cDnaPosition);
    words[count++] = lmCloneString(lm, effect->details.nonCodingExon.txRef);
    words[count++] = lmCloneString(lm, effect->details.nonCodingExon.txAlt);
    }
else if (effect->detailType == codingChange)
    {
    struct codingChange *cc = &(effect->details.codingChange);
    words[count++] = uintToString(lm, cc->exonNumber);
    words[count++] = uintToString(lm, cc->exonCount);
    words[count++] = uintToString(lm, cc->cDnaPosition);
    words[count++] = lmCloneString(lm, cc->txRef);
    words[count++] = lmCloneString(lm, cc->txAlt);
    words[count++] = uintToString(lm, cc->cdsPosition);
    words[count++] = uintToString(lm, cc->pepPosition);
    words[count++] = strUpper(lmCloneString(lm, blankIfNull(cc->aaOld)));
    words[count++] = strUpper(lmCloneString(lm, blankIfNull(cc->aaNew)));
    words[count++] = strUpper(lmCloneString(lm, blankIfNull(cc->codonOld)));
    words[count++] = strUpper(lmCloneString(lm, blankIfNull(cc->codonNew)));
    }
else if (effect->detailType != none)
    errAbort("annoGratorGpVar: unknown effect type %d", effect->detailType);

// Add max number of columns added in any if clause above
gpFxNumCols += 9;

while (count < gpFxNumCols)
    words[count++] = "";
}

struct gpFx *annoGratorGpVarGpFxFromRow(struct annoStreamer *sSelf, struct annoRow *row,
					struct lm *lm)
// Turn the string array representation back into a real gpFx.
// I know this is inefficient and am thinking about a better way.
{
if (row == NULL)
    return NULL;
struct gpFx *effect;
lmAllocVar(lm, effect);
struct annoGrator *gSelf = (struct annoGrator *)sSelf;
// get gpFx words which follow internal source's words:
char **words = (char **)(row->data);
int count = gSelf->mySource->numCols;

effect->gAllele = lmCloneString(lm, words[count++]);
effect->transcript = lmCloneString(lm, words[count++]);
effect->soNumber = atol(words[count++]);
effect->detailType = atol(words[count++]);

if (effect->detailType == intron)
    {
    effect->details.intron.intronNumber = atol(words[count++]);
    effect->details.intron.intronCount = atol(words[count++]);
    }
else if (effect->detailType == nonCodingExon)
    {
    effect->details.nonCodingExon.exonNumber = atol(words[count++]);
    effect->details.nonCodingExon.exonCount = atol(words[count++]);
    effect->details.nonCodingExon.cDnaPosition = atol(words[count++]);
    effect->details.nonCodingExon.txRef = lmCloneString(lm, words[count++]);
    effect->details.nonCodingExon.txAlt = lmCloneString(lm, words[count++]);
    }
else if (effect->detailType == codingChange)
    {
    effect->details.codingChange.exonNumber = atol(words[count++]);
    effect->details.codingChange.exonCount = atol(words[count++]);
    effect->details.codingChange.cDnaPosition = atol(words[count++]);
    effect->details.codingChange.txRef = lmCloneString(lm, words[count++]);
    effect->details.codingChange.txAlt = lmCloneString(lm, words[count++]);
    effect->details.codingChange.cdsPosition = atol(words[count++]);
    effect->details.codingChange.pepPosition = atol(words[count++]);
    effect->details.codingChange.aaOld = lmCloneString(lm, words[count++]);
    effect->details.codingChange.aaNew = lmCloneString(lm, words[count++]);
    effect->details.codingChange.codonOld = lmCloneString(lm, words[count++]);
    effect->details.codingChange.codonNew = lmCloneString(lm, words[count++]);
    }
else if (effect->detailType != none)
    errAbort("annoGratorGpVar: unknown effect type %d", effect->detailType);
return effect;
}

// Container for optional HGVS terms
struct hgvsTerms
    {
    char *hgvsG;     // HGVS g. term or NULL
    char *hgvsCN;    // HGVS c. or n. term or NULL
    char *hgvsP;     // HGVS p. term or NULL
    };

static  void hgvsTermsFree(struct hgvsTerms *hgvs)
/* Free struct hgvsTerms and its members. */
{
if (hgvs)
    {
    freez(&hgvs->hgvsG);
    freez(&hgvs->hgvsCN);
    freez(&hgvs->hgvsP);
    freeMem(hgvs);
    }
}

static struct annoRow *aggvEffectToRow(struct annoGratorGpVar *self, struct gpFx *effect,
				       struct annoRow *rowIn, struct hgvsTerms *hgvs,
                                       struct lm *callerLm)
// convert a single genePred annoRow and gpFx record to an augmented genePred annoRow;
{
struct annoGrator *gSelf = &(self->grator);
struct annoStreamer *sSelf = &(gSelf->streamer);
assert(sSelf->numCols > gSelf->mySource->numCols);

char **wordsOut;
lmAllocArray(self->lm, wordsOut, sSelf->numCols);

// copy the genePred fields over
int gpColCount = gSelf->mySource->numCols;
char **wordsIn = (char **)rowIn->data;
memcpy(wordsOut, wordsIn, sizeof(char *) * gpColCount);

// stringify the gpFx structure 
aggvStringifyGpFx(&wordsOut[gpColCount], effect, callerLm);

// Final columns: optional HGVS terms
wordsOut[sSelf->numCols-3] = lmCloneString(callerLm, hgvs && hgvs->hgvsG ? hgvs->hgvsG : "");
wordsOut[sSelf->numCols-2] = lmCloneString(callerLm, hgvs && hgvs->hgvsCN ? hgvs->hgvsCN : "");
wordsOut[sSelf->numCols-1] = lmCloneString(callerLm, hgvs && hgvs->hgvsP ? hgvs->hgvsP : "");

return annoRowFromStringArray(rowIn->chrom, rowIn->start, rowIn->end, rowIn->rightJoinFail,
			      wordsOut, sSelf->numCols, callerLm);
}

struct dnaSeq *genePredToGenomicSequence(struct genePred *pred, struct annoAssembly *aa,
					 struct lm *lm)
/* Return concatenated genomic sequence of exons of pred. */
{
int txLen = 0;
int i;
for (i=0; i < pred->exonCount; i++)
    txLen += (pred->exonEnds[i] - pred->exonStarts[i]);
char *seq = lmAlloc(lm, txLen + 1);
int offset = 0;
for (i=0; i < pred->exonCount; i++)
    {
    int exonStart = pred->exonStarts[i];
    int exonEnd = pred->exonEnds[i];
    annoAssemblyGetSeq(aa, pred->chrom, exonStart, exonEnd, seq+offset, txLen+1-offset);
    offset += (exonEnd - exonStart);
    }
if(pred->strand[0] == '-')
    reverseComplement(seq, txLen);
struct dnaSeq *txSeq = NULL;
lmAllocVar(lm, txSeq);
txSeq->name = lmCloneString(lm, pred->name);
txSeq->dna = seq;
txSeq->size = txLen;
return txSeq;
}

struct dnaSeq *translateTx(struct dnaSeq *txSeq, struct genbankCds *cds)
/* Translate txSeq into protein sequence, including 'X' for stop codon if present. */
{
struct dnaSeq *protSeq = translateSeq(txSeq, cds->start, FALSE);
aaSeqZToX(protSeq);
return protSeq;
}

static struct udcFile *getCachedUdcFile(char *fileOrUrl)
/* Return an open UDC file handle for fileOrUrl; cache open connections.  errAbort if can't open. */
{
static struct hash *hash = NULL;
if (hash == NULL)
    hash = hashNew(0);
struct udcFile *udcf = hashFindVal(hash, fileOrUrl);
if (udcf == NULL)
    {
    char *realFileOrUrl = hReplaceGbdb(fileOrUrl);
    udcf = udcFileOpen(realFileOrUrl, NULL);
    hashAdd(hash, fileOrUrl, udcf);
    freeMem(realFileOrUrl);
    }
return udcf;
}

static struct dnaSeq *getCachedSeq(char *fileOrUrl, off_t offset, size_t size, boolean isDna)
/* Parse FASTA in file fileOrUrl at offset and return a cached dnaSeq. */
{
static struct hash *hash = NULL;
if (hash == NULL)
    hash = hashNew(0);
char key[4096];
safef(key, sizeof(key), "%s_%lld_%lld", fileOrUrl, (long long)offset, (long long)size);
struct dnaSeq *seq = hashFindVal(hash, key);
if (seq == NULL)
    {
    char *buf = needMem(size+1);
    struct udcFile *udcf = getCachedUdcFile(fileOrUrl);
    udcSeek(udcf, offset);
    udcRead(udcf, buf, size);
    seq = faSeqFromMemText(buf, isDna);
    toUpperN(seq->dna, seq->size);
    hashAdd(hash, key, seq);
    }
return seq;
}

struct dnaSeq *getProtSeq(struct annoGratorGpVar *self, char *protAcc,
                          struct dnaSeq *txSeq, struct genbankCds *cds)
/* If protAcc is NULL, translate txSeq+cds; else look up protAcc. */
{
static struct hash *hash = NULL;
if (hash == NULL)
    hash = hashNew(0);
struct dnaSeq *accSeq = NULL;
if (isEmpty(protAcc))
    {
    accSeq = hashFindVal(hash, txSeq->name);
    if (accSeq == NULL)
        {
        accSeq = translateTx(txSeq, cds);
        hashAdd(hash, txSeq->name, accSeq);
        }
    }
else
    {
    accSeq = hashFindVal(hash, protAcc);
    char *db = self->grator.streamer.assembly->name;
    char query[1024];
    struct sqlConnection *conn = hAllocConn(db);
    //#*** Using presence or absence of dot-version is an ugly hack, but it will do for now;
    //#*** otherwise there's new config to pass in & store...
    if (strchr(protAcc, '.'))
        {
        // Use ncbiRefSeqPepTable
        sqlSafef(query, sizeof(query), "select seq from ncbiRefSeqPepTable where name = '%s'",
                 protAcc);
        char *seq = sqlQuickString(conn, query);
        if (seq)
            accSeq = newDnaSeq(seq, strlen(seq), protAcc);
        }
    else
        {
        // Get GenBank versioned acc and seq
        sqlSafef(query, sizeof(query),
                 "select path,file_offset,file_size from %s,%s "
                 "where acc = '%s' and gbExtFile.id = gbExtFile",
                 gbSeqTable, gbExtFileTable, protAcc);
        char **row;
        struct sqlResult *sr = sqlGetResult(conn, query);
        if ((row = sqlNextRow(sr)) != NULL)
            accSeq = getCachedSeq(row[0], atoll(row[1]), atoll(row[2]), FALSE);
        sqlFreeResult(&sr);
        }
    if (accSeq == NULL)
        // Store a dnaSeq with NULL name and seq so we don't waste time sql querying this again.
        accSeq = newDnaSeq(NULL, 0, NULL);
    hashAdd(hash, protAcc, accSeq);
    hFreeConn(&conn);
    }
return (accSeq->name == NULL) ? NULL : accSeq;
}

static struct hgvsTerms *getHgvsTerms(struct annoGratorGpVar *self, char *chromAcc,
                                      struct psl *psl, struct genbankCds *cds,
                                      struct dnaSeq *txSeq, struct dnaSeq *protSeq,
                                      struct vpTx *vpTx, struct vpPep *vpPep)
/* Return HGVS terms for a variant allele projected onto the genome. */
{
struct hgvsTerms *hgvs;
AllocVar(hgvs);
struct bed3 variantBed;
variantBed.chrom = psl->tName;
variantBed.chromStart = min(vpTx->start.gOffset, vpTx->end.gOffset);
variantBed.chromEnd = max(vpTx->start.gOffset, vpTx->end.gOffset);
if (self->hgvsMakeG)
    {
    int gAltLen = strlen(vpTx->gAlt);
    char gAlt[gAltLen+1];
    safecpy(gAlt, sizeof(gAlt), vpTx->gAlt);
    if (pslQStrand(psl) == '-')
        reverseComplement(gAlt, gAltLen);
    hgvs->hgvsG = hgvsGFromVariant(self->gSeqWin, &variantBed, gAlt, chromAcc,
                                   self->hgvsBreakDelIns);
    }
if ((self->hgvsMakeCN || self->hgvsMakeP) && txSeq)
    {
    if (cds->start != cds->end && cds->start >= 0)
        {
        if (self->hgvsMakeCN)
            hgvs->hgvsCN = hgvsCFromVpTx(vpTx, self->gSeqWin, psl, cds, txSeq,
                                         self->hgvsBreakDelIns);
        if (self->hgvsMakeP)
            {
            hgvs->hgvsP = hgvsPFromVpPep(vpPep, protSeq, self->hgvsAddParensToP);
            }
        }
    else if (self->hgvsMakeCN)
        hgvs->hgvsCN = hgvsNFromVpTx(vpTx, self->gSeqWin, psl, txSeq,
                                     self->hgvsBreakDelIns);
    }
return hgvs;
}


static struct annoRow *aggvPredict(struct annoGratorGpVar *self, struct variant *variant,
                                   struct psl *psl, struct genbankCds *cds,
                                   struct dnaSeq *txSeq, struct dnaSeq *protSeq,
                                   struct annoRow *inRow, struct lm *callerLm)
// Project variant through psl onto transcript and predict functional effects.
{
struct annoRow *rowList = NULL;
char *db = self->grator.streamer.assembly->name;
char *chromAcc = self->hgvsMakeG ? hRefSeqAccForChrom(db, variant->chrom) : NULL;
struct bed3 *variantBed = (struct bed3 *)variant;
vpExpandIndelGaps(psl, self->gSeqWin, txSeq);
struct allele *allele;
for (allele = variant->alleles;  allele != NULL;  allele = allele->next)
    {
    char *alt = allele->sequence;
    struct vpTx *vpTx = vpGenomicToTranscript(self->gSeqWin, variantBed, alt, psl, txSeq);
    if (!allele->isReference || vpTx->genomeMismatch)
        {
        struct vpPep *vpPep = vpTranscriptToProtein(vpTx, cds, txSeq, protSeq);
        struct hgvsTerms *hgvs = getHgvsTerms(self, chromAcc, psl, cds, txSeq, protSeq,
                                              vpTx, vpPep);
        struct gpFx *fxList = vpTranscriptToGpFx(vpTx, psl, cds, txSeq, vpPep, protSeq, self->lm);
        struct annoRow *rows = NULL;
        struct gpFx *fx;
        for(fx = fxList;  fx != NULL; fx = fx->next)
            {
            // Intergenic variants never pass through here so we don't have to filter them out
            // here if self->funcFilter is null.
            if (self->funcFilter == NULL || passesFilter(self, fx))
                {
                // Restore the original variant's alt allele
                fx->gAllele = lmCloneString(self->lm, allele->sequence);
                slAddHead(&rows, aggvEffectToRow(self, fx, inRow, hgvs, callerLm));
                }
            }
        slReverse(&rows);
        rowList = slCat(rows, rowList);
        hgvsTermsFree(hgvs);
        vpPepFree(&vpPep);
        }
    vpTxFree(&vpTx);
    }
freeMem(chromAcc);
return rowList;
}

static void lookupProtAcc(struct annoGratorGpVar *self, struct dnaSeq *protSeq)
/* For Gencode, try to find the ENSP* ID associated with the ENST* ID. */
{
char *db = self->grator.streamer.assembly->name;
if (! self->protLookupInited)
    {
    char *sourceName = self->grator.streamer.name;
    if (strstr(sourceName, "wgEncodeGencode"))
        {
        char *version = strrchr(sourceName, 'V');
        if (version)
            {
            if (!trackHubDatabase(db))
                {
                char attrsTable[512];
                safef(attrsTable, sizeof(attrsTable), "wgEncodeGencodeAttrs%s", version);
                if (hTableExists(db, attrsTable) && hHasField(db, attrsTable, "proteinId"))
                    {
                    self->protLookupHash = hashNew(0);
                    self->protLookupTable = cloneString(attrsTable);
                    }
                }
            }
        }
    self->protLookupInited = TRUE;
    }
if (self->protLookupHash != NULL)
    {
    char *txAcc = protSeq->name;
    struct hashEl *hel = hashLookup(self->protLookupHash, txAcc);
    if (hel == NULL)
        {
        struct sqlConnection *conn = hAllocConn(db);
        char query[2048];
        sqlSafef(query, sizeof(query), "select proteinId from %s where transcriptId = '%s'",
                 self->protLookupTable, txAcc);
        char *protAcc = sqlQuickString(conn, query);
        hel = hashAdd(self->protLookupHash, txAcc, protAcc);
        hFreeConn(&conn);
        }
    if (hel->val != NULL)
        {
        freeMem(protSeq->name);
        protSeq->name = cloneString(hel->val);
        }
    }
}

static struct annoRow *aggvGenRowsGp(struct annoGratorGpVar *self, struct variant *variant,
                                     struct genePred *pred, struct annoRow *inRow,
                                     struct lm *callerLm)
// put out annoRows for all the gpFx that arise from variant and pred
{
struct annoStreamer *sSelf = &(self->grator.streamer);
struct genbankCds cds;
genePredToCds(pred, &cds);
struct dnaSeq *txSeq = genePredToGenomicSequence(pred, sSelf->assembly, self->lm);
int chromSize = 0;  // unused
struct psl *psl = genePredToPsl(pred, chromSize, txSeq->size);
vpExpandIndelGaps(psl, self->gSeqWin, txSeq);
struct dnaSeq *protSeq = NULL;
if (cds.end > cds.start)
    {
    //#*** what if cds.startComplete is not true??
    protSeq = translateTx(txSeq, &cds);
    lookupProtAcc(self, protSeq);
    }
struct annoRow *rowList = aggvPredict(self, variant, psl, &cds, txSeq, protSeq, inRow, callerLm);
pslFree(&psl);
dnaSeqFree(&protSeq);
return rowList;
}

static struct annoRow *aggvGenRowsPsl(struct annoGratorGpVar *self, struct variant *variant,
                                     struct annoRow *inRow, struct lm *callerLm)
// put out annoRows for all the gpFx that arise from variant and transcript psl+
{
char **ppWords = inRow->data;
struct psl *psl = pslLoad(ppWords);
struct genbankCds cds;
genbankCdsParse(ppWords[PSLPLUS_CDS_IX], &cds);
struct dnaSeq *txSeq = getCachedSeq(ppWords[PSLPLUS_PATH_IX], atoll(ppWords[PSLPLUS_FILEOFFSET_IX]),
                                    atoll(ppWords[PSLPLUS_FILESIZE_IX]), TRUE);
struct dnaSeq *protSeq = getProtSeq(self, ppWords[PSLPLUS_PROTACC_IX], txSeq, &cds);
struct annoRow *rowList = aggvPredict(self, variant, psl, &cds, txSeq, protSeq, inRow, callerLm);
pslFree(&psl);
return rowList;
}

struct annoRow *aggvGenelessRow(struct annoGratorGpVar *self, struct variant *variant,
                                boolean rjFail, struct lm *callerLm)
/* If intergenic variants (no overlapping or nearby genes) are to be included in output,
 * make an output row with empty genePred and a gpFx that is empty except for soNumber. */
{
struct annoGrator *gSelf = &(self->grator);
struct annoStreamer *sSelf = &(gSelf->streamer);
char **wordsOut;
lmAllocArray(self->lm, wordsOut, sSelf->numCols);
// Add empty strings for genePred string columns:
int gpColCount = gSelf->mySource->numCols;
int i;
for (i = 0;  i < gpColCount;  i++)
    wordsOut[i] = "";
struct gpFx *gpFx;
lmAllocVar(self->lm, gpFx);
enum soTerm term = hasAltAllele(variant->alleles) ? intergenic_variant : no_sequence_alteration;
if (term == no_sequence_alteration)
    gpFx->gAllele = variant->alleles->sequence;
else
    gpFx->gAllele = firstAltAllele(variant->alleles);
if (isAllNt(gpFx->gAllele, strlen(gpFx->gAllele)))
    touppers(gpFx->gAllele);
gpFx->soNumber = term;
gpFx->detailType = none;
aggvStringifyGpFx(&wordsOut[gpColCount], gpFx, self->lm);
if (self->hgvsMakeG)
    {
    // Add HGVS genomic term
    struct bed3 *variantBed = (struct bed3 *)variant;
    char *chromAcc = hRefSeqAccForChrom(sSelf->assembly->name, variant->chrom);
    char *hgvsG = hgvsGFromVariant(self->gSeqWin, variantBed, gpFx->gAllele, chromAcc,
                                   self->hgvsBreakDelIns);
    wordsOut[sSelf->numCols-3] = lmCloneString(callerLm, hgvsG);
    freeMem(chromAcc);
    freeMem(hgvsG);
    }
return annoRowFromStringArray(variant->chrom, variant->chromStart, variant->chromEnd, rjFail,
			      wordsOut, sSelf->numCols, callerLm);
}

static struct variant *variantFromPgSnpTableRow(struct annoGratorGpVar *self, struct annoRow *row,
                                                char *refAllele)
/* Translate pgSnp array of words into variant. */
{
return variantFromPgSnpAnnoRow(row, refAllele, TRUE, self->lm);
}

static struct variant *variantFromPgSnpFileRow(struct annoGratorGpVar *self, struct annoRow *row,
                                               char *refAllele)
/* Translate pgSnp array of words into variant. */
{
return variantFromPgSnpAnnoRow(row, refAllele, FALSE, self->lm);
}

static struct variant *variantFromVcfRow(struct annoGratorGpVar *self, struct annoRow *row,
					 char *refAllele)
/* Translate vcf array of words into variant. */
{
return variantFromVcfAnnoRow(row, refAllele, self->lm, self->dyScratch);
}

static void setVariantFromRow(struct annoGratorGpVar *self, struct annoStreamRows *primaryData)
/* Depending on autoSql definition of primary source, choose a function to translate
 * incoming rows into generic variants. */
{
if (asObjectsMatch(primaryData->streamer->asObj, pgSnpAsObj()))
    self->variantFromRow = variantFromPgSnpTableRow;
else if (asObjectsMatch(primaryData->streamer->asObj, pgSnpFileAsObj()))
    self->variantFromRow = variantFromPgSnpFileRow;
else if (asObjectsMatch(primaryData->streamer->asObj, vcfAsObj()))
    self->variantFromRow = variantFromVcfRow;
}

struct annoRow *annoGratorGpVarIntegrate(struct annoGrator *gSelf,
					 struct annoStreamRows *primaryData,
					 boolean *retRJFilterFailed, struct lm *callerLm)
// integrate a variant and a genePred, generate as many rows as
// needed to capture all the changes
{
struct annoGratorGpVar *self = (struct annoGratorGpVar *)gSelf;
struct annoStreamer *sSelf = &(gSelf->streamer);
lmCleanup(&(self->lm));
self->lm = lmInit(0);
if (self->variantFromRow == NULL)
    setVariantFromRow(self, primaryData);
// TODO Performance improvement: instead of creating the transcript sequence for each
// variant that intersects the transcript, cache transcript sequence; possibly
// an slPair with a concatenation of {chrom, txStart, txEnd, cdsStart, cdsEnd,
// exonStarts, exonEnds} as the name, and sequence as the val.  When something in
// the list is no longer in the list of rows from the internal annoGratorIntegrate call,
// drop it.
// BETTER YET: make a callback for gpFx to get CDS sequence only when it needs it.
struct annoRow *primaryRow = primaryData->rowList;
int refAlBufSize = primaryRow->end - primaryRow->start + 1;
char refAllele[refAlBufSize];
annoAssemblyGetSeq(sSelf->assembly, primaryRow->chrom, primaryRow->start, primaryRow->end,
		   refAllele, sizeof(refAllele));
struct variant *variant = self->variantFromRow(self, primaryRow, refAllele);

// Temporarily tweak primaryRow's start and end to find upstream/downstream overlap:
int pStart = primaryRow->start, pEnd = primaryRow->end;
if (primaryRow->start <= GPRANGE)
    primaryRow->start = 0;
else
    primaryRow->start -= GPRANGE;
primaryRow->end += GPRANGE;
struct annoRow *rows = annoGratorIntegrate(gSelf, primaryData, retRJFilterFailed, self->lm);
primaryRow->start = pStart;
primaryRow->end = pEnd;

if (rows == NULL)
    {
    // No genePreds means that the primary variant is intergenic.
    if ((self->funcFilter == NULL || self->funcFilter->intergenic))
        return aggvGenelessRow(self, variant, *retRJFilterFailed, callerLm);
    else if (retRJFilterFailed && self->gpVarOverlapRule == agoMustOverlap)
	*retRJFilterFailed = TRUE;
    return NULL;
    }
if (retRJFilterFailed && *retRJFilterFailed)
    return NULL;

struct annoRow *outRows = NULL;

for(; rows; rows = rows->next)
    {
    struct annoRow *outRow = NULL;
    if (self->sourceIsPslPlus)
        {
        outRow = aggvGenRowsPsl(self, variant, rows, callerLm);
        }
    else
        {
        struct genePred *gp;
        if (self->sourceIsBigGenePred)
            gp = (struct genePred *)genePredFromBigGenePredRow(rows->data);
        else
            {
            // work around genePredLoad's trashing its input
            char **inWords = rows->data;
            char *saveExonStarts = lmCloneString(self->lm, inWords[8]);
            char *saveExonEnds = lmCloneString(self->lm, inWords[9]);
            gp = self->sourceHasFrames ? genePredExtLoad(inWords, GENEPREDX_NUM_COLS) :
                                         genePredLoad(inWords);
            inWords[8] = saveExonStarts;
            inWords[9] = saveExonEnds;
            }
        outRow = aggvGenRowsGp(self, variant, gp, rows, callerLm);
        genePredFree(&gp);
        }
    if (outRow != NULL)
        {
        slReverse(&outRow);
        outRows = slCat(outRow, outRows);
        }
    }
slReverse(&outRows);
// If all rows failed the filter, and we must overlap, set *retRJFilterFailed.
if (outRows == NULL && retRJFilterFailed && self->gpVarOverlapRule == agoMustOverlap)
    *retRJFilterFailed = TRUE;
return outRows;
}

static void aggvSetOverlapRule(struct annoGrator *gSelf, enum annoGratorOverlap rule)
/* We need an overlap rule that is independent of the genePred integration because
 * if we're including intergenic variants, then we return a row even though no genePred
 * rows overlap. */
{
struct annoGratorGpVar *self = (struct annoGratorGpVar *)gSelf;
self->gpVarOverlapRule = rule;
// For mustOverlap, if we're including intergenic variants in output, don't let simple
// integration set retRjFilterFailed:
if (rule == agoMustOverlap && self->funcFilter != NULL && self->funcFilter->intergenic)
    gSelf->overlapRule = agoNoConstraint;
else
    gSelf->overlapRule = rule;
}

void aggvClose(struct annoStreamer **pSSelf)
/* Close out localmem and all the usual annoGrator stuff. */
{
if (*pSSelf != NULL)
    {
    struct annoGratorGpVar *self = (struct annoGratorGpVar *)(*pSSelf);
    lmCleanup(&(self->lm));
    dyStringFree(&(self->dyScratch));
    freez(&self->protLookupTable);
    hashFree(&self->protLookupHash);
    annoGratorClose(pSSelf);
    }
}

struct annoGrator *annoGratorGpVarNew(struct annoStreamer *mySource)
/* Make a subclass of annoGrator that combines genePreds from mySource with
 * pgSnp rows from primary source to predict functional effects of variants
 * on genes.
 * mySource becomes property of the new annoGrator (don't close it, close annoGrator). */
{
struct annoGratorGpVar *self;
AllocVar(self);
struct annoGrator *gSelf = &(self->grator);
annoGratorInit(gSelf, mySource);
struct annoStreamer *sSelf = &(gSelf->streamer);
// We add columns beyond what comes from mySource, so update our public-facing asObject:
annoGratorSetAutoSqlObject(sSelf, annoGpVarAsObj(mySource->asObj));
gSelf->setOverlapRule = aggvSetOverlapRule;
sSelf->close = aggvClose;
// integrate by adding gpFx fields
gSelf->integrate = annoGratorGpVarIntegrate;
self->dyScratch = dyStringNew(0);
self->sourceHasFrames = (asColumnFindIx(mySource->asObj->columnList, "exonFrames") >= 0);
self->sourceIsBigGenePred = (asColumnFindIx(mySource->asObj->columnList, "chromStart") >= 0);
self->sourceIsPslPlus = (asColumnFindIx(mySource->asObj->columnList, "tStart") >= 0);
char *db = sSelf->assembly->name;
self->gSeqWin = chromSeqWindowNew(db, NULL, 0, 0);

return gSelf;
}

void annoGratorGpVarSetFuncFilter(struct annoGrator *gSelf,
				  struct annoGratorGpVarFuncFilter *funcFilter)
/* If funcFilter is non-NULL, it specifies which functional categories
 * to include in output; if NULL, by default intergenic variants are excluded and
 * all other categories are included.
 * NOTE: This calls gSelf->setOverlapRule() with the currently set overlap rule because
 * overlapRule is affected by filter settings.  */
{
struct annoGratorGpVar *self = (struct annoGratorGpVar *)gSelf;
freez(&self->funcFilter);
if (funcFilter != NULL)
    self->funcFilter = CloneVar(funcFilter);
// Since our overlapRule behavior depends on filter settings, reevaluate:
gSelf->setOverlapRule(gSelf, self->gpVarOverlapRule);
}

void annoGratorGpVarSetHgvsOutOptions(struct annoGrator *gSelf, uint hgvsOutOptions)
/* Import the HGVS output options described in hgHgvs.h */
{
struct annoGratorGpVar *self = (struct annoGratorGpVar *)gSelf;
if (hgvsOutOptions & HGVS_OUT_G)
    self->hgvsMakeG = TRUE;
if (hgvsOutOptions & HGVS_OUT_CN)
    self->hgvsMakeCN = TRUE;
if (hgvsOutOptions & HGVS_OUT_P)
    self->hgvsMakeP = TRUE;
if (hgvsOutOptions & HGVS_OUT_P_ADD_PARENS)
    self->hgvsAddParensToP = TRUE;
if (hgvsOutOptions & HGVS_OUT_BREAK_DELINS)
    self->hgvsBreakDelIns = TRUE;
}
