/* cMain - Print out our main module, which is in C even if
 * we are generating the bulk of the code in assembler. */

#include "common.h"
#include "hash.h"
#include "dyString.h"
#include "pfCompile.h"
#include "pfParse.h"
#include "codedType.h"
#include "recodedType.h"
#include "cMain.h"

char *cStackName = "_pf_stack";
char *cStackType = "_pf_Stack";

void cPrintPreamble(struct pfCompile *pfc, FILE *f, char *fileName, 
	boolean doExtern)
/* Print out C code for preamble. */
{
fprintf(f, "/* This file is a translation of %s by paraFlow. */\n", 
	fileName);
fprintf(f, "\n");
fprintf(f, "#include \"pfPreamble.h\"\n");
fprintf(f, "\n");
if (doExtern)
   fprintf(f, "extern ");
fprintf(f, "_pf_Var _pf_var_zero;   /* Helps initialize vars to zero. */\n");
if (doExtern)
   fprintf(f, "extern ");
fprintf(f, "_pf_Array %sargs;	    /* Command line arguments go here. */\n",
	pfcGlobalPrefix);
fprintf(f, "static _pf_Bit %strue=1, %sfalse=0;\n", pfcGlobalPrefix, pfcGlobalPrefix);
if (doExtern)
   fprintf(f, "extern ");
fprintf(f, "_pf_String %sprogramName; /* Name of program (argv[0]) */\n",
	pfcGlobalPrefix);
fprintf(f, "\n");
}

static void printSysVarsAndPrototypes(FILE *f)
/* Print stuff needed for main() */
{
fprintf(f, "struct _pf_activation *_pf_activation_stack;\n");
fprintf(f, "struct %s *%s;\n", cModuleRuntimeType, cModuleRuntimeList);
fprintf(f, "void _pf_init_args(int argc, char **argv, _pf_String *retProg, _pf_Array *retArgs, char *environ[]);\n");
fprintf(f, "\n");
}

static void printModuleTable(struct pfCompile *pfc, FILE *f, 
	struct pfParse *program)
/* Print out table with basic info on each module */
{
struct pfParse *module;
int moduleCount = 0;
for (module = program->children; module != NULL; module = module->next)
    {
    if (module->name[0] != '<')
        {
	char *mangledName = mangledModuleName(module->name);
	fprintf(f, "extern struct %s %s_%s[];\n", 
	    recodedStructType, recodedTypeTableName, mangledName);
        fprintf(f, "extern struct _pf_poly_info _pf_poly_info_%s[];\n",
		mangledName);
	fprintf(f, "void _pf_entry_%s(%s *%s);\n", 
		mangledName, cStackType, cStackName);
	}
    }
fprintf(f, "\n");
fprintf(f, "struct _pf_module_info _pf_module_info[] = {\n");
for (module = program->children; module != NULL; module = module->next)
    {
    if (module->name[0] != '<')
        {
	char *mangledName = mangledModuleName(module->name);
	fprintf(f, "  {\"%s\", %s_%s, _pf_poly_info_%s, _pf_entry_%s,},\n",
	    mangledName, recodedTypeTableName, mangledName, mangledName,
	    mangledName);
	++moduleCount;
	}
    }
fprintf(f, "};\n");
fprintf(f, "int _pf_module_info_count = %d;\n\n", moduleCount);
}

static void printConclusion(struct pfCompile *pfc, FILE *f, 
	struct pfParse *mainModule)
/* Print out C code for end of program. */
{
fprintf(f, 
"\n"
"int main(int argc, char *argv[], char *environ[])\n"
"{\n"
"static _pf_Stack stack[16*1024];\n"
"_pf_init_mem();\n"
"_pf_init_types(_pf_base_info, _pf_base_info_count,\n"
"               _pf_type_info, _pf_type_info_count,\n"
"               _pf_field_info, _pf_field_info_count,\n"
"               _pf_module_info, _pf_module_info_count);\n"
"_pf_init_args(argc, argv, &%sprogramName, &%sargs, environ);\n"
"_pf_punt_init();\n"
"_pf_paraRunInit();\n"
"_pf_entry_%s(stack);\n"
"while (%s != 0)\n"
"    {\n"
"    %s->cleanup(%s, stack);\n"
"    %s = %s->next;\n"
"    }\n"
"return 0;\n"
"}\n", 
pfcGlobalPrefix, pfcGlobalPrefix, mangledModuleName(mainModule->name),
cModuleRuntimeList, cModuleRuntimeList, cModuleRuntimeList,
cModuleRuntimeList, cModuleRuntimeList);
}

void cMain(struct pfCompile *pfc, struct pfParse *program, char *fileName)
/* cMain - Print out our main module, which is in C even if
 * we are generating the bulk of the code in assembler.  This
 * includes type info, a module table, and the main entry point. */
{
FILE *f = mustOpen(fileName, "w");
struct pfParse *mainModule = program->children;
assert(mainModule->type == pptMainModule);
cPrintPreamble(pfc, f, fileName, FALSE);
printSysVarsAndPrototypes(f);
printModuleTable(pfc, f, program);
codedTypesToC(pfc, program, f);
printConclusion(pfc, f, mainModule);
carefulClose(&f);
}
