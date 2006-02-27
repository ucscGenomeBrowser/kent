/* tokInto - tokenize main module, and also any modules that it
 * goes into.  Leave results in pfc->moduleList. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "portable.h"
#include "pfToken.h"
#include "pfParse.h"
#include "pfCompile.h"

static struct pfModule *tokenizeSource(struct pfTokenizer *tkz,
	struct pfSource *source, char *modName)
/* Attatch source to tokenizer and read all tokens. 
 * Create a pfModule structure to hold results. */
{
struct pfToken *tokList = NULL, *tok;
struct pfModule *module;
int endCount = 3;

/* Suck in all tokens from source. */
pfTokenizerNewSource(tkz, source);
while ((tok = pfTokenizerNext(tkz)) != NULL)
    {
    slAddHead(&tokList, tok);
    }

/* Add enough ends to satisfy all look-aheads */
while (--endCount >= 0)
    {
    AllocVar(tok);
    tok->type = pftEnd;
    tok->source = tkz->source;
    tok->text = tkz->endPos-1;
    slAddHead(&tokList, tok);
    }
slReverse(&tokList);

/* Create and return module. */
AllocVar(module);
module->name = cloneString(modName);
module->tokList = tokList;
module->source = source;
return module;
}

static struct pfModule *tokenizeFile(struct pfTokenizer *tkz,
	char *fileName, char *modName)
/* Tokenize a file. */
{
struct pfSource *source = pfSourceOnFile(fileName);
return tokenizeSource(tkz, source, modName);
}

void rTokInto(struct pfCompile *pfc, char *baseDir, char *modName,
    boolean lookForPfh)
/* Tokenize module, and recursively any thing it goes into. */
{
struct dyString *pfPath = dyStringNew(0);
struct dyString *pfhPath = dyStringNew(0);
struct dyString *oPath = dyStringNew(0);
char *fileName = NULL;
struct pfModule *module;
boolean isPfh = FALSE;
struct pfToken *tok;


/* Create path names, and make sure .pf file exists. */
dyStringAppend(pfPath, baseDir);
dyStringAppend(pfPath, modName);
dyStringAppend(pfPath, ".pf");
dyStringAppend(pfhPath, baseDir);
dyStringAppend(pfhPath, modName);
dyStringAppend(pfhPath, ".pfh");
dyStringAppend(oPath, baseDir);
dyStringAppend(oPath, modName);
dyStringAppend(oPath, ".o");
if (!fileExists(pfPath->string))
    errAbort("Can't find %s", pfPath->string);

/* Look too see if can use just module header.
 * We can if it exists and is newer than module. */
if (lookForPfh)
    {
    if (fileExists(pfhPath->string) && fileExists(oPath->string))
	{
	if (fileModTime(pfhPath->string) > fileModTime(pfPath->string))
	    {
	    fileName = pfhPath->string;
	    isPfh = TRUE;
	    }
	}
    }
if (fileName == NULL)
    fileName = pfPath->string;

/* Tokenize file and add module to hash. */
module = tokenizeFile(pfc->tkz, fileName, modName);
module->isPfh = isPfh;
hashAdd(pfc->moduleHash, modName, module);

/* Look and see if there are any 'include' or 'import' to follow. */
for (tok = module->tokList; tok != NULL; tok = tok->next)
    {
    if (tok->type == pftInclude || tok->type == pftImport)
        {
	struct pfToken *modTok = tok->next;
	if (modTok->type != pftString)
	   expectingGot("module name in single quotes", modTok); 
	if (sameString(modTok->val.s, modName))
	    errAt(modTok, "A module can't include/import itself.");
	if (!hashLookup(pfc->moduleHash, modTok->val.s))
	    rTokInto(pfc, baseDir, modTok->val.s, TRUE);
	}
    }

/* Add module to list */
slAddHead(&pfc->moduleList, module);

/* Clean up and go home. */
dyStringFree(&pfPath);
dyStringFree(&pfhPath);
dyStringFree(&oPath);
}

static void addBuiltIn(struct pfCompile *pfc, char *code, char *modName)
/* Tokenize a little built-in module and hang it on pfc. */
{
struct pfSource *source = pfSourceNew(modName, code);
struct pfModule *mod = tokenizeSource(pfc->tkz, source, modName);
mod->isPfh = TRUE;
hashAdd(pfc->moduleHash, modName, mod);
slAddHead(&pfc->moduleList, mod);
}

void pfTokenizeInto(struct pfCompile *pfc, char *baseDir, char *modName)
/* Tokenize module, and any modules that it goes into. */
{
addBuiltIn(pfc, fetchStringDef(), "<string>");
addBuiltIn(pfc, fetchBuiltInCode(), "<builtIn>");
rTokInto(pfc, baseDir, modName, FALSE);
}

