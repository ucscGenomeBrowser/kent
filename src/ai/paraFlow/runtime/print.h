/* print - Implements print function for variable types. */

#ifndef PRINT_H

void pf_print(_pf_Stack *stack);
/* Print out single variable where type is determined at run time. 
 * Add newline. */

void pf_prin(_pf_Stack *stack);
/* Print out single variable where type is determined at run time. 
 * No newline. */

void _pf_printField(FILE *f, void *data, struct _pf_base *base, 
	struct hash *idHash);
/* Print out a single field. */

#endif /* PRINT_H */
