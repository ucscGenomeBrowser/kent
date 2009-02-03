/* hgData_genome - functions dealing with genomes (genomes  and chromosomes). */
#include "common.h"
#include "hgData.h"
#include "hdb.h"
#include "chromInfo.h"
#include "trackDb.h"

static char const rcsid[] = "$Id: hgData_genome.c,v 1.1.2.3 2009/02/03 05:19:11 mikep Exp $";


static struct json_object *jsonOneGenome(struct dbDbClade *db)
{
struct json_object *res = json_object_new_object();
struct json_object *g = json_object_new_object();
char *chrom;
int start=0, end=0;
hgParseChromRange(NULL, db->defaultPos, &chrom, &start, &end);
if (!chrom)
    chrom = cloneString(db->defaultPos);
json_object_object_add(res, db->name, g);
json_object_object_add(g, "name", json_object_new_string(db->name));
json_object_object_add(g, "organism", json_object_new_string(db->organism));
json_object_object_add(g, "scientific_name", json_object_new_string(db->scientificName));
json_object_object_add(g, "ncbi_taxon_id", json_object_new_int(db->taxId));
json_object_object_add(g, "source_name", json_object_new_string(db->sourceName));
json_object_object_add(g, "description", json_object_new_string(db->description));
json_object_object_add(g, "genome", json_object_new_string(db->genome));
json_object_object_add(g, "clade", json_object_new_string(db->clade));
json_object_object_add(g, "default_chrom", json_object_new_string(chrom));
json_object_object_add(g, "default_start", json_object_new_int(start));
json_object_object_add(g, "default_end", json_object_new_int(end));
freez(&chrom);
return res;
}

static struct json_object *jsonAddGenomes(struct json_object *g, struct dbDbClade *db)
{
struct json_object *a = json_object_new_array();
json_object_object_add(g, "genomes", a);
for ( ; db ; db = db->next)
    json_object_array_add(a, jsonOneGenome(db));
return g;
}

static struct json_object *jsonAddHierarchy(struct json_object *g, struct dbDbClade *db)
// add a hierarchy of clades deduced from the flat list 
// hierarchy : [ clade => [ genome => [ description: assembly]]]
{
struct json_object *hArr = json_object_new_array();
struct json_object *cladeArr = NULL, *genomeArr = NULL;
struct dbDbClade *prev = NULL;
json_object_object_add(g, "genome_hierarchy", hArr);
for ( ; db ; db = db->next)
    {
    if (differentStringNullOk(db->clade, prev ? prev->clade : NULL))
	{
	cladeArr = json_object_new_array();
	struct json_object *clade = json_object_new_object();
	json_object_object_add(clade, db->clade, cladeArr);
	json_object_array_add(hArr, clade);
	}
    if (differentStringNullOk(db->genome, prev ? prev->genome : NULL))
	{
	genomeArr = json_object_new_array();
	struct json_object *genome = json_object_new_object();
	json_object_object_add(genome, db->genome, genomeArr);
	json_object_array_add(cladeArr, genome);
	}
    struct json_object *assembly = json_object_new_object();
    json_object_object_add(assembly, db->description, json_object_new_string(db->name));
    json_object_array_add(genomeArr, assembly);
    prev = db;
    }
return g;
}

static struct json_object *jsonOneChrom(struct chromInfo *ci)
{
struct json_object *c = json_object_new_object();
json_object_object_add(c, ci->chrom, json_object_new_int(ci->size));
return c;
}

static struct json_object *jsonAddChroms(struct json_object *c, struct chromInfo *ci)
{
struct json_object *a = json_object_new_array();
json_object_object_add(c, "chromosomes", a);
for ( ; ci ; ci = ci->next)
    json_object_array_add(a, jsonOneChrom(ci));
return c;
}

void printGenomes(struct dbDbClade *db, struct chromInfo *ci)
// print an array of all genomes in list,
// print genome hierarchy for all genomes
// if ci is not null, print array of chromosomes in ci list 
{
struct json_object *g = jsonAddGenomes(json_object_new_object(), db);
jsonAddHierarchy(g, db);
jsonAddChroms(g, ci);
printf(json_object_to_json_string(g));
json_object_put(g);
}

