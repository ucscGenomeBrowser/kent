/* hgData_maf - MAF files. */
#include "common.h"
#include "hgData.h"
#include "maf.h"
#include "hgMaf.h"

static char const rcsid[] = "$Id: hgData_maf.c,v 1.1.2.1 2009/03/07 06:04:27 mikep Exp $";

static struct json_object *jsonAddMafComp(struct json_object *o, struct mafComp *c)
{
struct json_object *m = json_object_new_object();
char strandStr[2];
char left[2];
char right[2];
strandStr[0] = c->strand;
left[0]      = c->leftStatus;
right[0]     = c->rightStatus;
strandStr[1] = left[1] = right[1] = '\0';
json_object_array_add(o, m);
json_object_object_add(m, "src", c->src ? json_object_new_string(c->src) : NULL);
json_object_object_add(m, "src_size", json_object_new_int(c->srcSize));
json_object_object_add(m, "strand", json_object_new_string(strandStr));
json_object_object_add(m, "start", json_object_new_int(c->start));
json_object_object_add(m, "size", json_object_new_int(c->size));
json_object_object_add(m, "text", c->text ? json_object_new_string(c->text) : NULL);
json_object_object_add(m, "quality", c->quality ? json_object_new_string(c->quality) : NULL);
json_object_object_add(m, "left_status", json_object_new_string(left));
json_object_object_add(m, "left_len", json_object_new_int(c->leftLen));
json_object_object_add(m, "right_status", json_object_new_string(right));
json_object_object_add(m, "right_len", json_object_new_int(c->rightLen));
return m;
}

static struct json_object *jsonAddMafRegDef(struct json_object *o, struct mafRegDef *r)
{
struct json_object *regDef = json_object_new_object();
if (r)
    {
    json_object_object_add(o, "reg_def", regDef);
    json_object_object_add(regDef, "type", r->type ? json_object_new_string(r->type) : NULL);
    json_object_object_add(regDef, "size", json_object_new_int(r->size));
    json_object_object_add(regDef, "id", r->id ? json_object_new_string(r->id) : NULL);
    }
return o;
}

static struct json_object *jsonAddMafComponents(struct json_object *o, struct mafComp *components)
{
struct mafComp *c;
struct json_object *comp = json_object_new_array();
json_object_object_add(o, "components", comp);
for (c = components ; c ; c = c->next)
    jsonAddMafComp(comp, c);
return o;
}

static struct json_object *jsonMafAli(struct mafAli *maf)
{
struct json_object *m = json_object_new_object();
if (maf)
    {
    json_object_object_add(m, "text_size", json_object_new_int(maf->textSize));
    json_object_object_add(m, "score", json_object_new_double(maf->score));
    jsonAddMafComponents(m, maf->components);
    jsonAddMafRegDef(m, maf->regDef);
    }
return m;
}

void printMaf(char *genome, char *track, char *chrom, int chromSize, int start, int end, char *strand)
// print maf records which intersect this start-end range
{
time_t modified = 0;
struct mafAli *maf;
struct json_object *m;
if (end <= start)
    errAbort("end before start!");
if (end > chromSize)
   errAbort("End past chromSize (%d > %d)", end, chromSize);
maf = hgMafFrag(genome, track, chrom, start, end, *strand, NULL, NULL);
m = jsonMafAli(maf);
mafAliFree(&maf);
okSendHeader(modified, TRACK_EXPIRES);
printf(json_object_to_json_string(m));
json_object_put(m);
}
