/* pfType - ParaFlow type heirarchy */

struct pfType
/* A type, may have parents in object heirarchy, and
 * be composed of multiple elements. */
    {
    struct pfType *next;
    char *name;			/* Type name */
    struct pfScore *scope;	/* The scope this class lives in */
    struct pfType *parentType;	/* Pointer to parent class if any */
    struct pfDef *components;	/* Component elements of this class */
    };
