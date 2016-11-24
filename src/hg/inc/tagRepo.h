
#ifndef TAGSTORMREPO_H
#define TAGSTORMREPO_H

#include "tagStorm.h"

struct slPair *tagRepoAllPairs(char *database);
struct slPair *tagRepoAllValues(char *database, char *tag);
struct slPair *tagRepoPairs(char *tagStormFile, char *tagName,  char *tagValue);
#endif
