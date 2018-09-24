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
#include "suggest.h"
#include "genbank.h"


int main(int argc, char *argv[])
{
long enteredMainTime = clock1000();

cgiSpoof(&argc, argv);
char *prefix = cgiOptionalString("prefix");
char *database = cgiOptionalString("db");
int exact = cgiOptionalInt("exact", 0);
char query[2048];
char **row;
struct sqlResult *sr;
int count = 0;
boolean hasKnownCanonical;
struct dyString *str = newDyString(10000);
char *table;

pushWarnHandler(htmlVaBadRequestAbort);
pushAbortHandler(htmlVaBadRequestAbort);
if(prefix == NULL || database == NULL)
    errAbort("%s", "Missing prefix and/or db CGI parameter");

initGenbankTableNames(database);
struct sqlConnection *conn = hAllocConn(database);
table = connGeneSuggestTable(conn);
if(table == NULL)
    errAbort("gene autosuggest is not supported for db '%s'", database);
popWarnHandler();
popAbortHandler();

hasKnownCanonical = sameString(table, "knownCanonical");

puts("Content-Type:text/plain");
puts("\n");

dyStringPrintf(str, "[\n");

if(exact)
    {
    // NOTE that exact is no longer used by the UI as of v271, but there are still some robots using it so we still support it.
    if(hasKnownCanonical)
        sqlSafef(query, sizeof(query), "select x.geneSymbol, k.chrom, kg.txStart, kg.txEnd, x.kgID, x.description "
              "from knownCanonical k, knownGene kg, kgXref x where k.transcript = x.kgID and k.transcript = kg.name "
              "and x.geneSymbol = '%s' order by x.geneSymbol, k.chrom, kg.txEnd - kg.txStart desc", prefix);
    else
        sqlSafef(query, sizeof(query), "select r.name2, r.chrom, r.txStart, r.txEnd, r.name, d.name "
              "from %s r, %s g, %s d where r.name2 = '%s' and g.acc = r.name "
              "and g.description = d.id order by r.name2, r.chrom, r.txEnd - r.txStart desc", table, gbCdnaInfoTable, descriptionTable, prefix);
    }
else
    {
    // We use a LIKE query b/c it uses the geneSymbol index (substr queries do not use indices in mysql).
    // Also note that we take advantage of the fact that searches are case-insensitive in mysql.
    // Unfortunately, knownCanonical sometimes has multiple entries for a given gene (e.g. 2 TTn's in mm9 knownCanonical;
    // 3 POU5F1's in hg19); we return all of them (#5962).
    if(hasKnownCanonical)
        sqlSafef(query, sizeof(query), "select x.geneSymbol, k.chrom, kg.txStart, kg.txEnd, x.kgID, x.description "
              "from knownCanonical k, knownGene kg, kgXref x where k.transcript = x.kgID and k.transcript = kg.name "
              "and x.geneSymbol LIKE '%s%%' order by x.geneSymbol, k.chrom, kg.txStart", prefix);
    else
        sqlSafef(query, sizeof(query), "select r.name2, r.chrom, r.txStart, r.txEnd, r.name, d.name "
              "from %s r, %s g, %s d where r.name2 LIKE '%s%%' and g.acc = r.name "
              "and g.description = d.id order by r.name2, r.chrom, r.txStart", table, gbCdnaInfoTable, descriptionTable, prefix);
    }
sr = sqlGetResult(conn, query);
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

dyStringPrintf(str, "\n]\n");
puts(dyStringContents(str));
cgiExitTime("hgSuggest", enteredMainTime);
return 0;
}
