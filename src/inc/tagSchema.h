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


#endif /* TAGSCHEMA_H */


