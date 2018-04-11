/* vcf - stuff to handle VCF stuff in table browser. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hgTables.h"
#include "asFilter.h"
#include "hubConnect.h"
#include "asParse.h"
#include "hgBam.h"
#include "linefile.h"
#include "localmem.h"
#include "obscure.h"
#include "vcf.h"
#include "web.h"

#define VCFDATALINE_NUM_COLS 10

boolean isVcfTable(char *table, boolean *retIsTabix)
/* Return TRUE if table corresponds to a VCF file. 
 * If retIsTabix is non-NULL, set *retIsTabix to TRUE if this is vcfTabix (not just vcf). */
{
boolean isVcfTabix = FALSE, isVcf = FALSE;
struct trackDb *tdb = hashFindVal(fullTableToTdbHash, table);
if (tdb)
    {
    isVcfTabix = startsWithWord("vcfTabix", tdb->type);
    isVcf = startsWithWord("vcf", tdb->type);
    }
else
    {
    isVcfTabix = trackIsType(database, table, curTrack, "vcfTabix", ctLookupName);
    if (!isVcfTabix)
	isVcf = trackIsType(database, table, curTrack, "vcf", ctLookupName);
    }
if (retIsTabix)
    *retIsTabix = isVcfTabix;
return (isVcfTabix || isVcf);
}

struct hTableInfo *vcfToHti(char *table, boolean isTabix)
/* Get standard fields of VCF into hti structure. */
{
struct hTableInfo *hti;
AllocVar(hti);
hti->rootName = cloneString(table);
hti->isPos= TRUE;
strcpy(hti->chromField, "chrom");
strcpy(hti->startField, "pos");
strcpy(hti->nameField, "id");
hti->type = cloneString(isTabix ? "vcfTabix" : "vcf");
return hti;
}

struct slName *vcfGetFields()
/* Get fields of vcf as simple name list. */
{
struct asObject *as = vcfAsObj();
struct slName *names = asColNames(as);
return names;
}

struct sqlFieldType *vcfListFieldsAndTypes()
/* Get fields of bigBed as list of sqlFieldType. */
{
struct asObject *as = vcfAsObj();
struct sqlFieldType *list = sqlFieldTypesFromAs(as);
return list;
}

void dyJoin(struct dyString *dy, char *delim, char **words, int wordCount)
/* Clear dy and append wordCount words interspersed with delim. */
{
dyStringClear(dy);
int i;
for (i = 0;  i < wordCount;  i++)
    {
    if (i > 0)
	dyStringAppend(dy, delim);
    dyStringAppend(dy, words[i]);
    }
}

void vcfInfoElsToString(struct dyString *dy, struct vcfFile *vcff, struct vcfRecord *rec)
/* Unpack rec's typed infoElements to semicolon-sep'd string in dy.*/
{
dyStringClear(dy);
if (rec->infoCount == 0)
    dyStringAppendC(dy, '.');
int i;
for (i = 0;  i < rec->infoCount;  i++)
    {
    if (i > 0)
	dyStringAppendC(dy, ';');
    const struct vcfInfoElement *el = &(rec->infoElements[i]);
    const struct vcfInfoDef *def = vcfInfoDefForKey(vcff, el->key);
    enum vcfInfoType type = def ? def->type : vcfInfoString;
    dyStringAppend(dy, el->key);
    if (el->count > 0)
	dyStringAppendC(dy, '=');
    int j;
    for (j = 0;  j < el->count;  j++)
	{
	if (j > 0)
	    dyStringAppendC(dy, ',');
	if (el->missingData[j])
	    {
	    dyStringAppend(dy, ".");
	    continue;
	    }
	union vcfDatum dat = el->values[j];
	switch (type)
	    {
	    case vcfInfoInteger:
		dyStringPrintf(dy, "%d", dat.datInt);
		break;
	    case vcfInfoFloat:
		{
		// use big precision and erase trailing zeros:
		char fbuf[64];
		safef(fbuf, sizeof(fbuf), "%.16lf", dat.datFloat);
		int i;
		for (i = strlen(fbuf) - 1;  i > 0;  i--)
		    if (fbuf[i] == '0')
			fbuf[i] = '\0';
		    else
			break;
		dyStringAppend(dy, fbuf);
		}
		break;
	    case vcfInfoCharacter:
		dyStringAppendC(dy, dat.datChar);
		break;
	    case vcfInfoFlag: // Flags could have values in older VCF
	    case vcfInfoString:
		dyStringAppend(dy, dat.datString);
		break;
	    default:
		errAbort("Invalid vcfInfoType %d (how did this get past parser?", type);
	    }
	}
    }
}

#define VCF_NUM_BUF_SIZE 256

void vcfRecordToRow(struct vcfRecord *rec, char *chrom, char *numBuf, struct dyString *dyAlt,
		    struct dyString *dyFilter, struct dyString *dyInfo, struct dyString *dyGt,
		    char *row[VCFDATALINE_NUM_COLS])
/* Convert vcfRecord to array of strings, using numBuf to store ascii
 * versions of numbers and dyStrings to store stringified arrays. */
{
struct vcfFile *vcff = rec->file;
char *numPt = numBuf;
row[0] = chrom; // VCF files' chrom field often lacks "chr" -- use region->chrom from caller.
row[1] = numPt;
numPt += safef(numPt, VCF_NUM_BUF_SIZE - (numPt-numBuf), "%u", rec->chromStart+1);
numPt++;
row[2] = rec->name;
row[3] = rec->alleles[0];
dyJoin(dyAlt, ",", &(rec->alleles[1]), rec->alleleCount-1);
row[4] = dyAlt->string;
row[5] = rec->qual;
dyJoin(dyFilter, ";", rec->filters, rec->filterCount);
row[6] = dyFilter->string;
vcfInfoElsToString(dyInfo, vcff, rec);
row[7] = dyInfo->string;
if (vcff->genotypeCount > 0)
    {
    row[8] = rec->format;
    dyJoin(dyGt, "\t", rec->genotypeUnparsedStrings, vcff->genotypeCount);
    row[9] = dyGt->string;
    }
else
    row[8] = row[9] = ""; // compatible with localmem usage
}

static char *vcfFileName(struct sqlConnection *conn, char *table, char *chrom, boolean isTabix)
// Look up the vcf or vcfTabix file name, using CUSTOM_TRASH if necessary; return NULL if not found.
{
char *fileName = bigFileNameFromCtOrHub(table, conn);
struct trackDb *tdb = hashFindVal(fullTableToTdbHash, table);
if (isCustomTrack(table) && ! isTabix)
    {
    // fileName is stored in the customTrash table
    struct customTrack *ct = ctLookupName(table);
    struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
    char query[1024];
    sqlSafef(query, sizeof(query), "select fileName from %s", ct->dbTableName);
    fileName = sqlQuickString(conn, query);
    hFreeConn(&conn);
    }
if (fileName == NULL)
    fileName = bbiNameFromSettingOrTableChrom(tdb, conn, table, chrom);
return fileName;
}

static char *vcfMustFindFileName(struct sqlConnection *conn, char *table, char *chrom,
                                 boolean isTabix)
/* Look up the vcf or vcfTabix file name; errAbort if not found. */
{
char *fileName = vcfFileName(conn, table, chrom, isTabix);
if (fileName == NULL)
    errAbort("vcfMustFindFileName: can't find VCF file; chrom=%s, table=%s", chrom, table);
return fileName;
}

void vcfTabOut(char *db, char *table, struct sqlConnection *conn, char *fields, FILE *f,
	       boolean isTabix)
/* Print out selected fields from VCF.  If fields is NULL, then print out all fields. */
{
struct hTableInfo *hti = NULL;
hti = getHti(db, table, conn);
struct hash *idHash = NULL;
char *idField = getIdField(db, curTrack, table, hti);
int idFieldNum = 0;

/* if we know what field to use for the identifiers, get the hash of names */
if (idField != NULL)
    idHash = identifierHash(db, table);

if (f == NULL)
    f = stdout;

/* Convert comma separated list of fields to array. */
int fieldCount = chopByChar(fields, ',', NULL, 0);
char **fieldArray;
AllocArray(fieldArray, fieldCount);
chopByChar(fields, ',', fieldArray, fieldCount);

/* Get list of all fields in big bed and turn it into a hash of column indexes keyed by
 * column name. */
struct hash *fieldHash = hashNew(0);
struct slName *bb, *bbList = vcfGetFields();
int i;
for (bb = bbList, i=0; bb != NULL; bb = bb->next, ++i)
    {
    /* if we know the field for identifiers, save it away */
    if ((idField != NULL) && sameString(idField, bb->name))
	idFieldNum = i;
    hashAddInt(fieldHash, bb->name, i);
    }

/* Create an array of column indexes corresponding to the selected field list. */
int *columnArray;
AllocArray(columnArray, fieldCount);
for (i=0; i<fieldCount; ++i)
    {
    columnArray[i] = hashIntVal(fieldHash, fieldArray[i]);
    }

// If we are outputting a subset of fields, invalidate the VCF header.
boolean allFields = (fieldCount == VCFDATALINE_NUM_COLS);
if (!allFields)
    fprintf(f, "# Only selected columns are included below; output is not valid VCF.\n");

struct asObject *as = vcfAsObj();
struct asFilter *filter = NULL;
if (anyFilter())
    filter = asFilterFromCart(cart, db, table, as);

/* Loop through outputting each region */
struct region *region, *regionList = getRegions();
int maxOut = bigFileMaxOutput();
// Include the header, absolutely necessary for VCF parsing.
boolean printedHeader = FALSE;
// Temporary storage for row-ification:
struct dyString *dyAlt = newDyString(1024);
struct dyString *dyFilter = newDyString(1024);
struct dyString *dyInfo = newDyString(1024);
struct dyString *dyGt = newDyString(1024);
struct vcfRecord *rec;
for (region = regionList; region != NULL && (maxOut > 0); region = region->next)
    {
    char *fileName = vcfFileName(conn, table, region->chrom, isTabix);
    if (fileName == NULL)
        continue;
    struct vcfFile *vcff;
    if (isTabix)
        {
        char *indexUrl = bigDataIndexFromCtOrHub(table, conn);
	vcff = vcfTabixFileAndIndexMayOpen(fileName, indexUrl, region->chrom, region->start, region->end,
				   100, maxOut);
        }
    else
	vcff = vcfFileMayOpen(fileName, region->chrom, region->start, region->end,
			      100, maxOut, TRUE);
    if (vcff == NULL)
	noWarnAbort();
    // If we are outputting all fields, but this VCF has no genotype info, omit the
    // genotype columns from output:
    if (allFields && vcff->genotypeCount == 0)
	fieldCount = VCFDATALINE_NUM_COLS - 2;
    if (!printedHeader)
	{
	fprintf(f, "%s", vcff->headerString);
	if (filter)
	    fprintf(f, "# Filtering on %d columns\n", slCount(filter->columnList));
	if (!allFields)
	    {
	    fprintf(f, "#%s", fieldArray[0]);
	    for (i=1; i<fieldCount; ++i)
		fprintf(f, "\t%s", fieldArray[i]);
	    fprintf(f, "\n");
	    }
	printedHeader = TRUE;
	}
    char *row[VCFDATALINE_NUM_COLS];
    char numBuf[VCF_NUM_BUF_SIZE];
    for (rec = vcff->records;  rec != NULL && (maxOut > 0);  rec = rec->next)
        {
	vcfRecordToRow(rec, region->chrom, numBuf, dyAlt, dyFilter, dyInfo, dyGt, row);
	if (asFilterOnRow(filter, row))
	    {
	    /* if we're looking for identifiers, check if this matches */
	    if ((idHash != NULL) && (hashLookup(idHash, row[idFieldNum]) == NULL))
		continue;
	    // All fields output: after asFilter'ing, preserve original VCF chrom
	    if (allFields && !sameString(rec->chrom, region->chrom))
		row[0] = rec->chrom;
	    int i;
	    fprintf(f, "%s", row[columnArray[0]]);
	    for (i=1; i<fieldCount; ++i)
		{
		fprintf(f, "\t%s", row[columnArray[i]]);
		}
	    fprintf(f, "\n");
	    maxOut --;
	    }
	}
    vcfFileFree(&vcff);
    freeMem(fileName);
    }
if (!printedHeader)
    explainWhyNoResults(f);
if (maxOut == 0)
    errAbort("Reached output limit of %d data values, please make region smaller,\n\tor set a higher output line limit with the filter settings.", bigFileMaxOutput());
/* Clean up and exit. */
dyStringFree(&dyAlt);  dyStringFree(&dyFilter);  dyStringFree(&dyInfo);  dyStringFree(&dyGt);
hashFree(&fieldHash);
freeMem(fieldArray);
freeMem(columnArray);
}

static void addFilteredBedsOnRegion(char *fileName, char *indexUrl, struct region *region, char *table,
				    struct asFilter *filter, struct lm *bedLm,
				    struct bed **pBedList, struct hash *idHash, int *pMaxOut,
				    boolean isTabix)
/* Add relevant beds in reverse order to pBedList */
{
struct vcfFile *vcff;
if (isTabix)
    vcff = vcfTabixFileAndIndexMayOpen(fileName, indexUrl, region->chrom, region->start, region->end,
            100, *pMaxOut);
else
    vcff = vcfFileMayOpen(fileName, region->chrom, region->start, region->end,
			  100, *pMaxOut, TRUE);
if (vcff == NULL)
    noWarnAbort();
struct lm *lm = lmInit(0);
char *row[VCFDATALINE_NUM_COLS];
char numBuf[VCF_NUM_BUF_SIZE];
// Temporary storage for row-ification:
struct dyString *dyAlt = newDyString(1024);
struct dyString *dyFilter = newDyString(1024);
struct dyString *dyInfo = newDyString(1024);
struct dyString *dyGt = newDyString(1024);
struct vcfRecord *rec;
for (rec = vcff->records;  rec != NULL;  rec = rec->next)
    {
    vcfRecordToRow(rec, region->chrom, numBuf, dyAlt, dyFilter, dyInfo, dyGt, row);
    if (asFilterOnRow(filter, row))
        {
	if ((idHash != NULL) && (hashLookup(idHash, rec->name) == NULL))
	    continue;
	struct bed *bed;
	lmAllocVar(bedLm, bed);
	bed->chrom = lmCloneString(bedLm, region->chrom);
	bed->chromStart = rec->chromStart;
	bed->chromEnd = rec->chromEnd;
	bed->name = lmCloneString(bedLm, rec->name);
	slAddHead(pBedList, bed);
	}
    (*pMaxOut)--;
    if (*pMaxOut <= 0)
	break;
    }
dyStringFree(&dyAlt);  dyStringFree(&dyFilter);  dyStringFree(&dyInfo);  dyStringFree(&dyGt);
lmCleanup(&lm);
vcfFileFree(&vcff);
}

struct bed *vcfGetFilteredBedsOnRegions(struct sqlConnection *conn,
	char *db, char *table, struct region *regionList, struct lm *lm,
	int *retFieldCount, boolean isTabix)
/* Get list of beds from VCF, in all regions, that pass filtering. */
{
int maxOut = bigFileMaxOutput();
/* Figure out vcf file name get column info and filter. */
struct asObject *as = vcfAsObj();
struct asFilter *filter = asFilterFromCart(cart, db, table, as);
struct hash *idHash = identifierHash(db, table);

/* Get beds a region at a time. */
struct bed *bedList = NULL;
struct region *region;
for (region = regionList; region != NULL; region = region->next)
    {
    char *fileName = vcfFileName(conn, table, region->chrom, isTabix);
    if (fileName == NULL)
	continue;
    char *indexUrl = bigDataIndexFromCtOrHub(table, conn);
    addFilteredBedsOnRegion(fileName, indexUrl, region, table, filter, lm, &bedList, idHash, &maxOut,
			    isTabix);
    freeMem(fileName);
    if (maxOut <= 0)
	{
	errAbort("Reached output limit of %d data values, please make region smaller,\n"
	     "\tor set a higher output line limit with the filter settings.", bigFileMaxOutput());
	}
    }
slReverse(&bedList);
return bedList;
}

struct slName *randomVcfIds(char *table, struct sqlConnection *conn, int count, boolean isTabix)
/* Return some semi-random IDs from a VCF file. */
{
/* Read 10000 items from vcf file,  or if they ask for a big list, then 4x what they ask for. */
char *fileName = vcfMustFindFileName(conn, table, hDefaultChrom(database), isTabix);
char *indexUrl = bigDataIndexFromCtOrHub(table, conn);

struct lineFile *lf = isTabix ? lineFileTabixAndIndexMayOpen(fileName, indexUrl, TRUE) :
				lineFileMayOpen(fileName, TRUE);
if (lf == NULL)
    noWarnAbort();
int orderedCount = count * 4;
if (orderedCount < 100)
    orderedCount = 100;
struct slName *idList = NULL;
char *words[4];
int i;
for (i = 0;  i < orderedCount && lineFileChop(lf, words); i++)
    {
    // compress runs of identical ID, in case most are placeholder
    if (i == 0 || !sameString(words[2], idList->name))
	slAddHead(&idList, slNameNew(words[2]));
    }
lineFileClose(&lf);
/* Shuffle list and trim it to count if necessary. */
shuffleList(&idList);
struct slName *sl;
for (sl = idList, i = 0; sl != NULL; sl = sl->next, i++)
    {
    if (i+1 >= count)
	{
	slNameFreeList(&(sl->next));
	break;
	}
    }
freez(&fileName);
return idList;
}

#define VCF_MAX_SCHEMA_COLS 20

void showSchemaVcf(char *table, struct trackDb *tdb, boolean isTabix)
/* Show schema on vcf. */
{
struct sqlConnection *conn = hAllocConn(database);
char *fileName = vcfMustFindFileName(conn, table, hDefaultChrom(database), isTabix);

struct asObject *as = vcfAsObj();
hPrintf("<B>Database:</B> %s", database);
hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;<B>Primary Table:</B> %s<br>", table);
hPrintf("<B>VCF File:</B> %s", fileName);
hPrintf("<BR>\n");
hPrintf("<B>Format description:</B> %s<BR>", as->comment);
hPrintf("See the <A HREF=\"%s\" target=_blank>Variant Call Format specification</A> for  more details<BR>\n",
	"http://www.1000genomes.org/wiki/analysis/vcf4.0");

/* Put up table that describes fields. */
hTableStart();
hPrintf("<TR><TH>field</TH>");
hPrintf("<TH>description</TH> ");
puts("</TR>\n");
struct asColumn *col;
int colCount = 0;
for (col = as->columnList; col != NULL; col = col->next)
    {
    hPrintf("<TR><TD><TT>%s</TT></TD>", col->name);
    hPrintf("<TD>%s</TD></TR>", col->comment);
    ++colCount;
    }
hTableEnd();

/* Put up another section with sample rows. */
webNewSection("Sample Rows");
hTableStart();

/* Fetch sample rows. */
char *indexUrl = bigDataIndexFromCtOrHub(table, conn);
struct lineFile *lf = isTabix ? lineFileTabixAndIndexMayOpen(fileName, indexUrl, TRUE) :
				lineFileMayOpen(fileName, TRUE);
if (lf == NULL)
    noWarnAbort();
char *row[VCF_MAX_SCHEMA_COLS];
int i;
for (i = 0;  i < 10;  i++)
    {
    int colCount = lineFileChop(lf, row);
    int colIx;
    if (i == 0)
	{
	// Print field names as column headers, using colCount to compute genotype span
	hPrintf("<TR>");
	for (colIx = 0, col = as->columnList; col != NULL && colIx < colCount;
	     colIx++, col = col->next)
	    {

	    if (sameString("genotypes", col->name) && colCount > colIx+1)
		hPrintf("<TH colspan=%d>%s</TH>", colCount - colIx, col->name);
	    else
		hPrintf("<TH>%s</TH>", col->name);
	    }
	hPrintf("</TR>\n");
	}
    hPrintf("<TR>");
    for (colIx=0; colIx < colCount; ++colIx)
	{
	if (colCount > VCFDATALINE_NUM_COLS && colIx == colCount - 1)
	    hPrintf("<TD>...</TD>");
	else
	    writeHtmlCell(row[colIx]);
	}
    hPrintf("</TR>\n");
    }
hTableEnd();
printTrackHtml(tdb);

/* Clean up and go home. */
lineFileClose(&lf);
freeMem(fileName);
hFreeConn(&conn);
}

