#ifndef HGCONFIG_H
#define HGCONFIG_H

char* cfgOption(char* name);
/* return the option with the given name, or null if not found */

char* cfgOptionDefault(char* name, char* def);
/* return the option with the given name or the given default if it doesn't exist */

char *cfgOptionEnv(char *envName, char* name);
/* get a configuration optional value, from either the environment or the cfg
 * file, with the env take precedence.  Return NULL if not found */

char *cfgVal(char *name);
/* Return option with given name.  Squawk and die if it
 * doesn't exist. */

struct slName *cfgNames();
/* get list of names in config file. slFreeList when finished */

unsigned long cfgModTime();
/* Return modification time of config file */

#endif
