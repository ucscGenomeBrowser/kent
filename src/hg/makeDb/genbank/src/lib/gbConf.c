/* configuration object */
#include "common.h"
#include "gbConf.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "localmem.h"

static char const rcsid[] = "$Id: gbConf.c,v 1.1 2003/07/10 16:49:28 markd Exp $";

/* standard conf file */
char *GB_CONF_FILE = "etc/genbank.conf";

struct gbConf* gbConfNew(char *confFile)
/* parse a configuration object into the hash */
{
struct gbConf* conf;
struct lineFile* lf;
struct dyString* lineBuf = dyStringNew(512);
char *line, *name, *value, *sep;
int size;
AllocVar(conf);
conf->hash = hashNew(10);

lf = lineFileOpen(confFile, TRUE);
while (lineFileNext(lf, &line, &size))
    {
    line = trimSpaces(line);
    if(!((line[0] == '#') || (line[0] == '\0')))
        {
        /* parse the key/value pair; must put in tmp buffer due to
         * trimSpaces.*/
        dyStringClear(lineBuf);
        dyStringAppend(lineBuf, line); 

        sep = strchr(lineBuf->string, '=');
        if (sep == NULL)
            errAbort("invalid config file line: %s:%d:%s",
                     lf->fileName, lf->lineIx, line);
        *sep = '\0';
        name = trimSpaces(lineBuf->string);
        value = trimSpaces(sep+1);
        hashAdd(conf->hash, name, lmCloneString(conf->hash->lm, value));
        }
    }
dyStringFree(&lineBuf);
return conf;
}

void gbConfFree(struct gbConf** confPtr)
/* free a configuration object */
{
struct gbConf* conf = *confPtr;
if (conf != NULL)
    {
    hashFree(&conf->hash);
    freez(confPtr);
    }
}

char *gbConfGet(struct gbConf* conf, char* name)
/* Lookup a configuration option, return NULL if not found */
{
return hashFindVal(conf->hash, name);
}

char *gbConfMustGet(struct gbConf* conf, char* name)
/* Lookup a configuration option, die if not found */
{
char *val = gbConfGet(conf, name);
if (val != NULL)
    errAbort("configuration parameters %s not found", name);
return val;
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */


