/* regexHelper: easy wrappers on POSIX Extended Regular Expressions (man 7 regex, man 3 regex) */

#ifndef REGEXHELPER_H
#define REGEXHELPER_H

#include "common.h"
#include <regex.h>

const regex_t *regexCompile(const char *exp, const char *description, int compileFlags);
/* Compile exp (or die with an informative-as-possible error message).
 * Cache pre-compiled regex's internally (so don't free result after use). */

boolean regexMatch(const char *string, const char *exp);
/* Return TRUE if string matches regular expression exp (case sensitive). */

boolean regexMatchNoCase(const char *string, const char *exp);
/* Return TRUE if string matches regular expression exp (case insensitive). */

boolean regexMatchSubstr(const char *string, const char *exp,
			 regmatch_t substrArr[], size_t substrArrSize);
/* Return TRUE if string matches regular expression exp (case sensitive);
 * regexec fills in substrArr with substring offsets. */

boolean regexMatchSubstrNoCase(const char *string, const char *exp,
			       regmatch_t substrArr[], size_t substrArrSize);
/* Return TRUE if string matches regular expression exp (case insensitive);
 * regexec fills in substrArr with substring offsets. */

#endif // REGEXHELPER_H
