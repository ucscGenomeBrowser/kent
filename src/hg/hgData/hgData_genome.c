/* hgData_genome - functions dealing with genomes (databases and chromosomes). */
#include "common.h"
#include "hgData.h"
#include "hdb.h"
#include "chromInfo.h"
#include "trackDb.h"

static char const rcsid[] = "$Id: hgData_genome.c,v 1.1.2.1 2009/01/13 15:48:54 mikep Exp $";


void printDb(struct dbDbClade *db)
// print information for one database
{
printf("{\"name\": %s,", quote(db->name));
printf("\"description\": %s,",  quote(db->description));
printf("\"organism\": %s,",  quote(db->organism));
printf("\"genome\": %s,",  quote(db->genome));
printf("\"scientificName\": %s,",  quote(db->scientificName));
printf("\"sourceName\": %s,",  quote(db->sourceName));
printf("\"clade\": %s,",  quote(db->clade));
printf("\"defaultPos\": %s,",  quote(db->defaultPos));
printf("\"orderKey\": %d,",  db->orderKey);
printf("\"priority\": %f}", db->priority);
}

void printDbs(struct dbDbClade *db)
// print an array of all databases
{
struct dbDbClade *t;
printf("\"databases\": [\n");
for (t = db ; t ; t = t->next)
    {
    printDb(t);
    printf("%c\n", t->next ? ',' : ' ');
    }
printf("]\n");
}

void printChrom(struct chromInfo *ci)
// print a chromosome 
{
printf("{\"id\":%s,\"size\":%u}", quote(ci->chrom), ci->size);
}

void printChroms(struct chromInfo *ci)
// print an array of all chromosomes
{
struct chromInfo *t;
printf("[");
for (t = ci ; t ; t = t->next)
    {
    printChrom(t);
    printf("%c\n", (t->next ? ',' : ' '));
    }
printf("]");
}

void printGenomeAsAnnoj(struct dbDbClade *db, struct chromInfo *ci)
// print information for a genome - the database and all its chromosomes
// using AnnoJ format (http://www.annoj.org)
{
char name[1024];
char desc[1024];

snprintf(name, sizeof(name), "%s (%s)", db->name, db->organism);
snprintf(desc, sizeof(desc), "%s (%s)", db->scientificName, db->sourceName);

printf("{\"success\": true,\n"
"\"data\": {"
"\"institution\": {\"name\":\"UCSC\",\"url\":\"http:\\/\\/genome.ucsc.edu\",\"logo\":\"\\/images\\/title.jpg\"},\n"
" \"engineer\":{\"name\":\"Mike Pheasant\",\"email\":\"mikep@soe.ucsc.edu\"},\n"
" \"service\":{\"title\":\"%s\",\"version\":\"%s\",\"description\":\"%s\"},\n"
" \"genome\":{\"species\":\"%s\",\"access\":\"public\",\"version\":\"%s\",\"description\":\"%s\",\n"
"  \"assemblies\":", 
    name, db->sourceName, desc, name, db->sourceName, desc);
printChroms(ci);
printf("\n  }\n }\n}\n");
}

void printGenome(struct dbDbClade *db, struct chromInfo *ci)
// print information for a genome - the database and all its chromosomes
{
printf("\"genome\" : ");
printDb(db);
printf(",\n");
printf("\"chroms\" : ");
printChroms(ci);
printf("\n");
}

