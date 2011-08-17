/* variation.c - hgTracks routines that are specific to the tracks in
 * the variation group */

#include "variation.h"
#include "imageV2.h"

static char const rcsid[] = "$Id: variation.c,v 1.148 2010/06/07 16:54:21 angie Exp $";

static double snp125AvHetCutoff = SNP125_DEFAULT_MIN_AVHET;
static int snp125WeightCutoff = SNP125_DEFAULT_MAX_WEIGHT;
static int snp132MinSubmitters = SNP132_DEFAULT_MIN_SUBMITTERS;
static float snp132MinMinorAlFreq = SNP132_DEFAULT_MIN_MINOR_AL_FREQ;
static float snp132MaxMinorAlFreq = SNP132_DEFAULT_MAX_MINOR_AL_FREQ;
static int snp132MinAlFreq2N = SNP132_DEFAULT_MIN_AL_FREQ_2N;

// Globals for caching cart coloring and filtering settings for snp125+ tracks:
static enum snp125ColorSource snp125ColorSource = SNP125_DEFAULT_COLOR_SOURCE;
static enum snp125Color *snp125LocTypeCart = NULL;
static enum snp125Color *snp125ClassCart = NULL;
static enum snp125Color *snp125MolTypeCart = NULL;
static enum snp125Color *snp125ValidCart = NULL;
static struct hash *snp125FuncCartColorHash = NULL;
static struct hash *snp125FuncCartNameHash = NULL;
static enum snp125Color *snp132ExceptionsCart = NULL;
static enum snp125Color *snp132BitfieldsCart = NULL;

static boolean snp125LocTypeFilterOn = FALSE;
static boolean snp125ClassFilterOn = FALSE;
static boolean snp125MolTypeFilterOn = FALSE;
static boolean snp125ValidFilterOn = FALSE;
static boolean snp125FuncFilterOn = FALSE;
static boolean snp132ExceptionFilterOn = FALSE;
static boolean snp132BitfieldFilterOn = FALSE;

static struct slName *snp125LocTypeFilter = NULL;
static struct slName *snp125ClassFilter = NULL;
static struct slName *snp125MolTypeFilter = NULL;
static struct slName *snp125ValidFilter = NULL;
static struct slName *snp125FuncFilter = NULL;
static struct slName *snp132ExceptionFilter = NULL;
static struct slName *snp132BitfieldFilter = NULL;

void filterSnpItems(struct track *tg, boolean (*filter)
		    (struct track *tg, void *item))
/* Filter out items from track->itemList. */
{
struct slList *newList = NULL, *el, *next;

for (el = tg->items; el != NULL; el = next)
    {
    next = el->next;
    if (filter(tg, el))
 	slAddHead(&newList, el);
    }
slReverse(&newList);
tg->items = newList;
}

boolean snpMapSourceFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter, i.e. has no excluded property. */
{
struct snpMap *el = item;
int    snpMapSource = 0;

for (snpMapSource=0; snpMapSource<snpMapSourceCartSize; snpMapSource++)
    if (containsStringNoCase(el->source,snpMapSourceDataName[snpMapSource]))
 	if (sameString(snpMapSourceCart[snpMapSource], "exclude"))
 	    return FALSE;
return TRUE;
}

boolean snpMapTypeFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter, i.e. has no excluded property. */
{
struct snpMap *el = item;
int    snpMapType = 0;

for (snpMapType=0; snpMapType<snpMapTypeCartSize; snpMapType++)
    if (containsStringNoCase(el->type,snpMapTypeDataName[snpMapType]))
 	if (sameString(snpMapTypeCart[snpMapType], "exclude"))
 	    return FALSE;
return TRUE;
}

boolean snpAvHetFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct snp *el = item;

if (el->avHet < atof(cartUsualString(cart, "snpAvHetCutoff", "0.0")))
    return FALSE;
return TRUE;
}

boolean snp125AvHetFilterItem(void *item)
/* Return TRUE if item passes filter. */
{
struct snp125 *el = item;

if (el->avHet < snp125AvHetCutoff)
    return FALSE;
return TRUE;
}

boolean snp125WeightFilterItem(void *item)
/* Return TRUE if item passes filter. */
{
struct snp125 *el = item;

if (el->weight > snp125WeightCutoff)
    return FALSE;
return TRUE;
}

boolean snp132MinSubmittersFilterItem(void *item)
/* Return TRUE if item passes filter. */
{
struct snp132Ext *el = item;

if (el->submitterCount < snp132MinSubmitters)
    return FALSE;
return TRUE;
}

static float snp132MajorAlleleFreq(const struct snp132Ext *snp)
/* Some SNPs have >2 alleles, so minor allele frequency is harder to define.
 * So instead, I'm using major allele frequency -- (1 - major) can be a proxy for minor. */
{
float majorAlF = 0.0;
int i;
for (i = 0;  i < snp->alleleFreqCount;  i++)
    if (snp->alleleFreqs[i] > majorAlF)
	majorAlF = snp->alleleFreqs[i];
return majorAlF;
}

static boolean snp132MinorAlFreqFilterItem(void *item)
/* Return TRUE if item passes filter, i.e. has a minor allele frequency >= threshold
 * (but if the range has not been changed from defaults, don't require that item has
 * any allele frequency data). */
{
if (snp132MinMinorAlFreq == SNP132_DEFAULT_MIN_MINOR_AL_FREQ &&
    snp132MaxMinorAlFreq == SNP132_DEFAULT_MAX_MINOR_AL_FREQ)
    return TRUE;
struct snp132Ext *el = item;
float majorAlFreq = snp132MajorAlleleFreq(el);
return (((1.0 - majorAlFreq) >= snp132MinMinorAlFreq) &&
	((1.0 - majorAlFreq) <= snp132MaxMinorAlFreq));
}

static boolean snp132MinAlFreq2NFilterItem(void *item)
/* Return TRUE if item passes filter, i.e. has a 2N chromosome count > threshold
 * (but if threshold is 0, don't require that item has any allele frequency data). */
{
if (snp132MinAlFreq2N == 0)
    return TRUE;
struct snp132Ext *el = item;
int twoN = 0;
int i;
for (i = 0;  i < el->alleleFreqCount;  i++)
    twoN += (int)(round(el->alleleNs[i]));
return (twoN >= snp132MinAlFreq2N);
}

boolean snpSourceFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter, i.e. has no excluded property. */
{
struct snp *el = item;
int    snpSource = 0;

for (snpSource=0; snpSource<snpSourceCartSize; snpSource++)
    if (containsStringNoCase(el->source,snpSourceDataName[snpSource]))
 	if (sameString(snpSourceCart[snpSource], "exclude") )
 	    return FALSE;
return TRUE;
}

boolean snpMolTypeFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter, i.e. has no excluded property. */
{
struct snp *el = item;
int    snpMolType = 0;

for (snpMolType=0; snpMolType<snpMolTypeCartSize; snpMolType++)
    if (containsStringNoCase(el->molType,snpMolTypeDataName[snpMolType]))
 	if ( sameString(snpMolTypeCart[snpMolType], "exclude") )
 	    return FALSE;
return TRUE;
}

static boolean snp125MolTypeFilterItem(void *item)
/* Return TRUE if item passes filter, i.e. has an included property. */
{
struct snp125 *el = (struct snp125 *)item;
if (! snp125MolTypeFilterOn)
    return TRUE;
else
    return slNameInList(snp125MolTypeFilter, el->molType);
}

boolean snpClassFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter, i.e. has no excluded property. */
{
struct snp *el = item;
int    snpClass = 0;

for (snpClass=0; snpClass<snpClassCartSize; snpClass++)
    if (containsStringNoCase(el->class,snpClassDataName[snpClass]))
 	if ( sameString(snpClassCart[snpClass], "exclude") )
 	    return FALSE;
return TRUE;
}

static boolean snp125ClassFilterItem(void *item)
/* Return TRUE if item passes filter, i.e. has an included property. */
{
struct snp125 *el = (struct snp125 *)item;
if (! snp125ClassFilterOn)
    return TRUE;
else
    return slNameInList(snp125ClassFilter, el->class);
}

boolean snpValidFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter, i.e. has no excluded property. */
{
struct snp *el = item;
int    snpValid = 0;

for (snpValid=0; snpValid<snpValidCartSize; snpValid++)
    if (containsStringNoCase(el->valid,snpValidDataName[snpValid]))
 	if ( sameString(snpValidCart[snpValid], "exclude") )
 	    return FALSE;
return TRUE;
}

static boolean snp125ValidFilterItem(void *item)
/* Return TRUE if item passes filter, i.e. has an included property. */
{
struct snp125 *el = (struct snp125 *)item;
if (! snp125ValidFilterOn)
    return TRUE;
else
    {
    char *s = el->valid, *e;
    char val[256]; // Longest validation code is much shorter than this
    while (s != NULL && s[0] != 0)
	{
	e = strchr(s, ',');
	if (e == NULL)
	    return slNameInList(snp125ValidFilter, s);
	else
	    {
	    safencpy(val, sizeof(val), s, e-s);
	    if (slNameInList(snp125ValidFilter, val))
		return TRUE;
	    e += 1;
	    }
	s = e;
	}
    return FALSE;
    }
}

boolean snpFuncFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter, i.e. has no excluded property. */
{
struct snp *el = item;
int    snpFunc = 0;

for (snpFunc=0; snpFunc<snpFuncCartSize; snpFunc++)
    if (containsStringNoCase(el->func,snpFuncDataName[snpFunc]))
 	if ( sameString(snpFuncCart[snpFunc], "exclude") )
 	    return FALSE;
return TRUE;
}

static boolean snp125FuncFilterItem(void *item)
/* Return TRUE if item passes filter, i.e. has an included property. */
{
struct snp125 *el = (struct snp125 *)item;
if (!snp125FuncFilterOn)
    return TRUE;
char *words[128];
int wordCount, i;
char funcString[4096];
safecpy(funcString, sizeof(funcString), el->func);
wordCount = chopCommas(funcString, words);
for (i = 0;  i < wordCount;  i++)
    {
    char *simpleFunc = (char *)hashMustFindVal(snp125FuncCartNameHash,
					       words[i]);
    if (slNameInList(snp125FuncFilter, simpleFunc))
	return TRUE;
    }
return FALSE;
}

boolean snpLocTypeFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter, i.e. has no excluded property. */
{
struct snp *el = item;
int    snpLocType = 0;

for (snpLocType=0; snpLocType<snpLocTypeCartSize; snpLocType++)
    if (containsStringNoCase(el->locType,snpLocTypeDataName[snpLocType]))
 	if ( sameString(snpLocTypeCart[snpLocType], "exclude") )
 	    return FALSE;
return TRUE;
}

static boolean snp125LocTypeFilterItem(void *item)
/* Return TRUE if item passes filter, i.e. has an included property. */
{
struct snp125 *el = (struct snp125 *)item;
if (! snp125LocTypeFilterOn)
    return TRUE;
else
    return slNameInList(snp125LocTypeFilter, el->locType);
}

static boolean snp132ExceptionFilterItem(void *item)
/* Return TRUE if item passes filter, i.e. has an included property. */
{
struct snp132Ext *el = item;
if (! snp132ExceptionFilterOn)
    return TRUE;
else
    {
    if (isEmpty(el->exceptions) && slNameInList(snp132ExceptionFilter, "NoExceptions"))
	return TRUE;
    char *s = el->exceptions, *e;
    char val[256]; // Longest exception name is much shorter than this
    while (s != NULL && s[0] != 0)
	{
	e = strchr(s, ',');
	if (e == NULL)
	    return slNameInList(snp132ExceptionFilter, s);
	else
	    {
	    safencpy(val, sizeof(val), s, e-s);
	    if (slNameInList(snp132ExceptionFilter, val))
		return TRUE;
	    e += 1;
	    }
	s = e;
	}
    return FALSE;
    }
}

static boolean snp132BitfieldFilterItem(void *item)
/* Return TRUE if item passes filter, i.e. has an included property. */
{
struct snp132Ext *el = item;
if (! snp132BitfieldFilterOn)
    return TRUE;
else
    {
    if (isEmpty(el->bitfields) && slNameInList(snp132BitfieldFilter, ""))
	return TRUE;
    char *s = el->bitfields, *e;
    char val[256]; // Longest bitfield name is much shorter than this
    while (s != NULL && s[0] != 0)
	{
	e = strchr(s, ',');
	if (e == NULL)
	    return slNameInList(snp132BitfieldFilter, s);
	else
	    {
	    safencpy(val, sizeof(val), s, e-s);
	    if (slNameInList(snp132BitfieldFilter, val))
		return TRUE;
	    e += 1;
	    }
	s = e;
	}
    return FALSE;
    }
}

void filterSnp125Items(struct track *tg, int version)
/* Filter out items from track->itemList using snp125 filters. */
{
struct slList *newList = NULL, *el, *next;

for (el = tg->items; el != NULL; el = next)
    {
    next = el->next;
    if (snp125AvHetFilterItem(el) &&
	snp125WeightFilterItem(el) &&
	snp125MolTypeFilterItem(el) &&
	snp125ClassFilterItem(el) &&
	snp125ValidFilterItem(el) &&
	snp125FuncFilterItem(el) &&
	(version >= 128 || snp125LocTypeFilterItem(el)) &&
	(version < 132 ||
	 (snp132MinSubmittersFilterItem(el) &&
	  snp132MinorAlFreqFilterItem(el) &&
	  snp132MinAlFreq2NFilterItem(el) &&
	  snp132ExceptionFilterItem(el) &&
	  snp132BitfieldFilterItem(el))))
        slAddHead(&newList, el);
    }
slReverse(&newList);
tg->items = newList;
}

struct orthoBed
/* Abbreviated version of orthoAlleles: bed4 plus chimp allele */
    {
    struct orthoBed *next;       /* Next in singly linked list. */
    char            *chrom;      /* Human chromosome or FPC contig */
    unsigned         chromStart; /* Start position in chromosome */
    unsigned         chromEnd;   /* End position in chromosome */
    char            *name;       /* Name of item */
    char            *chimp;      /* Chimp allele */
    };

struct orthoBed *orthoBedLoad(char **row)
/* Load a bed from row fetched with select * from bed
 * from database.  Dispose of this with bedFree(). */
/* This should be moved to kent/src/hg/lib. */
{
struct orthoBed *ret;
if (sameString(row[10], "?"))
    return NULL;
AllocVar(ret);
ret->chrom      = cloneString(row[0]);
ret->chromStart = sqlUnsigned(row[1]);
ret->chromEnd   = sqlUnsigned(row[2]);
ret->name       = cloneString(row[3]);
ret->chimp      = cloneString(row[10]);
return ret;
}

int snpOrthoCmp(const void *va, const void *vb)
/* Compare for sort based on bed4 -- like bedCmp, but more
 * deterministic because it uses chromEnd and name too. */
{
struct bed4 *a = *((struct bed4 **)va);
struct bed4 *b = *((struct bed4 **)vb);
int dif;

if(a==0||b==0)
    return 0;
dif = differentWord(a->chrom, b->chrom);
if (dif == 0)
    dif = a->chromStart - b->chromStart;
if (dif == 0)
    dif = a->chromEnd - b->chromEnd;
if (dif == 0)
    dif = differentWord(a->name, b->name);
return dif;
}

void setSnp125ExtendedNameExtra(struct track *tg)
/* add extra text to be drawn in snp name field.  This works by
   walking through two sorted lists and updating the name value
   for the SNP list with data from a table of orthologous state
   information */
{
char cartVar[512];
    safef(cartVar, sizeof(cartVar), "%s.extendedNames", tg->tdb->track);
boolean enabled = cartUsualBoolean(cart, cartVar,
			  // Check old cart var name for backwards compatibility w/ old sessions:
				   cartUsualBoolean(cart, "snp125ExtendedNames", FALSE));
if (!enabled)
    return;
struct sqlConnection *conn          = hAllocConn(database);
int                   rowOffset     = 0;
char                **row           = NULL;
struct orthoBed      *orthoItemList = NULL;      /* list of orthologous state info */
struct orthoBed      *orthoItem     = orthoItemList;
char                 *orthoTable    = snp125OrthoTable(tg->tdb, NULL);
struct sqlResult     *sr            = NULL;
int                   cmp           = 0;
struct dyString      *extra         = newDyString(256);

/* if orthologous info is not available, don't add it! */
if(isEmpty(orthoTable) || !sqlTableExists(conn, orthoTable))
    {
    hFreeConn(&conn);
    return;
    }
/* get list of orthologous alleles */
sr = hRangeQuery(conn, orthoTable, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    orthoItem = orthoBedLoad(row + rowOffset);
    if (orthoItem)
        slAddHead(&orthoItemList, orthoItem);
    }

/* List of SNPs is already sorted, so sort list of Ortho info */
slSort(&orthoItemList, snpOrthoCmp);

/* Walk through two sorted lists together */
struct snp125 *snpItem = tg->items;
orthoItem = orthoItemList;
while (snpItem!=NULL && orthoItem!=NULL)
    {
    /* check to see that we're not at the end of either list and that
     * the two list elements represent the same human position */
    cmp = snpOrthoCmp(&snpItem, &orthoItem);
    if (cmp < 0)
        {
	snpItem = snpItem->next;
	continue;
	}
    if (cmp > 0)
        {
	orthoItem = orthoItem->next;
	continue;
	}
    /* update the snp->name with the ortho data */
    dyStringPrintf(extra, "%s %s>%s", snpItem->name, orthoItem->chimp, snpItem->observed);
    snpItem->name = cloneString(extra->string);
    dyStringClear(extra);
    /* increment the list pointers */
    snpItem = snpItem->next;
    orthoItem = orthoItem->next;
    }
freeDyString(&extra);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

static char *snp125MapItemName(struct track *tg, void *item)
/* Now that snp125->name is overwritten when adding chimp allele suffix, we need
 * to strip it back off for links to hgc. */
{
static char mapName[256];
struct snp125 *snp = item;
safecpy(mapName, sizeof(mapName), snp->name);
char *ptr = strchr(mapName, ' ');
if (ptr != NULL)
    *ptr = '\0';
return mapName;
}

static Color snp132ColorByAlleleFreq(struct snp132Ext *snp, struct hvGfx *hvg)
/* If snp has allele freq data, return a shade from red (rare) to blue (common);
 * otherwise return black. */
{
static boolean colorsInited = FALSE;
static Color redToBlue[EXPR_DATA_SHADES];
static struct rgbColor red = {255, 0, 0};
static struct rgbColor blue = {0, 0, 255};
if (!colorsInited)
    hvGfxMakeColorGradient(hvg, &red, &blue, EXPR_DATA_SHADES, redToBlue);
if (snp->alleleFreqCount > 0)
    {
    float majorAlF = snp132MajorAlleleFreq(snp);
    // >2 common alleles (e.g. at VNTR sites) can cause low major allele freq;
    // cap at 0.5 to avoid overflow in the shade calculation.
    if (majorAlF < 0.5)
	majorAlF = 0.5;
    if (majorAlF > 1.0)
	majorAlF = 1.0;
    // Shade on a scale of 100% (red) to 50% (blue):
    int shadeIndex = (int)((1.0 - 2.0*(majorAlF - 0.5)) * (EXPR_DATA_SHADES-1));
    return redToBlue[shadeIndex];
    }
return MG_BLACK;
}

Color snp125Color(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of snp track item -- stashed in the weight column for set/enum
 * attributes that were used for sorting at draw time.  Allele frequency shading
 * must be done at draw time because it uses hvg.  Aside from allele frequencies
 * and overloaded weight, only the bed4 fields of snp are used at draw time). */
{
struct snp132Ext *snp = item;
if (snp125ColorSource == snp125ColorSourceAlleleFreq)
    return snp132ColorByAlleleFreq(snp, hvg);
else
    return (Color)(snp->weight);
}

int snp125ColorCmpRaw(const Color ca, const char *aName, const Color cb, const char *bName)
/* Compare to sort based on color -- black first, red last.  This is not
 * a slSort Cmp function, just a comparator of the values. */
{
/* order is important here */
if (ca==cb)
    return 0;
if (ca==MG_RED)
    return 1;
if (cb==MG_RED)
    return -1;
if (ca==MG_GREEN)
    return 1;
if (cb==MG_GREEN)
    return -1;
if (ca==MG_BLUE)
    return 1;
if (cb==MG_BLUE)
    return -1;
if (ca==MG_GRAY)
    return 1;
if (cb==MG_GRAY)
    return -1;
if (ca==MG_BLACK)
    return 1;
if (cb==MG_BLACK)
    return -1;
hPrintComment("SNP track: colors %d (%s) and %d (%s) not known", ca, aName, cb, bName);
return 0;
}

int snp125ColorCmp(const void *va, const void *vb)
/* Compare to sort based on color -- black first, red last */
{
const struct snp125 *a = *((struct snp125 **)va);
const struct snp125 *b = *((struct snp125 **)vb);
const Color ca = (Color)(a->weight);
const Color cb = (Color)(b->weight);

return snp125ColorCmpRaw(ca, a->name, cb, b->name);
}

int snp125ColorCmpDesc(const void *va, const void *vb)
/* Compare to sort based on color -- red first, black last */
{
return snp125ColorCmp(vb, va);
}

static enum snp125Color *snp125ColorsFromCart(char *track, char *attribute,
				 char *vars[], boolean varsAreOld, char *defaults[], int varCount)
/* Look up attribute colors in cart using both old and new cart var names where applicable. */
{
enum snp125Color *cartColors = NULL;
AllocArray(cartColors, varCount);
int i;
for (i=0; i < varCount; i++)
    {
    char cartVar[512];
    safef(cartVar, sizeof(cartVar), "%s.%s%s", track, attribute,
	  (varsAreOld ? snp125OldColorVarToNew(vars[i], attribute) : vars[i]));
    char *defaultCol = defaults[i];
    if (varsAreOld)
	defaultCol = cartUsualString(cart, vars[i], defaultCol);
    char *col = cartUsualString(cart, cartVar, defaultCol);
    cartColors[i] = stringArrayIx(col, snp125ColorLabel, snp125ColorArraySize);
    }
return cartColors;
}

static void snp125SetupFiltersAndColorsFromCart(struct trackDb *tdb)
/* Load the controls set by hgTrackUi into global vars. */
{
char *track = tdb->track;
char cartVar[512];
safef(cartVar, sizeof(cartVar), "%s.minAvHet", track);
snp125AvHetCutoff = cartUsualDouble(cart, cartVar,
			     // Check old cart var name:
			     cartUsualDouble(cart, "snp125AvHetCutoff", SNP125_DEFAULT_MIN_AVHET));
safef(cartVar, sizeof(cartVar), "%s.maxWeight", track);
int defaultMaxWeight = SNP125_DEFAULT_MAX_WEIGHT;
char *setting = trackDbSetting(tdb, "defaultMaxWeight");
if (isNotEmpty(setting))
    defaultMaxWeight = atoi(setting);
snp125WeightCutoff = cartUsualInt(cart, cartVar,
			     // Check old cart var name and tdb default:
			     cartUsualInt(cart, "snp125WeightCutoff", defaultMaxWeight));
safef(cartVar, sizeof(cartVar), "%s.minSubmitters", track);
snp132MinSubmitters = cartUsualInt(cart, cartVar, SNP132_DEFAULT_MIN_SUBMITTERS);
safef(cartVar, sizeof(cartVar), "%s.minMinorAlFreq", track);
snp132MinMinorAlFreq = cartUsualDouble(cart, cartVar, SNP132_DEFAULT_MIN_MINOR_AL_FREQ);
safef(cartVar, sizeof(cartVar), "%s.maxMinorAlFreq", track);
snp132MaxMinorAlFreq = cartUsualDouble(cart, cartVar, SNP132_DEFAULT_MAX_MINOR_AL_FREQ);
safef(cartVar, sizeof(cartVar), "%s.minAlFreq2N", tdb->track);
snp132MinAlFreq2N = cartUsualInt(cart, cartVar, SNP132_DEFAULT_MIN_AL_FREQ_2N);

snp125MolTypeFilter = snp125FilterFromCart(cart, track, "molType", &snp125MolTypeFilterOn);
snp125ClassFilter = snp125FilterFromCart(cart, track, "class", &snp125ClassFilterOn);
snp125ValidFilter = snp125FilterFromCart(cart, track, "valid", &snp125ValidFilterOn);
snp125FuncFilter = snp125FilterFromCart(cart, track, "func", &snp125FuncFilterOn);
snp125LocTypeFilter = snp125FilterFromCart(cart, track, "locType", &snp125LocTypeFilterOn);
snp132ExceptionFilter = snp125FilterFromCart(cart, track, "exceptions", &snp132ExceptionFilterOn);
snp132BitfieldFilter = snp125FilterFromCart(cart, track, "bitfields", &snp132BitfieldFilterOn);

snp125ColorSource = snp125ColorSourceFromCart(cart, tdb);
snp125MolTypeCart = snp125ColorsFromCart(track, "molType", snp125MolTypeOldColorVars, TRUE,
					 snp125MolTypeDefault, snp125MolTypeArraySize);
snp125ClassCart = snp125ColorsFromCart(track, "class", snp125ClassOldColorVars, TRUE,
				       snp125ClassDefault, snp125ClassArraySize);
snp125ValidCart = snp125ColorsFromCart(track, "valid", snp125ValidOldColorVars, TRUE,
				       snp125ValidDefault, snp125ValidArraySize);
snp125LocTypeCart = snp125ColorsFromCart(track, "locType", snp125LocTypeOldColorVars, TRUE,
					 snp125LocTypeDefault, snp125LocTypeArraySize);
snp132ExceptionsCart = snp125ColorsFromCart(track, "exception", snp132ExceptionVarName, FALSE,
					    snp132ExceptionDefault, snp132ExceptionArraySize);
snp132BitfieldsCart = snp125ColorsFromCart(track, "bitfields", snp132BitfieldVarName, FALSE,
					   snp132BitfieldDefault, snp132BitfieldArraySize);

snp125FuncCartColorHash = hashNew(0);
snp125FuncCartNameHash = hashNew(0);
int i;
for (i=0; i < snp125FuncArraySize; i++)
    {
    safef(cartVar, sizeof(cartVar), "%s.func%s",
	  track, snp125OldColorVarToNew(snp125FuncOldColorVars[i], "func"));
    char *cartVal = cartUsualString(cart, cartVar,
				    cartUsualString(cart, snp125FuncOldColorVars[i],
						    snp125FuncDefault[i]));
    /* There are many function types, some of which are mapped onto
     * simpler types in snp125Ui.c.  First store the indexes of
     * selected colors of simpler types that we present as coloring
     * choices; then (below) map the more detailed function types'
     * indexes onto the simpler types' indexes. */
    hashAddInt(snp125FuncCartColorHash, snp125FuncDataName[i],
	       stringArrayIx(cartVal, snp125ColorLabel, snp125ColorArraySize));
    /* Similarly, map names.  Self-mapping here, synonyms below. */
    hashAdd(snp125FuncCartNameHash, snp125FuncDataName[i],
	    snp125FuncDataName[i]);
    }
int j, k;
for (j = 0;  snp125FuncDataSynonyms[j] != NULL;  j++)
    {
    char *canonical = snp125FuncDataSynonyms[j][0];
    for (k = 1;  snp125FuncDataSynonyms[j][k] != NULL;  k++)
	{
	hashAddInt(snp125FuncCartColorHash, snp125FuncDataSynonyms[j][k],
		   hashIntVal(snp125FuncCartColorHash, canonical));
	hashAdd(snp125FuncCartNameHash, snp125FuncDataSynonyms[j][k], canonical);
	}
    }
}

Color snp125ColorToMg(enum snpColorEnum thisSnpColor)
/* Translate full range of snpColorEnum into memgfx MG_<COLOR>. */
{
switch (thisSnpColor)
    {
    case snp125ColorRed:
	return MG_RED;
	break;
    case snp125ColorGreen:
	return MG_GREEN;
	break;
    case snp125ColorBlue:
	return MG_BLUE;
	break;
    case snp125ColorGray:
	return MG_GRAY;
	break;
    case snp125ColorBlack:
    default:
	return MG_BLACK;
	break;
    }
}

static void snp125ColorItems(struct track *tg, int version)
/* Use cart settings and snp properties to assign a color to snp -- and stash it in snp->weight.
 * Note: we can't do the allele frequency shading here because that uses hvg for the shades,
 * and hvg isn't passed in until draw time. */
{
struct snp132Ext *snp;
for (snp = tg->items;  snp != NULL;  snp = snp->next)
    {
    enum snp125Color color = snp125ColorBlack;
    int valIx;
    char *words[128];
    int wordCount, i;
    char buf[4096];
    switch (snp125ColorSource)
	{
	case snp125ColorSourceMolType:
	    valIx = stringArrayIx(snp->molType, snp125MolTypeDataName, snp125MolTypeArraySize);
	    if (valIx < 0)
		valIx = 0;
	    color = snp125MolTypeCart[valIx];
	    break;
	case snp125ColorSourceClass:
	    valIx = stringArrayIx(snp->class, snp125ClassDataName, snp125ClassArraySize);
	    if (valIx < 0)
		valIx = 0;
	    color = snp125ClassCart[valIx];
	    break;
	case snp125ColorSourceValid:
	    for (i=0; i < snp125ValidArraySize; i++)
		if (containsStringNoCase(snp->valid, snp125ValidDataName[i]))
		    color = snp125ValidCart[i];
	    break;
	case snp125ColorSourceFunc:
	    {
	    safecpy(buf, sizeof(buf), snp->func);
	    wordCount = chopCommas(buf, words);
	    for (i = 0;  i < wordCount;  i++)
		{
		enum snp125Color wordColor = hashIntVal(snp125FuncCartColorHash, words[i]);
		if (snp125ColorCmpRaw(snp125ColorToMg(wordColor), "wordColor",
				      snp125ColorToMg(color), "color") > 0)
		    color = wordColor;
		}
	    }
	    break;
	case snp125ColorSourceLocType:
	    valIx = stringArrayIx(snp->locType,snp125LocTypeDataName,snp125LocTypeArraySize);
	    if (valIx < 0)
		valIx = 0;
	    color = snp125LocTypeCart[valIx];
	    break;
	case snp125ColorSourceExceptions:
	    {
	    if (isEmpty(snp->exceptions))
		color = snp132ExceptionsCart[0];
	    else
		{
		safecpy(buf, sizeof(buf), snp->exceptions);
		wordCount = chopCommas(buf, words);
		for (i = 0;  i < wordCount;  i++)
		    {
		    valIx = stringArrayIx(words[i], snp132ExceptionVarName, snp132ExceptionArraySize);
		    enum snp125Color wordColor = snp132ExceptionsCart[valIx];
		    if (snp125ColorCmpRaw(snp125ColorToMg(wordColor), "wordColor",
					  snp125ColorToMg(color), "color") > 0)
			color = wordColor;
		    }
		}
	    }
	    break;
	case snp125ColorSourceBitfields:
	    {
	    if (isEmpty(snp->bitfields))
		color = snp132BitfieldsCart[0];
	    else
		{
		safecpy(buf, sizeof(buf), snp->bitfields);
		wordCount = chopCommas(buf, words);
		for (i = 0;  i < wordCount;  i++)
		    {
		    valIx = stringArrayIx(words[i], snp132BitfieldDataName, snp132BitfieldArraySize);
		    enum snp125Color wordColor = snp132BitfieldsCart[valIx];
		    if (snp125ColorCmpRaw(snp125ColorToMg(wordColor), "wordColor",
					  snp125ColorToMg(color), "color") > 0)
			color = wordColor;
		    }
		}
	    }
	    break;
	case snp125ColorSourceAlleleFreq:
	default:
	    color = snp125ColorBlack;
	    break;
	}
    snp->weight = snp125ColorToMg(color);
    }
}

static void loadSnp125Basic(struct track *tg, int version, void *(loadFunction)(char **))
/* load snp125 or snp132Ext items from table */
{
struct sqlConnection *conn = hAllocConn(database);
struct snp132Ext *itemList = NULL;
int rowOffset = 0;
char **row = NULL;
struct sqlResult *sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct slList *snp = loadFunction(row + rowOffset);
    slAddHead(&itemList, snp);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
tg->items = itemList;
}

static int bedCmpStartName(const void *va, const void *vb)
/* Compare two bed4's by chromStart (chrom assumed to be same) and name. */
{
const struct bed *a = *((struct bed **)va);
const struct bed *b = *((struct bed **)vb);
int diff = a->chromStart - b->chromStart;
if (diff == 0)
    diff = strcmp(a->name, b->name);
return diff;
}

static int snp132AlFreqCmp(const void *va, const void *vb)
/* Compare two SNPs by allele frequency: (no info/)rare first, common last. */
{
const struct snp132Ext *a = *((struct snp132Ext **)va);
const struct snp132Ext *b = *((struct snp132Ext **)vb);
if (a->alleleFreqCount == 0 && b->alleleFreqCount == 0)
    return bedCmpStartName(va, vb);
else if (a->alleleFreqCount == 0 && b->alleleFreqCount != 0)
    return -1;
else if (a->alleleFreqCount == 0 && b->alleleFreqCount != 0)
    return 1;
else
    {
    float majorAlFA = snp132MajorAlleleFreq(a);
    float majorAlFB = snp132MajorAlleleFreq(b);
    if (majorAlFA > majorAlFB)
	return -1;
    else if (majorAlFA < majorAlFB)
	return 1;
    else
	return bedCmpStartName(va, vb);
    }
}

static int snp132AlFreqCmpDesc(const void *va, const void *vb)
/* Compare two SNPs by allele frequency: common first, (no info/)rare last. */
{
return snp132AlFreqCmp(vb, va);
}

void loadSnp125(struct track *tg)
/* load snps from table, ortho alleles from snpXXXOrthoXXX table, filter and color. */
{
int version = snpVersion(tg->table);
if (version >= 132)
    loadSnp125Basic(tg, version, (void *)(snp132ExtLoad));
else if (version >= 125)
    loadSnp125Basic(tg, version, (void *)(snp125Load));
else
    errAbort("How was loadSnp125 called on version < 125? (%d)", version);

snp125SetupFiltersAndColorsFromCart(tg->tdb);
filterSnp125Items(tg, version);
snp125ColorItems(tg, version);

// If in dense or squish mode, sort items by color or allele frequency:
boolean sortByAF = (snp125ColorSource == snp125ColorSourceAlleleFreq);
if (tg->visibility == tvDense)
    slSort(&tg->items, sortByAF ? snp132AlFreqCmp : snp125ColorCmp);
else if (tg->visibility == tvSquish)
    slSort(&tg->items, sortByAF ? snp132AlFreqCmpDesc : snp125ColorCmpDesc);
else
    {
    slSort(&tg->items, snpOrthoCmp);
    setSnp125ExtendedNameExtra(tg);
    enum trackVisibility newVis = limitVisibility(tg);
    if (newVis == tvDense)
	slSort(&tg->items, sortByAF ? snp132AlFreqCmp : snp125ColorCmp);
    else if (newVis == tvSquish)
	slSort(&tg->items, sortByAF ? snp132AlFreqCmpDesc : snp125ColorCmpDesc);
    }
}

void loadSnpMap(struct track *tg)
/* Load up snpMap from database table to track items. */
{
int  snpMapSource, snpMapType;

for (snpMapSource=0; snpMapSource<snpMapSourceCartSize; snpMapSource++)
    snpMapSourceCart[snpMapSource] = cartUsualString(cart,
       snpMapSourceStrings[snpMapSource], snpMapSourceDefault[snpMapSource]);
for (snpMapType=0; snpMapType<snpMapTypeCartSize; snpMapType++)
    snpMapTypeCart[snpMapType] = cartUsualString(cart,
       snpMapTypeStrings[snpMapType], snpMapTypeDefault[snpMapType]);
bedLoadItem(tg, "snpMap", (ItemLoader)snpMapLoad);
if (!startsWith("hg",database))
    return;
filterSnpItems(tg, snpMapSourceFilterItem);
filterSnpItems(tg, snpMapTypeFilterItem);
}

void loadSnp(struct track *tg)
/* Load up snp from database table to track items. */
{
int  i = 0;

snpAvHetCutoff = atof(cartUsualString(cart, "snpAvHetCutoff", "0.0"));

for (i=0; i < snpSourceCartSize;  i++)
    snpSourceCart[i]  = cartUsualString(cart, snpSourceStrings[i],  snpSourceDefault[i]);
for (i=0; i < snpMolTypeCartSize; i++)
    snpMolTypeCart[i] = cartUsualString(cart, snpMolTypeStrings[i], snpMolTypeDefault[i]);
for (i=0; i < snpClassCartSize;   i++)
    snpClassCart[i]   = cartUsualString(cart, snpClassStrings[i],   snpClassDefault[i]);
for (i=0; i < snpValidCartSize;   i++)
    snpValidCart[i]   = cartUsualString(cart, snpValidStrings[i],   snpValidDefault[i]);
for (i=0; i < snpFuncCartSize;    i++)
    snpFuncCart[i]    = cartUsualString(cart, snpFuncStrings[i],    snpFuncDefault[i]);
for (i=0; i < snpLocTypeCartSize; i++)
    snpLocTypeCart[i] = cartUsualString(cart, snpLocTypeStrings[i], snpLocTypeDefault[i]);

bedLoadItem(tg, "snp", (ItemLoader)snpLoad);

filterSnpItems(tg, snpAvHetFilterItem);
filterSnpItems(tg, snpSourceFilterItem);
filterSnpItems(tg, snpMolTypeFilterItem);
filterSnpItems(tg, snpClassFilterItem);
filterSnpItems(tg, snpValidFilterItem);
filterSnpItems(tg, snpFuncFilterItem);
filterSnpItems(tg, snpLocTypeFilterItem);
}

void freeSnpMap(struct track *tg)
/* Free up snpMap items. */
{
snpMapFreeList((struct snpMap**)&tg->items);
}

void freeSnp(struct track *tg)
/* Free up snp items. */
{
snpFreeList((struct snp**)&tg->items);
}

void freeSnp125(struct track *tg)
/* Free up snp125 items. */
{
snp125FreeList((struct snp125**)&tg->items);
}


Color snpMapColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of snpMap track item. */
{
struct snpMap *el = item;
enum   snpColorEnum thisSnpColor = stringArrayIx( snpMapSourceCart[stringArrayIx(el->source,snpMapSourceDataName,snpMapSourceDataNameSize)],
						  snpColorLabel,snpColorLabelSize);

switch (thisSnpColor)
    {
    case snpColorRed:
	return MG_RED;
	break;
    case snpColorGreen:
	return MG_GREEN;
	break;
    case snpColorBlue:
	return MG_BLUE;
	break;
    default:
	return MG_BLACK;
	break;
    }
}

Color snpColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of snp track item. */
{
struct snp *el = item;
enum   snpColorEnum thisSnpColor = snpColorBlack;
char  *snpColorSource = cartUsualString(cart, snpColorSourceDataName[0], snpColorSourceDefault[0]);
char  *validString = NULL;
char  *funcString = NULL;
int    snpValid = 0;
int    snpFunc = 0;
int    index1 = 0;
int    index2 = 0;

index1 =  stringArrayIx(snpColorSource, snpColorSourceStrings, snpColorSourceStringsSize);

switch (stringArrayIx(snpColorSource, snpColorSourceStrings, snpColorSourceStringsSize))
    {
    case snpColorSourceSource:
	index2 = stringArrayIx(el->source,snpSourceDataName,snpSourceDataNameSize);
	thisSnpColor=(enum snpColorEnum)stringArrayIx(snpSourceCart[index2],snpColorLabel,snpColorLabelSize);
	break;
    case snpColorSourceMolType:
	index2 = stringArrayIx(el->molType,snpMolTypeDataName,snpMolTypeDataNameSize);
	thisSnpColor=(enum snpColorEnum)stringArrayIx(snpMolTypeCart[index2],snpColorLabel,snpColorLabelSize);
	break;
    case snpColorSourceClass:
	index2 = stringArrayIx(el->class,snpClassDataName,snpClassDataNameSize);
	thisSnpColor=(enum snpColorEnum)stringArrayIx(snpClassCart[index2],snpColorLabel,snpColorLabelSize);
	break;
    case snpColorSourceValid:
	validString = cloneString(el->valid);
	for (snpValid=0; snpValid<snpValidCartSize; snpValid++)
	    if (containsStringNoCase(validString, snpValidDataName[snpValid]))
		thisSnpColor = (enum snpColorEnum) stringArrayIx(snpValidCart[snpValid],snpColorLabel,snpColorLabelSize);
	break;
    case snpColorSourceFunc:
	funcString = cloneString(el->func);
	for (snpFunc=0; snpFunc<snpFuncCartSize; snpFunc++)
	    if (containsStringNoCase(funcString, snpFuncDataName[snpFunc]))
		thisSnpColor = (enum snpColorEnum) stringArrayIx(snpFuncCart[snpFunc],snpColorLabel,snpColorLabelSize);
	break;
    case snpColorSourceLocType:
	index2 = stringArrayIx(el->locType,snpLocTypeDataName,snpLocTypeDataNameSize);
	thisSnpColor=(enum snpColorEnum)stringArrayIx(snpLocTypeCart[index2],snpColorLabel,snpColorLabelSize);
	break;
    default:
	thisSnpColor = snpColorBlack;
	break;
    }
switch (thisSnpColor)
    {
    case snpColorRed:
 	return MG_RED;
 	break;
    case snpColorGreen:
 	return MG_GREEN;
 	break;
    case snpColorBlue:
 	return MG_BLUE;
 	break;
    case snpColorBlack:
    default:
 	return MG_BLACK;
 	break;
    }
}

void snpMapDrawItemAt(struct track *tg, void *item, struct hvGfx *hvg, int xOff, int y,
	double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single snpMap item at position. */
{
struct snpMap *sm = item;
int heightPer = tg->heightPer;
int x1 = round((double)((int)sm->chromStart-winStart)*scale) + xOff;
int x2 = round((double)((int)sm->chromEnd-winStart)*scale) + xOff;
int w = x2-x1;
Color itemColor = tg->itemColor(tg, sm, hvg);

if ( w<1 )
    w = 1;
hvGfxBox(hvg, x1, y, w, heightPer, itemColor);
/* Clip here so that text will tend to be more visible... */
if (tg->drawName && vis != tvSquish)
    mapBoxHc(hvg, sm->chromStart, sm->chromEnd, x1, y, w, heightPer, tg->track, tg->mapItemName(tg, sm), NULL);
}

void snpDrawItemAt(struct track *tg, void *item, struct hvGfx *hvg, int xOff, int y,
	double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single snp item at position. */
{
struct snp *s = item;
int heightPer = tg->heightPer;
int x1 = round((double)((int)s->chromStart-winStart)*scale) + xOff;
int x2 = round((double)((int)s->chromEnd-winStart)*scale) + xOff;
int w = x2-x1;
Color itemColor = tg->itemColor(tg, s, hvg);

if ( w<1 )
    w = 1;
hvGfxBox(hvg, x1, y, w, heightPer, itemColor);
/* Clip here so that text will tend to be more visible... */
if (tg->drawName && vis != tvSquish)
    mapBoxHc(hvg, s->chromStart, s->chromEnd, x1, y, w, heightPer,
	     tg->track, tg->mapItemName(tg, s), NULL);
}

void snp125DrawItemAt(struct track *tg, void *item, struct hvGfx *hvg, int xOff, int y,
	double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single snp125 item at position. */
{
struct snp125 *s = item;
int heightPer = tg->heightPer;
int x1 = round((double)((int)s->chromStart-winStart)*scale) + xOff;
int x2 = round((double)((int)s->chromEnd-winStart)*scale) + xOff;
int w = x2-x1;

if (w < 1)
    w = 1;
hvGfxBox(hvg, x1, y, w, heightPer, color);
/* Clip here so that text will tend to be more visible... */
if (tg->drawName && vis != tvSquish)
    mapBoxHc(hvg, s->chromStart, s->chromEnd, x1, y, w, heightPer,
	     tg->track, tg->mapItemName(tg, s), NULL);
}

static void snpMapDrawItems(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width,
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw snpMap items. */
{
double scale = scaleForPixels(width);
int lineHeight = tg->lineHeight;
int heightPer = tg->heightPer;
int w, y;
boolean withLabels = (withLeftLabels && vis == tvPack && !tg->drawName);
#ifdef IMAGEv2_NO_LEFTLABEL_ON_FULL
if (theImgBox != NULL)
    withLabels = (withLeftLabels && (vis == tvPack || vis == tvFull) && !tg->drawName);
#endif ///def IMAGEv2_NO_LEFTLABEL_ON_FULL

if (!tg->drawItemAt)
    errAbort("missing drawItemAt in track %s", tg->track);

if (vis == tvPack || vis == tvSquish)
    {
    struct spaceSaver *ss = tg->ss;
    struct spaceNode *sn = NULL;
    hvGfxSetClip(hvg, insideX, yOff, insideWidth, tg->height);
    for (sn = ss->nodeList; sn != NULL; sn = sn->next)
        {
        struct slList *item = sn->val;
        int s = tg->itemStart(tg, item);
        int e = tg->itemEnd(tg, item);
        int x1 = round((s - winStart)*scale) + xOff;
        int x2 = round((e - winStart)*scale) + xOff;
        int textX = x1;
        char *name = tg->itemName(tg, item);
	Color itemColor = tg->itemColor(tg, item, hvg);
        Color itemNameColor = tg->itemNameColor(tg, item, hvg);

        y = yOff + lineHeight * sn->row;
        tg->drawItemAt(tg, item, hvg, xOff, y, scale, font, itemColor, vis);
        if (withLabels)
            {
            int nameWidth = mgFontStringWidth(font, name);
            int dotWidth = tl.nWidth/2;
            textX -= nameWidth + dotWidth;
        #ifdef IMAGEv2_NO_LEFTLABEL_ON_FULL
            if (theImgBox == NULL && textX < insideX)
        #else///ifndef IMAGEv2_NO_LEFTLABEL_ON_FULL
            if (textX < insideX)        /* Snap label to the left. */
        #endif ///ndef IMAGEv2_NO_LEFTLABEL_ON_FULL
		{
		textX = leftLabelX;
                assert(hvgSide != NULL);
		hvGfxUnclip(hvgSide);
		hvGfxSetClip(hvgSide, leftLabelX, yOff, insideWidth, tg->height);
		hvGfxTextRight(hvgSide, leftLabelX, y, leftLabelWidth-1, heightPer,
			    itemNameColor, font, name);
		hvGfxUnclip(hvgSide);
		hvGfxSetClip(hvgSide, insideX, yOff, insideWidth, tg->height);
		}
            else
		hvGfxTextRight(hvg, textX, y, nameWidth, heightPer,
			    itemNameColor, font, name);
            }
        if (!tg->mapsSelf && ( ( w = x2-textX ) > 0 ))
	    mapBoxHgcOrHgGene(hvg, s, e, textX, y, w, heightPer, tg->track, tg->mapItemName(tg, item), name, NULL, FALSE, NULL);
        }
    hvGfxUnclip(hvg);
    }
else
    {
    struct slList *item;
    y = yOff;
    for (item = tg->items; item != NULL; item = item->next)
        {
	Color itemColor = tg->itemColor(tg, item, hvg);
        tg->drawItemAt(tg, item, hvg, xOff, y, scale, font, itemColor, vis);
        if (vis == tvFull)
            {
        #ifdef IMAGEv2_NO_LEFTLABEL_ON_FULL
            if (theImgBox != NULL) // In dragScroll >1x item labels cannot be in leftLabel
                {                  // So they appear here in the image, just like in pack
                int s = tg->itemStart(tg, item);
                int textX = round((s - winStart)*scale) + xOff;
                if (textX >= insideX)
                    {
                    char *name = tg->itemName(tg, item);
                    int nameWidth = mgFontStringWidth(font, name);
                    Color itemNameColor = tg->itemNameColor(tg, item, hvg);
                    hvGfxTextRight(hvg, textX, y, nameWidth, heightPer, itemNameColor, font, name);
                    }
                }
        #endif ///def IMAGEv2_NO_LEFTLABEL_ON_FULL
	    y += lineHeight;
            }
        }
    }
}

static void snpDrawItems(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width,
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw snp items. */
{
double scale = scaleForPixels(width);
int lineHeight = tg->lineHeight;
int heightPer = tg->heightPer;
int y, w;
boolean withLabels = (withLeftLabels && vis == tvPack && !tg->drawName);
#ifdef IMAGEv2_NO_LEFTLABEL_ON_FULL
if (theImgBox != NULL)
    withLabels = (withLeftLabels && (vis == tvPack || tvFull) && !tg->drawName);
#endif ///def IMAGEv2_NO_LEFTLABEL_ON_FULL
snp125ColorSource = snp125ColorSourceFromCart(cart, tg->tdb);

if (!tg->drawItemAt)
    errAbort("missing drawItemAt in track %s", tg->track);
if (vis == tvPack || vis == tvSquish)
    {
    struct spaceSaver *ss = tg->ss;
    struct spaceNode *sn = NULL;
    hvGfxSetClip(hvg, insideX, yOff, insideWidth, tg->height);
    for (sn = ss->nodeList; sn != NULL; sn = sn->next)
        {
        struct slList *item = sn->val;
        int s = tg->itemStart(tg, item);
        int e = tg->itemEnd(tg, item);
        int x1 = round((s - winStart)*scale) + xOff;
        int x2 = round((e - winStart)*scale) + xOff;
        int textX = x1;
        char *name = tg->itemName(tg, item);
	Color itemColor = tg->itemColor(tg, item, hvg);
        Color itemNameColor = tg->itemNameColor(tg, item, hvg);
        boolean drawNameInverted = FALSE;

        y = yOff + lineHeight * sn->row;
        tg->drawItemAt(tg, item, hvg, xOff, y, scale, font, itemColor, vis);
        if (withLabels)
            {
            int nameWidth = mgFontStringWidth(font, name);
            int dotWidth = tl.nWidth/2;
	    boolean snapLeft = FALSE;
	    drawNameInverted = highlightItem(tg, item);
            textX -= nameWidth + dotWidth;
	    snapLeft = (textX < insideX);
        #ifdef IMAGEv2_NO_LEFTLABEL_ON_FULL
            if (theImgBox == NULL && snapLeft)
        #else///ifndef IMAGEv2_NO_LEFTLABEL_ON_FULL
            if (snapLeft)
        #endif ///ndef IMAGEv2_NO_LEFTLABEL_ON_FULL
		{
		textX = leftLabelX;
                assert(hvgSide != NULL);
		hvGfxUnclip(hvgSide);
		hvGfxSetClip(hvgSide, leftLabelX, yOff, insideWidth, tg->height);
		if (drawNameInverted)
		    {
		    int boxStart = leftLabelX + leftLabelWidth - 2 - nameWidth;
		    hvGfxBox(hvgSide, boxStart, y, nameWidth+1, heightPer - 1, color);
		    hvGfxTextRight(hvgSide, leftLabelX, y, leftLabelWidth-1, heightPer,
		                MG_WHITE, font, name);
		    }
		else
		    hvGfxTextRight(hvgSide, leftLabelX, y, leftLabelWidth-1, heightPer,
			    itemNameColor, font, name);
		hvGfxUnclip(hvgSide);
		hvGfxSetClip(hvgSide, insideX, yOff, insideWidth, tg->height);
		}
            else
	        {
		if (drawNameInverted)
		    {
		    hvGfxBox(hvg, textX - 1, y, nameWidth+1, heightPer-1, color);
		    hvGfxTextRight(hvg, textX, y, nameWidth, heightPer, MG_WHITE, font, name);
		    }
		else
		    hvGfxTextRight(hvg, textX, y, nameWidth, heightPer,
			    itemNameColor, font, name);
		}
            }
        if (!tg->mapsSelf && ( ( w = x2-textX ) > 0 ) )
	    mapBoxHgcOrHgGene(hvg, s, e, textX, y, w, heightPer, tg->track, tg->mapItemName(tg, item), name, NULL, FALSE, NULL);
        }
    hvGfxUnclip(hvg);
    }
else
    {
    struct slList *item;
    y = yOff;
    for (item = tg->items; item != NULL; item = item->next)
        {
	Color itemColor = tg->itemColor(tg, item, hvg);
        tg->drawItemAt(tg, item, hvg, xOff, y, scale, font, itemColor, vis);
        if (vis == tvFull)
	    y += lineHeight;
        }
    }
}

void snpMapMethods(struct track *tg)
/* Make track for snps. */
{
tg->drawItems = snpMapDrawItems;
tg->drawItemAt = snpMapDrawItemAt;
tg->loadItems = loadSnpMap;
tg->freeItems = freeSnpMap;
tg->itemColor = snpMapColor;
tg->itemNameColor = snpMapColor;
}

void snpMethods(struct track *tg)
/* Make track for snps. */
{
tg->drawItems     = snpDrawItems;
tg->drawItemAt    = snpDrawItemAt;
tg->loadItems     = loadSnp;
tg->freeItems     = freeSnp;
tg->itemColor     = snpColor;
tg->itemNameColor = snpColor;
}

void snp125Methods(struct track *tg)
{
tg->drawItems     = snpDrawItems;
tg->drawItemAt    = snp125DrawItemAt;
tg->freeItems     = freeSnp125;
tg->loadItems     = loadSnp125;
tg->mapItemName   = snp125MapItemName;
tg->itemNameColor = snp125Color;
tg->itemColor     = snp125Color;
}

char *perlegenName(struct track *tg, void *item)
/* return the actual perlegen name, in form xx/yyyy:
      cut off xx/ and return yyyy */
{
char *name;
struct linkedFeatures *lf = item;
name = strstr(lf->name, "/");
if (name != NULL)
    name ++;
if (name != NULL)
    return name;
return "unknown";
}

int haplotypeHeight(struct track *tg, struct linkedFeatures *lf,
                    struct simpleFeature *sf)
/* if the item isn't the first or the last make it smaller */
{
if(sf == lf->components || sf->next == NULL)
    return tg->heightPer;
return (tg->heightPer-4);
}

static void haplotypeLinkedFeaturesDrawAt(struct track *tg, void *item,
               struct hvGfx *hvg, int xOff, int y, double scale,
	       MgFont *font, Color color, enum trackVisibility vis)
/* draws and individual haplotype and a given location */
{
struct linkedFeatures *lf = item;
struct simpleFeature *sf = NULL;
int x1 = 0;
int x2 = 0;
int w = 0;
int heightPer = tg->heightPer;
int shortHeight = heightPer / 2;
int s = 0;
int e = 0;
Color *shades = tg->colorShades;
boolean isXeno = (tg->subType == lfSubXeno) || (tg->subType == lfSubChain);
boolean hideLine = (vis == tvDense && isXeno);

/* draw haplotype thick line ... */
if (lf->components != NULL && !hideLine)
    {
    x1 = round((double)((int)lf->start-winStart)*scale) + xOff;
    x2 = round((double)((int)lf->end-winStart)*scale) + xOff;
    w = x2 - x1;
    color = shades[lf->grayIx+isXeno];

    hvGfxBox(hvg, x1, y+3, w, shortHeight-2, color);
    if (vis==tvSquish)
	hvGfxBox(hvg, x1, y+1, w, tg->heightPer/2, color);
    else
	hvGfxBox(hvg, x1, y+3, w, shortHeight-1, color);
    }
for (sf = lf->components; sf != NULL; sf = sf->next)
    {
    s = sf->start;
    e = sf->end;
    heightPer = haplotypeHeight(tg, lf, sf);
    if (vis==tvSquish)
	drawScaledBox(hvg, s, e, scale, xOff, y, tg->heightPer, blackIndex());
    else
	drawScaledBox(hvg, s, e, scale, xOff, y+((tg->heightPer-heightPer)/2),
		      heightPer, blackIndex());
    }
}

void haplotypeMethods(struct track *tg)
/* setup special methods for haplotype track */
{
tg->drawItemAt = haplotypeLinkedFeaturesDrawAt;
tg->colorShades = shadesOfSea;
}

void perlegenMethods(struct track *tg)
/* setup special methods for Perlegen haplotype track */
{
haplotypeMethods(tg);
tg->itemName = perlegenName;
}


/*******************************************************************/

/* Declare our color gradients and the the number of colors in them */
Color ldShadesPos[LD_DATA_SHADES];
Color ldHighLodLowDprime; /* pink */
Color ldHighDprimeLowLod; /* blue */
int colorLookup[256];

void ldShadesInit(struct track *tg, struct hvGfx *hvg, boolean isDprime)
/* Allocate the LD for positive and negative values, and error cases */
{
static struct rgbColor white = {255, 255, 255};
static struct rgbColor red   = {255,   0,   0};
static struct rgbColor green = {  0, 255,   0};
static struct rgbColor blue  = {  0,   0, 255};
char *ldPos = NULL;

ldPos = cartUsualStringClosestToHome(cart, tg->tdb, FALSE, "_pos", ldPosDefault);
ldHighLodLowDprime = hvGfxFindColorIx(hvg, 255, 224, 224); /* pink */
ldHighDprimeLowLod = hvGfxFindColorIx(hvg, 192, 192, 240); /* blue */
if (sameString(ldPos,"red"))
    hvGfxMakeColorGradient(hvg, &white, &red,   LD_DATA_SHADES, ldShadesPos);
else if (sameString(ldPos,"blue"))
    hvGfxMakeColorGradient(hvg, &white, &blue,  LD_DATA_SHADES, ldShadesPos);
else if (sameString(ldPos,"green"))
    hvGfxMakeColorGradient(hvg, &white, &green, LD_DATA_SHADES, ldShadesPos);
else
    errAbort("LD fill color must be 'red', 'blue', or 'green'; '%s' is not recognized", ldPos);
}

void bedLoadLdItemByQuery(struct track *tg, char *table,
			char *query, ItemLoader loader)
/* LD specific tg->item loader, as we need to load items beyond
   the current window to load the chromEnd positions for LD values. */
{
struct sqlConnection *conn = hAllocConn(database);
int rowOffset = 0;
int chromEndOffset = min(winEnd-winStart, 250000); /* extended chromEnd range */
struct sqlResult *sr = NULL;
char **row = NULL;
struct slList *itemList = NULL, *item = NULL;

if(query == NULL)
    sr = hRangeQuery(conn, table, chromName, winStart, winEnd+chromEndOffset, NULL, &rowOffset);
else
    sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    item = loader(row + rowOffset);
    slAddHead(&itemList, item);
    }
slSort(&itemList, bedCmp);
sqlFreeResult(&sr);
tg->items = itemList;
hFreeConn(&conn);
}

void bedLoadLdItem(struct track *tg, char *table, ItemLoader loader)
/* LD specific tg->item loader. */
{
bedLoadLdItemByQuery(tg, table, NULL, loader);
}

void ldLoadItems(struct track *tg)
/* loadItems loads up items for the chromosome range indicated.   */
{
if (tg->tdb && sameString(tg->tdb->type, "ld2"))
    bedLoadLdItem(tg, tg->table, (ItemLoader)ld2Load);
else
    bedLoadLdItem(tg, tg->table, (ItemLoader)ldLoad);
tg->canPack = FALSE;
}

void mapDiamondUi(struct hvGfx *hvg, int xl, int yl, int xt, int yt,
		  int xr, int yr, int xb, int yb,
		  char *name, char *shortLabel, char *trackName)
/* Print out image map rectangle that invokes hgTrackUi. */
{
if(theImgBox && curImgTrack)
    {
    char link[512];
    safef(link,sizeof(link),"%s?%s=%u&g=%s&i=%s", hgTrackUiName(),
    cartSessionVarName(), cartSessionId(cart), trackName, name);
    char title[128];
    safef(title,sizeof(title),"%s controls", shortLabel);
    // Add map item to currnent map (TODO: pass in map)
    // FIXME: What am I going to do about poly cords???
    // FIXME: What am I going to do about poly cords???
    // FIXME: imgTrackAddMapItem(curImgTrack,link,title,
    // FIXME:     hvGfxAdjX(hvg, xl), yl,
    // FIXME:     hvGfxAdjX(hvg, xt), yt,
    // FIXME:     hvGfxAdjX(hvg, xr), yr,
    // FIXME:    hvGfxAdjX(hvg, xb), yb));
    // FIXME: What am I going to do about poly cords???
    // FIXME: What am I going to do about poly cords???
    warn("Track named %s has called for a POLY map titled '%s controls', but imageV2 doesn't yet support this. No map item made.",trackName,shortLabel);
    }
else
    {
    hPrintf("<AREA SHAPE=POLY COORDS=\"%d,%d,%d,%d,%d,%d,%d,%d\" ",
        hvGfxAdjX(hvg, xl), yl,
            hvGfxAdjX(hvg, xt), yt,
            hvGfxAdjX(hvg, xr), yr,
            hvGfxAdjX(hvg, xb), yb);
    /* move this to hgTracks when finished */
    hPrintf("HREF=\"%s?%s=%u&c=%s&g=%s&i=%s\"", hgTrackUiName(),
        cartSessionVarName(), cartSessionId(cart), chromName, trackName, name);
    mapStatusMessage("%s controls", shortLabel);
    hPrintf(">\n");
    }
}

void mapTrackBackground(struct track *tg, struct hvGfx *hvg, int xOff, int yOff)
/* Print out image map rectangle that invokes hgTrackUi. */
{
xOff = hvGfxAdjXW(hvg, xOff, insideWidth);
char *track = tg->tdb->parent ? tg->tdb->parent->track : tg->tdb->track;
if(theImgBox && curImgTrack)
    {
    char link[512];
    safef(link,sizeof(link),"%s?%s=%u&g=%s&i=%s",hgTrackUiName(),
    cartSessionVarName(), cartSessionId(cart), track, track);
    char title[128];
    safef(title,sizeof(title),"%s controls", tg->track);
    // Add map item to currnent map (TODO: pass in map)
    #ifdef IMAGEv2_SHORT_MAPITEMS
        if(xOff < insideX && xOff+insideWidth > insideX)
            warn("mapTrackBackground(%s) map item spanning slices. LX:%d TY:%d RX:%d BY:%d  link:[%s]",tg->track,xOff, yOff, xOff+insideWidth, yOff+tg->height, link);
    #endif//def IMAGEv2_SHORT_MAPITEMS
    imgTrackAddMapItem(curImgTrack,link,title,xOff, yOff, xOff+insideWidth, yOff+tg->height, tg->track);
    }
else
    {
    hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ",
        xOff, yOff, xOff+insideWidth, yOff+tg->height);
    hPrintf("HREF=\"%s?%s=%u&c=%s&g=%s&i=%s\"", hgTrackUiName(),
        cartSessionVarName(), cartSessionId(cart), chromName, track, track);
    mapStatusMessage("%s controls", tg->track);
    hPrintf(">\n");
    }
}

int ldTotalHeight(struct track *tg, enum trackVisibility vis)
/* Return total height. Called before and after drawItems.
 * Must set height, lineHeight, heightPer */
{
tg->lineHeight = tl.fontHeight + 1;
tg->heightPer  = tg->lineHeight - 1;
if ( vis==tvDense || ( tg->limitedVisSet && tg->limitedVis==tvDense ) )
    tg->height = tg->lineHeight;
else if (winEnd-winStart<250000)
    tg->height = insideWidth/2;
else
    tg->height = max((int)(insideWidth*(250000.0/2.0)/(winEnd-winStart)),tg->lineHeight);
return tg->height;
}

void initColorLookup(struct track *tg, struct hvGfx *hvg, boolean isDprime)
{
ldShadesInit(tg, hvg, isDprime);
colorLookup[(int)'a'] = ldShadesPos[0];
colorLookup[(int)'b'] = ldShadesPos[1];
colorLookup[(int)'c'] = ldShadesPos[2];
colorLookup[(int)'d'] = ldShadesPos[3];
colorLookup[(int)'e'] = ldShadesPos[4];
colorLookup[(int)'f'] = ldShadesPos[5];
colorLookup[(int)'g'] = ldShadesPos[6];
colorLookup[(int)'h'] = ldShadesPos[7];
colorLookup[(int)'i'] = ldShadesPos[8];
colorLookup[(int)'j'] = ldShadesPos[9];
colorLookup[(int)'y'] = ldHighLodLowDprime; /* LOD error case */
colorLookup[(int)'z'] = ldHighDprimeLowLod; /* LOD error case */
}

void drawDiamond(struct hvGfx *hvg,
	 int xl, int yl, int xt, int yt, int xr, int yr, int xb, int yb,
	 Color fillColor, Color outlineColor)
/* Draw diamond shape. */
{
struct gfxPoly *poly = gfxPolyNew();
gfxPolyAddPoint(poly, xl, yl);
gfxPolyAddPoint(poly, xt, yt);
gfxPolyAddPoint(poly, xr, yr);
gfxPolyAddPoint(poly, xb, yb);
hvGfxDrawPoly(hvg, poly, fillColor, TRUE);
if (outlineColor != 0)
    hvGfxDrawPoly(hvg, poly, outlineColor, FALSE);
gfxPolyFree(&poly);
}

void ldDrawDiamond(struct hvGfx *hvg, struct track *tg, int width,
		   int xOff, int yOff, int a, int b, int c, int d,
		   Color shade, Color outlineColor, double scale,
		   boolean drawMap, char *name, enum trackVisibility vis,
		   boolean trim, boolean ldInv)
/* Draw and map a single pairwise LD box */
{
int yMax = ldTotalHeight(tg, vis)+yOff;
/* convert from genomic coordinates to hvg coordinates */
int xl = round((double)(scale*((c+a)/2-winStart))) + xOff;
int xt = round((double)(scale*((d+a)/2-winStart))) + xOff;
int xr = round((double)(scale*((d+b)/2-winStart))) + xOff;
int xb = round((double)(scale*((c+b)/2-winStart))) + xOff;
int yl = round((double)(scale*(c-a)/2)) + yOff;
int yt = round((double)(scale*(d-a)/2)) + yOff;
int yr = round((double)(scale*(d-b)/2)) + yOff;
int yb = round((double)(scale*(c-b)/2)) + yOff;

if (!ldInv)
    {
    yl = yMax - yl + yOff;
    yt = yMax - yt + yOff;
    yr = yMax - yr + yOff;
    yb = yMax - yb + yOff;
    }
/* correct bottom coordinate if necessary */
if (yb<=0)
    yb=1;
if (yt>yMax && trim)
    yt=yMax;
drawDiamond(hvg, xl, yl, xt, yt, xr, yr, xb, yb, shade, outlineColor);
return; /* mapDiamondUI is working well, but there is a bug with
	   AREA=POLY on the Mac browsers, so this will be
	   postponed for now by not using this code */
	/* also, since it only goes to hgTrackUi, it is redundant with mapTrackBackground.
	 * so keep this disabled until there is something more specific like an hgc
	 * handler for diamonds. */
if (drawMap && xt-xl>5 && xb-xl>5)
    mapDiamondUi(hvg, xl, yl, xt, yt, xr, yr, xb, yb, name, tg->track,
		 tg->tdb->parent ? tg->tdb->parent->track : tg->tdb->track);
}

Color getOutlineColor(struct track *tg, int itemCount)
/* get outline color from cart and set outlineColor*/
{
char *outColor = NULL;
outColor = cartUsualStringClosestToHome(cart, tg->tdb, FALSE, "_out", ldOutDefault);
if (winEnd-winStart > 100000)
    return 0;
if (sameString(outColor,"yellow"))
    return MG_YELLOW;
else if (sameString(outColor,"red"))
    return MG_RED;
else if (sameString(outColor,"blue"))
    return MG_BLUE;
else if (sameString(outColor,"green"))
    return MG_GREEN;
else if (sameString(outColor,"white"))
    return MG_WHITE;
else if (sameString(outColor,"black"))
    return MG_BLACK;
else
    return 0;
}

boolean notInWindow(int a, int b, int c, int d, boolean trim)
/* determine if the LD diamond is within the drawing window */
{
if ((b+d)/2 <= winStart)        /* outside window on the left */
    return TRUE;
if ((a+c)/2 >= winEnd)          /* outside window on the right */
    return TRUE;
if ((c-b)/2 >= winEnd-winStart) /* bottom is outside window */
    return TRUE;
if (trim && d >= winEnd)        /* trim right edge */
    return TRUE;
if (trim && a <= winStart)      /* trim left edge */
    return TRUE;
if (!trim && b <= winStart-(winEnd-winStart))
    return TRUE;
if (!trim && c >= winEnd+(winEnd-winStart))
    return TRUE;
return FALSE;
}

int ldIndexCharToInt(char charValue)
/* convert from character encoding to intensity index */
{
switch (charValue)
    {
    case 'a': return 0;
    case 'b': return 1;
    case 'c': return 2;
    case 'd': return 3;
    case 'e': return 4;
    case 'f': return 5;
    case 'g': return 6;
    case 'h': return 7;
    case 'i': return 8;
    case 'j': return 9;
    case 'y': return -100;
    case 'z': return -101;
    }
return -102;
}

int ldIndexIntToChar(int index)
/* convert from intensity index to character encoding */
{
switch (index)
    {
    case 0: return 'a';
    case 1: return 'b';
    case 2: return 'c';
    case 3: return 'd';
    case 4: return 'e';
    case 5: return 'f';
    case 6: return 'g';
    case 7: return 'h';
    case 8: return 'i';
    case 9: return 'j';
    case -100: return 'y';
    case -101: return 'z';
    }
return 'x';
}

void ldAddToDenseValueHash(struct hash *ldHash, unsigned a, char charValue)
/* Add new values to LD hash or update existing values.
   Values are averaged along the diagonals. */
{
struct ldStats *stats;
char name[16];
int indexValue = ldIndexCharToInt(charValue);
int incrementN = 1;

if (a<winStart || a>winEnd)
    return;
/* Include SNPs with error codes, but don't add their values.
 * This will allow the marker to be drawn, even if all values
 *   are missing. */
if (indexValue<0)
    {
    incrementN = 0;
    indexValue = 0;
    }
safef(name, sizeof(name), "%d", a);
stats = hashFindVal(ldHash, name);
if (!stats)
    {
    AllocVar(stats);
    stats->chromStart = a;
    stats->n          = incrementN;
    stats->sumValues  = indexValue;
    hashAdd(ldHash, name, stats);
    }
else
    {
    stats->n         += incrementN;
    stats->sumValues += indexValue;
    }
}

void ldDrawDenseValue(struct hvGfx *hvg, struct track *tg, int xOff, int y1,
		      double scale, Color outlineColor, struct ldStats *d)
/* Draw single dense LD value */
{
if (d->chromStart < winStart)
    return;
int   colorInt  = (d->n > 0) ? round(d->sumValues/d->n) : -100;
char  colorChar = ldIndexIntToChar(colorInt);
Color shade     = (d->n > 0) ? colorLookup[(int)colorChar] : ldHighDprimeLowLod;
int   w         = 3; /* width of box */
int   w2        = w/2;
int   x         = round((d->chromStart-winStart)*scale) + xOff - w2;
int   x1=x-w2, x2=x1+w, y2=y1+tg->heightPer-1;

hvGfxBox(hvg, x1, y1, w, tg->heightPer, shade);
if (outlineColor!=0)
    {
    hvGfxLine(hvg, x1, y1, x2, y1, outlineColor);
    hvGfxLine(hvg, x1, y2, x2, y2, outlineColor);
    hvGfxLine(hvg, x1, y1, x1, y2, outlineColor);
    hvGfxLine(hvg, x2, y1, x2, y2, outlineColor);
    }
}

void ldDrawDenseValueHash(struct hvGfx *hvg, struct track *tg, int xOff, int yOff,
			  double scale, Color outlineColor, struct hash *ldHash)
/* Draw dynamically computed dense LD values */
{
struct hashEl *hashEl, *stats=hashElListHash(ldHash);
for (hashEl=stats; hashEl!=NULL; hashEl=hashEl->next)
    ldDrawDenseValue(hvg, tg, xOff, yOff, scale, outlineColor, hashEl->val);
hashElFreeList(&stats);
}

void ldDrawDense(struct hvGfx *hvg, struct track *tg, int xOff, int yOff,
		 double scale, Color outlineColor,
		 boolean isLod, boolean isRsquared)
/* Draw precomputed dense LD values from ld2 table. */
{
static struct ldStats lds;
struct ld2 *dPtr;
boolean useTInt = cartUsualBooleanClosestToHome(cart, tg->tdb, FALSE, "_gap", ldGapDefault);
for (dPtr = tg->items;  dPtr != NULL;  dPtr = dPtr->next)
    {
    lds.chromStart = dPtr->chromStart;
    if (isLod)
	lds.sumValues = ldIndexCharToInt(dPtr->avgLod);
    else if (isRsquared)
	lds.sumValues = ldIndexCharToInt(dPtr->avgRsquared);
    else
	lds.sumValues = ldIndexCharToInt(dPtr->avgDprime);
    lds.n = (lds.sumValues < 0) ? 0 : 1;
    if (useTInt &&
	dPtr->tInt >= 'c' && dPtr->tInt <= 'h' &&
	dPtr->next != NULL && dPtr->next->chromStart > winStart)
	{
	/* Use tInt to color the spaces between SNPs. */
	Color shade = shadesOfGray[(int)(dPtr->tInt - 'a')];
	/* Clip to winStart here to avoid unsigned subtraction underflow: */
	int e1 = (dPtr->chromEnd < winStart) ? winStart : dPtr->chromEnd;
	int x1 = round((e1-winStart)*scale) + xOff;
	int x2 = round((dPtr->next->chromStart-winStart)*scale) + xOff;
	int w  = x2 - x1;
	hvGfxBox(hvg, x1, yOff, w, tg->heightPer-1, shade);
	}
    ldDrawDenseValue(hvg, tg, xOff, yOff, scale, outlineColor, &lds);
    }
}

void ldDrawLeftLabels(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width, int height,
	boolean withCenterLabels, MgFont *font,
	Color color, enum trackVisibility vis)
/* Draw left labels. */
{
char  label[17];
char *ldVal;
int   yVisOffset;
if (vis == tvDense)
    {
    if (withCenterLabels && !tdbIsComposite(tg->tdb))
	yVisOffset = tg->heightPer;
    else
	yVisOffset = 0;
    }
else
    yVisOffset = tg->heightPer + height/2;

ldVal = cartUsualStringClosestToHome(cart, tg->tdb, FALSE, "_val", ldValDefault);
if (sameString(ldVal, "lod"))
    ldVal = cloneString("LOD");
else if (sameString(ldVal, "rsquared"))
    ldVal = cloneString("R^2");
else if (sameString(ldVal, "dprime"))
    ldVal = cloneString("D'");
else
    errAbort("%s values are not recognized", ldVal);

if (isNotEmpty(tg->shortLabel) && strlen(tg->shortLabel) <= 12)
    safef(label, sizeof(label), "%s %s", tg->shortLabel, ldVal);
else
    {
    char *pop = cloneString(tg->table);
    char *ptr = strstr(pop, "ChbJpt");
    if (ptr != NULL)
	safef(label, sizeof(label), "LD JPT+CHB %s", ldVal);
    else if ((ptr = strstr(pop, "Ceu")) || (ptr = strstr(pop, "Chb")) ||
	     (ptr = strstr(pop, "Jpt")) || (ptr = strstr(pop, "Yri")))
	{
	ptr[3] = '\0';
	safef(label, sizeof(label), "LD %s %s", ptr, ldVal);
	toUpperN(label, sizeof(label));
	}
    else
	safef(label, sizeof(label), "LD %s", ldVal);
    }

hvGfxUnclip(hvg);
hvGfxSetClip(hvg, leftLabelX, yOff+yVisOffset, leftLabelWidth, tg->heightPer);
hvGfxTextRight(hvg, leftLabelX, yOff+yVisOffset, leftLabelWidth, tg->heightPer, color, font, label);
hvGfxUnclip(hvg);
hvGfxSetClip(hvg, insideX, yOff, insideWidth, tg->height);
}


/* ldDrawItems -- lots of disk and cpu optimizations here.
 * There are three data fields, (lod, rsquared, and dprime) in each
   item, and each is a list of pairwise values.  The values are
   encoded in ascii to save disk space and color calculation (drawing)
   time.  As these are pairwise values and it is expensive to store
   the second coordinate for each value, each item has only a
   chromosome coordinate for the first of the pair (chromStart), and
   the second coordinate is inferred from the list of items.  This
   means that two pointers are necessary: a data pointer (dPtr) to
   keep track of the current data, and a second pointer (sPtr) to
   retrieve the second coordinate.
 * The length of each of the three value vectors (rsquared, dprime,
   and lod) is the same and is stored in the ldCount field.
 * These fields are used *if* the table has them:
   * The average dprime, rsquared and lod for markers both before and
     after this marker are stored in the avg{Dprime,Rsquared,Lod} fields.
   * The T-int measure of LD info content between markers is in the tInt field.
 * The ascii values are mapped to colors in the colorLookup[] array.
 * The four points of each diamond are calculated from the chromosomal
   coordinates of four SNPs:
     a: the SNP at dPtr->chromStart
     b: the SNP immediately 3' of the chromStart (dPtr->next->chromStart)
     c: the SNP immediately 5' of the second position's chromStart (sPtr->chromStart)
     d: the SNP at the second position's chromStart (sPtr->next->chromStart)
 * The chromosome coordinates are converted into hvg coordinates in
   ldDrawDiamond.
 * A counter (i) is used to keep from reading beyond the end of the
   value array.  */
void ldDrawItems(struct track *tg, int seqStart, int seqEnd,
		 struct hvGfx *hvg, int xOff, int yOff, int width,
		 MgFont *font, Color color, enum trackVisibility vis)
/* Draw item list, one per track. */
{
struct ld2  *dPtr = NULL, *sPtr = NULL; /* pointers to 5' and 3' ends */
boolean      isLod     = FALSE, isRsquared = FALSE, isDprime = FALSE;
double       scale     = scaleForPixels(insideWidth);
int          itemCount = slCount((struct slList *)tg->items);
Color        shade     = 0, outlineColor = getOutlineColor(tg, itemCount);
int          a, b, c, d, i; /* chromosome coordinates and counter */
boolean      drawMap   = FALSE; /* ( itemCount<1000 ? TRUE : FALSE ); */
struct hash *ldHash    = newHash(20);
Color        yellow    = hvGfxFindRgb(hvg, &undefinedYellowColor);
char        *ldVal     = NULL;
boolean      ldTrm;
boolean      ldInv;
boolean dynamicDense = FALSE;

ldInv = cartUsualBooleanClosestToHome(cart, tg->tdb, FALSE, "_inv", ldInvDefault);
ldVal = cartUsualStringClosestToHome( cart, tg->tdb, FALSE, "_val", ldValDefault);
ldTrm = cartUsualBooleanClosestToHome(cart, tg->tdb, FALSE, "_trm", ldTrmDefault);

if (tg->limitedVisSet)
    vis = tg->limitedVis;
if (vis != tvDense && vis != tvFull)
    errAbort("Visibility '%s'(%d) is not supported for the LD track yet.",
	     hStringFromTv(vis), vis);
if (vis == tvDense)
    {
    if (tg->tdb && sameString(tg->tdb->type, "ld2"))
	dynamicDense = FALSE;
    else
	dynamicDense = TRUE;
    }
if (vis == tvDense)
    hvGfxBox(hvg, insideX, yOff, insideWidth, tg->height-1, yellow);
mapTrackBackground(tg, hvg, xOff, yOff);
if (tg->items==NULL)
    return;

if (sameString(ldVal, "lod")) /* only one value can be drawn at a time, so figure it out here */
    isLod = TRUE;
else if (sameString(ldVal, "dprime"))
    isDprime = TRUE;
else if (sameString(ldVal, "rsquared"))
    isRsquared = TRUE;
else
    errAbort ("LD score value must be 'rsquared', 'dprime', or 'lod'; '%s' is not known", ldVal);

/* initialize arrays to convert from ascii encoding to color values */
initColorLookup(tg, hvg, isDprime);

/* If this is an ld2 table with precomputed averages, skip the big loop. */
if (!dynamicDense && vis == tvDense)
    {
    ldDrawDense(hvg, tg, xOff, yOff, scale, outlineColor, isLod, isRsquared);
    return;
    }

/* Loop through all items to get values and initial coordinates (a and b) */
for (dPtr=tg->items; dPtr!=NULL && dPtr->next!=NULL; dPtr=dPtr->next)
    {
    a = dPtr->chromStart;
    b = dPtr->next->chromStart;
    i = 0;
    if ((ldTrm && a <=winStart)||(!ldTrm && b <=winStart-(winEnd-winStart))) /* return if this item is not to be drawn */
	continue;
    if (isLod) /* point to the right data values to be drawn.  'ldVal' variable is reused */
	ldVal = dPtr->lod;
    else if (isRsquared)
	ldVal = dPtr->rsquared;
    else if (isDprime)
	ldVal = dPtr->dprime;
    /* Loop through all items again to get end coordinates (c and d): used to be 'drawNecklace' */
    for ( sPtr=dPtr; i<dPtr->ldCount && sPtr!=NULL && sPtr->next!=NULL; sPtr=sPtr->next )
	{
	c = sPtr->chromStart;
	d = sPtr->next->chromStart;
	if (notInWindow(a, b, c, d, ldTrm)) /* Check to see if this diamond needs to be drawn, or if it is out of the window */
	    {
	    if ((c-b)/2 >= winEnd-winStart || (ldTrm&&d>=winEnd) || (!ldTrm&&c>=winEnd+(winEnd-winStart)))
		i = dPtr->ldCount;
	    continue;
	    }
	if ( d-a > 250000 ) /* Check to see if we are trying to reach across a window that is too wide (centromere) */
	    continue;
	shade = colorLookup[(int)ldVal[i]];
	if (vis == tvFull)
	    ldDrawDiamond(hvg, tg, width, xOff, yOff, a, b, c, d, shade, outlineColor, scale, drawMap, dPtr->name, vis, ldTrm, ldInv);
	else if (dynamicDense)
	    {
	    ldAddToDenseValueHash(ldHash, a, ldVal[i]);
	    ldAddToDenseValueHash(ldHash, d, ldVal[i]);
	    }
	i++;
	}
    /* reached end of chromosome, so sPtr->next is null; draw last diamond in list */
    if (sPtr->next==NULL)
	{
	a = dPtr->chromStart;
	b = dPtr->chromEnd;
	c = sPtr->chromStart;
	d = sPtr->chromEnd;
	if (notInWindow(a, b, c, d, ldTrm)) /* Check to see if this diamond needs to be drawn, or if it is out of the window */
	    continue;
	shade = colorLookup[(int)ldVal[i]];
	if (vis == tvFull)
	    ldDrawDiamond(hvg, tg, width, xOff, yOff, a, b, c, d, shade, outlineColor, scale, drawMap, dPtr->name, vis, ldTrm, ldInv);
	else if (dynamicDense)
	    {
	    ldAddToDenseValueHash(ldHash, a, ldVal[i]);
	    ldAddToDenseValueHash(ldHash, d, ldVal[i]);
	    }
	}
    }
/* starting at last snp on chromosome, so dPtr->next is null; draw this diamond */
if (dPtr->next==NULL)
    {
    a = dPtr->chromStart;
    b = dPtr->chromEnd;
    if (!notInWindow(a, b, a, b, ldTrm)) /* Continue only if this diamond needs to be drawn */
	{
	if (isLod) /* point to the right data values to be drawn.  'ldVal' variable is reused */
	    ldVal = dPtr->lod;
	else if (isRsquared)
	    ldVal = dPtr->rsquared;
	else if (isDprime)
	    ldVal = dPtr->dprime;
	shade = colorLookup[(int)ldVal[0]];
	if (vis == tvFull)
	    ldDrawDiamond(hvg, tg, width, xOff, yOff, a, b, a, b, shade, outlineColor, scale, drawMap, dPtr->name, vis, ldTrm, ldInv);
	else if (dynamicDense)
	    {
	    ldAddToDenseValueHash(ldHash, a, ldVal[0]);
	    ldAddToDenseValueHash(ldHash, b, ldVal[0]);
	    }
	}
    }
if (dynamicDense)
    ldDrawDenseValueHash(hvg, tg, xOff, yOff, scale, outlineColor, ldHash);
}

void ldFreeItems(struct track *tg)
/* Free item list. */
{
if (tg->tdb && sameString(tg->tdb->type, "ld2"))
    ld2FreeList((struct ld2**)&tg->items);
else
    ldFreeList((struct ld**)&tg->items);
}

void ldMethods(struct track *tg)
/* setup special methods for Linkage Disequilibrium track */
{
if(tg->subtracks != 0) /* Only load subtracks, not top level track. */
    return;
tg->loadItems      = ldLoadItems;
tg->totalHeight    = ldTotalHeight;
tg->drawItems      = ldDrawItems;
tg->freeItems      = ldFreeItems;
tg->drawLeftLabels = ldDrawLeftLabels;
tg->mapsSelf       = TRUE;
tg->canPack        = FALSE;
}

void cnpSharpLoadItems(struct track *tg)
{
bedLoadItem(tg, "cnpSharp", (ItemLoader)cnpSharpLoad);
}

void cnpSharp2LoadItems(struct track *tg)
{
bedLoadItem(tg, "cnpSharp2", (ItemLoader)cnpSharp2Load);
}

void cnpIafrateLoadItems(struct track *tg)
{
bedLoadItem(tg, "cnpIafrate", (ItemLoader)cnpIafrateLoad);
}

void cnpIafrate2LoadItems(struct track *tg)
{
bedLoadItem(tg, "cnpIafrate2", (ItemLoader)cnpIafrate2Load);
}

void cnpSebatLoadItems(struct track *tg)
{
bedLoadItem(tg, "cnpSebat", (ItemLoader)cnpSebatLoad);
}

void cnpSebat2LoadItems(struct track *tg)
{
bedLoadItem(tg, "cnpSebat2", (ItemLoader)cnpSebat2Load);
}

void cnpFosmidLoadItems(struct track *tg)
{
bedLoadItem(tg, "cnpFosmid", (ItemLoader)bedLoad);
}

void cnpRedonLoadItems(struct track *tg)
{
bedLoadItem(tg, "cnpRedon", (ItemLoader)bedLoad);
}

void cnpLockeLoadItems(struct track *tg)
{
bedLoadItem(tg, "cnpLocke", (ItemLoader)cnpLockeLoad);
}

void cnpSharpFreeItems(struct track *tg)
{
cnpSharpFreeList((struct cnpSharp**)&tg->items);
}

void cnpSharp2FreeItems(struct track *tg)
{
cnpSharp2FreeList((struct cnpSharp2**)&tg->items);
}

void cnpIafrateFreeItems(struct track *tg)
{
cnpIafrateFreeList((struct cnpIafrate**)&tg->items);
}

void cnpIafrate2FreeItems(struct track *tg)
{
cnpIafrate2FreeList((struct cnpIafrate2**)&tg->items);
}

void cnpSebatFreeItems(struct track *tg)
{
cnpSebatFreeList((struct cnpSebat**)&tg->items);
}

void cnpSebat2FreeItems(struct track *tg)
{
cnpSebat2FreeList((struct cnpSebat2**)&tg->items);
}

void cnpFosmidFreeItems(struct track *tg)
{
bedFreeList((struct bed**)&tg->items);
}

void cnpRedonFreeItems(struct track *tg)
{
bedFreeList((struct bed**)&tg->items);
}

void cnpLockeFreeItems(struct track *tg)
{
bedFreeList((struct bed**)&tg->items);
}

Color cnpSharpItemColor(struct track *tg, void *item, struct hvGfx *hvg)
{
struct cnpSharp *cnpSh = item;

if (sameString(cnpSh->variationType, "Gain"))
    return MG_GREEN;
if (sameString(cnpSh->variationType, "Loss"))
    return MG_RED;
if (sameString(cnpSh->variationType, "Gain and Loss"))
    return MG_BLUE;
return MG_BLACK;
}

Color cnpSharp2ItemColor(struct track *tg, void *item, struct hvGfx *hvg)
{
struct cnpSharp2 *cnpSh = item;

if (sameString(cnpSh->variationType, "Gain"))
    return MG_GREEN;
if (sameString(cnpSh->variationType, "Loss"))
    return MG_RED;
if (sameString(cnpSh->variationType, "Gain and Loss"))
    return MG_BLUE;
return MG_BLACK;
}

Color cnpIafrateItemColor(struct track *tg, void *item, struct hvGfx *hvg)
{
struct cnpIafrate *cnpIa = item;

if (sameString(cnpIa->variationType, "Gain"))
    return MG_GREEN;
if (sameString(cnpIa->variationType, "Loss"))
    return MG_RED;
if (sameString(cnpIa->variationType, "Gain and Loss"))
    return MG_BLUE;
return MG_BLACK;
}

Color cnpIafrate2ItemColor(struct track *tg, void *item, struct hvGfx *hvg)
{
struct cnpIafrate2 *cnpIa = item;
int gainCount = cnpIa->normalGain + cnpIa->patientGain;
int lossCount = cnpIa->normalLoss + cnpIa->patientLoss;

if (gainCount > 0 && lossCount == 0)
    return MG_GREEN;
if (lossCount > 0 && gainCount == 0)
    return MG_RED;
if (gainCount > 0 && lossCount > 0)
    return MG_BLUE;
return MG_BLACK;
}

Color cnpSebatItemColor(struct track *tg, void *item, struct hvGfx *hvg)
{
struct cnpSebat *cnpSe = item;

if (sameString(cnpSe->name, "Gain"))
    return MG_GREEN;
if (sameString(cnpSe->name, "Loss"))
    return MG_RED;
if (sameString(cnpSe->name, "Gain and Loss"))
    return MG_BLUE;
return MG_BLACK;
}

Color cnpSebat2ItemColor(struct track *tg, void *item, struct hvGfx *hvg)
{
struct cnpSebat2 *cnpSe = item;

if (sameString(cnpSe->name, "Gain"))
    return MG_GREEN;
if (sameString(cnpSe->name, "Loss"))
    return MG_RED;
return MG_BLACK;
}

Color cnpFosmidItemColor(struct track *tg, void *item, struct hvGfx *hvg)
{
struct bed *cnpFo = item;

if (cnpFo->name[0] == 'I')
    return MG_GREEN;
if (cnpFo->name[0] == 'D')
    return MG_RED;
return MG_BLACK;
}

Color cnpTuzunItemColor (struct track *tg, void *item, struct hvGfx *hvg)
{
return MG_GRAY;
}

Color cnpRedonItemColor (struct track *tg, void *item, struct hvGfx *hvg)
{
return MG_GRAY;
}

Color cnpLockeItemColor(struct track *tg, void *item, struct hvGfx *hvg)
{
struct cnpLocke *thisItem = item;

if (sameString(thisItem->variationType, "Gain"))
    return MG_GREEN;
if (sameString(thisItem->variationType, "Loss"))
    return MG_RED;
if (sameString(thisItem->variationType, "Gain and Loss"))
    return MG_BLUE;
return MG_BLACK;
}

Color delConradItemColor (struct track *tg, void *item, struct hvGfx *hvg)
{
return MG_RED;
}

Color delConrad2ItemColor (struct track *tg, void *item, struct hvGfx *hvg)
{
return MG_RED;
}

Color delMccarrollItemColor (struct track *tg, void *item, struct hvGfx *hvg)
{
return MG_RED;
}

Color delHindsItemColor (struct track *tg, void *item, struct hvGfx *hvg)
{
return MG_RED;
}


void cnpSharpMethods(struct track *tg)
{
tg->loadItems = cnpSharpLoadItems;
tg->freeItems = cnpSharpFreeItems;
tg->itemColor = cnpSharpItemColor;
tg->itemNameColor = cnpSharpItemColor;
}

void cnpSharp2Methods(struct track *tg)
{
tg->loadItems = cnpSharp2LoadItems;
tg->freeItems = cnpSharp2FreeItems;
tg->itemColor = cnpSharp2ItemColor;
tg->itemNameColor = cnpSharp2ItemColor;
}

void cnpIafrateMethods(struct track *tg)
{
tg->loadItems = cnpIafrateLoadItems;
tg->freeItems = cnpIafrateFreeItems;
tg->itemColor = cnpIafrateItemColor;
tg->itemNameColor = cnpIafrateItemColor;
}

void cnpIafrate2Methods(struct track *tg)
{
tg->loadItems = cnpIafrate2LoadItems;
tg->freeItems = cnpIafrate2FreeItems;
tg->itemColor = cnpIafrate2ItemColor;
tg->itemNameColor = cnpIafrate2ItemColor;
}

void cnpSebatMethods(struct track *tg)
{
tg->loadItems = cnpSebatLoadItems;
tg->freeItems = cnpSebatFreeItems;
tg->itemColor = cnpSebatItemColor;
tg->itemNameColor = cnpSebatItemColor;
}

void cnpSebat2Methods(struct track *tg)
{
tg->loadItems = cnpSebat2LoadItems;
tg->freeItems = cnpSebat2FreeItems;
tg->itemColor = cnpSebat2ItemColor;
tg->itemNameColor = cnpSebat2ItemColor;
}

void cnpFosmidMethods(struct track *tg)
{
tg->loadItems = cnpFosmidLoadItems;
tg->freeItems = cnpFosmidFreeItems;
tg->itemColor = cnpFosmidItemColor;
tg->itemNameColor = cnpFosmidItemColor;
}

void cnpRedonMethods(struct track *tg)
{
tg->loadItems = cnpRedonLoadItems;
tg->freeItems = cnpRedonFreeItems;
tg->itemColor = cnpRedonItemColor;
tg->itemNameColor = cnpRedonItemColor;
}

void cnpLockeMethods(struct track *tg)
{
tg->loadItems = cnpLockeLoadItems;
tg->freeItems = cnpLockeFreeItems;
tg->itemColor = cnpLockeItemColor;
tg->itemNameColor = cnpLockeItemColor;
}

void cnpTuzunMethods(struct track *tg)
{
tg->itemColor = cnpTuzunItemColor;
tg->itemNameColor = cnpTuzunItemColor;
}

void delConradMethods(struct track *tg)
{
tg->itemColor = delConradItemColor;
tg->itemNameColor = delConradItemColor;
}

void delConrad2Methods(struct track *tg)
{
tg->itemColor = delConrad2ItemColor;
tg->itemNameColor = delConrad2ItemColor;
}

void delMccarrollMethods(struct track *tg)
{
tg->itemColor = delMccarrollItemColor;
tg->itemNameColor = delMccarrollItemColor;
}

void delHindsMethods(struct track *tg)
{
tg->itemColor = delHindsItemColor;
tg->itemNameColor = delHindsItemColor;
}
