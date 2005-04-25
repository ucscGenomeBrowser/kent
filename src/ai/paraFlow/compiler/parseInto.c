/* parseInto - Handle parsing across multiple modules. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "localmem.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfToken.h"
#include "pfCompile.h"
#include "pfParse.h"
#include "pfBindVars.h"
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

static struct pfParse *pfParseFile(struct pfCompile *pfc, char *fileName, 
	struct pfParse *parent)
/* Convert file to parse tree using tkz. */
{
struct pfSource *source = pfSourceOnFile(fileName);
struct pfScope *scope = pfScopeNew(pfc, pfc->scope, 16, TRUE);
return pfParseSource(pfc, source, parent, scope);
}


void expandInto(struct pfParse *program, 
	struct pfCompile *pfc, struct pfParse *pp)
/* Go through program walking into modules. */
{
if (pp->type == pptInto)
    {
    struct dyString *dy = dyStringNew(0);
    struct hashEl *hel;
    char *module = NULL;
    collectDots(dy, pp->children);
    dyStringAppend(dy, ".pf");

    hel = hashLookup(pfc->modules, dy->string);
    if (hel != NULL)
        module = hel->name;

    if (module == NULL)
        {
	struct pfTokenizer *tkz = pfc->tkz;
	struct pfSource *oldSource = tkz->source;
	char *oldPos = tkz->pos;
	char *oldEndPos = tkz->endPos;
	struct pfParse *mod;
	tkz->source = pfSourceOnFile(dy->string);
	tkz->pos = tkz->source->contents;
	tkz->endPos = tkz->pos + tkz->source->contentSize;
	mod = pfParseFile(pfc, dy->string, program);
	expandInto(program, pfc, mod);
	tkz->source = oldSource;
	tkz->pos = oldPos;
	tkz->endPos = oldEndPos;
	slAddHead(&program->children, mod);
	module = hashStoreName(pfc->modules, dy->string);
	}
    pp->name = module;
    dyStringFree(&dy);
    }
for (pp = pp->children; pp != NULL; pp = pp->next)
    expandInto(program, pfc, pp);
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


struct pfParse *pfParseProgram(struct pfCompile *pfc, char *builtinCode, char *fileName)
/* Return parse tree of file and any files gone into. Variables are
 * not bound yet. */
{
struct pfParse *program = pfParseNew(pptProgram, NULL, NULL, pfc->scope);
struct pfSource *stringSource = pfSourceNew("<string>", fetchStringDef());
struct pfScope *stringScope = pfScopeNew(pfc, pfc->scope, 3, TRUE);
struct pfParse *stringModule = pfParseSource(pfc, stringSource, program, stringScope);
struct pfSource *builtinSource = pfSourceNew("<builtin>", builtinCode);
struct pfParse *builtinModule = pfParseSource(pfc, builtinSource, program, pfc->scope);
struct pfParse *mainModule = pfParseFile(pfc, fileName, program);

attachStringsAndThings(pfc, stringModule);

/* Go into other modules, and put initial module at end of list. */
slAddHead(&program->children, builtinModule);
slAddHead(&program->children, mainModule);
expandInto(program, pfc, mainModule);
slReverse(&program->children);

/* Restore order of scopes. */
slReverse(&pfc->scopeList);

return program;
}


struct pfParse *pfParseInto(struct pfCompile *pfc, char *builtinCode, char *fileName)
/* Parse file.  Also parse .pfh files for any modules the main file goes into.
 * If the .pfh files don't exist then create them. */
{
return pfParseProgram(pfc, builtinCode, fileName);
}
