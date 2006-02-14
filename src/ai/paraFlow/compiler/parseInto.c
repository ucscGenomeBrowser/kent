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

#ifdef OLD_BUT_KEEP_FOR_REFERENCE
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
#endif /* OLD_BUT_KEEP_FOR_REFERENCE */

#ifdef OLD_BUT_KEEP_FOR_REFERENCE
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
#endif /* OLD_BUT_KEEP_FOR_REFERENCE */

#ifdef OLD
static void expandInclude(struct pfParse *program, 
	struct pfCompile *pfc, struct pfParse *pp, struct pfScope *scope)
/* Go through program walking into modules. */
{
if (pp->type == pptInclude)
    {
    struct dyString *dy = dyStringNew(0);
    struct hashEl *hel;
    char *module = NULL;
    collectDots(dy, pp->children);

    hel = hashLookup(pfc->moduleHash, dy->string);
    if (hel != NULL)
        module = hel->name;

    if (module == NULL)
        {
	struct pfParse *mod = createAndParseDef(pfc, 
		pp->children->tok, dy->string, scope);
	mod->parent = program;
	module = hashStoreName(pfc->moduleHash, dy->string);
	expandInclude(program, pfc, mod, scope);
	slAddHead(&program->children, mod);
	}
    pp->name = module;
    dyStringFree(&dy);
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    expandInclude(program, pfc, pp, scope);
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
#endif /* OLD */

static struct pfType *findTypeInModule(struct pfParse *module, char *typeName)
/* Return base type in module or die. */
{
struct pfBaseType *base = pfScopeFindType(module->scope, typeName);
if (base == NULL)
    errAbort("Can't find class %s in %s", typeName, module->name);
return pfTypeNew(base);
}

static void attachStringsAndThings(struct pfCompile *pfc, 
	struct pfParse *stringModule)
/* Finish parsing, binding and type checking the string module.  Rummage around for 
 * string class in it and attach it to pfc->stringFullType. */
{
pfBindVars(pfc, stringModule);
pfTypeCheck(pfc, &stringModule);
pfc->stringFullType = findTypeInModule(stringModule, "_pf_string");
pfc->arrayFullType = findTypeInModule(stringModule, "_pf_array");
pfc->elTypeFullType = findTypeInModule(stringModule, "_pf_elType");
pfc->arrayType->methods = pfc->arrayFullType->base->methods;
}

static void addCompoundScopes(struct pfCompile *pfc, struct pfToken *tokList,
	struct pfScope *scope)
/* Add scoping info to opening braces in token list */
{
int braceDepth = 0;
struct pfToken *tok;
for (tok = tokList; tok != NULL; tok = tok->next)
    {
    if (tok->type == '{')
        {
	++braceDepth;
	scope = pfScopeNew(pfc, scope, 0, FALSE);
	tok->val.scope = scope;
	}
    else if (tok->type == '}')
        {
	if (braceDepth == 0)
	    errAt(tok, "} without preceding {");
	--braceDepth;
	scope = scope->parent;
	tok->val.scope = scope;
	}
    }
if (braceDepth != 0)
    errAbort("Unclosed brace in %s", tokList->source->name);
}

static void addClasses(struct pfCompile *pfc, struct pfToken *tokList, 
	struct pfScope *scope)
/* Add types in classes to appropriate scope.  We do this as
 * a first pass to help disambiguate the grammar. */
{
struct pfToken *tok;
for (tok = tokList; tok->type != pftEnd; tok = tok->next)
    {
    if (tok->type == '{' || tok->type == '}')
	{
        scope = tok->val.scope;
	}
    else if (tok->type == pftClass)
	{
	struct pfBaseType *base;
	tok = tok->next;
	if (tok->type != pftName)
	    expectingGot("class name", tok);
	base = pfScopeAddType(scope, tok->val.s, FALSE, 
		pfc->classType, sizeof(void *), TRUE);
	base->isClass = TRUE;
	}
    else if (tok->type == pftInterface)
	{
	struct pfBaseType *base;
	tok = tok->next;
	if (tok->type != pftName)
	    expectingGot("interface name", tok);
	base = pfScopeAddType(scope, tok->val.s, FALSE,
	    pfc->interfaceType, sizeof(void *), TRUE);
	base->isInterface = TRUE;
	}
    }
}

struct pfParse *pfParseInto(struct pfCompile *pfc)
/* Parse file.  Also parse .pfh files for any modules the main file goes into.
 * If the .pfh files don't exist then create them.  Returns parse tree
 * of file and associated .pfh files.  Variables are not yet bound. */
{
struct pfParse *program = pfParseNew(pptProgram, NULL, NULL, pfc->scope);
struct pfModule *stringModule = hashMustFindVal(pfc->moduleHash, "<string>");
struct pfModule *builtInModule = hashMustFindVal(pfc->moduleHash, "<builtIn>");
struct pfScope *sysScope = pfc->scope;
struct pfScope *userScope  = pfScopeNew(pfc, pfc->scope, 16, TRUE);
struct pfModule *mod;

/* Loop through assigning scopes and recording classes. */
for (mod = pfc->moduleList; mod != NULL; mod = mod->next)
    {
    struct pfScope *scope = (mod->name[0] == '<' ? sysScope : userScope);
    addCompoundScopes(pfc, mod->tokList, scope);
    addClasses(pfc, mod->tokList, scope);
    }

/* Compile everything. */
for (mod = pfc->moduleList; mod != NULL; mod = mod->next)
    {
    struct pfParse *modPp;
    boolean isSys = (mod->name[0] == '<');
    struct pfScope *scope = (isSys ? sysScope : userScope);
    enum pfParseType type = pptMainModule;
    if (mod != pfc->moduleList)
	type = (mod->isPfh ? pptModuleRef : pptModule);
    pfc->isSys = isSys;
    modPp = pfParseModule(pfc, mod, program, scope, type);
    modPp->name = mod->name;
    /* Exclude string module since code generator can't handle it. */
    if (mod != stringModule)
	slAddHead(&program->children, modPp);
    if (type != pptModuleRef)
        {
	char defFile[PATH_LEN];
	safef(defFile, sizeof(defFile), "%s%s.pfh", pfc->baseDir, mod->name);
	pfMakeDefFile(pfc, modPp, defFile);
	}
    }
slReverse(&program->children);
attachStringsAndThings(pfc, stringModule->pp);

/* Restore order of scopes. */
slReverse(&pfc->scopeList);

return program;
}


