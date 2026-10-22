/* cartContentsValTester - check that cartContentsVal finds one variable in a stored cart
 * contents string, and returns NULL when the variable is not there.
 *
 * The session lists in hgSession and hgPublicSessions each had their own copy of this parse,
 * and each added 4 to the result of strstr(contents, "&db=") before testing it for NULL.  A
 * session saved from an empty cart has no db=, so the copy read past the end of the string and
 * the CGI segfaulted.  They call cartContentsVal now.
 *
 * refs #38456 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "cart.h"

static void check(char *contents, char *var)
/* Print what cartContentsVal finds for var in contents. */
{
char *label = (contents == NULL) ? "NULL" : ((*contents == 0) ? "(empty string)" : contents);
char *val = cartContentsVal(contents, var);
if (val == NULL)
    printf("  %-44s %-8s -> NULL\n", label, var);
else
    printf("  %-44s %-8s -> \"%s\"\n", label, var, val);
freeMem(val);
}

int main(int argc, char *argv[])
{
printf("the variable first, in the middle and last\n");
check("db=hg38&position=chr1:1-1000", "db");
check("hgsid=1&db=hg38&position=chr1:1-1000", "db");
check("hgsid=1&position=chr1:1-1000&db=hg38", "db");
check("db=hg38", "db");

printf("\nno such variable\n");
check("hgsid=1&position=chr1:1-1000", "db");
check("", "db");
check(NULL, "db");

printf("\na longer name that ends in the one asked for is a different variable\n");
check("hubdb=hg19&position=chr1:1-1000", "db");
check("hubdb=hg19&db=hg38", "db");
check("mydb=hg19", "db");

printf("\nan encoded & or = inside a value does not start a variable\n");
check("position=x%26db%3Dhg19&db=hg38", "db");
check("position=x%26db%3Dhg19", "db");

printf("\nthe value is CGI-decoded\n");
check("db=hub_1_GCA%5F000001405.15", "db");
check("db=%3Cb%3E", "db");

printf("\nan empty value is an empty string, not NULL\n");
check("db=&position=chr1:1-1000", "db");
check("position=chr1:1-1000&db=", "db");
return 0;
}
