/* hgGeneSuggest - suggest a gene. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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

// Optional CGI param type can specify what kind of thing to suggest (default: gene)
#define ALT_OR_PATCH "altOrPatch"

void suggestGene(char *database, char *table, char *prefix)
/* Print out a Javascript list of objects describing genes that start with prefix. */
{
struct dyString *str = newDyString(10000);
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
                 "select x.geneSymbol, k.chrom, kg.txStart, kg.txEnd, x.kgID, x.description "
                 "from knownCanonical k, knownGene kg, kgXref x "
                 "where k.transcript = x.kgID and k.transcript = kg.name and x.geneSymbol = '%s' "
                 "order by x.geneSymbol, k.chrom, kg.txEnd - kg.txStart desc", prefix);
    else
        sqlSafef(query, sizeof(query),
                 "select r.name2, r.chrom, r.txStart, r.txEnd, r.name, d.name "
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
                 "select x.geneSymbol, k.chrom, kg.txStart, kg.txEnd, x.kgID, x.description "
                 "from knownCanonical k, knownGene kg, kgXref x "
                 "where k.transcript = x.kgID and k.transcript = kg.name "
                 "and x.geneSymbol LIKE '%s%%' "
                 "order by x.geneSymbol, k.chrom, kg.txStart", prefix);
    else
        sqlSafef(query, sizeof(query), "select r.name2, r.chrom, r.txStart, r.txEnd, r.name, d.name "
                 "from %s r, %s g, %s d "
                 "where r.name2 LIKE '%s%%' and g.acc = r.name and g.description = d.id "
                 "order by r.name2, r.chrom, r.txStart",
                 table, gbCdnaInfoTable, descriptionTable, prefix);
    }
struct sqlConnection *conn = hAllocConn(database);
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

struct slName *queryQNames(struct sqlConnection *conn, char *table, char *term, boolean prefixOnly)
/* If table exists, return qNames in table that match term, otherwise NULL.  Exclude items whose
 * tName contains '_' so the mappings between _alt and _fix sequences don't sneak into the wrong
 * category's results. */
{
struct slName *names = NULL;
if (sqlTableExists(conn, table))
    {
    // If there is a ".", make it into a single-character wildcard so that "GL383518.1"
    // can match "chr1_GL383518v1_alt".
    char termCpy[strlen(term)+1];
    safecpy(termCpy, sizeof termCpy, term);
    subChar(termCpy, '.', '?');
    // Escape '_' because that is an important character in alt/fix sequence names, and support
    // wildcards:
    char *escapedTerm = sqlLikeFromWild(termCpy);
    char query[2048];
    sqlSafef(query, sizeof query, "select distinct(qName) from %s where qName like '%s%s%%' "
             "and tName not rlike '.*_.*' order by qName",
             table, (prefixOnly ? "" : "%"), escapedTerm);
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
             "order by chrom", (prefixOnly ? "" : "%"), term);
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
struct slName *fixMatches = queryQNames(conn, "fixSeqLiftOverPsl", term, TRUE);
struct slName *altMatches = queryQNames(conn, "altSeqLiftOverPsl", term, TRUE);
// Add category labels only if we get both types of matches.
writeAltFixMatches(jw, fixMatches, altMatches ? "Fix Patches" : "");
writeAltFixMatches(jw, altMatches, fixMatches ? "Alt Patches" : "");
// If there are no prefix matches, look for partial matches
if (fixMatches == NULL && altMatches == NULL)
    {
    fixMatches = queryQNames(conn, "fixSeqLiftOverPsl", term, FALSE);
    altMatches = queryQNames(conn, "altSeqLiftOverPsl", term, FALSE);
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

char *checkParams(char *database, char *prefix, char *type)
/* If we don't have valid CGI parameters, quit with a Bad Request HTTP response. */
{
pushWarnHandler(htmlVaBadRequestAbort);
pushAbortHandler(htmlVaBadRequestAbort);
if(prefix == NULL || database == NULL)
    errAbort("%s", "Missing prefix and/or db CGI parameter");
if (! hDbIsActive(database))
    errAbort("'%s' is not a valid, active database", htmlEncode(database));
if (isNotEmpty(type) && differentString(type, ALT_OR_PATCH))
    errAbort("'%s' is not a valid type", type);
char *table = NULL;
if (! sameOk(type, ALT_OR_PATCH))
    {
    struct sqlConnection *conn = hAllocConn(database);
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
else
    suggestGene(database, table, prefix);

cgiExitTime("hgSuggest", enteredMainTime);
return 0;
}
