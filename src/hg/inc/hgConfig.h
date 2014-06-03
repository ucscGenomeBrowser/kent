/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef HGCONFIG_H
#define HGCONFIG_H

char* cfgOption(char* name);
/* return the option with the given name, or null if not found */

char* cfgOptionDefault(char* name, char* def);
/* return the option with the given name or the given default if it doesn't exist */

boolean cfgOptionBooleanDefault(char* name, boolean def);
/* return the boolean value for option with the given name or the given
 * default if it doesn't exist. Booleans of yes/no/on/off/true/false are
 * recognized */

char *cfgOptionEnv(char *envName, char* name);
/* get a configuration optional value, from either the environment or the cfg
 * file, with the env take precedence.  Return NULL if not found */

char *cfgOptionEnvDefault(char *envName, char* name, char *def);
/* get a configuration optional value, from either the environment or the cfg
 * file, with the env take precedence.  Return default if not found */

char *cfgOption2(char *prefix, char *suffix);
/* Return the option with the given two-part name, formed from prefix.suffix.
 * Return NULL if it doesn't exist. */

char* cfgOptionDefault2(char *prefix, char *suffix, char* def);
/* Return the option with the given two-part name, formed from prefix.suffix,
 * or the given default if it doesn't exist. */

char *cfgVal(char *name);
/* Return option with given name.  Squawk and die if it
 * doesn't exist. */

struct slName *cfgNames();
/* get list of names in config file. slFreeList when finished */

struct slName *cfgNamesWithPrefix();
/* get list of names in config file with prefix. slFreeList when finished */

unsigned long cfgModTime();
/* Return modification time of config file */

#endif
