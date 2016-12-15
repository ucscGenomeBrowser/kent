/* cdwFixFrazer3 - Fix up frazer metadata tree some more.  See code for details.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tagStorm.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwFixFrazer3 - Fix up frazer metadata tree some more: moving solo meta stanzas up and\n"
  "adding a sample_label tag\n"
  "usage:\n"
  "   cdwFixFrazer3 in.tags out.tags\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

boolean doFixMeta(struct tagStorm *ts, struct tagStanza *stanza)
/* Do fix adding sampleLabel */
{
int tagCount = slCount(stanza->tagList);
if (tagCount == 1 && stanza->children == NULL)  
    {
    char *meta = tagFindLocalVal(stanza, "meta");
    if (meta != NULL)
        {
	struct tagStanza *parent = stanza->parent;
	tagStanzaAppend(ts, parent, "meta", meta);
	slRemoveEl(&parent->children, stanza);
	return TRUE;
	}
    }
return FALSE;
}

static void rFixMeta(struct tagStorm *ts, struct tagStanza *list, int *pCount)
/* Recursively call doFixMeta on all stanzas. */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    if (doFixMeta(ts, stanza))
       ++(*pCount);
    rFixMeta(ts, stanza->children, pCount);
    }
}

struct hash *uuidHash = NULL;
int maxUuidInt = 0;

char *uuidToInt(char *uuid)
/* Return sequential numbers for different uuids. */
{
if (uuidHash == NULL)
    uuidHash = newHash(0);
char *shortId = hashFindVal(uuidHash, uuid);
if (shortId == NULL)
    {
    char buf[10];
    maxUuidInt += 1;
    safef(buf, sizeof(buf), "d%d", maxUuidInt);
    shortId = cloneString(buf);
    hashAdd(uuidHash, uuid, shortId);
    }
return shortId;
}

boolean doFixLabel(struct tagStorm *ts, struct tagStanza *stanza)
/* Do fix adding sampleLabel */
{
char *cell_type = tagFindLocalVal(stanza, "cell_type");
if (cell_type != NULL)
    {
    char label[256];
    char *assay = naForNull(tagFindVal(stanza, "assay"));
    if (startsWith("broad", assay))
        assay = "H3K27Ac";
    char *donor = naForNull(tagFindVal(stanza, "biosample_source_id"));
    if (strchr(donor, '-') != NULL)
         donor = uuidToInt(donor);
    safef(label, sizeof(label), "%s %s %s", donor, assay, cell_type);
    tagStanzaAdd(ts, stanza, "sample_label", label);
    return TRUE;
    }
return FALSE;
}

static void rFixLabel(struct tagStorm *ts, struct tagStanza *list, int *pCount)
/* Recursively call doFixLabel on all stanzas. */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    if (doFixLabel(ts, stanza))
       ++(*pCount);
    rFixLabel(ts, stanza->children, pCount);
    }
}

void cdwFixFrazer3(char *inTags, char *outTags)
/* cdwFixFrazer3 - Fix up frazer metadata tree some more.  See code for details.. */
{
struct tagStorm *ts = tagStormFromFile(inTags);
int metaCount = 0, labelCount = 0;
rFixMeta(ts, ts->forest, &metaCount);
rFixLabel(ts, ts->forest, &labelCount);
verbose(1, "Fixed %d metas and %d labels\n", metaCount, labelCount);
tagStormWrite(ts, outTags, 0);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
cdwFixFrazer3(argv[1], argv[2]);
return 0;
}
