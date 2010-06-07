/* dtdParse - parse an XML DTD file.  Actually this only
 * parses a relatively simple subset of DTD's.  It's still
 * useful for autoXml and xmlToSql. */

#ifndef DTDPARSE_H
#define DTDPARSE_H

struct dtdElement
/* An element in an XML file. */
    {
    struct dtdElement *next;	/* Next in list. */
    char *name;			/* Element Name. */
    char *mixedCaseName;	/* Name converted from EL_NAME or el-name to elName. */
    struct dtdElChild *children;     /* Child elements. */
    struct dtdAttribute *attributes; /* Attributes. */
    int lineIx;			/* Line where element occurs in dtd file. */
    char *textType;		/* Text between tags if any. */
    };

struct dtdElChild
/* A reference to a child element. */
   {
   struct dtdElChild *next;	/* Next in list. */
   char *name;			/* Name of element. */
   char copyCode;		/* '1', '+', '?', or '*' */
   boolean isOr;                /* Is this part of a ( n | m ) "or" list? */
   struct dtdElement *el;	/* Element definition. */
   };

struct dtdAttribute
/* An attribute of some sort. */
    {
    struct dtdAttribute *next;	/* Next in list. */
    char *name;			/* Name of attribute. */
    char *mixedCaseName;	/* Name converted from EL_NAME or el-name to elName. */
    char *type;			/* Element type - CDATA, INT, FLOAT, etc. */
    boolean required;		/* True if required. */
    char *usual;		/* Default value (or NULL if none) */
    };

void dtdParse(char *fileName, char *prefix, char *textField,
	struct dtdElement **retList, struct hash **retHash);
/* Parse out a dtd file into elements that are returned in retList,
 * and for your convenience also in retHash (which is keyed by the
 * name of the element.  Note that XML element names can include the '-'
 * character.  For this and other reasons in addition to the element
 * name as it appears in the XML tag, the element has a mixedCaseName
 * that strips '-' and '_' chars, and tries to convert the name to
 * a mixed-case convention style name.  The prefix if any will be
 * prepended to mixed-case names.  The textField is what to name
 * the field that contains the letters between tags.  By default
 * (if NULL) it is "text." */

void dtdElementDump(struct dtdElement *el, FILE *f);
/* Dump info on element. */

#endif /* DTDPARSE_H */
