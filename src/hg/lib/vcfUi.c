/* vcfUi - Variant Call Format user interface controls that are shared
 * between more than one CGI. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include <regex.h>
#include "common.h"
#include "cheapcgi.h"
#include "errCatch.h"
#include "hCommon.h"
#include "hui.h"
#include "jsHelper.h"
#include "regexHelper.h"
#include "vcf.h"
#include "vcfUi.h"
#include "knetUdc.h"
#include "udc.h"
#include "obscure.h"
#include "bigBedFilter.h"
#include "htmlColor.h"
#include "basicBed.h"
#include "web.h"

// helper functions for each type of of vcfInfoFilter
static struct vcfInfoDef *vcfInfoDefForFilter(struct vcfFile *vcff, struct trackDb *tdb,
                                              char *field, boolean wantNumber, boolean isList)
/* Return the vcfInfoDef for a filter on INFO field. Warn and return NULL when the field
 * is not in the VCF header, has the wrong Number, or has the wrong type. The field must be
 * Number=1, or Number=1 or Number=. for a list filter. A numeric filter needs an Integer or
 * Float field, and a text or value filter needs a String field. */
{
struct vcfInfoDef *infoDef = vcfInfoDefForKey(vcff, field);
if (infoDef == NULL)
    warn("track %s: can't filter on INFO field %s: it is not in the VCF header",
         tdb->track, field);
else if (isList && infoDef->fieldCount != 1 && infoDef->fieldCount != -1)
    warn("track %s: can't filter on INFO field %s: a list filter needs a Number=1 or "
         "Number=. field", tdb->track, field);
else if (!isList && infoDef->fieldCount != 1)
    warn("track %s: can't filter on INFO field %s: only Number=1 fields can be filtered",
         tdb->track, field);
else if (wantNumber && infoDef->type != vcfInfoInteger && infoDef->type != vcfInfoFloat)
    warn("track %s: can't filter on INFO field %s: a numeric filter needs an Integer or "
         "Float field", tdb->track, field);
else if (!wantNumber && infoDef->type != vcfInfoString)
    warn("track %s: can't filter on INFO field %s: a text or value filter needs a String field",
         tdb->track, field);
else
    return infoDef;
return NULL;
}

static struct vcfInfoFilter *vcfInfoFilterMakeNumberFilter(struct cart *cart, struct vcfFile *vcff, struct trackDb *tdb, char *filterName, char *defaultLimits, char *fieldName, boolean isHighlight)
{
struct vcfInfoFilter *filter = NULL;
char *setting = trackDbSettingClosestToHome(tdb, filterName);
struct vcfInfoDef *infoDef = vcfInfoDefForFilter(vcff, tdb, fieldName, TRUE, FALSE);
if (infoDef == NULL)
    return NULL;

if (setting)
    {
    boolean invalid = FALSE;
    double minValueTdb = 0,maxValueTdb = NO_VALUE;
    double minLimit=NO_VALUE,maxLimit=NO_VALUE,min = minValueTdb,max = maxValueTdb;
    colonPairToDoubles(setting,&minValueTdb,&maxValueTdb);
    colonPairToDoubles(defaultLimits,&minLimit,&maxLimit);
    getScoreFloatRangeFromCart(cart,tdb,FALSE,filterName,&minLimit,&maxLimit,&min,&max);
    if ((int)minLimit != NO_VALUE || (int)maxLimit != NO_VALUE)
        {
        // assume tdb default values within range!
        // (don't give user errors that have no consequence)
        if ((min != minValueTdb && (((int)minLimit != NO_VALUE && min < minLimit)
                                || ((int)maxLimit != NO_VALUE && min > maxLimit)))
        ||  (max != maxValueTdb && (((int)minLimit != NO_VALUE && max < minLimit)
                                || ((int)maxLimit != NO_VALUE && max > maxLimit))))
            {
            invalid = TRUE;
            char value[64];
            if ((int)max == NO_VALUE) // min only is allowed, but max only is not
                safef(value, sizeof(value), "entered minimum (%g)", min);
            else
                safef(value, sizeof(value), "entered range (min:%g and max:%g)", min, max);
            char limits[64];
            if ((int)minLimit != NO_VALUE && (int)maxLimit != NO_VALUE)
                safef(limits, sizeof(limits), "violates limits (%g to %g)", minLimit, maxLimit);
            else if ((int)minLimit != NO_VALUE)
                safef(limits, sizeof(limits), "violates lower limit (%g)", minLimit);
            else //if ((int)maxLimit != NO_VALUE)
                safef(limits, sizeof(limits), "violates uppper limit (%g)", maxLimit);
            warn("invalid filter by %s: %s %s for track %s", fieldName, value, limits, tdb->track);
            }
        }
    if (invalid)
        {
        char filterLimitName[64];
        safef(filterLimitName, sizeof(filterLimitName), "%s%s", filterName, _MIN);
        cartRemoveVariableClosestToHome(cart,tdb,FALSE,filterLimitName);
        safef(filterLimitName, sizeof(filterLimitName), "%s%s", filterName, _MAX);
        cartRemoveVariableClosestToHome(cart,tdb,FALSE,filterLimitName);
        }
    else if (((int)min != NO_VALUE && ((int)minLimit == NO_VALUE || minLimit != min))
         ||  ((int)max != NO_VALUE && ((int)maxLimit == NO_VALUE || maxLimit != max)))
         // Assumes min==NO_VALUE or min==minLimit is no filter
         // Assumes max==NO_VALUE or max==maxLimit is no filter!
        {
        AllocVar(filter);
        filter->infoDef = infoDef;
        if ((int)max == NO_VALUE || ((int)maxLimit != NO_VALUE && maxLimit == max))
            {
            filter->comparisonType = COMPARE_MORE;
            filter->value1 = min;
            }
        else if ((int)min == NO_VALUE || ((int)minLimit != NO_VALUE && minLimit == min))
            {
            filter->comparisonType = COMPARE_LESS;
            filter->value1 = max;
            }
        else
            {
            filter->comparisonType = COMPARE_BETWEEN;
            filter->value1 = min;
            filter->value2 = max;
            }
        if (isHighlight)
            filter->isHighlight = TRUE;
        }
    }
return filter;
}

static struct vcfInfoFilter *vcfInfoFilterMakeFilterText(struct cart *cart, struct vcfFile *vcff, struct trackDb *tdb, char *filterName, char *fieldName, boolean isHighlight)
{
struct vcfInfoFilter *filter;
char *setting = trackDbSettingClosestToHome(tdb, filterName);
char *value = cartUsualStringClosestToHome(cart, tdb, FALSE, filterName, setting);
if (isEmpty(value))
    return NULL;
char *typeValue = getFilterType(cart, tdb, fieldName, FILTERTEXT_WILDCARD);
struct vcfInfoDef *infoDef = vcfInfoDefForFilter(vcff, tdb, fieldName, FALSE, FALSE);
if (infoDef == NULL)
    return NULL;

AllocVar(filter);
filter->infoDef = infoDef;
if (sameString(typeValue, FILTERTEXT_REGEXP))
    {
    filter->comparisonType = COMPARE_REGEXP;
    regcomp(&filter->regEx, value, REG_NOSUB);
    }
else
    {
    filter->comparisonType = COMPARE_WILDCARD;
    filter->wildCardString = cloneString(value);
    }

filter->isHighlight = isHighlight;
return filter;
}

int vcfInfoDefSubFieldIndex(const struct vcfInfoDef *def, const char *subFieldName)
/* Parse the "Format: A|B|C|..." clause out of def->description (the same syntax
 * looksTabular() in lib/vcf.c keys off of) and return the 0-based index of subFieldName,
 * or -1 if the description has no Format clause or the name is absent. */
{
if (def == NULL || isEmpty(def->description) || isEmpty(subFieldName))
    return -1;
regmatch_t substrs[8];
if (!regexMatchSubstr(def->description, COL_DESC_REGEX, substrs, ArraySize(substrs)))
    return -1;
int matchSize = substrs[0].rm_eo - substrs[0].rm_so;
char copy[matchSize + 1];
safencpy(copy, sizeof(copy), def->description + substrs[0].rm_so, matchSize);
char *words[256];
int nWords = chopByChar(copy, '|', words, ArraySize(words));
int i;
for (i = 0; i < nWords; i++)
    if (sameString(words[i], subFieldName))
        return i;
return -1;
}

static struct vcfInfoFilter *vcfInfoFilterMakeFilterBy(struct cart *cart, struct vcfFile *vcff, struct trackDb *tdb, char *field, struct slName *choices, boolean isHighlight)
/* Add a vcfInfoFilter using trackDb filterBy statement.
 * field may be a plain INFO key, or a dotted key.subField referring to a named
 * sub-field of a pipe-separated INFO annotation (e.g. vep.Consequence). */
{
struct vcfInfoFilter *filter;
char *setting = NULL;
if (isHighlight)
    setting = getHighlightType(cart, tdb, field, HIGHLIGHTBY_DEFAULT);
else
    setting = getFilterType(cart, tdb, field, FILTERBY_DEFAULT);

enum bigBedFilterType comparisonType = COMPARE_HASH;
if (setting)
    {
    if (sameString(setting, FILTERBY_SINGLE_LIST)
            || sameString(setting, FILTERBY_MULTIPLE_LIST_OR)
            || sameString(setting, FILTERBY_MULTIPLE_LIST_ONLY_OR)
            || sameString(setting, HIGHLIGHTBY_SINGLE_LIST)
            || sameString(setting, HIGHLIGHTBY_MULTIPLE_LIST_OR)
            || sameString(setting, HIGHLIGHTBY_MULTIPLE_LIST_ONLY_OR))
                comparisonType = COMPARE_HASH_LIST_OR;
    else if (sameString(setting, FILTERBY_MULTIPLE_LIST_AND)
            || sameString(setting, FILTERBY_MULTIPLE_LIST_ONLY_AND)
            || sameString(setting, HIGHLIGHTBY_MULTIPLE_LIST_AND)
            || sameString(setting, HIGHLIGHTBY_MULTIPLE_LIST_ONLY_AND))
                comparisonType = COMPARE_HASH_LIST_AND;
    }
boolean isList = (comparisonType != COMPARE_HASH);

char *dotPos = strchr(field, '.');
struct vcfInfoDef *infoDef;
char *baseKey;
char *subFieldName = NULL;
int subFieldIndex = -1;
if (dotPos != NULL)
    {
    baseKey = cloneStringZ(field, dotPos - field);
    subFieldName = cloneString(dotPos + 1);
    // Sub-field filters allow any Number, since fields like vep are Number=.
    infoDef = vcfInfoDefForKey(vcff, baseKey);
    if (infoDef == NULL || infoDef->type != vcfInfoString)
        {
        warn("track %s: can't filter on %s: %s is not a String INFO field in the VCF header",
             tdb->track, field, baseKey);
        freeMem(baseKey);
        freeMem(subFieldName);
        return NULL;
        }
    subFieldIndex = vcfInfoDefSubFieldIndex(infoDef, subFieldName);
    if (subFieldIndex < 0)
        {
        warn("track %s: filterValues.%s: sub-field '%s' not found in %s INFO Format clause",
             tdb->track, field, subFieldName, baseKey);
        freeMem(baseKey);
        freeMem(subFieldName);
        return NULL;
        }
    freeMem(baseKey);
    }
else
    {
    infoDef = vcfInfoDefForFilter(vcff, tdb, field, FALSE, isList);
    if (infoDef == NULL)
        return NULL;
    }

AllocVar(filter);
filter->infoDef = infoDef;
filter->subFieldIndex = subFieldIndex;
filter->subFieldName = subFieldName;
filter->comparisonType = comparisonType;
filter->valueHash = newHash(5);
filter->numValuesInHash = slCount(choices);

for(; choices; choices = choices->next)
    hashStore(filter->valueHash, choices->name);

filter->isHighlight = isHighlight;
return filter;
}

struct vcfColorByInfo *vcfColorByInfoFromTdb(struct trackDb *tdb, struct vcfFile *vcff)
/* Parse colorByInfo / colorByInfo.<FIELD> settings; returns NULL when
 * the feature is not configured on this track. The vcff header is used to check
 * that the field is a String INFO field and to resolve the named sub-field of a
 * pipe-separated INFO annotation (e.g. vep.Consequence). Warns and returns NULL
 * when the field can't be used. */
{
char *fieldKey = trackDbSetting(tdb, VCF_COLOR_BY_INFO);
if (isEmpty(fieldKey))
    return NULL;
char settingKey[256];
safef(settingKey, sizeof(settingKey), "%s.%s", VCF_COLOR_BY_INFO, fieldKey);
char *mapStr = trackDbSetting(tdb, settingKey);
if (isEmpty(mapStr))
    return NULL;

// If fieldKey is dotted (e.g. vep.Consequence), resolve the named sub-field's
// index within the INFO def's Format clause; the stored fieldKey drops the
// suffix so vcfRecordFindInfo finds the parent INFO element at lookup time.
char *baseKey = fieldKey;
char *subFieldName = NULL;
int subFieldIndex = -1;
char *dotPos = strchr(fieldKey, '.');
char baseKeyBuf[256];
if (dotPos == NULL)
    {
    struct vcfInfoDef *def = vcfInfoDefForKey(vcff, fieldKey);
    if (def == NULL || def->type != vcfInfoString)
        {
        warn("track %s: can't color by %s: it is not a String INFO field in the VCF header",
             tdb->track, fieldKey);
        return NULL;
        }
    }
else
    {
    int baseLen = dotPos - fieldKey;
    safencpy(baseKeyBuf, sizeof(baseKeyBuf), fieldKey, baseLen);
    baseKey = baseKeyBuf;
    subFieldName = dotPos + 1;
    struct vcfInfoDef *def = vcfInfoDefForKey(vcff, baseKey);
    if (def == NULL || def->type != vcfInfoString)
        {
        warn("track %s: can't color by %s: %s is not a String INFO field in the VCF header",
             tdb->track, fieldKey, baseKey);
        return NULL;
        }
    subFieldIndex = vcfInfoDefSubFieldIndex(def, subFieldName);
    if (subFieldIndex < 0)
        {
        warn("track %s: colorByInfo %s: sub-field '%s' not in %s INFO Format clause",
             tdb->track, fieldKey, subFieldName, baseKey);
        return NULL;
        }
    }

struct vcfColorByInfo *cbi;
AllocVar(cbi);
cbi->fieldKey = cloneString(baseKey);
cbi->valueToRgb = newHash(5);
cbi->orderedColors = NULL;
cbi->subFieldIndex = subFieldIndex;
cbi->subFieldName = (subFieldName != NULL) ? cloneString(subFieldName) : NULL;

// Comma-separated key=colorSpec pairs. Only #rrggbb hex codes and HTML
// color names are accepted; the r,g,b triple form would collide with
// the pair separator. Build a hash for fast top-level lookup AND an
// ordered slPair list so the sub-field path can iterate in declaration
// order (first declared key with any match across annotations wins).
char *clone = cloneString(mapStr);
char *pairs[64];
int nPairs = chopByChar(clone, ',', pairs, ArraySize(pairs));
int i;
for (i = 0; i < nPairs; i++)
    {
    char *eq = strchr(pairs[i], '=');
    if (eq == NULL)
        continue;
    *eq++ = 0;
    char *key = trimSpaces(pairs[i]);
    char *colorSpec = trimSpaces(eq);
    if (isEmpty(key) || isEmpty(colorSpec))
        continue;
    unsigned rgb;
    if (!htmlColorForCode(colorSpec, &rgb) && !htmlColorForName(colorSpec, &rgb))
        continue;
    struct rgbColor *c;
    AllocVar(c);
    *c = bedColorToRgb(rgb);
    hashAdd(cbi->valueToRgb, key, c);
    slPairAdd(&cbi->orderedColors, key, c);
    }
slReverse(&cbi->orderedColors);
freeMem(clone);
return cbi;
}

boolean vcfColorByInfoLookup(struct vcfColorByInfo *cbi,
                             const struct vcfRecord *rec,
                             struct rgbColor *out)
/* Look up rec's value for cbi->fieldKey and copy its RGB into *out.
 * Returns FALSE when no mapping applies (caller should use a fallback). */
{
if (cbi == NULL || rec == NULL)
    return FALSE;
const struct vcfInfoElement *ele = vcfRecordFindInfo((struct vcfRecord *)rec, cbi->fieldKey);
if (ele == NULL || ele->count < 1 || ele->missingData[0])
    return FALSE;

if (cbi->subFieldIndex < 0)
    {
    // Top-level lookup: single value per record, direct hash hit.
    char *valStr = ele->values[0].datString;
    if (isEmpty(valStr))
        return FALSE;
    struct rgbColor *c = hashFindVal(cbi->valueToRgb, valStr);
    if (c == NULL)
        return FALSE;
    *out = *c;
    return TRUE;
    }

// Sub-field lookup: a record can carry many transcript annotations; collect
// every map key any of them matches, then walk the ordered list to return the
// earliest-declared (highest-priority) hit.
struct hash *seen = hashNew(4);
int v;
for (v = 0; v < ele->count; v++)
    {
    if (ele->missingData[v])
        continue;
    char *annot = ele->values[v].datString;
    if (isEmpty(annot))
        continue;
    char *clone = cloneString(annot);
    char *tokens[128];
    int nTok = chopByChar(clone, '|', tokens, ArraySize(tokens));
    if (cbi->subFieldIndex < nTok)
        {
        // The Consequence sub-field can be '&'-joined SO terms within one
        // annotation; each term participates independently in the map lookup.
        char *terms[16];
        int nTerms = chopByChar(tokens[cbi->subFieldIndex], '&',
                                terms, ArraySize(terms));
        int t;
        for (t = 0; t < nTerms; t++)
            if (hashLookup(cbi->valueToRgb, terms[t]))
                hashStore(seen, terms[t]);
        }
    freeMem(clone);
    }

struct slPair *p;
for (p = cbi->orderedColors; p != NULL; p = p->next)
    {
    if (hashLookup(seen, p->name))
        {
        *out = *(struct rgbColor *)p->val;
        hashFree(&seen);
        return TRUE;
        }
    }
hashFree(&seen);
return FALSE;
}

struct vcfInfoFilter *buildVcfInfoFilters(struct vcfFile *vcff, struct cart *cart, struct trackDb *tdb)
/* Parse the cart/trackDb current filters into something we can filter the records on.
 * Warns about and skips any filter whose INFO field is missing or has the wrong type. */
{
struct vcfInfoFilter *filters = NULL, *filter;

struct trackDbFilter *tdbFilters = tdbGetTrackNumFilters(tdb);
for (; tdbFilters; tdbFilters = tdbFilters->next)
    {
    if ((filter = vcfInfoFilterMakeNumberFilter(cart, vcff, tdb, tdbFilters->name, NULL, tdbFilters->fieldName, FALSE)) != NULL)
        slAddHead(&filters, filter);
    }

// then the text filters
tdbFilters = tdbGetTrackTextFilters(tdb);
for (; tdbFilters; tdbFilters = tdbFilters->next)
    {
    if ((filter = vcfInfoFilterMakeFilterText(cart, vcff, tdb, tdbFilters->name, tdbFilters->fieldName, FALSE)) != NULL)
        slAddHead(&filters, filter);
    }

// finally the hash filters
filterBy_t *filterBySet = filterBySetGet(tdb, cart, NULL);
filterBy_t *filterBy = filterBySet;
for (; filterBy != NULL; filterBy = filterBy->next)
    {
    if (filterBy->slChoices && differentString(filterBy->slChoices->name, "All"))
        {
        if ((filter = vcfInfoFilterMakeFilterBy(cart, vcff, tdb, filterBy->column, filterBy->slChoices, FALSE)) != NULL)
            slAddHead(&filters, filter);
        }
    }
return filters;
}

boolean vcfInfoFilterOneRecord(struct vcfRecord *rec, struct vcfInfoFilter *vcfInfoFilters)
/* Return true if rec passes all the filters on the INFO fields defined in vcfInfoFilters */
{
struct vcfInfoFilter *filter;
for (filter = vcfInfoFilters; filter != NULL; filter = filter->next)
    {
    struct vcfInfoDef *def = filter->infoDef;
    const struct vcfInfoElement *vcfInfoEle = vcfRecordFindInfo(rec, def->key);
    // Records missing the filtered key, or with an explicit "." value, don't satisfy the filter.
    if (vcfInfoEle == NULL || vcfInfoEle->count < 1 || vcfInfoEle->missingData[0])
        return FALSE;
    if (filter->subFieldName != NULL)
        {
        // The filter targets a named sub-field of a pipe-separated INFO value (e.g.
        // vep.Consequence). One VCF record can carry multiple transcript-level annotations;
        // the variant passes if any annotation's indexed sub-field hits the value set.
        boolean anyMatch = FALSE;
        int v;
        for (v = 0; v < vcfInfoEle->count && !anyMatch; v++)
            {
            if (vcfInfoEle->missingData[v])
                continue;
            char *annot = vcfInfoEle->values[v].datString;
            if (isEmpty(annot))
                continue;
            char *clone = cloneString(annot);
            char *tokens[128];
            int nTok = chopByChar(clone, '|', tokens, ArraySize(tokens));
            if (filter->subFieldIndex < nTok)
                {
                // Consequence and similar SO-term fields can be ampersand-joined within one
                // annotation (e.g. splice_donor_variant&intron_variant); any term hit passes.
                char *terms[16];
                int nTerms = chopByChar(tokens[filter->subFieldIndex], '&',
                                        terms, ArraySize(terms));
                int t;
                for (t = 0; t < nTerms; t++)
                    {
                    if (hashLookup(filter->valueHash, terms[t]))
                        {
                        anyMatch = TRUE;
                        break;
                        }
                    }
                }
            freeMem(clone);
            }
        if (!anyMatch)
            return FALSE;
        continue;
        }
    union vcfDatum val = vcfInfoEle->values[0];
    // vcfDatum is a union typed by def->type; pick the right member as a double for numeric ops.
    double dval = 0;
    if (def->type == vcfInfoInteger)
        dval = (double)val.datInt;
    else if (def->type == vcfInfoFloat)
        dval = val.datFloat;
    switch (filter->comparisonType)
        {
        case COMPARE_WILDCARD:
            if (!wildMatch(filter->wildCardString, val.datString))
                return FALSE;
            break;
        case COMPARE_REGEXP:
            if (regexec(&filter->regEx, val.datString, 0, NULL, 0) != 0)
                return FALSE;
            break;
        case COMPARE_HASH_LIST_AND:
        case COMPARE_HASH_LIST_OR:
            {
            // the VCF parser already split the comma-separated values into ele->values
            unsigned found = 0;
            struct hash *seenHash = newHash(3);
            int v;
            for (v = 0; v < vcfInfoEle->count; v++)
                {
                char *value = vcfInfoEle->values[v].datString;
                if (vcfInfoEle->missingData[v] || isEmpty(value))
                    continue;
                if (hashLookup(seenHash, value))
                    continue;
                hashStore(seenHash, value);
                if (hashLookup(filter->valueHash, value))
                    {
                    found++;
                    if (filter->comparisonType == COMPARE_HASH_LIST_OR)
                        break;
                    }
                }
            hashFree(&seenHash);
            if (filter->comparisonType == COMPARE_HASH_LIST_AND)
                {
                if (found < filter->numValuesInHash)
                    return FALSE;
                }
            else if (!found)
                return FALSE;
            }
            break;
        case COMPARE_HASH:
            if (!hashLookup(filter->valueHash, val.datString))
                return FALSE;;
            break;
        case COMPARE_LESS:
            if (!(dval <= filter->value1))
                return FALSE;
            break;
        case COMPARE_MORE:
            if (!(dval >= filter->value1))
                return FALSE;
            break;
        case COMPARE_BETWEEN:
            if (!((dval >= filter->value1) && (dval <= filter->value2)))
                return FALSE;
            break;
        default:
            break;
        }
    }
return TRUE;
}

INLINE char *nameOrDefault(char *thisName, char *defaultVal)
/* If thisName is not a placeholder value, return it; otherwise return default. */
{
if (isNotEmpty(thisName) && !sameString(thisName, "."))
    return thisName;
return defaultVal;
}

#define VCF_HAPLOSORT_DEFAULT_DESC "middle variant in viewing window"

static void vcfCfgHaplotypeCenterHiddens(char *track, char *ctrName, char *ctrChrom, int ctrPos)
/* Make hidden form inputs and button for setting the center variant for haplotype
 * clustering/sorting in hgTracks. */
{
char cartVar[1024];
safef(cartVar, sizeof(cartVar), "%s.centerVariantChrom", track);
cgiMakeHiddenVar(cartVar, ctrChrom);
safef(cartVar, sizeof(cartVar), "%s.centerVariantPos", track);
char ctrPosStr[16];
safef(ctrPosStr, sizeof(ctrPosStr), "%d", ctrPos);
cgiMakeHiddenVar(cartVar, ctrPosStr);
safef(cartVar, sizeof(cartVar), "%s.centerVariantName", track);
cgiMakeHiddenVar(cartVar, ctrName);
}

char *vcfHaplotypeOrSample(struct cart *cart)
/* Return "Sample" if the current organism is uniploid (like SARS-CoV-2), "Haplotype" otherwise. */
{
// We should make a better way of determining whether the organism is diploid,
// but for now this will prevent David from being bothered by diploid terminology
// when viewing SARS-CoV-2 variants:
return sameOk(cartOptionalString(cart, "db"), "wuhCor1") ? "Sample" : "Haplotype";
}

void vcfCfgHaplotypeCenter(struct cart *cart, struct trackDb *tdb, char *track,
			   boolean parentLevel, struct vcfFile *vcff,
			   char *thisName, char *thisChrom, int thisPos, char *formName)
/* If vcff has genotype data, show status and controls for choosing the center variant
 * for haplotype clustering/sorting in hgTracks. */
{
if (vcff != NULL && vcff->genotypeCount > 1)
    {
    printf("using ");
    char *centerChrom = cartOptionalStringClosestToHome(cart, tdb, parentLevel,
							"centerVariantChrom");
    if (isEmpty(centerChrom))
	{
	// Unspecified in cart -- describe the default action
	printf(VCF_HAPLOSORT_DEFAULT_DESC " as anchor.</TD></TR>\n");
	if (isNotEmpty(thisChrom))
	    {
	    // but we do have a candidate, so offer to make it the center:
	    puts("<TR><TD></TD><TD>");
	    vcfCfgHaplotypeCenterHiddens(track, thisName, thisChrom, thisPos);
	    char label[256];
	    safef(label, sizeof(label), "Use %s", nameOrDefault(thisName, "this variant"));
	    cgiMakeButton("setCenterSubmit", label);
	    printf(" as anchor</TD></TR>\n");
	    }
	else
            {
	    printf("<TR><TD></TD><TD>");
            char *hapOrSample = vcfHaplotypeOrSample(cart);
            if (sameString(hapOrSample, "Sample"))
                {
                puts("Samples are clustered by similarity around a central variant. "
                     "Samples are reordered for display using the clustering tree, which is "
                     "drawn in the left label area.");
                }
            else
                {
                puts("If this mode is selected and genotypes are phased or homozygous, "
                     "then each genotype is split into two independent haplotypes. "
                     "These local haplotypes are clustered by similarity around a central variant. "
                     "Haplotypes are reordered for display using the clustering tree, which is "
                     "drawn in the left label area. "
                     "Local haplotype blocks can often be identified using this display.");
                }
            printf("<br>To anchor the sorting to a particular variant, "
		   "click on the variant in the genome browser, "
		   "and then click on the 'Use this variant' button on the next page."
		   "</TD></TR>\n");
            }
	}
    else
	{
	// Describe the one specified in cart.
	int centerPos = cartUsualIntClosestToHome(cart, tdb, parentLevel, "centerVariantPos",
						  -1);
	char *centerName = cartStringClosestToHome(cart, tdb, parentLevel, "centerVariantName");
	if (isNotEmpty(thisChrom))
	    {
	    // These form inputs are for either "use me" or clear:
	    vcfCfgHaplotypeCenterHiddens(track, thisName, thisChrom, thisPos);
	    // Is this variant the same as the center variant specified in cart?
	    if (sameString(thisChrom, centerChrom) && sameString(thisName, centerName) &&
		thisPos == centerPos)
		printf("this variant as anchor.</TD></TR>\n");
	    else
		{
		// make a "use me" button
		printf("%s at %s:%d as anchor.</TD></TR>\n<TR><TD></TD><TD>\n",
		       nameOrDefault(centerName, "variant"), centerChrom, centerPos+1);
		char label[256];
		safef(label, sizeof(label), "Use %s", nameOrDefault(thisName, "this variant"));
		cgiMakeButton("replaceCenterSubmit", label);
		printf(" as anchor</TD></TR>\n");
		}
	    }
	else
	    {
	    // Form inputs (in case the clear button is clicked)
	    vcfCfgHaplotypeCenterHiddens(track, centerName, centerChrom, centerPos);
	    printf("%s at %s:%d as anchor.</TD></TR>\n",
		   nameOrDefault(centerName, "variant"), centerChrom, centerPos+1);
	    }
	// Make a clear button that modifies the hiddens using onClick
	puts("<TR><TD></TD><TD>");
	struct dyString *onClick = dyStringNew(0);
	dyStringPrintf(onClick, "updateOrMakeNamedVariable(%s, '%s.centerVariantChrom', ''); ",
		       formName, track);
	dyStringPrintf(onClick, "updateOrMakeNamedVariable(%s, '%s.centerVariantName', ''); ",
		       formName, track);
	dyStringPrintf(onClick, "updateOrMakeNamedVariable(%s, '%s.centerVariantPos', 0);",
		       formName, track);
	dyStringPrintf(onClick, "document.%s.submit(); return false;", formName);
	cgiMakeButtonWithOnClick("clearCenterSubmit", "Clear selection", NULL, onClick->string);
	printf(" (use " VCF_HAPLOSORT_DEFAULT_DESC ")</TD></TR>\n");
	}
    }
}

static void vcfCfgHaplotypeMethod(struct cart *cart, struct trackDb *tdb, char *track,
                                  boolean parentLevel, struct vcfFile *vcff)
/* If vcff has genotype data, offer the option of whether to cluster or just use the order
 * of genotypes in the VCF file.  For clustering, show status and controls for choosing the
 * center variant for haplotype clustering/sorting in hgTracks. */
{
if (vcff != NULL && vcff->genotypeCount > 1)
    {
    printf("<TABLE cellpadding=0><TR><TD colspan=2>"
	   "<B>%s sorting order:</B></TD></TR>\n", vcfHaplotypeOrSample(cart));
    // If trackDb specifies a treeFile, offer that as an option
    char *hapMethod = cartOrTdbString(cart, tdb, VCF_HAP_METHOD_VAR, VCF_DEFAULT_HAP_METHOD);
    char *hapMethodTdb = trackDbSetting(tdb, VCF_HAP_METHOD_VAR);
    char varName[1024];
    safef(varName, sizeof(varName), "%s." VCF_HAP_METHOD_VAR, track);
    if (hapMethodTdb && startsWithWord("treeFile", hapMethodTdb))
        {
        puts("<TR><TD>");
        cgiMakeRadioButton(varName, VCF_HAP_METHOD_TREE_FILE,
                           startsWithWord(VCF_HAP_METHOD_TREE_FILE, hapMethod));
        printf("</TD><TD>using the tree specified in file associated with track</TD></TR>");
        }
    printf("<TR><TD>");
    cgiMakeRadioButton(varName, VCF_HAP_METHOD_CENTER_WEIGHTED,
                       sameString(hapMethod, VCF_HAP_METHOD_CENTER_WEIGHTED));
    printf("</TD><TD>");
    vcfCfgHaplotypeCenter(cart, tdb, track, parentLevel, vcff, NULL, NULL, 0, "mainForm");
    puts("<TR><TD>");
    cgiMakeRadioButton(varName, VCF_HAP_METHOD_FILE_ORDER,
                       sameString(hapMethod, VCF_HAP_METHOD_FILE_ORDER));
    puts("</TD><TD>using the order in which samples appear in the underlying VCF file</TD></TR>");
    puts("</TABLE>");
    jsInlineF("$('input[type=radio][name=\"%s\"]').change(function() { "
              "if (this.value == '"VCF_HAP_METHOD_CENTER_WEIGHTED"') {"
              "  $('#leafShapeContainer').show();"
              "  $('#sampleColorContainer').hide();"
              "} else if (this.value == '"VCF_HAP_METHOD_TREE_FILE"') {"
              "  $('#sampleColorContainer').show();"
              "  $('#leafShapeContainer').hide();"
              "} else {"
              "  $('#sampleColorContainer').hide();"
              "  $('#leafShapeContainer').hide();"
              "}});\n",
              varName);
    }
}

//TODO: share this code w/hgTracks, hgc in hg/lib/vcfFile.c
static struct vcfFile *vcfHopefullyOpenHeader(struct cart *cart, struct trackDb *tdb)
/* Defend against network errors and return the vcfFile object with header data, or NULL. */
{
knetUdcInstall();
if (udcCacheTimeout() < 300)
    udcSetCacheTimeout(300);
char *fileOrUrl = trackDbSetting(tdb, "bigDataUrl");
if (isEmpty(fileOrUrl))
    {
    char *db = cartString(cart, "db");
    char *table = tdb->table;
    char *dbTableName = trackDbSetting(tdb, "dbTableName");
    struct sqlConnection *conn;
    if (isCustomTrack(tdb->track) && isNotEmpty(dbTableName))
        {
        conn =  hAllocConn(CUSTOM_TRASH);
        table = dbTableName;
        }
    else
        conn = hAllocConnTrack(db, tdb);
    char *chrom = cartOptionalString(cart, "c");
    if (chrom != NULL)
        fileOrUrl = bbiNameFromSettingOrTableChrom(tdb, conn, table, chrom);
    if (fileOrUrl == NULL)
        fileOrUrl = bbiNameFromSettingOrTableChrom(tdb, conn, table, hDefaultChrom(db));
    hFreeConn(&conn);
    }
if (fileOrUrl == NULL)
    return NULL;
int vcfMaxErr = 100;
struct vcfFile *vcff = NULL;
/* protect against temporary network error */
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    if (startsWithWord("vcfTabix", tdb->type))
	vcff = vcfTabixFileMayOpen(fileOrUrl, NULL, 0, 0, vcfMaxErr, -1);
    else
	vcff = vcfFileMayOpen(fileOrUrl, NULL, 0, 0, vcfMaxErr, -1, FALSE);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    if (isNotEmpty(errCatch->message->string))
	warn("unable to open %s: %s", fileOrUrl, errCatch->message->string);
    }
errCatchFree(&errCatch);
return vcff;
}

static void vcfCfgHapClusterEnable(struct cart *cart, struct trackDb *tdb, char *name,
				   boolean parentLevel)
/* Let the user enable/disable haplotype sorting display. */
{
boolean hapClustEnabled = cartOrTdbBoolean(cart, tdb, VCF_HAP_ENABLED_VAR, TRUE);
char cartVar[1024];
safef(cartVar, sizeof(cartVar), "%s." VCF_HAP_ENABLED_VAR, name);
cgiMakeCheckBox(cartVar, hapClustEnabled);
printf("<B>Enable %s sorting display</B><BR>\n", vcfHaplotypeOrSample(cart));
}

static void vcfCfgHapClusterColor(struct cart *cart, struct trackDb *tdb, char *name,
				   boolean parentLevel)
/* Let the user choose how to color the sorted haplotypes. */
{
printf("<B>Allele coloring scheme:</B><BR>\n");
char *colorBy = cartOrTdbString(cart, tdb, VCF_HAP_COLORBY_VAR, VCF_DEFAULT_HAP_COLORBY);
char varName[1024];
safef(varName, sizeof(varName), "%s." VCF_HAP_COLORBY_VAR, name);
cgiMakeRadioButton(varName, VCF_HAP_COLORBY_ALTONLY, sameString(colorBy, VCF_HAP_COLORBY_ALTONLY));
printf("reference alleles invisible, alternate alleles in black<BR>\n");
char *geneTrack = cartOrTdbString(cart, tdb, "geneTrack", NULL);
if (isNotEmpty(geneTrack))
    {
    cgiMakeRadioButton(varName, VCF_HAP_COLORBY_FUNCTION,
                       sameString(colorBy, VCF_HAP_COLORBY_FUNCTION));
    printf("reference alleles invisible, alternate alleles in "
           "<span style='color:red'>red</span> for non-synonymous, "
           "<span style='color:green'>green</span> for synonymous, "
           "<span style='color:blue'>blue</span> for UTR/noncoding, "
           "black otherwise<BR>\n");
    }
cgiMakeRadioButton(varName, VCF_HAP_COLORBY_REFALT, sameString(colorBy, VCF_HAP_COLORBY_REFALT));
printf("reference alleles in blue, alternate alleles in red<BR>\n");
cgiMakeRadioButton(varName, VCF_HAP_COLORBY_BASE, sameString(colorBy, VCF_HAP_COLORBY_BASE));
printf("first base of allele (A = red, C = blue, G = green, T = magenta)<BR>\n");
}

static void vcfCfgHapClusterSampleColor(struct cart *cart, struct trackDb *tdb, char *name,
                                        boolean parentLevel)
/* If sampleColorFile specifies multiple files, when hapClusterMethod treeFile is selected,
 * let the user choose sample-coloring scheme for the tree. */
{
char *tdbSetting = trackDbSetting(tdb, VCF_SAMPLE_COLOR_FILE);
if (tdbSetting && strchr(tdbSetting, ' '))
    {
    char *hapMethod = cartOrTdbString(cart, tdb, VCF_HAP_METHOD_VAR, VCF_DEFAULT_HAP_METHOD);
    printf("<div id='sampleColorContainer'%s>\n",
           startsWithWord(VCF_HAP_METHOD_TREE_FILE, hapMethod) ? "" : " style='display: none;'");
    printf("<b>Sample coloring scheme for tree:</b><br>\n");
    char *setting = cartOrTdbString(cart, tdb, VCF_SAMPLE_COLOR_FILE, tdbSetting);
    char *options[16];
    int optionCount = chopLine(tdbSetting, options);
    char *labels[optionCount];
    char *values[optionCount];
    int i;
    for (i = 0;  i < optionCount;  i++)
        {
        char *eq = strchr(options[i], '=');
        if (eq)
            {
            *eq = '\0';
            labels[i] = options[i];
            replaceChar(options[i], '_', ' ');
            values[i] = eq+1;
            }
        else
            {
            labels[i] = values[i] = options[i];
            }
        }
    char *selected = strchr(setting, ' ') ? values[0] : setting;
    char varName[1024];
    safef(varName, sizeof varName, "%s." VCF_SAMPLE_COLOR_FILE, name);
    cgiMakeDropListWithVals(varName, labels, values, optionCount, selected);
    puts("</div>");
    }
}

static void vcfCfgHapClusterTreeAngle(struct cart *cart, struct trackDb *tdb, char *name,
				   boolean parentLevel)
/* Let the user choose branch shape. */
{
// This option applies only to center-weighted clustering; don't show option when some other
// method is selected.
char *hapMethod = cartOrTdbString(cart, tdb, VCF_HAP_METHOD_VAR, VCF_DEFAULT_HAP_METHOD);
printf("<div id='leafShapeContainer'%s>\n",
       differentString(hapMethod, VCF_HAP_METHOD_CENTER_WEIGHTED) ? " style='display: none;'" : "");
printf("<B>%s clustering tree leaf shape:</B><BR>\n", vcfHaplotypeOrSample(cart));
char *treeAngle = cartOrTdbString(cart, tdb, VCF_HAP_TREEANGLE_VAR, VCF_DEFAULT_HAP_TREEANGLE);
char varName[1024];
safef(varName, sizeof(varName), "%s." VCF_HAP_TREEANGLE_VAR, name);
cgiMakeRadioButton(varName, VCF_HAP_TREEANGLE_TRIANGLE,
		   sameString(treeAngle, VCF_HAP_TREEANGLE_TRIANGLE));
printf("draw branches whose samples are all identical as &lt;<BR>\n");
cgiMakeRadioButton(varName, VCF_HAP_TREEANGLE_RECTANGLE,
		   sameString(treeAngle, VCF_HAP_TREEANGLE_RECTANGLE));
printf("draw branches whose samples are all identical as [<BR>\n");
puts("</div>");
}

static void vcfCfgHapClusterHeight(struct cart *cart, struct trackDb *tdb, struct vcfFile *vcff,
				   char *name, boolean parentLevel)
/* Let the user specify a height for the track. */
{
if (vcff != NULL && vcff->genotypeCount > 1)
    {
    printf("<B>%s sorting display height:</B> \n", vcfHaplotypeOrSample(cart));
    int cartHeight = cartOrTdbInt(cart, tdb, VCF_HAP_HEIGHT_VAR, VCF_DEFAULT_HAP_HEIGHT);
    char varName[1024];
    safef(varName, sizeof(varName), "%s." VCF_HAP_HEIGHT_VAR, name);
    cgiMakeIntVarInRange(varName, cartHeight, "Height (in pixels) of track", 5, "4", "10000");
    puts("<BR>");
    }
}

static void vcfCfgHapCluster(struct cart *cart, struct trackDb *tdb, struct vcfFile *vcff,
			     char *name, boolean parentLevel)
/* Show controls for haplotype-sorting display, which only makes sense to do when
 * the VCF file describes multiple genotypes. */
{
char *hapOrSample = vcfHaplotypeOrSample(cart);
printf("<H3>%s sorting display</H3>\n", hapOrSample);
vcfCfgHapClusterEnable(cart, tdb, name, parentLevel);
vcfCfgHaplotypeMethod(cart, tdb, name, parentLevel, vcff);
vcfCfgHapClusterTreeAngle(cart, tdb, name, parentLevel);
vcfCfgHapClusterSampleColor(cart, tdb, name, parentLevel);
vcfCfgHapClusterColor(cart, tdb, name, parentLevel);
vcfCfgHapClusterHeight(cart, tdb, vcff, name, parentLevel);
}

static void vcfCfgMinQual(struct cart *cart, struct trackDb *tdb, struct vcfFile *vcff,
			  char *name, boolean parentLevel)
/* If checkbox is checked, apply minimum value filter to QUAL column. */
{
char cartVar[1024];
safef(cartVar, sizeof(cartVar), "%s." VCF_APPLY_MIN_QUAL_VAR, name);
boolean applyFilter = cartOrTdbBoolean(cart, tdb, VCF_APPLY_MIN_QUAL_VAR,
				       VCF_DEFAULT_APPLY_MIN_QUAL);
cgiMakeCheckBox(cartVar, applyFilter);
printf("<B>Exclude variants with Quality/confidence score (QUAL) score less than</B>\n");
double minQual = cartOrTdbDouble(cart, tdb, VCF_MIN_QUAL_VAR, VCF_DEFAULT_MIN_QUAL);
safef(cartVar, sizeof(cartVar), "%s." VCF_MIN_QUAL_VAR, name);
cgiMakeDoubleVar(cartVar, minQual, 10);
printf("<BR>\n");
}

static void vcfCfgFilterColumn(struct cart *cart, struct trackDb *tdb, struct vcfFile *vcff,
			       char *name, boolean parentLevel)
/* Show controls for filtering by value of VCF's FILTER column, which uses values defined
 * in the header. */
{
int filterCount = slCount(vcff->filterDefs);
if (filterCount < 1)
    return;
printf("<B>Exclude variants with these FILTER values:</B><BR>\n");
char cartVar[1024];
safef(cartVar, sizeof(cartVar), "%s."VCF_EXCLUDE_FILTER_VAR, name);
if (slCount(vcff->filterDefs) > 1)
    {
    jsMakeCheckboxGroupSetClearButton(cartVar, TRUE);
    puts("&nbsp;");
    jsMakeCheckboxGroupSetClearButton(cartVar, FALSE);
    }
char *values[filterCount];
char *labels[filterCount];
int i;
struct vcfInfoDef *filt;
for (i=0, filt=vcff->filterDefs;  filt != NULL;  i++, filt = filt->next)
    {
    values[i] = filt->key;
    struct dyString *dy = dyStringNew(0);
    dyStringAppend(dy, filt->key);
    if (isNotEmpty(filt->description))
	dyStringPrintf(dy, " (%s)", filt->description);
    labels[i] = dyStringCannibalize(&dy);
    }
struct slName *selectedValues = NULL;
if (cartListVarExistsAnyLevel(cart, tdb, FALSE, VCF_EXCLUDE_FILTER_VAR))
    selectedValues = cartOptionalSlNameListClosestToHome(cart, tdb, FALSE, VCF_EXCLUDE_FILTER_VAR);
cgiMakeCheckboxGroupWithVals(cartVar, labels, values, filterCount, selectedValues, 1);
}

static void vcfCfgMinAlleleFreq(struct cart *cart, struct trackDb *tdb, struct vcfFile *vcff,
				char *name, boolean parentLevel)
/* Show input for minimum allele frequency, if we can extract it from the VCF INFO column. */
{
printf("<B>Minimum minor allele frequency (if INFO column includes AF or AC+AN):</B>\n");
double cartMinFreq = cartOrTdbDouble(cart, tdb, VCF_MIN_ALLELE_FREQ_VAR,
				     VCF_DEFAULT_MIN_ALLELE_FREQ);
char varName[1024];
safef(varName, sizeof(varName), "%s." VCF_MIN_ALLELE_FREQ_VAR, name);
cgiMakeDoubleVarInRange(varName, cartMinFreq, "minor allele frequency between 0.0 and 0.5", 5,
			"0.0", "0.5");
puts("<BR>");
}

static void vcfCfgMinAc(struct cart *cart, struct trackDb *tdb, struct vcfFile *vcff,
			char *name, boolean parentLevel)
/* Show input for minimum alternate allele count, if INFO column includes AC.  This lets
 * the user filter on a whole-number count (e.g. 2 to hide singletons) instead of having to
 * enter a tiny frequency cutoff. */
{
printf("<B>Minimum allele count (if INFO column includes AC), e.g. 2 to hide singletons:</B>\n");
int cartMinAc = cartOrTdbInt(cart, tdb, VCF_MIN_AC_VAR, VCF_DEFAULT_MIN_AC);
char varName[1024];
safef(varName, sizeof(varName), "%s." VCF_MIN_AC_VAR, name);
cgiMakeIntVarWithMin(varName, cartMinAc, "minimum allele count", 5, 0);
puts("<BR>");
}

static char *getChildSample(struct trackDb *tdb)
/* Return just the VCF sample name of the phased trio child setting */
{
char *childSampleMaybeAlias = cloneString(trackDbLocalSetting(tdb, VCF_PHASED_CHILD_SAMPLE_SETTING));
char *pt = strchr(childSampleMaybeAlias, '|');
if (pt != NULL)
    *pt = '\0';
return childSampleMaybeAlias;
}

static struct slPair *vcfPhasedGetSamplesFromTdb(struct trackDb *tdb, boolean hideOtherSamples)
/* Get the different VCF Phased Trio setings out of trackDb onto a list */
{
// cloneString here because we will be munging the result if there are alternate labels
char *childSampleMaybeAlias = cloneString(trackDbLocalSetting(tdb, VCF_PHASED_CHILD_SAMPLE_SETTING));
char *parentSamplesMaybeAlias = cloneString(trackDbLocalSetting(tdb, VCF_PHASED_PARENTS_SAMPLE_SETTING));
char *samples[VCF_PHASED_MAX_OTHER_SAMPLES+1]; // for now only allow at most two parents
int numOthers = 0;
if (parentSamplesMaybeAlias && !hideOtherSamples)
    {
    numOthers = chopCommas(cloneString(parentSamplesMaybeAlias), samples);
    if (numOthers > VCF_PHASED_MAX_OTHER_SAMPLES)
        {
        warn("More than %d other samples specified for phased trio", VCF_PHASED_MAX_OTHER_SAMPLES);
        numOthers = VCF_PHASED_MAX_OTHER_SAMPLES;
        }
    // shove child into middle of array, and if there are two parents, scoot the second one to the end
    int lastParentIx = VCF_PHASED_MAX_OTHER_SAMPLES - 1;
    if (samples[lastParentIx] != NULL)
        samples[VCF_PHASED_MAX_OTHER_SAMPLES] = cloneString(samples[lastParentIx]);
    samples[lastParentIx] = cloneString(childSampleMaybeAlias);
    }
else
    samples[0] = cloneString(childSampleMaybeAlias);

boolean gotAlias = strchr(samples[0], '|') != NULL; // default to whatever is first
struct slPair *ret = NULL;
int i;
for (i = 0; i < numOthers+1; i++)
    {
    char *val = strchr(samples[i], '|');
    boolean foundAlias = val != NULL;
    if (val != NULL)
        {
        if (foundAlias != gotAlias)
            errAbort("Either all samples have aliases or none.");
        else
            *val++ = 0;
        }
    char *name = samples[i];
    struct slPair *temp = slPairNew(cloneString(name), cloneString(val));
    slAddHead(&ret, temp);
    }
slReverse(&ret);
return ret;
}

struct slPair *vcfPhasedGetSampleOrder(struct cart *cart, struct trackDb *tdb, boolean parentLevel, boolean hideOtherSamples)
/* Parse out a trio sample order from either trackDb or the cart.
 * If the trackName.sortChildBelow cart variable is true, then ensure
 * the vcfChildSample sample is last in the order, otherwise, use what's
 * in the trackName.vcfSampleOrder cart variable. */
{
char sampleOrderVar[1024];
safef(sampleOrderVar, sizeof(sampleOrderVar), "%s.%s", tdb->track, VCF_PHASED_SAMPLE_ORDER_VAR);
char *cartOrder = cartOptionalString(cart, sampleOrderVar);
boolean childBelow = cartUsualBooleanClosestToHome(cart, tdb, parentLevel, VCF_PHASED_CHILD_BELOW_VAR, FALSE);
struct slPair *tdbOrder = vcfPhasedGetSamplesFromTdb(tdb, hideOtherSamples);
if (!hideOtherSamples)
    {
    // if the user used drag and drop to reorder the trios then that takes precedence
    // over the childBelow checkbox
    if (cartOrder != NULL)
        {
        struct slName *name, *fromCart = slNameListFromComma(cartOrder);
        struct slPair *ret = NULL;
        for (name = fromCart; name != NULL; name = name->next)
            {
            struct slPair *temp = slPairFind(tdbOrder, name->name);
            struct slPair *toAdd = slPairNew(temp->name, temp->val);
            slAddHead(&ret, toAdd);
            }
        slReverse(&ret);
        return ret;
        }
    else if (childBelow)
        {
        char *childName = getChildSample(tdb);
        struct slPair *ret = NULL, *child = NULL, *temp = NULL;
        for (temp = tdbOrder; temp != NULL; temp = temp->next)
            {
            struct slPair *toAdd = slPairNew(temp->name, temp->val);
            if (sameString(temp->name, childName))
                child = toAdd;
            else
                slAddHead(&ret, toAdd);
            }
        if (child)
            slAddHead(&ret, child);
        slReverse(&ret);
        return ret;
        }
    }
// we're hiding the parents OR (we unchecked the childBelow checkbox AND we didn't drag reorder)
return tdbOrder;
}

static boolean hasSampleAliases(struct trackDb *tdb)
/* Check whether trackDb has aliases for the sample names  */
{
struct slPair *nameVals = vcfPhasedGetSamplesFromTdb(tdb,FALSE);
return nameVals->val != NULL;
}

static void vcfPhasedSampleSortUi(struct cart *cart, struct trackDb *tdb, struct vcfFile *vcff, char *name,
                                    boolean parentLevel)
/* Put up the UI for sorting the samples */
{
struct dyString *sortOrder = dyStringNew(0);
struct slPair *pair, *tdbOrder = vcfPhasedGetSampleOrder(cart, tdb, parentLevel, FALSE);
if (slCount(tdbOrder) == 1) // no sorting if there are no parents
    return;
char childBelowSortOrder[1024];
safef(childBelowSortOrder, sizeof(childBelowSortOrder), "%s.%s", name, VCF_PHASED_CHILD_BELOW_VAR);
boolean isBelowChecked = cartUsualBooleanClosestToHome(cart, tdb, parentLevel, VCF_PHASED_CHILD_BELOW_VAR, FALSE);
printf("<b>Show child haplotypes below parents:</b>\n");
cgiMakeCheckBox(childBelowSortOrder, isBelowChecked);
char *infoText = "Check this box to sort the child haplotypes below the parents, leave unchecked"
    " to use the default sort order of the child in the middle. Click into each subtrack to arbitrarily"
    " order the samples which overrides this setting.";
printInfoIcon(infoText);
printf("<br>");
if (!parentLevel)
    {
    printf("<b>or:</b><br>\n");
    printf("<b>Drag to change order:</b>\n");
    printf("<div>\n");
    printf("<table id=\"%s_table\" class=\"tableWithDragAndDrop\">\n", tdb->track);
    for (pair = tdbOrder; pair != NULL; pair = pair->next)
        {
        char id[256];
        safef(id, sizeof(id), "%s_drag", pair->name);
        printf("<tr id=\"%s_row\" class=\"trDraggable\"><td id=\"%s\" class=\"dragHandle\">%s - %s</td></tr>\n", pair->name, id, pair->name, (char *)pair->val);
        dyStringPrintf(sortOrder, "%s,", pair->name);
        }
    printf("</table>\n");
    printf("</div>\n");
    printf("<input type=\"hidden\" name=\"%s.%s\" value=\"%s\">",tdb->track, VCF_PHASED_SAMPLE_ORDER_VAR, dyStringCannibalize(&sortOrder));
    // add the hidden variable for setting the order and the javascript to change it
    jsInlineF(""
    "dragReorder.init();\n"
    "var imgTable = $(\"#%s_table\");\n"
    "if ($(imgTable).length > 0) {\n"
    "   $(imgTable).tableDnD({\n"
    "       onDragClass: \"trDrag\",\n"
    "       dragHandle: \"dragHandle\",\n"
    "       scrollAmount: 40,\n"
    "       onDragStart: function(ev, table, row) {\n"
    "           mouse.saveOffset(ev);\n"
    "           table.tableDnDConfig.dragObjects = [ row ]; // defaults to just the one\n"
    "       },\n"
    "       onDrop: function(table, row, dragStartIndex) {\n"
    "           if ($(row).attr('rowIndex') !== dragStartIndex) {\n"
    "               // NOTE Even if dragging a contiguous set of rows,\n"
    "               // still only need to check the one under the cursor.\n"
    "               if (dragReorder.setOrder) {\n"
    "                   dragReorder.setOrder(table);\n"
    "               }\n"
    "               // save the order of the samples into the input variable named above\n"
    "               var newVal = \"\";\n"
    "               var inp = $(\"input[name='%s.%s']\")[0];\n"
    "               for (i = 0; i < table.rows.length; i++) {\n"
    "                   newVal += table.rows[i].id.slice(0,-4) + \",\";\n"
    "               }\n"
    "               if (newVal.slice(-1) === \",\") {\n"
    "                   newVal = newVal.slice(0,-1);\n"
    "               }\n"
    "               inp.value = newVal;\n"
    "           }\n"
    "       }\n"
    "   });\n"
    "}\n"
    "", tdb->track, tdb->track, VCF_PHASED_SAMPLE_ORDER_VAR);
    }
}

static void vcfCfgPhasedTrioUi(struct cart *cart, struct trackDb *tdb, struct vcfFile *vcff, char *name,
                                boolean parentLevel)
/* Put up the phased trio specific config settings */
{
//if (!parentLevel) // don't put up this display at the composite level
vcfPhasedSampleSortUi(cart, tdb, vcff, name, parentLevel);
if (hasSampleAliases(tdb))
    {
    printf("<b>Label samples by:</b>");
    char defaultLabel[1024], aliasLabel[1024];
    safef(defaultLabel, sizeof(defaultLabel), "%s.%s", name, VCF_PHASED_DEFAULT_LABEL_VAR);
    safef(aliasLabel, sizeof(aliasLabel), "%s.%s", name, VCF_PHASED_ALIAS_LABEL_VAR);
    boolean isDefaultChecked = cartUsualBooleanClosestToHome(cart, tdb, parentLevel, VCF_PHASED_DEFAULT_LABEL_VAR, FALSE);
    boolean isAliasChecked = cartUsualBooleanClosestToHome(cart, tdb, parentLevel, VCF_PHASED_ALIAS_LABEL_VAR, TRUE);
    cgiMakeCheckBox(defaultLabel, isDefaultChecked);
    printf("VCF file sample names &nbsp;");
    cgiMakeCheckBox(aliasLabel, isAliasChecked);
    printf("Family Labels");
    printf("<br>");
    }
if (trackDbSetting(tdb,VCF_PHASED_PARENTS_SAMPLE_SETTING))
    {
    printf("<b>Hide parent sample(s)");
    char hideVarName[1024];
    safef(hideVarName, sizeof(hideVarName), "%s.%s", name, VCF_PHASED_HIDE_OTHER_VAR);
    boolean hidingOtherSamples = cartUsualBooleanClosestToHome(cart, tdb, parentLevel, VCF_PHASED_HIDE_OTHER_VAR, FALSE);
    cgiMakeCheckBox(hideVarName, hidingOtherSamples);
    }
printf("<br>");
printf("Allele coloring scheme:");
printf("<br>");
char *colorBy = cartOrTdbString(cart, tdb, VCF_PHASED_COLORBY_VAR, VCF_PHASED_COLORBY_DEFAULT);
char varName[1024];
safef(varName, sizeof(varName), "%s.%s", name, VCF_PHASED_COLORBY_VAR);
cgiMakeRadioButton(varName, VCF_PHASED_COLORBY_DEFAULT, sameString(colorBy, VCF_PHASED_COLORBY_DEFAULT));
printf("No color<br>");
char *geneTrack = cartOrTdbString(cart, tdb, "geneTrack", NULL);
if (isNotEmpty(geneTrack))
    {
    cgiMakeRadioButton(varName, VCF_PHASED_COLORBY_FUNCTION, sameString(colorBy, VCF_PHASED_COLORBY_FUNCTION));
    printf("predicted functional affect: ");
    printf("reference alleles invisible, alternate alleles in "
           "<span style='color:red'>red</span> for non-synonymous, "
           "<span style='color:green'>green</span> for synonymous, "
           "<span style='color:blue'>blue</span> for UTR/noncoding, "
           "black otherwise<BR>\n");
    }
cgiMakeRadioButton(varName, VCF_PHASED_COLORBY_DE_NOVO, sameString(colorBy, VCF_PHASED_COLORBY_DE_NOVO));
printf("predicted de novo child mutations <span style='color:red'>red</span>");
char *deNovoInfoText = "Check this box to color child variants red if they are unique to the child";
printInfoIcon(deNovoInfoText);
printf("<br>");
cgiMakeRadioButton(varName, VCF_PHASED_COLORBY_MENDEL_DIFF, sameString(colorBy, VCF_PHASED_COLORBY_MENDEL_DIFF));
printf("child variants that are inconsistent with phasing <span style='color:red'>red</span>");
char *phasedInfoText = "Check this box to color child variants red if they do not agree with the implied "
    "parental transmitted allele at this location. This configuration is only available when parent "
    "haplotypes are displayed.";
printInfoIcon(phasedInfoText);
}

static void vcfCfgInfoFilterUi(struct cart *cart, struct trackDb *tdb, struct vcfFile *vcff, char *name, boolean parentLevel)
/* Show the filters on the INFO fields, if any */
{
if (cartOptionalString(cart, "ajax") == NULL)
    {
    webIncludeResourceFile("ui.dropdownchecklist.css");
    jsIncludeFile("ui.dropdownchecklist.js",NULL);
    jsIncludeFile("ddcl.js",NULL);
    }

if (parentLevel)
    if (trackDbSettingOn(tdb->parent, "noParentConfig"))
        return;

// Skip everything (including the heading) when no INFO filters are defined.
struct trackDbFilter *numFilters = tdbGetTrackNumFilters(tdb);
struct trackDbFilter *textFilters = tdbGetTrackTextFilters(tdb);
filterBy_t *filterBySet = filterBySetGet(tdb, cart, name);
if (numFilters == NULL && textFilters == NULL && filterBySet == NULL)
    return;

printf("<B>Filter on INFO fields:</B><BR>\n");

char *db = cartString(cart, "db");
boolean isBoxOpened = FALSE;
numericFiltersShowAll(db, cart, tdb, &isBoxOpened, TRUE, parentLevel, name, NULL, FALSE);

textFiltersShowAll(db, cart, tdb, FALSE);

if (filterBySet != NULL)
    {
    if (!tdbIsComposite(tdb) && cartOptionalString(cart, "ajax") == NULL)
        jsIncludeFile("hui.js",NULL);

    if (!isBoxOpened)   // filterBy boxes are not double "boxed" when alone
        printf("<BR>");
    filterBySetCfgUi(cart, tdb, filterBySet, TRUE, name);
    filterBySetFree(&filterBySet);
    }
}

void vcfCfgUi(struct cart *cart, struct trackDb *tdb, char *name, char *title, boolean boxed)
/* VCF: Variant Call Format.  redmine #3710 */
{
boxed = cfgBeginBoxAndTitle(tdb, boxed, title);
printf("<TABLE%s><TR><TD>", boxed ? " width='100%'" : "");
struct vcfFile *vcff = vcfHopefullyOpenHeader(cart, tdb);
if (vcff != NULL)
    {
    boolean parentLevel = isNameAtParentLevel(tdb, name);
    boolean doVcfFilterUi = cartOrTdbBoolean(cart, tdb, VCF_DO_FILTER_UI, TRUE);
    boolean doVcfQualUi = cartOrTdbBoolean(cart, tdb, VCF_DO_QUAL_UI, TRUE);
    boolean doVcfMafUi = cartOrTdbBoolean(cart, tdb, VCF_DO_MAF_UI, TRUE);
    boolean doVcfMinAcUi = cartOrTdbBoolean(cart, tdb, VCF_DO_MIN_AC_UI, TRUE);
    boolean doVcfInfoFilterUi = cartOrTdbBoolean(cart, tdb, VCF_DO_INFOFILTER_UI, TRUE)
                                && bedHasFilters(tdb);
    if (vcff->genotypeCount > 1 && !sameString(tdb->type, "vcfPhasedTrio"))
        {
        vcfCfgHapCluster(cart, tdb, vcff, name, parentLevel);
        }
    if (sameString(tdb->type, "vcfPhasedTrio"))
        {
        vcfCfgPhasedTrioUi(cart, tdb, vcff, name, parentLevel);
        }
    boolean isEvsEsp = sameString(tdb->track, "evsEsp6500");
    boolean printFiltersH3 = (!isEvsEsp && (doVcfQualUi || doVcfFilterUi))
                             || doVcfMafUi || doVcfMinAcUi || doVcfInfoFilterUi;
    if (printFiltersH3)
        puts("<H3>Filters</H3>");
    if (!isEvsEsp)
        {
        if (doVcfQualUi)
            vcfCfgMinQual(cart, tdb, vcff, name, parentLevel);
        if (doVcfFilterUi)
            vcfCfgFilterColumn(cart, tdb, vcff, name, parentLevel);
        }
    if (doVcfMafUi)
        vcfCfgMinAlleleFreq(cart, tdb, vcff, name, parentLevel);
    if (doVcfMinAcUi)
        vcfCfgMinAc(cart, tdb, vcff, name, parentLevel);
    if (doVcfInfoFilterUi)
        vcfCfgInfoFilterUi(cart, tdb, vcff, name, parentLevel);
    }
else
    {
    printf("Sorry, couldn't access VCF file.<BR>\n");
    }

puts("</TD>");
if (boxed && fileExists(hHelpFile("hgVcfTrackHelp")))
    printf("<TD style='text-align:right'><A HREF=\"../goldenPath/help/hgVcfTrackHelp.html\" "
           "TARGET=_BLANK>VCF configuration help</A></TD>");

printf("</TR></TABLE>");
wigOption(cart, name, title, tdb);

if (!boxed && fileExists(hHelpFile("hgVcfTrackHelp")))
    printf("<P><A HREF=\"../goldenPath/help/hgVcfTrackHelp.html\" TARGET=_BLANK>VCF "
	   "configuration help</A></P>");
cfgEndBox(boxed);
}
