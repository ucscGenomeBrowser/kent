#include <stdio.h>
#include <math.h>
#include <string.h>
#include "calc.h"
#include "mfcalc.h"
#include "yy.h"

extern YYLTYPE yylloc;		/*  location data for the lookahead	*/
				/*  symbol				*/
symrec *sym_table = NULL;

symrec *putsym(char const *sym_name, int sym_type)
{
symrec *ptr;
ptr = (symrec *)malloc(sizeof(symrec));
ptr->name = strdup(sym_name);
ptr->value.var = 0;
ptr->type = sym_type;
ptr->next = sym_table;
sym_table = ptr;
return ptr;
}

symrec *getsym(char const *sym_name)
{
symrec *ptr;
for (ptr = sym_table; ptr != NULL; ptr = ptr->next)
    if (strcmp(ptr->name, sym_name) == 0)
	{
        return ptr;
	}
return NULL;
}

struct builtinFunction
    {
    char const *fname;
    double (*fnct)(double);
    };

struct builtinFunction const arith_fncts[] =
    {
    { "sin", sin, },
    { "cos", cos, },
    { "tan", tan, },
    { "asin", asin,},
    { "atan", atan, },
    { "ln", log, },
    { "exp", exp, },
    { "sqrt", sqrt, },
    { 0, 0, },
    };

void init_table()
/* Put arithmetic functions in table. */
{
int i;
symrec *ptr;
for (i=0; arith_fncts[i].fname != 0; i++)
    {
    ptr = putsym(arith_fncts[i].fname, FNCT);
    ptr->value.fnctptr = arith_fncts[i].fnct;
    }
ptr = putsym("pi", VAR);
ptr->value.var = 2*asin(1);
ptr = putsym("e", VAR);
ptr->value.var = exp(1);
}

int main(void)
{
yylloc.first_line = yylloc.last_line = 1;
yylloc.first_column = yylloc.last_column = 0;
init_table();
return yyparse();
}

