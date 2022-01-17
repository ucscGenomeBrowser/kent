/* configuration object */

/* Copyright (C) 2008 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
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

char* gbConfGetDb(struct gbConf* conf, char* db, char* baseName);
/* parse an option for a database; check for database-specific value or
 * default value, NULL if not found */

char* gbConfMustGetDb(struct gbConf* conf, char* db, char* baseName);
/* parse an option for a database; check for database-specific value and
 * default */

boolean gbConfGetDbBoolean(struct gbConf* conf, char* db, char* baseName);
/* parse boolean option for a database; check for database-specific value and
 * default, FALSE if not specified */

boolean gbConfMustGetDbBoolean(struct gbConf* conf, char* db, char* baseName);
/* parse boolean option for a database; check for database-specific value and
 * default */

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
