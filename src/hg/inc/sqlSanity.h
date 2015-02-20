#ifndef SQLSANITY_H
#define SQLSANITY_H

void sqlSanityCheckWhere(char *rawQuery, struct dyString *clause);
/* Let the user type in an expression that may contain
 * - field names
 * - parentheses
 * - comparison/arithmetic/logical operators
 * - numbers
 * - patterns with wildcards
 * Make sure they don't use any SQL reserved words, ;'s, etc.
 * Let SQL handle the actual parsing of nested expressions etc. -
 * this is just a token cop. */

#endif /* SQLSANITY_H */
