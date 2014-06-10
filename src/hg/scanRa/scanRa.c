/* scanRa - scan through ra files counting or printing 
 * ones that meet a certain criteria. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "hash.h"
#include "obscure.h"
#include "keys.h"
#include "hdb.h"


enum commandType
    {
    ctCount, ctPrint, ctStats, ctHist,
    } command;

struct useCount
    {
    struct useCount *next;
    char *what;
    int count;
    int val;
    };

int cmpUse(const void *va, const void *vb)
/* Compare two useCounts by count. */
{
const struct useCount *a = *((struct useCount **)va);
const struct useCount *b = *((struct useCount **)vb);
int dif = b->count - a->count;
if (dif != 0)
	return dif;
return strcmp(b->what, a->what);
}

int cmpVal(const void *va, const void *vb)
/* Compare two useCounts by whatVal. */
{
const struct useCount *a = *((struct useCount **)va);
const struct useCount *b = *((struct useCount **)vb);
return a->val - b->val;
}

void fillInVal(struct useCount *list)
/* Convert what field to number and store in val field. */
{
struct useCount *el;
for (el = list; el != NULL; el = el->next)
    {
    el->val = atoi(el->what);
    }
}

FILE *out;
char *selectKey;
struct kvt *kvt;
struct hash *statHash;
struct useCount *useCounts;
int matchCount;

void scanFile(struct keyExp *exp, char *fileName)
/* Scan file for things that match expression. */
{
FILE *f = mustOpen(fileName, "r");
int keyBufSize = 32*1024;
char *keysBuf = needMem(keyBufSize+1);
int lastc, c;
int kbIx;
int modMax = 10000;
int mod = modMax;

printf("scanning %s", fileName);
fflush(stdout);
for (;;)
    {
    if (--mod <= 0)
        {
        printf(".");
        fflush(stdout);
        mod = modMax;
        }
    kvtClear(kvt);
    kbIx = 0;
    lastc = 0;
    for (;;)
        {
        if (((c = fgetc(f)) == EOF) || (c == '\n' && lastc == '\n'))
            break;         
        keysBuf[kbIx] = c;
        if (++kbIx >= keyBufSize)
            errAbort("Record too long\n");
        lastc = c;
        }
    if (kbIx > 0)
        {
        kvtParseAdd(kvt, keysBuf);
        if (exp == NULL || keyExpEval(exp, kvt))
            {
            ++matchCount;
            if (command == ctPrint)
                {
                char *kv = kvtLookup(kvt, selectKey);
                if (kv == NULL)
                    kv = "NULL";
                fprintf(out, "%s\n", kv);
                }
            if (command == ctStats || command == ctHist)
                {
                char *kv = kvtLookup(kvt, selectKey);
                struct useCount *u;
                struct hashEl *hel;
                if (kv == NULL)
                    kv = "NULL";
                if ((hel = hashLookup(statHash, kv)) == NULL)
                    {
                    AllocVar(u);
                    hel = hashAdd(statHash, kv, u);
                    u->what = hel->name;
                    u->count = 1;
                    slAddHead(&useCounts, u);
                    }
                else
                    {
                    u = hel->val;
                    ++u->count;
                    }
                }
            }
        }
    if (c == EOF)
        break;
    }
freeMem(keysBuf);
fclose(f);
printf("\n");
}

void usage()
/* Print usage instructions and exit. */
{
errAbort(
    "scanRa - scan through ra files for info.\n"
    "usage:\n"
    "    scanRa expression command [command args] raFile(s)\n"
    "where expression is:\n"
    "    all - matches anything in ra files\n"
    "    fileName - a file containing an expression\n"
    "where command is:\n"
    "    count - to count ones that match expression\n"
    "    print key - to print key from ones matching expression\n"
    "    save key file - to print key of matcher to file\n"
    "    stats key file - to gather statistics on key to file\n"
    "    hist key file - to put histogram of key into file\n"
    );
}

int main(int argc, char *argv[])
{
char *com;
char **raFiles = NULL;
int raCount = 0;
char *expFile;
char *expression;
size_t size;
struct keyExp *exp;
int i;

if (argc < 4)
    usage();
expFile = argv[1];
com = argv[2];
if (sameWord(com, "count"))
    {
    command = ctCount;
    out = stdout;
    raCount = argc-3;
    raFiles = argv+3;
    }
else if (sameWord(com, "print"))
    {
    if (argc < 5)
        usage();
    command = ctPrint;
    selectKey = argv[3];
    out = stdout;
    raCount = argc-4;
    raFiles = argv+4;
    }
else if (sameWord(com, "save"))
    {
    if (argc < 6)
        usage();
    command = ctPrint;
    selectKey = argv[3];
    out = mustOpen(argv[4], "w");
    raCount = argc-5;
    raFiles = argv+5;
    }
else if (sameWord(com, "stats"))
    {
    if (argc < 6)
        usage();
    command = ctStats;
    selectKey = argv[3];
    out = mustOpen(argv[4], "w");
    raCount = argc-5;
    raFiles = argv+5;
    statHash = newHash(20);
    }
else if (sameWord(com, "hist"))
    {
    if (argc < 6)
        usage();
    command = ctHist;
    selectKey = argv[3];
    out = mustOpen(argv[4], "w");
    raCount = argc-5;
    raFiles = argv+5;
    statHash = newHash(18);
    }
if (sameWord(expFile, "all"))
    exp = NULL;
else
    {
    readInGulp(expFile, &expression, &size);
    exp = keyExpParse(expression);
    }
kvt = newKvt(128);
for (i=0; i<raCount; ++i)
    {
    scanFile(exp, raFiles[i]);
    }
printf("%d matched %s\n", matchCount, expFile);

if (command == ctStats)
    {
    struct useCount *u;
    slSort(&useCounts, cmpUse);
    for (u = useCounts; u != NULL; u = u->next)
        {
        fprintf(out, "%d %s\n", u->count, u->what);
        }
    printf("%d unique values for %s\n", slCount(useCounts), selectKey);
    }
else if (command == ctHist)
    {
    struct useCount *u;
    fillInVal(useCounts);
    slSort(&useCounts, cmpVal);
    fprintf(out, "value   uses\n");
    fprintf(out, "------------\n");
    for (u=useCounts; u != NULL; u = u->next)
        {
        fprintf(out, "%5d  %5d\n", u->val, u->count);
        }
    }
return 0;
}
