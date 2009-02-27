/* hgData_genome - functions dealing with genomes (genomes  and chromosomes). */
#include "common.h"
#include "hgData.h"
#include "hdb.h"
#include "chromInfo.h"
#include "trackDb.h"

static char const rcsid[] = "$Id: hgData_genome.c,v 1.1.2.8 2009/02/27 11:28:43 mikep Exp $";

//sqlDateToUnixTime(sqlTableUpdateTime(conn, "dbDb"));

struct json_object *addGenomeUrl(struct json_object *o, char *url_name, char *genome, char *chrom)
// Add a genome url to object o with name 'url_name'.
// Genome required if chrom specified, otherwise can be NULL
// Chrom can be NULL
// Returns object o
{
char url[1024];
if (genome && *genome)
  {
  if (chrom && *chrom)
      safef(url, sizeof(url), PREFIX GENOMES_CMD "/%s/%s", genome, chrom);
  else
      safef(url, sizeof(url), PREFIX GENOMES_CMD "/%s", genome);
  json_object_object_add(o, url_name, json_object_new_string(url));
  }
else
  json_object_object_add(o, url_name, json_object_new_string(PREFIX GENOMES_CMD));
return o;
}


static struct json_object *jsonAddOneGenome(struct json_object *o, struct dbDbClade *db)
// add a genome to object o
// return o
{
struct json_object *g = json_object_new_object();
char *defChrom;
int start=0, end=0;
hgParseChromRange(NULL, db->defaultPos, &defChrom, &start, &end);
if (!defChrom)
    defChrom = cloneString(db->defaultPos);
json_object_object_add(o, db->name, g);
json_object_object_add(g, "name", json_object_new_string(db->name));
json_object_object_add(g, "organism", json_object_new_string(db->organism));
json_object_object_add(g, "scientific_name", json_object_new_string(db->scientificName));
json_object_object_add(g, "ncbi_taxon_id", json_object_new_int(db->taxId));
json_object_object_add(g, "source_name", json_object_new_string(db->sourceName));
json_object_object_add(g, "description", json_object_new_string(db->description));
json_object_object_add(g, "genome", json_object_new_string(db->genome));
json_object_object_add(g, "clade", json_object_new_string(db->clade));
json_object_object_add(g, "default_chrom", json_object_new_string(defChrom));
json_object_object_add(g, "default_start", json_object_new_int(start));
json_object_object_add(g, "default_end", json_object_new_int(end));
addGenomeUrl(g, "url_self", db->name, NULL);
addTrackUrl(g, "url_track_list_genome", db->name, NULL);
freez(&defChrom);
return o;
}

static struct json_object *jsonAddGenomes(struct json_object *o, struct dbDbClade *db)
// add genomes to object o
// return object o
{
struct json_object *g = json_object_new_object();
json_object_object_add(o, "genomes", g);
for ( ; db ; db = db->next)
    jsonAddOneGenome(g, db);
return o;
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
    json_object_object_add(assembly, "id", json_object_new_string(db->name));
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

static struct json_object *jsonAddChroms(struct json_object *c, char *genome, struct chromInfo *ci)
{
struct json_object *a = json_object_new_array();
json_object_object_add(c, "chromosomes", a);
for ( ; ci ; ci = ci->next)
    json_object_array_add(a, jsonOneChrom(ci));
return c;
}

void printGenomes(char *genome, char *chrom, struct dbDbClade *db, struct chromInfo *ci, time_t modified)
// print an array of all genomes in list,
// print genome hierarchy for all genomes
// if only one genome in list, 
//   print array of chromosomes in ci list (or empty list if null)
// modified is latest update (unix) time of all relevant tables 
{
struct json_object *g = json_object_new_object();
addGenomeUrl(g, "url_self", genome, chrom);
addGenomeUrl(g, "url_genome_list", NULL, NULL);
if (genome && *genome)
    {
    addGenomeUrl(g, "url_genome_chroms", genome, NULL);
    addTrackUrl(g, "url_track_list_genome", genome, NULL);
    if (chrom && *chrom)
	addGenomeUrl(g, "url_genome_one_chrom", genome, chrom);
    }
jsonAddGenomes(g, db);
jsonAddHierarchy(g, db);
if (genome && *genome)
    jsonAddChroms(g, genome, ci);
okSendHeader(modified, GENOME_EXPIRES);
printf(json_object_to_json_string(g));
json_object_put(g);
}

