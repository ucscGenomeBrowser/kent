#include <stdio.h>
#include <math.h>
#include <string.h>
#include "calc.h"

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

