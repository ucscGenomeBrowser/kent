/* hgGeneSuggest - suggest a gene. */

#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "cheapcgi.h"
#include "dystring.h"
#include "suggest.h"

static void fail(char *msg)
{
puts("Status: 400\n\n");
puts(msg);
exit(-1);
}

int main(int argc, char *argv[])
{
long enteredMainTime = clock1000();

char *prefix = cgiOptionalString("prefix");
char *database = cgiOptionalString("db");

int exact = cgiOptionalInt("exact", 0);
struct sqlConnection *conn;
char query[2048];
char **row;
struct sqlResult *sr;
int count = 0;
boolean hasKnownCanonical;
struct dyString *str = newDyString(10000);
char *table;

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

if(exact)
    {
    // NOTE that exact is no longer used by the UI as of v271, but there are still some robots using it so we still support it.
    if(hasKnownCanonical)
        sqlSafef(query, sizeof(query), "select x.geneSymbol, k.chrom, kg.txStart, kg.txEnd, x.kgID, x.description "
              "from knownCanonical k, knownGene kg, kgXref x where k.transcript = x.kgID and k.transcript = kg.name "
              "and x.geneSymbol = '%s' order by x.geneSymbol, k.chrom, kg.txEnd - kg.txStart desc", prefix);
    else
        sqlSafef(query, sizeof(query), "select r.name2, r.chrom, r.txStart, r.txEnd, r.name, description.name "
              "from %s r, gbCdnaInfo, description where r.name2 = '%s' and gbCdnaInfo.acc = r.name "
              "and gbCdnaInfo.description = description.id order by r.name2, r.chrom, r.txEnd - r.txStart desc", table, prefix);
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
        sqlSafef(query, sizeof(query), "select r.name2, r.chrom, r.txStart, r.txEnd, r.name, description.name "
              "from %s r, gbCdnaInfo, description where r.name2 LIKE '%s%%' and gbCdnaInfo.acc = r.name "
              "and gbCdnaInfo.description = description.id order by r.name2, r.chrom, r.txStart", table, prefix);
    }
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    // ignore funny chroms (e.g. _hap chroms. See redmine #4257.
    if(!strchr(row[1], '_'))
        {
        count++;
        dyStringPrintf(str, "%s{\"value\": \"%s (%s)\", \"id\": \"%s:%d-%s\", \"internalId\": \"%s\"}", count == 1 ? "" : ",\n",
                       row[0], javaScriptLiteralEncode(row[5]), row[1], atoi(row[2])+1, row[3], javaScriptLiteralEncode(row[4]));
        }
    }

dyStringPrintf(str, "\n]\n");
puts(dyStringContents(str));
cgiExitTime("hgSuggest", enteredMainTime);
return 0;
}
