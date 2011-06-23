/* validateCv - validate controlled vocabulary file and metadata. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "cv.h"
#include "ra.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "validateCv - validates a controlled vocabulary file\n"
  "usage:\n"
  "   validateCv [-type={} [-setting={}]] [-level] cv.ra\n"
  "options:\n"
  "   -type={type} Type of terms to check, otherwise all types are checked.\n"
  "          -setting={setting} Check for just a single setting (only valid with -type).\n"
  "   -level       Level of scrutiny (ignored if -setting is used):\n"
  "                0 (default) Only must haves.\n"
  "                1 Must haves and should haves.\n"
  "                2 Must haves, should haves and the kitchen sink.\n"
  "   -verbose=2   Will list all errors.  Otherwise just counts errors.\n"
  );
}

static struct optionSpec options[] = {
    {"level",  OPTION_INT},
    {"type",   OPTION_STRING},
    {"setting",OPTION_STRING},
    {NULL, 0},
};

int cvTagsAreTermDuplicates(struct hash *cvHashOfHashes)
// returns count of tags that are also terms
{
int count = 0;
struct hashCookie brownie = hashFirst(cvHashOfHashes);
struct hashEl* el = NULL;
while ((el = hashNext(&brownie)) != NULL)
    {
    struct hash *hash = el->val;
    char *val = hashFindVal(hash,CV_TAG);
    if (val != NULL)
        {
        if (sameString(val,el->name)) // term and tag can be identical in the same stanza
            continue;

        struct hashEl* conflictingEl = hashLookup(cvHashOfHashes,val);
        if (conflictingEl != NULL)
            {
            count++;
            char *type = hashFindVal(hash,CV_TYPE);
            char *conflictingType = hashFindVal(conflictingEl->val,CV_TYPE);
            if (type != NULL && conflictingType)
                verbose(2,"Tag '%s' in '%s => %s' is itself a term '%s => %s'.\n",
                        val,type,el->name,conflictingType,conflictingEl->name);
            else
                verbose(2,"Tag '%s' in '%s' is itself a term.\n",val,el->name);
            continue;
            }
        }
    }
return count;
}

int cvHashesWithoutSetting(struct hash *cvHashOfHashes,char *setting,
                           boolean mustHave,boolean unique)
// returns count of hashes that do not have a given setting.
{
int count = 0;
struct hash *valHash = NULL;
struct hashCookie brownie = hashFirst(cvHashOfHashes);
struct hashEl* el = NULL;
while ((el = hashNext(&brownie)) != NULL)
    {
    struct hash *hash = el->val;
    char *type = NULL;
    if (differentString(setting,CV_TYPE))
        type = hashFindVal(hash,CV_TYPE);
    char *val = hashFindVal(hash,setting);
    if (val == NULL)
        {
        if (mustHave)
            {
            count++;
            if (type != NULL)
                verbose(2,"Term '%s => %s' is missing '%s'\n",type,el->name,setting);
            else
                verbose(2,"Term '%s' is missing '%s'\n",el->name,setting);
            }
        continue;
        }

    if (unique)
        {
        if (valHash == NULL)
            valHash = hashNew(0);
        else if (hashLookup(valHash,val) != NULL)
            {
            count++;
            if (type != NULL)
                verbose(2,"Term '%s => %s' setting '%s' is not unique\n",type,el->name,setting);
            else
                verbose(2,"Term '%s' setting '%s' is not unique\n",el->name,setting);
            continue;
            }
        hashAdd(valHash, val, (void *)1);
        }
    }
if (valHash != NULL)
    hashFree(&valHash);

return count;
}

struct slName *cvGetAllTypes(struct hash *cvHashOfHashes)
// returns a list of all types in the cvHashOfHashes)
{
struct slName *cvTypes = NULL;

struct hashCookie brownie = hashFirst(cvHashOfHashes);
struct hashEl* el = NULL;
while ((el = hashNext(&brownie)) != NULL)
    {
    struct hash *hash = el->val;
    char *type = hashFindVal(hash,CV_TYPE);
    if (type != NULL)
        slNameStore(&cvTypes, type);
    }
return cvTypes;
}

int cvTypeMustHaveSettings(struct slName **cvTypes,const char *type,const char *mustHaveSettings)
// checks that each memeber of the cvHash for the term has all settings required.
// returns count of errors and removes type from list of types
{
int count = 0;
char *settings = cloneString(mustHaveSettings); // will spill this memory
int ix = slNameFindIx(*cvTypes, (char *)type);
if (ix > -1)
    {
    struct slName *cvType = slElementFromIx(*cvTypes,ix);
    assert(cvType != NULL);
    char *normalizedTerm = (char *)cvTermNormalized(cvType->name);

    const struct hash *termHash = cvTermHash(normalizedTerm);
    if (termHash != NULL)
        {
        char *setting = NULL;
        while ((setting = nextWord(&settings)) != NULL)
            count += cvHashesWithoutSetting((struct hash *)termHash,setting,TRUE,FALSE);
        }
    else
        {
        count++;
        verbose(2,"Type %s has no members.\n",cvType->name);
        }
    slRemoveEl(cvTypes, cvType);
    slNameFree(&cvType);
    }
else
    {
    count++;
    verbose(2,"Type '%s' cannot be found.\n",type);
    }
return count;
}

int validateCv(char *cvName,char *type,char *setting,int level)
/* validateCv - validate controlled vocabulary file and metadata. */
{

int count = 0;
struct slName *cvTypes = NULL;
if (type == NULL)
    {
    struct hash *cvHash = raReadAll(cvName, CV_TERM);

    // Now we can walk through some checks
    // All stanzas have unique terms - already shown by reading in cvHash?
    // All stanzas have types
    count += cvHashesWithoutSetting(cvHash,CV_TYPE,TRUE,FALSE); // must have, unique not necessary

    // All terms must have uniq tags
    count += cvHashesWithoutSetting(cvHash,CV_TAG,FALSE,TRUE); // Not necessary but must be unique
    count += cvTagsAreTermDuplicates(cvHash);

    // Get a list of all types, then walk throgh the types with specific or general restrictions
    cvTypes = cvGetAllTypes(cvHash);

    // At this point we are done with looking at cv as a single hash
    // and will use standard cv routines to examine the file.
    hashFree(&cvHash);
    }
else
    {
    if (sameWord(type,CV_TERM_ANTIBODY))
        cvTypes = slNameNew((char *)cvTypeNormalized(CV_TERM_ANTIBODY));
    else if (sameWord((char *)cvTermNormalized(type),CV_TERM_CELL))
        {
        // Curretly this is shielded in the lib and there is no code to get it
        #define CV_UGLY_TERM_CELL_LINE  "Cell Line"
        cvTypes = slNameNew(CV_UGLY_TERM_CELL_LINE);
        }
    else
        cvTypes = slNameNew(type);
    }


// override looking for the cv.ra file in the standard place.
cvFileDeclare(cvName);
struct dyString *dySettings = dyStringNew(512);
char *checkSettings = setting;

// typeOfTerms is the set of type definitions
if (type == NULL || sameWord(type,CV_TOT))
    {
    dyStringClear(dySettings);
    if (setting != NULL)
        dyStringAppend(dySettings,setting);
    else
        {
        dyStringAppend(dySettings,CV_LABEL " " CV_DESCRIPTION " " CV_VALIDATE " "
                CV_TOT_PRIORITY " " CV_TOT_CV_DEFINED);
        if (type != NULL && setting == NULL)
            verbose(1,"Must haves: %s\n",dyStringContents(dySettings));
        if (level > 0)
            {
            checkSettings = " " CV_TOT_SEARCHABLE " " CV_TOT_HIDDEN;
            if (type != NULL && setting == NULL)
                verbose(1,"Should haves:%s\n",checkSettings);
            dyStringAppend(dySettings,checkSettings);
            }
        }
    count += cvTypeMustHaveSettings(&cvTypes,CV_TOT,dyStringContents(dySettings));
    }

// Antibody: is special
if (type == NULL || sameWord(type,CV_TERM_ANTIBODY))
    {
    dyStringClear(dySettings);
    if (setting != NULL)
        dyStringAppend(dySettings,setting);
    else
        {
        dyStringAppend(dySettings,CV_TAG " " CV_TERM_LAB " " CV_VENDER_NAME " " CV_VENDOR_ID
                                " antibodyDescription " CV_TARGET " targetDescription");
        if (type != NULL && setting == NULL)
            verbose(1,"Must haves: %s\n",dyStringContents(dySettings));
        if (level > 0)
            {
            checkSettings = " " CV_ORDER_URL " validation targetId targetUrl";
            dyStringAppend(dySettings,checkSettings);
            if (type != NULL && setting == NULL)
                verbose(1,"Should haves:%s\n",checkSettings);
            }
        }
    count += cvTypeMustHaveSettings(&cvTypes,cvTypeNormalized(CV_TERM_ANTIBODY),
                                     dyStringContents(dySettings));
    }

// "Cell Line" is very special
if (type == NULL || sameWord((char *)cvTermNormalized(type),CV_TERM_CELL))
    {
    dyStringClear(dySettings);
    if (setting != NULL)
        dyStringAppend(dySettings,setting);
    else
        {
        dyStringAppend(dySettings,CV_TAG " " CV_DESCRIPTION " " CV_ORGANISM " " CV_SEX);
        if (type != NULL && setting == NULL)
            verbose(1,"Must haves: %s\n",dyStringContents(dySettings));
        if (level > 0)
            {
            checkSettings = " " CV_PROTOCOL " " CV_VENDER_NAME " " CV_VENDOR_ID
                                    " " CV_ORDER_URL " " CV_TERM_ID  " " CV_TERM_URL;
            dyStringAppend(dySettings,checkSettings);
            if (type != NULL && setting == NULL)
                verbose(1,"Should haves:%s\n",checkSettings);
            }
        if (level > 1)
            {
            checkSettings = " " CV_LINEAGE " " CV_TIER " " CV_TISSUE " color karyotype";
            if (type != NULL && setting == NULL)
                verbose(1,"Kitchen sink:%s\n",checkSettings);
            dyStringAppend(dySettings,checkSettings);
            }
        }
    count += cvTypeMustHaveSettings(&cvTypes,CV_UGLY_TERM_CELL_LINE,dyStringContents(dySettings));
    }

// Other types with non-standard requirements
checkSettings = setting;
if (type == NULL || sameWord(type,CV_TERM_LAB))
    {
    if (setting == NULL)
        checkSettings = CV_TAG " " CV_DESCRIPTION " " CV_LABEL " " CV_ORGANISM
                        " labInst labPi labPiFull grantPi";
    if (type != NULL && setting == NULL)
        verbose(1,"Must haves: %s\n",checkSettings);
    count += cvTypeMustHaveSettings(&cvTypes,CV_TERM_LAB,checkSettings);
    }
if (type == NULL || sameWord(type,CV_TERM_GRANT))
    {
    if (setting == NULL)
        checkSettings = CV_TAG " " CV_DESCRIPTION " grantInst projectName";
    if (type != NULL && setting == NULL)
        verbose(1,"Must haves: %s\n",checkSettings);
    count += cvTypeMustHaveSettings(&cvTypes,CV_TERM_GRANT,checkSettings);
    }
if (type == NULL || sameWord(type,CV_TERM_LOCALIZATION))
    {
    if (setting == NULL)
        checkSettings = CV_TAG " " CV_DESCRIPTION " " CV_TERM_ID " " CV_TERM_URL;
    if (type != NULL && setting == NULL)
        verbose(1,"Must haves: %s\n",checkSettings);
    count += cvTypeMustHaveSettings(&cvTypes,CV_TERM_LOCALIZATION,checkSettings);
    }
if (type == NULL || sameWord(type,CV_TERM_SEQ_PLATFORM))
    {
    if (setting == NULL)
        checkSettings = CV_TAG " " CV_DESCRIPTION " geo";
    if (type != NULL && setting == NULL)
        verbose(1,"Must haves: %s\n",checkSettings);
    count += cvTypeMustHaveSettings(&cvTypes,CV_TERM_SEQ_PLATFORM,checkSettings);
    }

// walk through all the rest of types with standard requirements: tag and description
if (setting == NULL)
    checkSettings = CV_TAG " " CV_DESCRIPTION;
while(cvTypes != NULL)
    {
    if (type != NULL && setting == NULL)
        verbose(1,"Must haves: %s\n",checkSettings);
    count += cvTypeMustHaveSettings(&cvTypes,cvTypes->name,checkSettings);
    }

if (count > 0 || type != NULL)
    verbose(1,"Found %d error%s.\n",count,(count==1?"":"s"));

return count;
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
int level = optionInt("level", 0);
char *type   = optionVal("type",NULL);
char *setting   = optionVal("setting",NULL);
if (setting != NULL && type == NULL)
    {
    verbose(1,"ERROR: -setting=%s requires -type=?.\n",setting);
    usage();
    }

return validateCv(argv[1],type,setting,level);
}
