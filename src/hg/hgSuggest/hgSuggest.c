/* hgGeneSuggest - suggest a gene. */

#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "cheapcgi.h"
#include "dystring.h"
#include "suggest.h"

static char const rcsid[] = "$Id: hgSuggest.c,v 1.4 2010/03/04 19:53:28 larrym Exp $";

static void fail(char *msg)
{
puts("Status: 400\n\n");
puts(msg);
exit(-1);
}

int main(int argc, char *argv[])
{
char *prefix = sqlEscapeString(cgiOptionalString("prefix"));
char *database = sqlEscapeString(cgiOptionalString("db"));
int exact = cgiOptionalInt("exact", 0);
struct sqlConnection *conn;
char query[2048];
char **row;
struct sqlResult *sr;
int count = 0;
boolean hasKnownCanonical;
struct dyString *str = newDyString(10000);
char *table, previous[256];

if(prefix == NULL || database == NULL)
    fail("Missing prefix or database parameter");

conn = hAllocConn(database);
table = connGeneSuggestTable(conn);
if(table == NULL)
    fail("gene autosuggest is not supported for this assembly");

hasKnownCanonical = sameString(table, "knownCanonical");

puts("Content-Type:text/plain");
puts("\n");

dyStringPrintf(str, "[\n");

// We have to choose one isoform, so we choose the largest one; we order by chrom to make sure we choose normal chromosomes 
// over _random and _hap chromosomes (see redmine #4257).
if(exact)
    {
    if(hasKnownCanonical)
        safef(query, sizeof(query), "select x.geneSymbol, k.chrom, k2.txStart, k2.txEnd from knownCanonical k, knownGene k2, kgXref x where k.transcript = x.kgID and k.transcript = k2.name and x.geneSymbol = '%s' order by x.geneSymbol, k.chrom, k2.txEnd - k2.txStart desc", prefix);
    else
        safef(query, sizeof(query), "select r.name2, r.chrom, r.txStart, r.txEnd from %s r where r.name2 = '%s' order by r.name2, r.chrom, r.txEnd - r.txStart desc", table, prefix);
    }
else
    {
    // We use a LIKE query b/c it uses the geneSymbol index (substr queries do not use indices in mysql).
    // Also note that we take advantage of the fact that searches are case-insensitive in mysql.
    // Some tables have duplicates (e.g. 2 TTn's in mm9 knownCanonical); currently the larger one wins.
    if(hasKnownCanonical)
        safef(query, sizeof(query), "select x.geneSymbol, k.chrom, k2.txStart, k2.txEnd from knownCanonical k, knownGene k2, kgXref x where k.transcript = x.kgID and k.transcript = k2.name and x.geneSymbol LIKE '%s%%' order by x.geneSymbol, k.chrom, k2.txEnd - k2.txStart desc", prefix);
    else
        safef(query, sizeof(query), "select r.name2, r.chrom, r.txStart, r.txEnd from %s r where r.name2 LIKE '%s%%' order by r.name2, r.chrom, r.txEnd - r.txStart desc", table, prefix);
    }
sr = sqlGetResult(conn, query);
previous[0] = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    if(!previous[0] || !sameString(previous, row[0]))
        {
        count++;
        dyStringPrintf(str, "%s{\"value\": \"%s\", \"id\": \"%s:%d-%s\"}", count == 1 ? "" : ",\n",
                       row[0], row[1], atoi(row[2])+1, row[3]);
        safecpy(previous, sizeof(previous), row[0]);
        }
    }
dyStringPrintf(str, "\n]\n");
puts(dyStringContents(str));
return 0;
}
