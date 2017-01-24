/* tagStormAssignMeta - assign meta fields to leaves. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tagStorm.h"
#include "ra.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tagStormAssignMeta - assign meta fields to leaves\n"
  "usage:\n"
  "   tagStormAssignMeta tagStorm.txt trackDb.txt newTagStorm.txt newTrackDb.txt\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

int metaNumber = 0;

void tagStormAssignMeta(char *tagStormName, char *trackDbName, char *newTagStormName, 
    char *newTrackDbName)
/* tagStormAssignMeta - assign meta fields to leaves. */
{
struct tagStorm *tagStorm = tagStormFromFile(tagStormName);
struct hash *hash = tagStormUniqueIndex(tagStorm, "track");
struct hash *trackToMetaHash = newHash(5);

struct hashCookie cookie = hashFirst(hash);
struct hashEl *hel;
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct tagStanza *stanza = hel->val;
    struct tagStanza *parent = stanza->parent;
    char *metaName =  tagFindVal(parent, "meta");

    if (metaName == NULL)
        {
        char metaBuffer[1024];

        safef(metaBuffer, sizeof metaBuffer, "meta%0d", metaNumber++);
        metaName = cloneString(metaBuffer);

        tagStanzaUpdateTag(tagStorm, parent, "meta", metaBuffer);  
        }

    hashAdd(trackToMetaHash, hel->name, metaName);

    struct tagStanza *prev = NULL;
    struct tagStanza *this = NULL;

    for(this = parent->children; this; prev = this, this = this->next)
        {
        if (this == stanza)
            {
            if (prev == NULL)
                parent->children = this->next;
            else
                prev->next = this->next;
            break;
            }
        }
    if (this == NULL)
        errAbort("Couldn't find stanza");
    }

tagStormWrite(tagStorm, newTagStormName, 0);
struct lineFile *trackDbLf = lineFileOpen(trackDbName, TRUE);
FILE *newTrackDb = mustOpen(newTrackDbName, "w");

char *start;
int size;
while (lineFileNext(trackDbLf, &start, &size))
    {
    fprintf(newTrackDb, "%s\n",  start);
    char *tag = skipLeadingSpaces(start);
    if (startsWith("track", tag))
        {
        char *ptr;
        for(ptr = start; ptr < tag; ptr++)
            fputc(*ptr, newTrackDb);

        nextWord(&tag);
        char *value = nextWord(&tag);
        char *mapped = (char *)hashFindVal(trackToMetaHash, value);
        if (mapped)
            fprintf(newTrackDb, "meta %s\n", mapped);
        }
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
tagStormAssignMeta(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
