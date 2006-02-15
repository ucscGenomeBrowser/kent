/* Built in string handling. */

#ifndef PFSTRING_H

struct _pf_string *_pf_string_new(char *s, int size);
/* Wrap string buffer of given size. */

struct _pf_string *_pf_string_dupe(char *s, int size);
/* Clone string of given size and wrap string around it. */

struct _pf_string *_pf_string_from_const(char *s);
/* Wrap string around zero-terminated string. */

struct _pf_string *_pf_string_cat(_pf_Stack *stack, int count);
/* Create new string that's a concatenation of the above strings. */

struct _pf_string *_pf_string_from_long(_pf_Long ll);
/* Wrap string around Long. */

struct _pf_string *_pf_string_from_double(_pf_Double d);
/* Wrap string around Double. */

void _pf_cm_string_dupe(_pf_Stack *stack);
/* Return duplicate of string */

void _pf_cm_string_start(_pf_Stack *stack);
/* Return start of string */

void _pf_cm_string_rest(_pf_Stack *stack);
/* Return rest of string (skipping up to start) */

void _pf_cm_string_middle(_pf_Stack *stack);
/* Return middle of string */

void _pf_cm_string_end(_pf_Stack *stack);
/* Return end of string */

void _pf_cm_string_upper(_pf_Stack *stack);
/* Uppercase existing string */

void _pf_cm_string_lower(_pf_Stack *stack);
/* Lowercase existing string */

void _pf_cm_string_append(_pf_Stack *stack);
/* Append to end of string. */

void _pf_cm_string_find(_pf_Stack *stack);
/* Find occurence of substring within string. */

int _pf_strcmp(_pf_Stack *stack);
/* Return comparison between strings.  Cleans them off
 * of stack.  Does not put result on stack because
 * the code generator may use it in different ways. */

#endif /* PFSTRING_H */
