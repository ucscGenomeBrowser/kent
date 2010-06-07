#include "common.h"
#include "dbLoadOptions.h"
#include "options.h"
#include "gbConf.h"


static void parseOrgCatAttrs(struct gbConf* conf, char* db,
                             struct dbLoadOptions* options,
                             unsigned srcDb, unsigned type, unsigned orgCat,
                             unsigned restrictFlags)
/* parse srcDb+type+orgCat options from the command line, checking the
 * restrictions from command line. */
{
if (((restrictFlags & GB_ORG_CAT_MASK) == 0) || (restrictFlags & orgCat))
    {
    struct dbLoadAttr* attr = dbLoadOptionsGetAttr(options, srcDb, type, orgCat);
    char typeName[128], baseName[128];
    safef(typeName, sizeof(typeName), "%s.%s.%s",
          gbSrcDbName(srcDb), gbTypeName(type), gbOrgCatName(orgCat));
    tolowers(typeName);
    if (restrictFlags & orgCat)
        attr->load = TRUE;  /* force from cmd line */
    else
        {
        safef(baseName, sizeof(baseName), "%s.%s", typeName, "load");
        attr->load = gbConfMustGetDbBoolean(conf, db, baseName);
        }
    /* load description always comes from conf file */
    if (attr->load)
        {
        safef(baseName, sizeof(baseName), "%s.%s", typeName, "loadDesc");
        attr->loadDesc = gbConfMustGetDbBoolean(conf, db, baseName);
        }
    }
}

static void parseTypeAttrs(struct gbConf* conf, char* db,
                           struct dbLoadOptions* options,
                           unsigned srcDb, unsigned type,
                           unsigned restrictFlags)
/* parse srcDb+type options from the command line, checking the restrictions
 * from command line. */
{
if (((restrictFlags & GB_TYPE_MASK) == 0) || (restrictFlags & type))
    {
    parseOrgCatAttrs(conf, db, options, srcDb, type, GB_NATIVE, restrictFlags);
    parseOrgCatAttrs(conf, db, options, srcDb, type, GB_XENO, restrictFlags);
    }
}

static void parseSrcDbAttrs(struct gbConf* conf, char* db,
                            struct dbLoadOptions* options,
                            unsigned srcDb, unsigned restrictFlags)
/* parse srcDb options from the command line, checking the restrictions from
 * command line. */
{
if (((restrictFlags & GB_SRC_DB_MASK) == 0) || (restrictFlags & srcDb))
    {
    parseTypeAttrs(conf, db, options, srcDb, GB_MRNA, restrictFlags);
    if (srcDb != GB_REFSEQ)
        parseTypeAttrs(conf, db, options, srcDb, GB_EST, restrictFlags);
    }
}

static unsigned getCmdSelect(struct gbConf* conf)
/* get command override of what should be installed.  flags are not
 * set for selectios that should come from the conf file. */
{
char *relRestrict = optionVal("release", NULL);
unsigned restrictFlags = 0;

/* get possible source databases */
if (relRestrict != NULL)
    {
    if (startsWith("genbank.", relRestrict))
        restrictFlags |= GB_GENBANK;
    else if (startsWith("refseq.", relRestrict))
        restrictFlags |= GB_REFSEQ;
    else
        errAbort("invalid release name: %s", relRestrict);
    }
else if (optionExists("srcDb"))
    restrictFlags |= gbParseSrcDb(optionVal("srcDb", NULL));

/* get possible types */
if (optionExists("type"))
    restrictFlags |= gbParseType(optionVal("type", NULL));

/* get possible orgCats */
if (optionExists("orgCat"))
    restrictFlags |= gbParseOrgCat(optionVal("orgCat", NULL));

return restrictFlags;
}


struct dbLoadOptions dbLoadOptionsParse(char* db)
/* parse many of the command line options and the options file. */
{
unsigned restrictFlags;
struct dbLoadOptions options;
ZeroVar(&options);
options.conf = gbConfNew(optionVal("conf", GB_CONF_FILE));

/* parse various command line options */
options.relRestrict = optionVal("release", NULL);
options.accPrefixRestrict = optionVal("accPrefix", NULL);
if (options.accPrefixRestrict != NULL)
        tolowers(options.accPrefixRestrict);
if (optionExists("goFaster"))
    options.flags |= DBLOAD_GO_FASTER;
if (optionExists("dryRun"))
    options.flags |= DBLOAD_DRY_RUN;
if (optionExists("initialLoad"))
    options.flags |= DBLOAD_INITIAL|DBLOAD_GO_FASTER;
if (optionExists("allowLargeDeletes"))
    options.flags |= DBLOAD_LARGE_DELETES;
if (optionExists("byPassGbLoaded"))
    options.flags |= DBLOAD_BYPASS_GBLOADED;
if (optionExists("extFileUpdate"))
    {
    options.flags |= DBLOAD_EXT_FILE_UPDATE;
    options.maxExtFileUpdate = BIGNUM;      /* no limit */
    }
if (optionExists("maxExtFileUpdate"))
    {
    options.flags |= DBLOAD_EXT_FILE_UPDATE;
    options.maxExtFileUpdate = optionInt("maxExtFileUpdate", 0);
    }

/* Get restrictions on what to select from the command line, and combine
 * with conf file. */
restrictFlags = getCmdSelect(options.conf);
parseSrcDbAttrs(options.conf, db, &options, GB_GENBANK, restrictFlags);
parseSrcDbAttrs(options.conf, db, &options, GB_REFSEQ, restrictFlags);

/* other conf file options */
if (gbConfMustGetDbBoolean(options.conf, db, "perChromTables"))
    options.flags |= DBLOAD_PER_CHROM_ALIGN;

return options;
}

struct dbLoadAttr* dbLoadOptionsGetAttr(struct dbLoadOptions* options,
                                        unsigned srcDb, unsigned type,
                                        unsigned orgCat)
/* get a pointer the load attributes */
{
return &(options->loadAttr[gbSrcDbIdx(srcDb)][gbTypeIdx(type)][gbOrgCatIdx(orgCat)]);

}
/*
 * local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
