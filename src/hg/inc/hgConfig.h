#ifndef HGCONFIG_H
#define HGCONFIG_H

/* return the option with the given name, or null if not found */
char* cfgOption(char* name);

/* return the option with the given name or the given default if it doesn't exist */
char* cfgOptionDefault(char* name, char* def);

#endif
