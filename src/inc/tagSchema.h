/* tagSchema - help deal with tagStorm schemas */

#ifndef TAGSCHEMA_H
#define TAGSCHEMA_H


struct tagSchema
/* Represents schema for a single tag */
    {
    struct tagSchema *next;
    char *name;   // Name of tag. Usually of form this.that, possibly this.[].that
    char required; // ! for required, ^ for required unique at each leaf, 0 for whatever
    char type;   // # for integer, % for floating point, $ for string
    double minVal, maxVal;  // Bounds for numerical types
    struct slName *allowedVals;  // Allowed values for string types
    struct hash *uniqHash;   // Help make sure that all values are unique
    boolean isArray;	// If true then can have multiple values
    struct slName *objArrayPieces;  // For schema entries representing a field in an array of
    				    // objects, such as this.[].that, a list of the pieces
				    // around array parts.  "this." and ".that" for instance.
    };


struct tagSchema *tagSchemaFromFile(char *fileName);
/* Read in a tagSchema file */

struct hash *tagSchemaHash(struct tagSchema *list);
/* Return a hash of tagSchemas keyed by name */

int tagSchemaDigitsUpToDot(char *s);
/* Return number of digits if is all digit up to next dot or end of string.
 * Otherwise return 0.  A specialized function but used by a couple of tag
 * storm modules. */

char *tagSchemaFigureArrayName(char *tagName, struct dyString *scratch);
/* Return tagName modified to indicate the array
 * status. For names with .# in them substitute a '[]' for
 * the number.   Example:
 *      person.12.name becomes person.[].name
 *      animal.13.children.4.name becomes animal.[].children.[].name
 *      person.12.cars.1 becomes person.[].cars.[]
 */

int tagSchemaParseIndexes(char *input, int indexes[], int maxIndexCount);
/* This will parse out something that looks like:
 *     this.array.2.subfield.subarray.3.name
 * into
 *     2,3   
 * Where input is what we parse,   indexes is an array maxIndexCount long
 * where we put the numbers. */

char *tagSchemaInsertIndexes(char *bracketed, int indexes[], int indexCount,
    struct dyString *scratch);
/* Convert something that looks like:
 *     this.array.[].subfield.subarray.[].name and 2,3
 * into
 *     this.array.2.subfield.subarray.3.name
 * The scratch string holds the return value. */

#endif /* TAGSCHEMA_H */


