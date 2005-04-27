/* tokInto - tokenize main module, and also any modules that it
 * goes into.  Leave results in pfc->moduleList. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
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
struct dyString *path = dyStringNew(0);
char *fileName = NULL;
struct pfModule *module;
boolean isPfh = FALSE;
struct pfToken *tok;

/* Look for just module header if possible. */
if (lookForPfh)
    {
    dyStringAppend(path, baseDir);
    dyStringAppend(path, modName);
    dyStringAppend(path, ".pfh");
    if (fileExists(path->string))
	{
        fileName = path->string;
	isPfh = TRUE;
	}
    }
if (fileName == NULL)
    {
    dyStringClear(path);
    dyStringAppend(path, baseDir);
    dyStringAppend(path, modName);
    dyStringAppend(path, ".pf");
    fileName = path->string;
    }

/* Tokenize file and add module to hash. */
module = tokenizeFile(pfc->tkz, fileName, modName);
module->isPfh = isPfh;
hashAdd(pfc->moduleHash, modName, module);

/* Look and see if there are any 'into' to follow. */
for (tok = module->tokList; tok != NULL; tok = tok->next)
    {
    if (tok->type == pftInclude)
        {
	struct pfToken *modTok = tok->next;
	if (modTok->type != pftName)
	   expectingGot("module name", modTok); 
	if (!hashLookup(pfc->moduleHash, modTok->val.s))
	    rTokInto(pfc, baseDir, modTok->val.s, TRUE);
	}
    }

/* Add module to list */
slAddHead(&pfc->moduleList, module);

/* Clean up and go home. */
dyStringFree(&path);
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

