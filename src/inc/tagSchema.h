/* tagSchema - help deal with tagStorm schemas */

#ifndef TAGSCHEMA_H
#define TAGSCHEMA_H


struct tagSchema
/* Represents schema for a single tag */
    {
    struct tagSchema *next;
    char *name;   // Name of tag
    char required; // ! for required, ^ for required unique at each leaf, 0 for whatever
    char type;   // # for integer, % for floating point, $ for string
    double minVal, maxVal;  // Bounds for numerical types
    struct slName *allowedVals;  // Allowed values for string types
    struct hash *uniqHash;   // Help make sure that all values are unique
    boolean isArray;	// If true then can have multiple values
    };


struct tagSchema *tagSchemaFromFile(char *fileName);
/* Read in a tagSchema file */

struct hash *tagSchemaHash(struct tagSchema *list);
/* Return a hash of tagSchemas keyed by name */

char *tagSchemaFigureArrayName(char *tagName, struct dyString *scratch, boolean clearScratch);
/* Return tagName modified to indicate the array
 * status. For names with .# in them substitute a '[' for
 * the number and put a ']' at the end.   Example:
 *      person.12.name becomes person[name]
 *      animal.13.children.4.name becomes animal[children[name]]
 *      person.12.cars.1 becomes person[cars[]] 
 * Puts result into scratch and returns scratch->string.  Will clear previous contents
 * of scratch optionally.  Just an option for easier composition in to lists. */

#endif /* TAGSCHEMA_H */


