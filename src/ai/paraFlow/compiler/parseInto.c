/* parseInto - Handle parsing across multiple modules. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "localmem.h"
#include "portable.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfToken.h"
#include "pfCompile.h"
#include "pfParse.h"
#include "pfBindVars.h"
#include "defFile.h"
#include "parseInto.h"

static void collectDots(struct dyString *dy, struct pfParse *pp)
/* Make sure that pp is really a pptDots or a pptName. 
 * Output name plus any necessary dots into dy. */
{
if (pp->type == pptNameUse)
    {
    dyStringAppend(dy, pp->name);
    return;
    }
else if (pp->type == pptDot)
    {
    for (pp = pp->children; pp != NULL; pp = pp->next)
        {
	switch (pp->type)
	    {
	    case pptNameUse:
		dyStringAppend(dy, pp->name);
		if (pp->next != NULL)
		    dyStringAppend(dy, ".");
		break;
	    case pptRoot:
	        dyStringAppend(dy, "/");
		break;
	    case pptParent:
	        dyStringAppend(dy, "../");
		break;
	    case pptSys:
	    case pptUser:
	    case pptSysOrUser:
	        break;
	    }
	}
    }
else
    expectingGot("name", pp->tok);
}

static struct pfParse *createAndParseDef(struct pfCompile *pfc, 
	struct pfToken *tok, char *module, struct pfScope *scope)
/* If .pfh file exists and is more recent than .pf then parse it.  
 * Otherwise parse .pf file and save .pfh for next time. */
{
/* Get tokenizer and save it's current state. */
struct pfTokenizer *tkz = pfc->tkz;
struct pfSource *oldSource = tkz->source;
char *oldPos = tkz->pos;
char *oldEndPos = tkz->endPos;
struct pfSource *source;
struct pfParse *mod;
char pfName[PATH_LEN], pfhName[PATH_LEN];
boolean needRemake = TRUE;
char *modName;
enum pfParseType modType;

safef(pfName, sizeof(pfName), "%s%s.pf", pfc->baseDir, module);
safef(pfhName, sizeof(pfhName), "%s%s.pfh", pfc->baseDir, module);
if (!fileExists(pfName))
    errAt(tok, "%s doesn't exist", pfName);
if (fileExists(pfhName))
    {
    unsigned long pfTime = fileModTime(pfName);
    unsigned long pfhTime = fileModTime(pfhName);
    needRemake = (pfTime >= pfhTime);
    }

if (needRemake)
    {
    modName = pfName;
    modType = pptModule;
    }
else
    {
    modName = pfhName;
    modType = pptModuleRef;
    }
    
source = tkz->source = pfSourceOnFile(modName);
tkz->pos = source->contents;
tkz->endPos = tkz->pos + source->contentSize;
mod = pfParseSource(pfc, source, NULL, scope, modType);

if (needRemake)
    pfMakeDefFile(pfc, mod, pfhName);
else
    mod->type = pptModuleRef;

/* Restore tokenizer state. */
tkz->source = oldSource;
tkz->pos = oldPos;
tkz->endPos = oldEndPos;
return mod;
}

void expandInto(struct pfParse *program, 
	struct pfCompile *pfc, struct pfParse *pp, struct pfScope *scope)
/* Go through program walking into modules. */
{
if (pp->type == pptInto)
    {
    struct dyString *dy = dyStringNew(0);
    struct hashEl *hel;
    char *module = NULL;
    collectDots(dy, pp->children);

    hel = hashLookup(pfc->modules, dy->string);
    if (hel != NULL)
        module = hel->name;

    if (module == NULL)
        {
	struct pfParse *mod = createAndParseDef(pfc, 
		pp->children->tok, dy->string, scope);
	mod->parent = program;
	module = hashStoreName(pfc->modules, dy->string);
	expandInto(program, pfc, mod, scope);
	slAddHead(&program->children, mod);
	}
    pp->name = module;
    dyStringFree(&dy);
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    expandInto(program, pfc, pp, scope);
}

void pfParseTypeSub(struct pfParse *pp, enum pfParseType oldType,
        enum pfParseType newType)
/* Convert type of pp and any children that are of oldType to newType */
{
if (pp->type == oldType)
    pp->type = newType;
for (pp = pp->children; pp != NULL; pp = pp->next)
    pfParseTypeSub(pp, oldType, newType);
}

static struct pfType *findTypeInModule(struct pfParse *module, char *typeName)
/* Return base type in module or die. */
{
struct pfBaseType *base = pfScopeFindType(module->scope, typeName);
if (base == NULL)
    errAbort("Can't find class %s in %s", typeName, module->name);
return pfTypeNew(base);
}

static void attachStringsAndThings(struct pfCompile *pfc, struct pfParse *stringModule)
/* Finish parsing, binding and type checking the string module.  Rummage around for 
 * string class in it and attach it to pfc->stringFullType. */
{
pfBindVars(pfc, stringModule);
pfTypeCheck(pfc, &stringModule);
pfc->stringFullType = findTypeInModule(stringModule, "_pf_string");
pfc->arrayFullType = findTypeInModule(stringModule, "_pf_array");
}


struct pfParse *pfParseInto(struct pfCompile *pfc, char *builtinCode, 
	char *fileName)
/* Parse file.  Also parse .pfh files for any modules the main file goes into.
 * If the .pfh files don't exist then create them.  Returns parse tree
 * of file and associated .pfh files.  Variables are not yet bound. */
{
struct pfParse *program = pfParseNew(pptProgram, NULL, NULL, pfc->scope);
struct pfSource *stringSource = pfSourceNew("<string>", fetchStringDef());
struct pfScope *stringScope = pfScopeNew(pfc, pfc->scope, 3, TRUE);
struct pfParse *stringModule = pfParseSource(pfc, stringSource, 
	program, stringScope, pptModuleRef);
struct pfSource *builtinSource = pfSourceNew("<builtin>", builtinCode);
struct pfParse *builtinModule = pfParseSource(pfc, builtinSource, 
	program, pfc->scope, pptModuleRef);
struct pfSource *mainSource = pfSourceOnFile(fileName);
struct pfScope *userScope  = pfScopeNew(pfc, pfc->scope, 16, TRUE);
struct pfParse *mainModule = pfParseSource(pfc, mainSource, 
	program, userScope, pptMainModule);

hashStoreName(pfc->modules, mainModule->name);
attachStringsAndThings(pfc, stringModule);

/* Go into other modules, and put initial module at end of list. */
slAddHead(&program->children, mainModule);
expandInto(program, pfc, mainModule, userScope);
slAddHead(&program->children, builtinModule);
slReverse(&program->children);

/* Restore order of scopes. */
slReverse(&pfc->scopeList);

return program;
}


