/* hgData - simple RESTful interface to genome data. */
#include "common.h"
#include "hgData.h"
// #include "dystring.h"
// #include "bedGraph.h"
// #include "bed.h"

#include "cart.h"
#include "hgFind.h"
#include "hgFindSpec.h"

static char const rcsid[] = "$Id: hgData_search.c,v 1.1.2.1 2009/03/03 07:42:33 mikep Exp $";
struct cart *cart; /* The cart where we keep persistent variables. */

struct json_object *addSearchGenomeUrl(struct json_object *o, char *url_name, char *genome, char *term)
// Add a search url to object o with name 'url_name'.
// Genome can be NULL
// Item can be NULL
// Returns object o
{
char url[1024];
safef(url, sizeof(url), PREFIX SEARCH_CMD "/%s/%s", 
    (genome && *genome) ? genome : GENOME_VAR,
    (term && *term) ? term : TERM_VAR);
json_object_object_add(o, url_name, json_object_new_string(url));
return o;
}

struct json_object *addSearchGenomeTrackUrl(struct json_object *o, char *url_name, char *genome, char *track, char *term)
// Add a search url to object o with name 'url_name'.
// Genome must be supplied if track is supplied, otherwise can be NULL
// Track can be NULL
// Item can be NULL
// Returns object o
{
char url[1024];
safef(url, sizeof(url), PREFIX SEARCH_CMD "/%s/%s/%s", 
    (genome && *genome) ? genome : GENOME_VAR,
    (track && *track) ? track : TRACK_VAR,
    (term && *term) ? term : TERM_VAR);
json_object_object_add(o, url_name, json_object_new_string(url));
return o;
}

static struct json_object *jsonHgPos(struct hgPos *pos, char *genome, char *track)
{
struct json_object *a = json_object_new_array();
struct json_object *o;
struct hgPos *p;
char *desc;
if (pos)
    for (p = pos ; p ; p = p->next)
	{
	o = json_object_new_object();
	json_object_array_add(a, o);
	if (p->chrom)
	    {
	    json_object_object_add(o, "chrom", json_object_new_string(p->chrom));
	    json_object_object_add(o, "start", json_object_new_int(p->chromStart));
	    json_object_object_add(o, "end", json_object_new_int(p->chromEnd));
	    addRangeUrl(o, "url_range", "", track, genome, p->chrom, p->chromStart, p->chromEnd);
	    addRangeUrl(o, "url_range_annoj_nested", ANNOJ_NESTED_FMT, track, genome, p->chrom, p->chromStart, p->chromEnd);
	    addRangeUrl(o, "url_range_annoj_flat", ANNOJ_FLAT_FMT, track, genome, p->chrom, p->chromStart, p->chromEnd);
	    addCountUrl(o, "url_count", track, genome, p->chrom, p->chromStart, p->chromEnd);
	    }
	else // if we have no chrom location, the returned URL is a second search by item  key
	    {
	    if (p->chromStart > 0 || p->chromEnd > 0)
		errAbort("Error: chromStart (%d) and/or chromEnd (%d) specified without chrom\n", p->chromStart, p->chromEnd);
	    if (p->browserName == NULL)
		errAbort("Error: no chrom position and no item name returned from search\n");
	    addSearchGenomeUrl(o, "url_search_item_genome_track", genome, p->browserName);
	    }
	json_object_object_add(o, "name", p->name ? json_object_new_string(p->name) : NULL);

	chopbywhyate 12
	//url could be PSL file 
	// Strip off URL from description
	if ( (desc = rStringIn("</A> - ", p->description)) )
	    desc += strlen("</A> - ");
	else
	    desc = p->description;
	json_object_object_add(o, "description", desc ? json_object_new_string(desc) : NULL);
	json_object_object_add(o, "id", p->browserName ? json_object_new_string(p->browserName) : NULL);
	}
return a;
}

static struct json_object *jsonHgPosTable(struct hgPosTable *pos, char *genome, char *track)
{
struct json_object *a = json_object_new_array();
struct json_object *o;
struct hgPosTable *p;
for (p = pos ; p ; p = p->next)
    {
    o = json_object_new_object();
    json_object_array_add(a, o);
    json_object_object_add(o, "track", p->name ? json_object_new_string(p->name) : NULL);
    json_object_object_add(o, "description", p->description ? json_object_new_string(p->description) : NULL);
    json_object_object_add(o, "hits", jsonHgPos(p->posList, genome, track));
    //json_object_object_add(o, "hits", jsonHgPos(p->posList, genome, p->name));
    }
return a;
}

static struct json_object *jsonFindPos(struct hgPositions *p, char *genome, char *track, char *query)
{
struct json_object *o = json_object_new_object();
json_object_object_add(o, "genome", json_object_new_string(genome));
json_object_object_add(o, "track", *track ? json_object_new_string(track) : NULL);
json_object_object_add(o, "query", json_object_new_string(query));
json_object_object_add(o, "result_count", json_object_new_int(p->posCount));
addSearchGenomeUrl(o, "url_search_item_genome", genome, query);
addSearchGenomeTrackUrl(o, "url_search_item_genome_track", genome, track, query);
json_object_object_add(o, "single_hit", jsonHgPos(p->singlePos, genome, track));
json_object_object_add(o, "tracks", jsonHgPosTable(p->tableList, genome, track));
return o;
}

void searchTracks(time_t modified, struct hgPositions *hgp, char *genome, char *track, char *query)
// search for data within tracks or whole genomes
{
struct json_object *o;
if (hgp && hgp->next)
    errAbort("Multiple hgPositions objects (found %d, expected only one)\n", slCount(hgp));
o = jsonFindPos(hgp, genome, track, query);
okSendHeader(modified, SEARCH_EXPIRES);
printf(json_object_to_json_string(o));
json_object_put(o);
}

