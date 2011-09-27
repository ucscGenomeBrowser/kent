/* vcf - stuff to handle VCF stuff in table browser. */

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

#if (defined USE_TABIX && defined KNETFILE_HOOKS)
#include "knetUdc.h"
#include "udc.h"
#endif//def USE_TABIX && KNETFILE_HOOKS

#define VCFDATALINE_NUM_COLS 10

char *vcfDataLineAutoSqlString =
"table vcfDataLine"
"\"The fields of a Variant Call Format data line\""
"    ("
"    string chrom;	\"An identifier from the reference genome\""
"    uint pos;		\"The reference position, with the 1st base having position 1\""
"    string id;		\"Semi-colon separated list of unique identifiers where available\""
"    string ref;		\"Reference base(s)\""
"    string alt;		\"Comma separated list of alternate non-reference alleles called on at least one of the samples\""
"    string qual;	\"Phred-scaled quality score for the assertion made in ALT. i.e. give -10log_10 prob(call in ALT is wrong)\""
"    string filter;	\"PASS if this position has passed all filters. Otherwise, a semicolon-separated list of codes for filters that fail\""
"    string info;	\"Additional information encoded as a semicolon-separated series of short keys with optional comma-separated values\""
"    string format;	\"If genotype columns are specified in header, a semicolon-separated list of of short keys starting with GT\""
"    string genotypes;	\"If genotype columns are specified in header, a tab-separated set of genotype column values; each value is a colon-separated list of values corresponding to keys in the format column\""
"    )";

boolean isVcfTable(char *table)
/* Return TRUE if table corresponds to a VCF file. */
{
if (isHubTrack(table))
    {
    struct trackDb *tdb = hashFindVal(fullTrackAndSubtrackHash, table);
    return startsWithWord("vcfTabix", tdb->type);
    }
else
    return trackIsType(database, table, curTrack, "vcfTabix", ctLookupName);
}

char *vcfFileName(char *table, struct sqlConnection *conn, char *seqName)
/* Return file name associated with VCF.  This handles differences whether it's
 * a custom or built-in track.  Do a freeMem on returned string when done. */
{
char *fileName = bigFileNameFromCtOrHub(table, conn);
if (fileName == NULL)
    fileName = bamFileNameFromTable(conn, table, seqName);
return fileName;
}

struct asObject *vcfAsObj()
/* Return asObject describing fields of VCF */
{
return asParseText(vcfDataLineAutoSqlString);
}

struct hTableInfo *vcfToHti(char *table)
/* Get standard fields of VCF into hti structure. */
{
struct hTableInfo *hti;
AllocVar(hti);
hti->rootName = cloneString(table);
hti->isPos= TRUE;
strcpy(hti->chromField, "chrom");
strcpy(hti->startField, "pos");
strcpy(hti->nameField, "id");
hti->type = cloneString("vcfTabix");
return hti;
}

struct slName *vcfGetFields(char *table)
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
    enum vcfInfoType type = def? def->type : vcfInfoNoType;
    dyStringAppend(dy, el->key);
    if (el->count > 0)
	dyStringAppendC(dy, '=');
    int j;
    for (j = 0;  j < el->count;  j++)
	{
	if (j > 0)
	    dyStringAppendC(dy, ',');
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
	    case vcfInfoNoType:
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

void vcfTabOut(char *db, char *table, struct sqlConnection *conn, char *fields, FILE *f)
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
struct slName *bb, *bbList = vcfGetFields(table);
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

/* Output row of labels if we are outputting only selected columns.
 * We will include original VCF header below, and adding a comment line
 * at the top invalidates the VCF. */
boolean allFields = (fieldCount == VCFDATALINE_NUM_COLS);
if (!allFields)
    {
    fprintf(f, "#%s", fieldArray[0]);
    for (i=1; i<fieldCount; ++i)
	fprintf(f, "\t%s", fieldArray[i]);
    fprintf(f, "\n");
    }

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
    char *fileName = vcfFileName(table, conn, region->chrom);
    struct vcfFile *vcff = vcfTabixFileMayOpen(fileName, region->chrom, region->start, region->end,
					       100);
    // If we are outputting all fields, but this VCF has no genotype info, omit the
    // genotype columns from output:
    if (allFields && vcff->genotypeCount == 0)
	fieldCount = VCFDATALINE_NUM_COLS - 2;
    if (!printedHeader)
	{
	fprintf(f, "%s", vcff->headerString);
	if (filter)
	    fprintf(f, "# Filtering on %d columns\n", slCount(filter->columnList));
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

if (maxOut == 0)
    warn("Reached output limit of %d data values, please make region smaller,\n\tor set a higher output line limit with the filter settings.", bigFileMaxOutput());
/* Clean up and exit. */
dyStringFree(&dyAlt);  dyStringFree(&dyFilter);  dyStringFree(&dyInfo);  dyStringFree(&dyGt);
hashFree(&fieldHash);
freeMem(fieldArray);
freeMem(columnArray);
}

static void addFilteredBedsOnRegion(char *fileName, struct region *region, char *table,
				    struct asFilter *filter, struct lm *bedLm,
				    struct bed **pBedList, struct hash *idHash)
/* Add relevant beds in reverse order to pBedList */
{
struct vcfFile *vcff = vcfTabixFileMayOpen(fileName, region->chrom, region->start, region->end,
					   100);
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
    }
dyStringFree(&dyAlt);  dyStringFree(&dyFilter);  dyStringFree(&dyInfo);  dyStringFree(&dyGt);
lmCleanup(&lm);
vcfFileFree(&vcff);
}

struct bed *vcfGetFilteredBedsOnRegions(struct sqlConnection *conn,
	char *db, char *table, struct region *regionList, struct lm *lm,
	int *retFieldCount)
/* Get list of beds from VCF, in all regions, that pass filtering. */
{
/* Figure out vcf file name get column info and filter. */
struct asObject *as = vcfAsObj();
struct asFilter *filter = asFilterFromCart(cart, db, table, as);
struct hash *idHash = identifierHash(db, table);

/* Get beds a region at a time. */
struct bed *bedList = NULL;
struct region *region;
for (region = regionList; region != NULL; region = region->next)
    {
    char *fileName = vcfFileName(table, conn, region->chrom);
    addFilteredBedsOnRegion(fileName, region, table, filter, lm, &bedList, idHash);
    freeMem(fileName);
    }
slReverse(&bedList);
return bedList;
}

struct slName *randomVcfIds(char *table, struct sqlConnection *conn, int count)
/* Return some semi-random IDs from a VCF file. */
{
/* Read 10000 items from vcf file,  or if they ask for a big list, then 4x what they ask for. */
char *fileName = vcfFileName(table, conn, NULL);
struct lineFile *lf = lineFileTabixMayOpen(fileName, TRUE);
if (lf == NULL)
    errAbort("%s", "");
int orderedCount = count * 4;
if (orderedCount < 10000)
    orderedCount = 10000;
struct slName *idList = NULL;
char *words[4];
int i;
for (i = 0;  i < orderedCount && lineFileChop(lf, words); i++)
    slAddHead(&idList, slNameNew(words[2]));
lineFileClose(&lf);
/* Shuffle list and trim it to count if necessary. */
shuffleList(&idList, 1);
struct slName *sl;
for (sl = idList, i = 0; sl != NULL; i++, sl = sl->next, i++)
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

void showSchemaVcf(char *table)
/* Show schema on vcf. */
{
struct sqlConnection *conn = hAllocConn(database);
char *fileName = vcfFileName(table, conn, NULL);

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
struct lineFile *lf = lineFileTabixMayOpen(fileName, TRUE);
if (lf == NULL)
    errAbort("%s", "");
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

/* Clean up and go home. */
lineFileClose(&lf);
freeMem(fileName);
hFreeConn(&conn);
}

