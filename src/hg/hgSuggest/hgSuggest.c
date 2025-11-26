/* hgGeneSuggest - suggest a gene. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "dystring.h"
#include "jsonParse.h"
#include "jsonWrite.h"
#include "suggest.h"
#include "genbank.h"
#include "hgFind.h"
#include "trix.h"

// Optional CGI param type can specify what kind of thing to suggest (default: gene)
#define ALT_OR_PATCH "altOrPatch"
#define HELP_DOCS "helpDocs"
#define TRACK "track"
#define HGVS "hgvs"

void suggestTrack(char *database,  char *prefix)
{
char query[4096];
sqlSafef(query, sizeof query,
    "select tableName, shortLabel from trackDb where shortLabel like '%%%s%%'", prefix);
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
struct jsonWrite *jw = jsonWriteNew();
jsonWriteListStart(jw, NULL);
while ((row = sqlNextRow(sr)) != NULL)
    {
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "label", row[1]);
    jsonWriteObjectEnd(jw);
    }
jsonWriteListEnd(jw);
puts(jw->dy->string);
jsonWriteFree(&jw);
}

void suggestGene(char *database, char *table, char *prefix)
/* Print out a Javascript list of objects describing genes that start with prefix. */
{
struct dyString *str = dyStringNew(10000);
dyStringPrintf(str, "[\n");

int exact = cgiOptionalInt("exact", 0);
boolean hasKnownCanonical = sameString(table, "knownCanonical");
initGenbankTableNames(database);
char query[2048];
if(exact)
    {
    // NOTE that exact is no longer used by the UI as of v271, but there are still some robots
    // using it so we still support it.
    if(hasKnownCanonical)
        sqlSafef(query, sizeof(query),
                 "select distinct x.geneSymbol, k.chrom, kg.txStart, kg.txEnd, x.kgID, x.description "
                 "from knownCanonical k, knownGene kg, kgXref x "
                 "where k.transcript = x.kgID and k.transcript = kg.name and x.geneSymbol = '%s' "
                 "order by x.geneSymbol, k.chrom, kg.txEnd - kg.txStart desc", prefix);
    else
        sqlSafef(query, sizeof(query),
                 "select distinct r.name2, r.chrom, r.txStart, r.txEnd, r.name, d.name "
                 "from %s r, %s g, %s d "
                 "where r.name2 = '%s' and g.acc = r.name and g.description = d.id "
                 "order by r.name2, r.chrom, r.txEnd - r.txStart desc",
                 table, gbCdnaInfoTable, descriptionTable, prefix);
    }
else
    {
    // We use a LIKE query b/c it uses the geneSymbol index (substr queries do not use indices in mysql).
    // Also note that we take advantage of the fact that searches are case-insensitive in mysql.
    // Unfortunately, knownCanonical sometimes has multiple entries for a given gene (e.g. 2 TTn's in mm9 knownCanonical;
    // 3 POU5F1's in hg19); we return all of them (#5962).
    if(hasKnownCanonical)
        sqlSafef(query, sizeof(query),
                 "select distinct x.geneSymbol, k.chrom, kg.txStart, kg.txEnd, x.kgID, x.description "
                 "from knownCanonical k, knownGene kg, kgXref x "
                 "where k.transcript = x.kgID and k.transcript = kg.name "
                 "and x.geneSymbol LIKE '%s%%' "
                 "order by x.geneSymbol, k.chrom, kg.txStart", prefix);
    else
        sqlSafef(query, sizeof(query), "select distinct r.name2, r.chrom, r.txStart, r.txEnd, r.name, d.name "
                 "from %s r, %s g, %s d "
                 "where r.name2 LIKE '%s%%' and g.acc = r.name and g.description = d.id "
                 "order by r.name2, r.chrom, r.txStart",
                 table, gbCdnaInfoTable, descriptionTable, prefix);
    }
char *knownDatabase = hdbDefaultKnownDb(database);
struct sqlConnection *conn = hAllocConn(knownDatabase);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
int count = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    // ignore funny chroms (e.g. _hap chroms. See redmine #4257.
    if(!strchr(row[1], '_'))
        {
        // We have some very long descriptions, e.g. 4277 chars for hg38 CLOCK, so truncate:
        const int maxDesc = 120;
        char *description = row[5];
        if (strlen(description) > maxDesc + 4)
            strcpy(description + maxDesc, "...");
        count++;
        dyStringPrintf(str, "%s{\"value\": \"%s (%s)\", "
                       "\"id\": \"%s:%d-%s\", "
                       "\"geneSymbol\": \"%s\", "
                       "\"internalId\": \"%s\"}",
                       count == 1 ? "" : ",\n", row[0], jsonStringEscape(description),
                       row[1], atoi(row[2])+1, row[3],
                       jsonStringEscape(row[0]),
                       jsonStringEscape(row[4]));
        }
    }
hFreeConn(&conn);
dyStringPrintf(str, "\n]\n");
puts(dyStringContents(str));
}

char *escapeAltFixTerm(char *term)
/* Special tweaks for SQL search of alt/fix terms that may include '.' and '_' characters. */
{
// If there is a ".", make it into a single-character wildcard so that "GL383518.1"
// can match "chr1_GL383518v1_alt".
char termCpy[strlen(term)+1];
safecpy(termCpy, sizeof termCpy, term);
subChar(termCpy, '.', '?');
// Escape '_' because that is an important character in alt/fix sequence names, and support
// wildcards:
return sqlLikeFromWild(termCpy);
}

struct slName *queryAltFixNames(struct sqlConnection *conn, char *table, char *term,
                                char *excludeSuffix, boolean prefixOnly)
/* If table exists, return names in table that match term, otherwise NULL.
 * Chop after ':' if there is one and exclude items that end with excludeSuffix so the
 * mappings between _alt and _fix sequences don't sneak into the wrong category's results. */
{
struct slName *names = NULL;
if (sqlTableExists(conn, table))
    {
    char query[2048];
    sqlSafef(query, sizeof query, "select distinct(substring_index(name, ':', 1)) from %s "
             "where name like '%s%s%%' "
             "and name not like '%%%s' and name not like '%%%s:%%'"
             "order by name",
             table, (prefixOnly ? "" : "%"), escapeAltFixTerm(term), excludeSuffix, excludeSuffix);
    names = sqlQuickList(conn, query);
    }
return names;
}

void writeAltFixMatches(struct jsonWrite *jw, struct slName *matches, char *category)
/* Append JSON objects containing alt or fix patch sequence names & optional category. */
{
struct slName *match;
for (match = matches; match != NULL; match = match->next)
    {
    if (strchr(match->name, '_'))
        {
        jsonWriteObjectStart(jw, NULL);
        jsonWriteString(jw, "value", match->name);
        if (isNotEmpty(category))
            jsonWriteString(jw, "category", category);
        jsonWriteObjectEnd(jw);
        }
    }
}

struct slPair *queryChromAlias(struct sqlConnection *conn, char *term, boolean prefixOnly)
/* Search chromAlias for prefix matches for term. */
{
struct slPair *matches = NULL;
if (sqlTableExists(conn, "chromAlias"))
    {
    char query[1024];
    sqlSafef(query, sizeof query, "select chrom, alias from chromAlias where alias like '%s%s%%' "
             "order by chrom", (prefixOnly ? "" : "%"), escapeAltFixTerm(term));
    matches = sqlQuickPairList(conn, query);
    }
return matches;
}

void writeValLabelMatches(struct jsonWrite *jw, struct slPair *matches, char *category)
/* Append JSON objects containing alt/fix seqs with extra label info & optional category. */
{
struct slPair *match;
for (match = matches; match != NULL; match = match->next)
    {
    char *seqName = match->name;
    if (strchr(seqName, '_') && !endsWith(seqName, "_random") && !startsWith("chrUn", seqName))
        {
        jsonWriteObjectStart(jw, NULL);
        jsonWriteString(jw, "value", seqName);
        char *extraInfo = match->val;
        if (isNotEmpty(extraInfo))
            {
            int len = strlen(seqName) + strlen(extraInfo) + 32;
            char label[len];
            safef(label, sizeof label, "%s (%s)", seqName, extraInfo);
            jsonWriteString(jw, "label", label);
            }
        if (isNotEmpty(category))
            jsonWriteString(jw, "category", category);
        jsonWriteObjectEnd(jw);
        }
    }
}

void suggestAltOrPatch(char *database, char *term)
/* Print out a Javascript list of objects describing alternate haplotype or fix patch sequences
 * from database that match term. */
{
struct jsonWrite *jw = jsonWriteNew();
jsonWriteListStart(jw, NULL);
struct sqlConnection *conn = hAllocConn(database);
// First, search for prefix matches
struct slName *fixMatches = queryAltFixNames(conn, "fixLocations", term, "alt", TRUE);
struct slName *altMatches = queryAltFixNames(conn, "altLocations", term, "fix", TRUE);
// Add category labels only if we get both types of matches.
writeAltFixMatches(jw, fixMatches, altMatches ? "Fix Patches" : "");
writeAltFixMatches(jw, altMatches, fixMatches ? "Alt Patches" : "");
// If there are no prefix matches, look for partial matches
if (fixMatches == NULL && altMatches == NULL)
    {
    fixMatches = queryAltFixNames(conn, "fixLocations", term, "alt", FALSE);
    altMatches = queryAltFixNames(conn, "altLocations", term, "fix", FALSE);
    writeAltFixMatches(jw, fixMatches, altMatches ? "Fix Patches" : "");
    writeAltFixMatches(jw, altMatches, fixMatches ? "Alt Patches" : "");
    }
// If there are still no matches, try chromAlias.
if (fixMatches == NULL && altMatches == NULL)
    {
    struct slPair *aliasMatches = queryChromAlias(conn, term, TRUE);
    writeValLabelMatches(jw, aliasMatches, "");
    if (aliasMatches == NULL)
        {
        struct slPair *aliasMatches = queryChromAlias(conn, term, FALSE);
        writeValLabelMatches(jw, aliasMatches, "");
        }
    }
hFreeConn(&conn);
jsonWriteListEnd(jw);
puts(jw->dy->string);
jsonWriteFree(&jw);
}

void suggestHelpPage(char *prefix)
/* Print out a Javascript list of objects describing help pages that start with prefix. */
{
struct jsonWrite *jw = jsonWriteNew();
jsonWriteListStart(jw, NULL);
char *lowered = cloneString(prefix);
char *keyWords[16];
int keyCount;
tolowers(lowered);
keyCount = chopLine(lowered, keyWords);
struct trix *helpTrix = openStaticTrix(helpDocsTrix);
struct trixSearchResult *tsr, *tsrList = trixSearch(helpTrix, keyCount, keyWords, tsmExpand);
int i;
for (i = 0, tsr = tsrList; i < 25 && tsr != NULL; i++, tsr = tsr->next)
    {
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "value", tsr->itemId); // value is necessary
    jsonWriteString(jw, "category", "helpDocs"); // hardcode this for client
    jsonWriteObjectEnd(jw);
    }
jsonWriteListEnd(jw);
puts(jw->dy->string);
jsonWriteFree(&jw);
}

void suggestHGVS(char *database, char *prefix)
/* Print out a list of javascript objects that define positions matching the HGVS term prefix */
{
struct hgPositions *hgp = NULL;
AllocVar(hgp);
boolean match = matchesHgvs(NULL, database, prefix, hgp, FALSE);
if (match)
    {
fprintf(stderr, "suggesting hgvs for db: '%s' and prefix '%s'\n", database, prefix);
    fixSinglePos(hgp);
    struct jsonWrite *jw = jsonWriteNew();
    jsonWriteListStart(jw,NULL);
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "type", HGVS);
    jsonWriteString(jw, "value", hgp->singlePos->description);
    jsonWriteString(jw, "id", hgp->singlePos->description);
    jsonWriteString(jw, "chrom", hgp->singlePos->chrom);
    jsonWriteNumber(jw, "start", hgp->singlePos->chromStart);
    jsonWriteNumber(jw, "end", hgp->singlePos->chromEnd);
    jsonWriteObjectEnd(jw);
    jsonWriteListEnd(jw);
    puts(jw->dy->string);
    jsonWriteFree(&jw);
    }
}

char *checkParams(char *database, char *prefix, char *type)
/* If we don't have valid CGI parameters, quit with a Bad Request HTTP response. */
{
pushWarnHandler(htmlVaBadRequestAbort);
pushAbortHandler(htmlVaBadRequestAbort);
if(prefix == NULL || database == NULL)
    errAbort("%s", "Missing prefix and/or db CGI parameter");
if (! hDbIsActive(database))
    errAbort("'%s' is not a valid, active database", htmlEncode(database));
if (isNotEmpty(type) && differentString(type, ALT_OR_PATCH) && differentString(type, HELP_DOCS) && differentString(type, TRACK) && differentString(type, HGVS))
    errAbort("'%s' is not a valid type", type);
char *table = NULL;
if (! sameOk(type, ALT_OR_PATCH) && !sameOk(type, HELP_DOCS) && !sameOk(type, HGVS))
    {
    char *knownDatabase = hdbDefaultKnownDb(database);
    struct sqlConnection *conn = hAllocConn(knownDatabase);
    table = connGeneSuggestTable(conn);
    hFreeConn(&conn);
    if(table == NULL)
        errAbort("gene autosuggest is not supported for db '%s'", database);
    }
popWarnHandler();
popAbortHandler();
return table;
}

int main(int argc, char *argv[])
{
long enteredMainTime = clock1000();

cgiSpoof(&argc, argv);
char *database = cgiOptionalString("db");
char *prefix = cgiOptionalString("prefix");
char *type = cgiOptionalString("type");
char *table = checkParams(database, prefix, type);

puts("Content-Type:text/plain");
puts("\n");

if (sameOk(type, ALT_OR_PATCH))
    suggestAltOrPatch(database, prefix);
else if (sameOk(type, HELP_DOCS))
    suggestHelpPage(prefix);
else if (sameOk(type, TRACK))
    suggestTrack(database, prefix);
else if (sameOk(type, HGVS))
    suggestHGVS(database, prefix);
else
    suggestGene(database, table, prefix);

cgiExitTime("hgSuggest", enteredMainTime);
return 0;
}
