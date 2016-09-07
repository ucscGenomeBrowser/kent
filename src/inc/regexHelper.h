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

INLINE boolean regexSubstrMatched(const regmatch_t substr)
/* Return TRUE if both start and end offsets are nonnegative (when a substring is not matched,
 * regexec sets the start and end offsets to -1). */
{
return (substr.rm_so >= 0 && substr.rm_eo >= 0 && substr.rm_eo >= substr.rm_so);
}

void regexSubstringCopy(const char *string, const regmatch_t substr,
                        char *buf, size_t bufSize);
/* Copy a substring from string into buf using start and end offsets from substr.
 * If the substring was not matched then make buf an empty string. */

char *regexSubstringClone(const char *string, const regmatch_t substr);
/* Clone and return a substring from string using start and end offsets from substr.
 * If the substring was not matched then return a cloned empty string. */

int regexSubstringInt(const char *string, const regmatch_t substr);
/* Return the integer value of the substring specified by substr.
 * If substr was not matched, return 0; you can check first with regexSubstrMatched() if
 * that's not the desired behavior for unmatched substr. */

#endif // REGEXHELPER_H
