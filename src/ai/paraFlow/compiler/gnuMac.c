/* gnuMac - Stuff concerned especially with code generation for
 * the Gnu Mac pentium assembler . */

#include "common.h"
#include "dlist.h"
#include "pfParse.h"
#include "pfToken.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfCompile.h"
#include "pfPreamble.h"
#include "isx.h"
#include "pentCode.h"
#include "backEnd.h"
#include "gnuMac.h"

#define cstrPrefix "JK_"	/* Prefix before string constants */

void gnuMacModuleVars(struct dlList *iList, FILE *f)
/* Print out info on uninitialized variables - actually all vars..... */
{
struct dlNode *node;
for (node = iList->head; !dlEnd(node); node = node->next)
    {
    struct isx *isx = node->val;
    struct isxAddress *dest = isx->dest;
    struct pfVar *var = dest->val.var;
    enum pfAccessType access = var->ty->access;
    boolean isGlobal = (access == paGlobal || access == paWritable);
    int size = pentTypeSize(dest->valType);
    if (isGlobal)
	{
	fprintf(f, ".align %d\n", size);
	fprintf(f, ".comm");
	fprintf(f, " %s%s,%d\n", isxPrefixC, var->cName,
		size);
	}
    else
	{
	fprintf(f, ".lcomm");
	fprintf(f, " %s%s,%d,%d\n", isxPrefixC, var->cName,
		size, size);
	}
    }
}

void gnuMacModulePreamble(FILE *f)
/* Print out various incantations needed at start of every
 * source file for working on Mac OS X on Pentiums, or at
 * least on my mini. Also print initialized vars. */
{
fprintf(f, "# ParaFlow Pentium Output\n\n");
fprintf(f, "%s",
"# Preamble found in all modules for Mac OS-X Pentium\n"
"       .text\n"
"doNothing:\n"
"       ret\n"
"\n"
"       .data\n"
"       .align 3\n"
"longLongMinusOne:\n"
"       .long -1\n"
"       .long -1\n"
"bunchOfZero:\n"
"       .long 0\n"
"       .long 0\n"
"       .long 0\n"
"       .long 0\n"
"\n"
#ifdef OLD
"       .cstring\n"
"LC0:\n"
"       .ascii \"%d\\12\\0\"\n"
"       .text\n"
".globl __printInt\n"
"__printInt:\n"
"       pushl   %ebp\n"
"       movl    %esp, %ebp\n"
"       pushl   %ebx\n"
"       subl    $20, %esp\n"
"       call    ___i686.get_pc_thunk.bx\n"
"\"L00000000001$pb\":\n"
"       movl    8(%ebp), %eax\n"
"       movl    %eax, 4(%esp)\n"
"       leal    LC0-\"L00000000001$pb\"(%ebx), %eax\n"
"       movl    %eax, (%esp)\n"
"       call    L_printf$stub\n"
"       addl    $20, %esp\n"
"       popl    %ebx\n"
"       popl    %ebp\n"
"       ret\n"
"	.cstring\n"
"LC1:\n"
"	.ascii \"%f\\12\\0\"\n"
"	.text\n"
".globl __printDouble\n"
"__printDouble:\n"
"	pushl	%ebp\n"
"	movl	%esp, %ebp\n"
"	pushl	%ebx\n"
"	subl	$20, %esp\n"
"	call	___i686.get_pc_thunk.bx\n"
"\"L00000000002$pb\":\n"
"	movsd	8(%ebp), %xmm0\n"
"	movsd	%xmm0, 4(%esp)\n"
"	leal	LC1-\"L00000000002$pb\"(%ebx), %eax\n"
"	movl	%eax, (%esp)\n"
"	call	L_printf$stub\n"
"	addl	$20, %esp\n"
"	popl	%ebx\n"
"	popl	%ebp\n"
"	ret\n"
"LC2:\n"
"	.ascii \"%lld\\12\\0\"\n"
"	.text\n"
".globl __printLong\n"
"__printLong:\n"
"	pushl	%ebp\n"
"	movl	%esp, %ebp\n"
"	pushl	%ebx\n"
"	subl	$20, %esp\n"
"	call	___i686.get_pc_thunk.bx\n"
"\"L00000000003$pb\":\n"
"	movsd	8(%ebp), %xmm0\n"
"	movsd	%xmm0, 4(%esp)\n"
"	leal	LC2-\"L00000000003$pb\"(%ebx), %eax\n"
"	movl	%eax, (%esp)\n"
"	call	L_printf$stub\n"
"	addl	$20, %esp\n"
"	popl	%ebx\n"
"	popl	%ebp\n"
"	ret\n"
#endif /* OLD */

#ifdef OLD 
".globl __addTwo\n"
"__addTwo:\n"
"       pushl   %ebp\n"
"       movl    %esp, %ebp\n"
"       movl    12(%ebp), %eax\n"
"       addl    8(%ebp), %eax\n"
"       popl    %ebp\n"
"       ret\n"
#endif /* OLD */
"\n"
);
}

void gnuMacFunkyThunky(FILE *f)
/* Do that call to the funky get thunk thingie
 * that Mac does to make the code more relocatable
 * at the expense of burning the ebx register and
 * adding overhead to every single d*ng subroutine
 * almost! */
{
fprintf(f, "\tcall    ___i686.get_pc_thunk.bx\n");
}

void gnuMacModulePostscript(FILE *f)
/* Print out various incantations needed at end of every
 * source file for working on Mac OS X on Pentiums, or at
 * least on my mini. Also print uninitialized vars. */
{
fprintf(f, "%s",
"\n"
#ifdef OLD 
"# Postscript found in all modules for Mac OS-X Pentium\n"
"       .section __IMPORT,__jump_table,symbol_stubs,self_modifying_code+pure_instructions,5\n"
"L_printf$stub:\n"
"       .indirect_symbol _printf\n"
"       hlt ; hlt ; hlt ; hlt ; hlt\n"
"       .indirect_symbol __pfaNewString\n"
"       hlt ; hlt ; hlt ; hlt ; hlt\n"
"       .subsections_via_symbols\n"
"       .section __TEXT,__textcoal_nt,coalesced,pure_instructions\n"
".weak_definition       ___i686.get_pc_thunk.bx\n"
".private_extern        ___i686.get_pc_thunk.bx\n"
"___i686.get_pc_thunk.bx:\n"
"       movl    (%esp), %ebx\n"
"       ret\n"
#endif /* OLD */
);
}

void gnuMacMainStart(FILE *f)
/* Declare main function start. */
{
fprintf(f, "%s",
"# Main routine\n"
".globl _main\n"
"_main:\n"
"       pushl   %ebp\n"
"       movl    %esp, %ebp\n"
"       pushl   %edi\n"
"       pushl   %esi\n"
"       pushl   %ebx\n"
"       subl    $124, %esp\n"
/* NOTE: It looks like the MacTel is veeerrrry picky about
 * what the stack is at.  It seems to want to be on boundaries
 * that are even multiples of 16.  It somehow messes with the
 * 'calling helpers' they have otherwise, leading to illegal
 * instruction crashes.  I think this is all part of the
 * attempts to make the code relocatable somehow. */
);
}

void gnuMacMainEnd(FILE *f)
/* Declare main function end. */
{
fprintf(f, "%s",
"       movl    $0,%eax\n"
"       addl    $124, %esp\n"
"       popl    %ebx\n"
"       popl    %esi\n"
"       popl    %edi\n"
"       popl    %ebp\n"
"       ret\n"
);
}

static void dataSegment(struct pfBackEnd *backEnd, FILE *f)
/* Switch to data segment */
{
if (backEnd->segment != pfbData)
    {
    fprintf(f, "\t.data\n");
    backEnd->segment = pfbData;
    }
}

static void codeSegment(struct pfBackEnd *backEnd, FILE *f)
/* Switch to code segment */
{
if (backEnd->segment != pfbCode)
    {
    fprintf(f, "\t.text\n");
    backEnd->segment = pfbCode;
    }
}

static void bssSegment(struct pfBackEnd *backEnd, FILE *f)
/* Switch to bss segment */
{
codeSegment(backEnd, f);	/* Seems to be case on mac....??? */
}

static void stringSegment(struct pfBackEnd *backEnd, FILE *f)
/* Switch to string segment */
{
if (backEnd->segment != pfbString)
    {
    fprintf(f, "\t.cstring\n");
    backEnd->segment = pfbString;
    }
}

static void emitLabel(struct pfBackEnd *backEnd, char *label, 
	int aliSize, boolean isGlobal, FILE *f)
/* Emit label aligned to aliSize. */
{
if (aliSize > 8)
    fprintf(f, "\t.align\t4\n");
else if (aliSize > 4)
    fprintf(f, "\t.align\t3\n");
else if (aliSize > 2)
    fprintf(f, "\t.align\t2\n");
if (isGlobal)
    fprintf(f, ".globl\t%s\n", label);
fprintf(f, "%s:\n", label);
}

static void emitAscii(struct pfBackEnd *backEnd, char *string,int size,FILE *f)
/* Emit string of ascii chars of given size. */
{
char c;
int i;
fprintf(f, "\t.ascii\t\"");
for (i=0; i<size; ++i)
    {
    c = string[i];
    switch (c)
        {
	case '"':
	case '\\':
	   fputc('\\', f);
	   fputc(c, f);
	   break;
	default:
	   if (isprint(c))
	      fputc(c, f);
	   else
	      {
	      fputc('\\', f);
	      fprintf(f, "%o", c);
	      }
	   break;
	}
    }
fprintf(f, "\\0\"\n");
}

static void emitByte(struct pfBackEnd *backEnd, _pf_Byte x, FILE *f)
/* Emit Byte */
{
fprintf(f, "\t.byte\t%d\n", x);
}

static void emitShort(struct pfBackEnd *backEnd, _pf_Short x, FILE *f)
/* Emit Short */
{
fprintf(f, "\t.short\t%d\n", x);
}

static void emitInt(struct pfBackEnd *backEnd, _pf_Int x, FILE *f)
/* Emit Int */
{
fprintf(f, "\t.long\t%d\n", x);
}

static void emitLong(struct pfBackEnd *backEnd, _pf_Long x, FILE *f)
/* Emit Long */
{
_pf_Int *p = (_pf_Int*)&x;
fprintf(f, "\t.long\t%d\n", p[0]);
fprintf(f, "\t.long\t%d\n", p[1]);
}

static void emitFloat(struct pfBackEnd *backEnd, _pf_Float x, FILE *f)
/* Emit Float */
{
_pf_Int *p = (_pf_Int*)&x;
fprintf(f, "\t.long\t%d\n", p[0]);
}

static void emitDouble(struct pfBackEnd *backEnd, _pf_Double x, FILE *f)
/* Emit Double */
{
_pf_Int *p = (_pf_Int*)&x;
fprintf(f, "\t.long\t%d\n", p[0]);
fprintf(f, "\t.long\t%d\n", p[1]);
}

static void emitPointer(struct pfBackEnd *backEnd, char *label, FILE *f)
/* Emit Pointer */
{
fprintf(f, "\t.long\t%s\n", label);
}

struct pfBackEnd macPentiumBackEnd = {
/* Interface to mac-pentium back end. */
    "mac-pentium",
    pfbNone,
    "_",
    dataSegment,
    codeSegment,
    bssSegment,
    stringSegment,
    emitLabel,
    emitAscii,
    emitByte,
    emitShort,
    emitInt,
    emitLong,
    emitFloat,
    emitDouble,
    emitPointer,
    };

