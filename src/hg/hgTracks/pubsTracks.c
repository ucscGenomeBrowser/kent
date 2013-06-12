/* pubsTracks - code for the publications tracks */
#include "common.h"
#include "hgTracks.h"
#include "hgFind.h"
#include "bedCart.h"

// we distinguish between four levels of impact factors <1, <3, <10 and >10
static struct rgbColor impact1Color  = {80, 80, 80};
static struct rgbColor impact2Color  = {0, 80, 255};
static struct rgbColor impact3Color  = {0, 100, 0};
static struct rgbColor impact4Color  = {255, 255, 0};

static char *pubsArticleTable(struct track *tg)
/* return the name of the pubs articleTable, either
 * the value from the trackDb statement 'articleTable'
 * or the default value: <trackName>Article */
{
char *articleTable = trackDbSettingClosestToHome(tg->tdb, "pubsArticleTable");
if (isEmpty(articleTable))
    {
    char buf[256];
    safef(buf, sizeof(buf), "%sArticle", tg->track);
    articleTable = cloneString(buf);
    }
return articleTable;
}

static char *makeMysqlMatchStr(char *str)
{
// return a string with all words prefixed with a '+' to force a boolean AND query;
// we also strip leading/trailing spaces.
char *matchStr = needMem(strlen(str) * 2 + 1);
int i = 0;
for(;*str && isspace(*str);str++)
    ; while(*str)
    {
    matchStr[i++] = '+';
    for(; *str && !isspace(*str);str++)
        matchStr[i++] = *str;
    for(;*str && isspace(*str);str++)
        ;
    }
matchStr[i++] = 0;
return matchStr;
}

struct pubsExtra 
/* additional info needed for publication blat linked features: author+year and title */
{
    char *label; // usually author+year
    char *mouseOver; // usually title of article
    char *class; // class of article, usually a curated database
    // color depends on cart settings, either based on topic, impact or year
    // support to ways to color: either by shade (year, impact) or directly with rgb values
    int shade;  // year or impact are shades which we can't resolve to rgb easily
    struct rgbColor *color; 
};

/* assignment of pubs classes to colors */
static struct hash* pubsClassColors = NULL;

static void pubsParseClassColors() 
/* parse class colors from hgFixed.pubsClassColors into the hash pubsClassColors */
{
if (pubsClassColors!=NULL)
    return;

pubsClassColors = hashNew(0);
struct sqlConnection *conn = hAllocConn(database);
if (!sqlTableExists(conn, "hgFixed.pubsClassColors")) 
    {
    return;
    }
char *query = "NOSQLINJ SELECT class, rgbColor FROM hgFixed.pubsClassColors";
struct sqlResult *sr = sqlGetResult(conn, query);
char **row = NULL;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *class = row[0];
    char *colStr = row[1];
    // copied from genePredItemClassColor - is there no function for this?
    // convert comma sep rgb string to array
    char *rgbVals[5];
    chopString(colStr, ",", rgbVals, sizeof(rgbVals));
    struct rgbColor *rgb;
    AllocVar(rgb);
    rgb->r = (sqlUnsigned(rgbVals[0]));
    rgb->g = (sqlUnsigned(rgbVals[1]));
    rgb->b = (sqlUnsigned(rgbVals[2]));
    //printf("Adding hash: %s -> %d,%d,%d", class, rgb->r, rgb->g, rgb->b);
    hashAdd(pubsClassColors, cloneString(class), rgb);
    }
sqlFreeResult(&sr);
}

static char *pubsFeatureLabel(char *author, char *year) 
/* create label <author><year> given authors and year strings */
{
char *authorYear = NULL;

if (isEmpty(author))
    author = "NoAuthor";
if (isEmpty(year))
    year = "NoYear";
authorYear  = catTwoStrings(author, year);

return authorYear;
}

static struct pubsExtra *pubsMakeExtra(struct track* tg, char *articleTable, 
    struct sqlConnection* conn, struct linkedFeatures* lf)
/* bad solution: a function that is called before the extra field is 
 * accessed and that fills it from a sql query. Will need to redo this like gencode, 
 * drawing from atom, variome and bedLoadN or bedDetails */
{
char query[LARGEBUF];
struct sqlResult *sr = NULL;
char **row = NULL;
struct pubsExtra *extra = NULL;

/* support two different storage places for article data: either the bed table directly 
 * includes the title + author of the article or we have to look it up from the articles 
 * table. Having a copy of the title in the bed table is faster */
bool newFormat = FALSE;
if (sqlColumnExists(conn, tg->table, "title")) 
    {
    sqlSafef(query, sizeof(query), "SELECT firstAuthor, year, title, impact, classes FROM %s "
    "WHERE chrom = '%s' and chromStart = '%d' and name='%s'", tg->table, chromName, lf->start, lf->name);
    newFormat = TRUE;
    }
else 
    {
    sqlSafef(query, sizeof(query), "SELECT firstAuthor, year, title FROM %s WHERE articleId = '%s'", 
        articleTable, lf->name);
    newFormat = FALSE;
    }

sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    char *firstAuthor = row[0];
    char *year    = row[1];
    char *title   = row[2];
    char *impact  = NULL;
    char *classes = NULL;


    extra = needMem(sizeof(struct pubsExtra));
    extra->label = pubsFeatureLabel(firstAuthor, year);
    if (isEmpty(title))
        extra->mouseOver = extra->label;
    else
        extra->mouseOver = cloneString(title);
    extra->color  = NULL;
    extra->shade  = -1;

    if (newFormat) 
        {
        impact  = row[3];
        classes = row[4];
        if (!isEmpty(impact)) 
            {
            char *colorBy = cartOptionalStringClosestToHome(cart, tg->tdb, FALSE, "pubsColorBy");
            if ((colorBy==NULL) || strcmp(colorBy,"topic")==0) 
                {
                char *classCopy = classes;
                char *mainClass = cloneNextWordByDelimiter(&classes, ',');
                classes = classCopy;
                if (mainClass!=NULL)
                    {
                    struct rgbColor *col = (struct rgbColor*) hashFindVal(pubsClassColors, mainClass);
                    extra->color = col;
                    // add class to mouseover text
                    struct dyString *mo = dyStringNew(0);
                    dyStringAppend(mo, extra->mouseOver);
                    dyStringAppend(mo, " (categories: ");
                    dyStringAppend(mo, classes);
                    dyStringAppend(mo, ")");
                    freeMem(extra->mouseOver);
                    extra->mouseOver = dyStringContents(mo);
                    }
                }
            else 
                {
                if (sameString(colorBy,"impact")) 
                    {
                    int impInt = atoi(impact);
                    if (impInt<=1)
                        extra->color = &impact1Color;
                    else if (impInt<=3)
                        extra->color = &impact2Color;
                    else if (impInt<=10)
                        extra->color = &impact3Color;
                    else
                        extra->color = &impact4Color;
                    
                    // add impact to mouseover text
                    struct dyString *mo = dyStringNew(0);
                    dyStringAppend(mo, extra->mouseOver);
                    dyStringAppend(mo, " (impact ");
                    dyStringAppend(mo, impact);
                    dyStringAppend(mo, ")");
                    freeMem(extra->mouseOver);
                    extra->mouseOver = dyStringContents(mo);
                    }

                if (sameString(colorBy,"year")) 
                    {
                    int relYear = (atoi(year)-1990); 
                    extra->shade = min(relYear/3, 10);
                    //extra->color = shadesOfGray[yearShade];
                    }
                }
            }
        }
    }


sqlFreeResult(&sr);
return extra;
}

static void pubsAddExtra(struct track* tg, struct linkedFeatures* lf)
/* add authorYear and title to linkedFeatures->extra */
{
char *articleTable = trackDbSettingClosestToHome(tg->tdb, "pubsArticleTable");
if(isEmpty(articleTable))
    return;
if (lf->extra != NULL) 
    return;

struct sqlConnection *conn = hAllocConn(database);
struct pubsExtra* extra = pubsMakeExtra(tg, articleTable, conn, lf);
lf->extra = extra;
hFreeConn(&conn);
}

static void sqlDyStringPrintfWithSep(struct dyString *ds, char* sep, char *format, ...)
/*  Printf to end of dyString. Prefix with sep if dyString is not empty. */
{
if (ds->stringSize!=0)
    dyStringAppend(ds, sep);
va_list args;
va_start(args, format);
sqlDyStringVaPrintfFrag(ds, format, args);
va_end(args);
}

struct hash* searchForKeywords(struct sqlConnection* conn, char *articleTable, char *keywords)
/* return hash with the articleIds that contain a given keyword in the abstract/title/authors */
{
if (isEmpty(keywords))
    return NULL;

char query[12000];
sqlSafef(query, sizeof(query), "SELECT articleId FROM %s WHERE "
"MATCH (citation, title, authors, abstract) AGAINST ('%s' IN BOOLEAN MODE)", articleTable, keywords);
//printf("query %s", query);
struct slName *artIds = sqlQuickList(conn, query);
if (artIds==NULL || slCount(artIds)==0)
    return NULL;

// convert list to hash
struct hash *hashA = hashNew(0);
struct slName *el;
for (el = artIds; el != NULL; el = el->next)
    hashAddInt(hashA, el->name, 1);
freeMem(keywords);
slFreeList(artIds);
return hashA;
}

static void pubsLoadKeywordYearItems(struct track *tg)
/* load items that fulfill keyword and year filter */
{
pubsParseClassColors();
struct sqlConnection *conn = hAllocConn(database);
char *keywords = cartOptionalStringClosestToHome(cart, tg->tdb, FALSE, "pubsFilterKeywords");
char *yearFilter = cartOptionalStringClosestToHome(cart, tg->tdb, FALSE, "pubsFilterYear");
char *publFilter = cartOptionalStringClosestToHome(cart, tg->tdb, FALSE, "pubsFilterPublisher");
char *articleTable = pubsArticleTable(tg);

if(sameOk(yearFilter, "anytime"))
    yearFilter = NULL;
if(sameOk(publFilter, "all"))
    publFilter = NULL;

if(isNotEmpty(keywords))
    keywords = makeMysqlMatchStr(keywords);

if (isEmpty(yearFilter) && isEmpty(keywords) && isEmpty(publFilter))
{
    loadGappedBed(tg);
}
else
    {
    // put together an "extra" query to hExtendedRangeQuery that removes articles
    // without the keywords specified in hgTrackUi
    char *oldLabel = tg->longLabel;
    tg->longLabel = catTwoStrings(oldLabel, " (filter activated)");
    freeMem(oldLabel);

    char **row;
    struct linkedFeatures *lfList = NULL;
    struct trackDb *tdb = tg->tdb;
    int scoreMin = atoi(trackDbSettingClosestToHomeOrDefault(tdb, "scoreMin", "0"));
    int scoreMax = atoi(trackDbSettingClosestToHomeOrDefault(tdb, "scoreMax", "1000"));
    boolean useItemRgb = bedItemRgb(tdb);

    char *extra = NULL;
    struct dyString *extraDy = dyStringNew(0);
    struct hash *articleIds = searchForKeywords(conn, articleTable, keywords);
    if (sqlColumnExists(conn, tg->table, "year"))
        // new table schema: filter fields are on main bed table
        {
        if (isNotEmpty(yearFilter))
            sqlDyStringPrintfWithSep(extraDy, " AND ", " year >= '%s'", yearFilter);
        if (isNotEmpty(publFilter))
            sqlDyStringPrintfWithSep(extraDy, " AND ", " publisher = '%s'", publFilter);
        }
    else
        // old table schema, filter by doing a join on article table
        {
        if(isNotEmpty(yearFilter))
            sqlDyStringPrintfFrag(extraDy, "name IN (SELECT articleId FROM %s WHERE year>='%s')", articleTable, \
                yearFilter);
        }


    if (extraDy->stringSize > 0)
        extra = extraDy->string;
    else
        extra = NULL;

    int rowOffset = 0;
    struct sqlResult *sr = hExtendedRangeQuery(conn, tg->table, chromName, winStart, winEnd, extra,
                                               FALSE, NULL, &rowOffset);
    freeDyString(&extraDy);

    while ((row = sqlNextRow(sr)) != NULL)
	{
        struct bed *bed = bedLoad12(row+rowOffset);
        if (articleIds==NULL || hashFindVal(articleIds, bed->name))
            slAddHead(&lfList, bedMungToLinkedFeatures(&bed, tdb, 12, scoreMin, scoreMax, useItemRgb));
        }
    sqlFreeResult(&sr);
    slReverse(&lfList);
    slSort(&lfList, linkedFeaturesCmp);
    tg->items = lfList;
    }
hFreeConn(&conn);
}

#define PUBSFILTERNAME "pubsFilterArticleId"

static void activatePslTrackIfCgi(struct track *tg)
/* the publications hgc creates links back to the browser with 
 * the cgi param pubsFilterArticleId to show only a single type
 * of feature for the pubsBlatPsl track. 
 * If the parameter was supplied, we save it here 
 * into the cart and activate the track.
 */
{
char *articleId = cgiOptionalString(PUBSFILTERNAME);
//if (articleId==NULL) 
    //articleId = cartOptionalString(cart, PUBSFILTERNAME);

if (articleId!=NULL) 
{
    cartSetString(cart, PUBSFILTERNAME, articleId);
    tdbSetCartVisibility(tg->tdb, cart, hCarefulTrackOpenVis(database, tg->track));
    tg->visibility=tvPack;
}
}

Color pubsItemColor(struct track *tg, void *item, struct hvGfx *hvg)
/* get color from extra field */
{
//pubsParseClassColors();
struct linkedFeatures *lf = item;
pubsAddExtra(tg, lf);

struct pubsExtra* extra = lf->extra;
if (extra==NULL || (extra->color==NULL && extra->shade==-1))
    return MG_BLACK;

if (extra->shade != -1)
    return shadesOfBlue[extra->shade];
else
    {
    //printf("got item color %d", extra->color->r);
    return hvGfxFindRgb(hvg, extra->color);
    }
}

static char *pubsItemName(struct track *tg, void *item)
/* get author/year from extra field */
{
struct linkedFeatures *lf = item;
pubsAddExtra(tg, lf);

struct pubsExtra* extra = lf->extra;
if (extra!=NULL)
    return extra->label;
else
    return lf->name;

}

static void pubsMapItem(struct track *tg, struct hvGfx *hvg, void *item,
                        char *itemName, char *mapItemName, int start, int end,
                        int x, int y, int width, int height)
/* create mouse over with title for pubs blat features. */
{
if (!theImgBox || tg->limitedVis != tvDense || !tdbIsCompositeChild(tg->tdb)) 
{
    struct linkedFeatures *lf = item;
    pubsAddExtra(tg, lf);
    struct pubsExtra* extra = lf->extra;
    char *mouseOver = NULL;
    if (extra != NULL) 
        mouseOver = extra->mouseOver;
    else
        mouseOver = itemName;

    mapBoxHc(hvg, start, end, x, y, width, height, tg->track, mapItemName, mouseOver); 
}
}

static char *pubsMarkerItemName(struct track *tg, void *item)
/* retrieve article count from score field and return.*/
{
struct bed *bed = item;
char newName[64];
safef(newName, sizeof(newName), "%d articles", (int) bed->score);
return cloneString(newName);
}

static void pubsMarkerMapItem(struct track *tg, struct hvGfx *hvg, void *item,
                              char *itemName, char *mapItemName, int start, int end,
                              int x, int y, int width, int height)
{
struct bed *bed = item;
genericMapItem(tg, hvg, item, bed->name, bed->name, start, end, x, y, width, height);
}

static struct hash* pubsLookupSequences(struct track *tg, struct sqlConnection* conn, char *articleId, bool getSnippet)
/* create a hash with a mapping annotId -> snippet or annotId -> shortSeq for an articleId*/
{
    struct dyString *dy = dyStringNew(LARGEBUF);
    char *sequenceTable = trackDbRequiredSetting(tg->tdb, "pubsSequenceTable");

    // work around sql injection fix problem, suggested by galt
    sqlDyStringPrintf(dy, "SELECT annotId, ");

     if (getSnippet)
        dyStringAppend(dy, "replace(replace(snippet, \"<B>\", \"\\n>>> \"), \"</B>\", \" <<<\\n\")" );
    else
        dyStringAppend(dy, "concat(substr(sequence,1,4),\"...\",substr(sequence,-4))" );
    dyStringPrintf(dy, " FROM %s WHERE articleId='%s' ", sequenceTable, articleId);
    // end sql injection fix

    struct hash *seqIdHash = sqlQuickHash(conn, dy->string);

    //freeMem(sequenceTable); // trackDbRequiredSetting returns a value in a hash, so do not free
    freeDyString(&dy);
    return seqIdHash;
}

static char *pubsArticleDispId(struct track *tg, struct sqlConnection *conn, char *articleId)
/* given an articleId, lookup author and year and create <author><year> label for it */
{
char *dispLabel = NULL;
char *articleTable = pubsArticleTable(tg);
char query[LARGEBUF];
sqlSafef(query, sizeof(query), "SELECT firstAuthor, year FROM %s WHERE articleId = '%s'", 
    articleTable, articleId);
struct sqlResult *sr = sqlGetResult(conn, query);
if (sr!=NULL)
    {
    char **row = NULL;
    row = sqlNextRow(sr);
    if (row != NULL)
        dispLabel = pubsFeatureLabel(row[0], row[1]);
    else
        dispLabel = articleId;
    }
else
    dispLabel = articleId;
sqlFreeResult(&sr);
return dispLabel;
}

static void pubsPslLoadItems(struct track *tg)
/* load only psl items from a single article */
{
// get articleId to filter on
char *articleId = cartOptionalString(cart, PUBSFILTERNAME);
if (articleId==NULL)
    return;

struct sqlConnection *conn = hAllocConn(database);
char *dispLabel = pubsArticleDispId(tg, conn, articleId);
struct hash *idToSnip = pubsLookupSequences(tg, conn, articleId, TRUE);
struct hash *idToSeq = pubsLookupSequences(tg, conn, articleId, FALSE);

// change track label 
char *oldLabel = tg->longLabel;
tg->longLabel = catTwoStrings("Individual matches for article ", dispLabel);
freeMem(oldLabel);

// filter and load items for this articleId
char where[256];
safef(where, sizeof(where), " articleId=%s ", articleId);

int rowOffset = 0;
struct sqlResult *sr = NULL;
sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd, where, &rowOffset);

struct linkedFeatures *lfList = NULL;
char **row = NULL;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct psl *psl = pslLoad(row+rowOffset);
    slAddHead(&lfList, lfFromPsl(psl, TRUE));
    char *shortSeq  = hashFindVal(idToSeq,  lfList->name);
    char *snip = hashFindVal(idToSnip, lfList->name);
    struct pubsExtra *extra = needMem(sizeof(struct pubsExtra));
    extra->mouseOver=snip;
    extra->label=shortSeq;
    lfList->extra = extra;
    }
sqlFreeResult(&sr);
slReverse(&lfList);
slSort(&lfList, linkedFeaturesCmp);
tg->items = lfList;
hFreeConn(&conn);
}

void pubsBlatPslMethods(struct track *tg)
/* a track that shows only the indiv matches for one single article */
{
activatePslTrackIfCgi(tg);
tg->loadItems = pubsPslLoadItems;
tg->itemName  = pubsItemName;
tg->mapItem   = pubsMapItem;
}

void pubsBlatMethods(struct track *tg)
/* publication blat tracks are bed12+2 tracks of sequences in text, mapped with BLAT */
{
//bedMethods(tg);
tg->loadItems = pubsLoadKeywordYearItems;
tg->itemName  = pubsItemName;
tg->itemColor = pubsItemColor;
tg->mapItem   = pubsMapItem;
}

void pubsMarkerMethods(struct track *tg)
/* publication marker tracks are bed5 tracks of genome marker occurences like rsXXXX found in text*/
{
tg->mapItem   = pubsMarkerMapItem;
tg->itemName  = pubsMarkerItemName;
}

