/* metaCollect - build collections using metadata. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "mdb.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "metaCollect - build collections using metadata\n"
  "usage:\n"
  "   metaCollect metaDb.ra reqViews.txt expVars.txt\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct pairList 
{
struct pairList *next;
char *name;
char *val;
};

struct group
{
struct group *next;
struct hash *viewHash;
struct hash *repHash;

struct pairList *pairs;
};

void printGroup(struct group *group)
{
struct pairList *pairs = group->pairs;

printf("{");
for(; pairs; pairs=pairs->next)
    {
    printf("%s = %s",pairs->name, pairs->val);
    if (pairs->next)
        printf(",");
    }
printf("}");
printf("\n");
}

struct listOfList
{
    struct listOfList *next;
    char *expVar;
    struct slName *names;
    struct pairList *pairs;
};

struct slName *getListOfVals(struct slName *exp, struct mdbObj *mdbObjs)
{
struct slName *list = NULL;
struct hash *foundHash = newHash(10);

for(; mdbObjs; mdbObjs = mdbObjs->next)
    {
    struct mdbVar *mdbVar = hashFindVal(mdbObjs->varHash, exp->name);

    if (mdbVar != NULL)
        {
        verbose(2, "found %s with value %s in object %s\n", exp->name, mdbVar->val, mdbObjs->obj);

        if (hashLookup(foundHash, mdbVar->val) == NULL)
            {
            hashStore(foundHash, mdbVar->val);

            struct slName *n = newSlName(mdbVar->val);;
            slAddHead(&list, n);
            }
        }
    }
return list;
}

struct pairList *dupList(struct pairList *pairs)
{
struct pairList *newList = NULL;

//if (pairs == NULL)
    //errAbort("dupPlist pairs is NULL");
for(;pairs; pairs = pairs->next)
    {
    struct pairList *newPair;

    AllocVar(newPair);
    newPair->name = pairs->name;
    newPair->val = pairs->val;

    slAddHead(&newList, newPair);
    }

slReverse(&newList);
return newList;
}

void addGroup(struct listOfList *list, struct pairList *pairs,  struct group **groups)
{
if (list == NULL)
    {
    struct group *group;
    AllocVar(group);
    slAddHead(groups, group);

    group->pairs = pairs;
    //group->viewHash = newHash(10);

//    printf("adding group: ");
 //   for(;pairs; pairs = pairs->next)
  //      printf("%s + %s,",pairs->name, pairs->val);
  //  printf("\n");
    return;
    }

struct pairList *pair;
for (pair=list->pairs; pair; pair = pair->next)
    {
    struct pairList *newPairs = dupList(pairs);
    struct pairList *newPair;
    struct pairList *newList = NULL;

    AllocVar(newPair);
    newPair->name = pair->name;
    newPair->val = pair->val;
    newList = slCat(newPairs, newPair);
    addGroup( list->next, newList, groups);
    }
}

struct group *groupByExpVar(struct mdbObj *mdbObjs, struct slName *expVars)
{
struct listOfList *list = NULL, *listIter;
struct slName *ev;

for(ev = expVars; ev; ev = ev->next)
    {
    struct listOfList *el;
    AllocVar(el);
    el->names = getListOfVals(ev, mdbObjs);
    el->expVar = ev->name;

    slAddHead(&list, el);
    }

for(listIter = list; listIter; listIter = listIter->next)
    {
    struct slName *names = listIter->names;
    for(; names; names = names->next)
        {
        struct pairList *pair;

        AllocVar(pair);
        slAddHead(&listIter->pairs, pair);
        pair->name = listIter->expVar;
        pair->val = names->name;
        }
    }

struct group *groups = NULL; // = createGroups(expVars);

#ifdef NOTNOW
for(listIter = list; listIter; listIter = listIter->next)
    {
    printf("%s\n", listIter->expVar);
    struct pairList *pair;
    for (pair=listIter->pairs; pair; pair = pair->next)
        printf("%s = %s,",pair->name, pair->val);
    printf("\n");
    }
#endif

addGroup(list, NULL,  &groups);

return groups;
}

void checkForReqView(struct group *groups, struct slName *reqViews)
{
int errs = 0;
for(; groups; groups = groups->next)
    {
    struct slName *view = reqViews;
    for(; view; view = view->next)
        {
        if (groups->viewHash != NULL)
            {
            if (groups->repHash != NULL)
                {
                struct hashCookie cook = hashFirst(groups->repHash);
                struct hashEl *hel;

                while((hel = hashNext(&cook)) != NULL)
                    {
                    char buffer[10*1024];
                    safef(buffer, sizeof buffer, "%s-%s",view->name, 
                        (char *)hel->name);
                    if (hashLookup(groups->viewHash, buffer) == NULL)
                        {
                        printf("didn't find view %s for replicate %s in ", 
                            view->name,(char *)hel->name);
                        printGroup(groups);
                        errs++;
                        }
                    }
                }
            else
                {
                if (hashLookup(groups->viewHash, view->name) == NULL)
                    {
                    printf("didn't find view %s in group\n", view->name);
                    printGroup(groups);
                    errs++;
                    }
                }
            }
        }
    }
if (errs > 0)
    errAbort("aborting");
}

struct group *findGroup(struct mdbObj *mdbObjs, struct group *groups)
{
for(; groups; groups = groups->next)
    {
    struct pairList *pairs = groups->pairs;

    for(; pairs; pairs = pairs->next)
        {
        struct mdbVar *mdbVar = hashFindVal(mdbObjs->varHash, pairs->name);

        if (mdbVar == NULL)
            errAbort("can't find var %s in obj %s\n",pairs->name, mdbObjs->obj);

        if (differentString(mdbVar->val, pairs->val))
            break;
        }
    if (pairs == NULL)
        return groups;
    }
return NULL;
}

void checkOverlap(struct mdbObj *mdbObjs, struct group *groups)
{
int errs = 0;
for(; mdbObjs; mdbObjs = mdbObjs->next)
    {
    struct mdbVar *mdbVar = hashFindVal(mdbObjs->varHash, "objType");
    if (sameString(mdbVar->val, "composite"))
        continue;
    struct group *ourGroup = findGroup(mdbObjs, groups);
    if (ourGroup == NULL)
        errAbort("cannot find group for %s\n", mdbObjs->obj);
    mdbVar = hashFindVal(mdbObjs->varHash, "view");

    if (mdbVar == NULL)
        errAbort("obj %s has no view", mdbObjs->obj);


    struct hashEl *hel;
    if (ourGroup->viewHash == NULL)
        ourGroup->viewHash = newHash(10);

    struct mdbVar *mdbReplicate = hashFindVal(mdbObjs->varHash, "replicate");
    char buffer[10*1024];

    if (mdbReplicate != NULL)
        {
        if (ourGroup->repHash == NULL)
            ourGroup->repHash = newHash(10);
        hashStore(ourGroup->repHash, mdbReplicate->val);
        safef(buffer, sizeof buffer, "%s-%s",mdbVar->val, mdbReplicate->val);
        }
    else
        safef(buffer, sizeof buffer, "%s",mdbVar->val);

    if ((hel = hashLookup(ourGroup->viewHash, buffer)) != NULL)
        {
        printf("two views of type %s in same group\n",buffer);
        printf("objects are %s and %s\n", (char *)hel->val, mdbObjs->obj);
        printGroup(ourGroup);
        errs++;
        }
    else
        hashAdd(ourGroup->viewHash, buffer, mdbObjs->obj);
    }
if (errs)
    errAbort("aborting");
}

void metaCollect(char *metaDb, char *reqViewsName, 
    char *expVarsName)
/* metaCollect - build collections using metadata. */
{
boolean validated;
struct mdbObj *mdbObjs = mdbObjsLoadFromFormattedFile(metaDb, &validated);
struct slName *expVars = slNameLoadReal(expVarsName);
struct slName *reqViews = slNameLoadReal(reqViewsName);

struct group *groups = groupByExpVar(mdbObjs, expVars);

checkOverlap(mdbObjs, groups);

checkForReqView(groups, reqViews);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
metaCollect(argv[1], argv[2], argv[3]);
return 0;
}
