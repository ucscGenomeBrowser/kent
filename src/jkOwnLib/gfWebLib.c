/* gfWebLib - some helper routines for the webBlat and webPcr.
 * Copyright 2004-2005 Jim Kent.  All rights reserved. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "gfWebLib.h"


struct gfServerAt *gfWebFindServer(struct gfServerAt *serverList, char *varName)
/* Find active server (that matches contents of CGI variable varName). */
{
struct gfServerAt *server = serverList;
if (cgiVarExists(varName))
     {
     char *db = cgiString(varName);
     for (server = serverList; server != NULL; server = server->next)
          {
	  if (sameString(db, server->name))
	      break;
	  }
     if (server == NULL)
          errAbort("%s %s not found", varName, db);
     }
return server;
}

struct gfWebConfig *gfWebConfigRead(char *fileName)
/* Read configuration file into globals. */
{
struct gfWebConfig *cfg;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *word;
struct hash *uniqHash = newHash(0), *uniqTransHash = newHash(0), *hash;

AllocVar(cfg);
cfg->company = "";
while (lineFileNextReal(lf, &line))
    {
    word = nextWord(&line);
    if (sameWord("company", word))
        {
	cfg->company = cloneString(trimSpaces(line));
	}
    else if (sameWord("gfServer", word) || sameWord("gfServerTrans", word))
        {
	struct gfServerAt *server;
	char *dupe = cloneString(line);
	AllocVar(server);
	server->host = nextWord(&dupe);
	server->port = nextWord(&dupe);
	server->seqDir = nextWord(&dupe);
	server->name = trimSpaces(dupe);
	if (server->name == NULL || server->name[0] == 0)
	    errAbort("Badly formed gfServer command line %d of %s:\n%s",
	    	lf->lineIx, fileName, line);
	if (sameString("gfServerTrans", word))
	    {
	    slAddTail(&cfg->transServerList, server);
	    hash = uniqTransHash;
	    }
	else
	    {
	    slAddTail(&cfg->serverList, server);
	    hash = uniqHash;
	    }
	if (hashLookup(hash, server->name))
	    errAbort("Duplicate %s name %s line %d of %s",
	    	word, server->name, lf->lineIx, fileName);
	hashAdd(hash, server->name, NULL);
	}
    else if (sameWord("tempDir", word))
        {
	cfg->tempDir = cloneString(trimSpaces(line));
	}
    else if (sameWord("background", word))
        {
	cfg->background = cloneString(trimSpaces(line));
	}
    else
        {
	errAbort("Unrecognized command %s line %d of  %s", word, lf->lineIx, fileName);
	}
    }

if (cfg->serverList == NULL && cfg->transServerList == NULL)
    errAbort("no gfServer's specified in %s", fileName);
freeHash(&uniqHash);
freeHash(&uniqTransHash);
return cfg;
}

