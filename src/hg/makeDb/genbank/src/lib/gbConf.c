/* configuration object */
#include "common.h"
#include "gbConf.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "localmem.h"

static char const rcsid[] = "$Id: gbConf.c,v 1.4 2006/03/11 00:07:59 markd Exp $";

/* standard conf file */
char *GB_CONF_FILE = "etc/genbank.conf";

static char *findVarEnd(struct lineFile* lf, char *vstart)
/* find the end of a variable reference, returning char after close '}' */
{
char *vend = NULL;
assert(*vstart = '$');
if (vstart[1] == '{')
    {
    char *sep = strchr(vstart, '}');
    if (sep != NULL)
        vend = sep+1;
    }
if (vend == NULL)
    errAbort("invalid config variable reference at: %s:%d:%s",
             lf->fileName, lf->lineIx, vstart);
return vend;
}

static void expandVarRef(struct lineFile* lf, struct gbConf* conf,
                         char *vstart, char *vend, struct dyString *valueBuf)
/* expand a variable reference and append to valueBuf */
{
char vname[512];
char *vval;
safef(vname, sizeof(vname), "var.%.*s", (int)((vend-1)-(vstart+2)), vstart+2);
vval = hashFindVal(conf->hash, vname);
if (vval == NULL)
    errAbort("undefined variable referenced in config file: %s:%d:%s", lf->fileName, lf->lineIx, vname);
dyStringAppend(valueBuf, vval);
}

static void parseConfVal(struct lineFile* lf, struct gbConf* conf,
                         char *value, struct dyString *valueBuf)
/* copy value to valueBuf, expanding variables as encountered */
{
char *vstart, *vend;
dyStringClear(valueBuf);
while (*value != '\0')
    {
    /* search for next variable reference, or end of string */
    vstart = strchr(value, '$');
    if (vstart == NULL)
        break;  /* no more variables */
    /* append before valiable */
    dyStringAppendN(valueBuf, value, vstart-value);
    /* expand variable */
    vend = findVarEnd(lf, vstart);
    expandVarRef(lf, conf, vstart, vend, valueBuf);
    value = vend;
    }
/* remainder of value, if any */
dyStringAppend(valueBuf, value);
}

static void parseConfLine(struct lineFile* lf, struct gbConf* conf, char *line,
                          struct dyString *valueBuf)
/* parse a configuration data line, expanding variables.*/
{
char *name, *sep;

sep = strchr(line, '=');
if (sep == NULL)
    errAbort("invalid config file line: %s:%d:%s", lf->fileName, lf->lineIx, line);
*sep = '\0';
name = trimSpaces(line);
parseConfVal(lf, conf, trimSpaces(sep+1), valueBuf);
hashAdd(conf->hash, name, lmCloneString(conf->hash->lm, valueBuf->string));
}

struct gbConf* gbConfNew(char *confFile)
/* parse a configuration object into the hash */
{
struct gbConf* conf;
struct lineFile* lf;
struct dyString* lineBuf = dyStringNew(512);
struct dyString* valueBuf = dyStringNew(512);
char *line;
AllocVar(conf);
conf->hash = hashNew(10);

/* dyString line buffer avoids trimSpaces mangling lineFile input.*/
lf = lineFileOpen(confFile, TRUE);
while (lineFileNextReal(lf, &line))
    {
    dyStringClear(lineBuf);
    dyStringAppend(lineBuf, line); 
    parseConfLine(lf, conf, lineBuf->string, valueBuf);
    }
lineFileClose(&lf);
dyStringFree(&lineBuf);
dyStringFree(&valueBuf);
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


