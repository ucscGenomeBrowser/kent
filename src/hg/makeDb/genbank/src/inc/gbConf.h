/* configuration object */
#ifndef GBCONF_H
#define GBCONF_H

struct gbConf
/* genbank configuration object */
{
    struct hash* hash;
};

extern char *GB_CONF_FILE;  /* standard conf file */

struct gbConf* gbConfNew(char *confFile);
/* parse a configuration object into the hash */

void gbConfFree(struct gbConf** confPtr);
/* free a configuration object */

char *gbConfGet(struct gbConf* conf, char* name);
/* Lookup a configuration option, return NULL if not found */

char *gbConfMustGet(struct gbConf* conf, char* name);
/* Lookup a configuration option, die if not found */

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
