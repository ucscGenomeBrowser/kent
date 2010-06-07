#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"

int main(int argc, char *argv[])
{
if (argc != 2)
    errAbort("%s: Specify an input file with strings key<space>value on commandline.\n"
       , argv[0]);

struct lineFile * lf = lineFileOpen(argv[1], TRUE); 
char *line = NULL;
char *row[2];
int wordCount;
struct hash *hash = newHash(0);
struct slName *keyList = NULL, *node = NULL; 
char *key, *val;
while (lineFileNext(lf, &line, NULL))
    {
    wordCount = chopLine(line, row);
    if (wordCount == 0 || row[0][0] == '#')
	continue;
    if (wordCount != 2)
	 {
	 warn("Invalid line %d of %s", lf->lineIx, lf->fileName);
	 continue;
	 }
    key = row[0];
    val = row[1];
    printf("%s=%s\n", key, val);
    if (!hashLookup(hash, key))
	{
	node = newSlName(key);
	slAddHead(&keyList, node);
	}
    hashAdd(hash, key, cloneString(val));
    }
lineFileClose(&lf);
slReverse(&keyList);

printf("-----------\n");
printf("keys and values:\n");
/* read and print the hash values in keyList order */
for(node=keyList; node; node=node->next)
    {
    key = node->name; 
    printf("%s", key);
    struct hashEl *hel;
    for (hel = hashLookup(hash, key); hel; hel = hashLookupNext(hel))
	{
	printf(" %s", (char *)hel->val);
	}
    printf("\n");
    }

/* double the size */
hashResize(hash, digitsBaseTwo(hash->size));

printf("----RESIZED HASH-------\n");
printf("keys and values:\n");
/* read and print the hash values in keyList order */
for(node=keyList; node; node=node->next)
    {
    key = node->name; 
    printf("%s", key);
    struct hashEl *hel;
    for (hel = hashLookup(hash, key); hel; hel = hashLookupNext(hel))
	{
	printf(" %s", (char *)hel->val);
	}
    printf("\n");
    }


slFreeList(&keyList);
freeHash(&hash);
return 0;

}

