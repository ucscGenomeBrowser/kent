#ifndef HGCONFIG_H
#define HGCONFIG_H

char* cfgOption(char* name);
/* return the option with the given name, or null if not found */

char* cfgOptionDefault(char* name, char* def);
/* return the option with the given name or the given default if it doesn't exist */

char *cfgVal(char *name);
/* Return option with given name.  Squawk and die if it
 * doesn't exist. */

unsigned long cfgModTime();
/* Return modification time of config file */

#endif
