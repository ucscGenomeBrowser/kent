/* recodedType - Types in form that can be used at run time. */

#ifndef RECODEDTYPE_H
#define RECODEDTYPE_H

#define recodedTypeTableName "_pf_lti"
/* Name of type table. */

#define recodedStructType "_pf_local_type_info"
/* Name of local type */


int recodedTypeId(struct pfCompile *pfc, struct pfType *type);
/* Return run time type ID associated with type */

void recodedTypeTableToC(struct pfCompile *pfc, char *moduleName, FILE *f);
/* Print out type table for one module. */

void recodedTypeTableToBackend(struct pfCompile *pfc, char *moduleName,
	FILE *f);
/* Write type table for module to back end. */

#endif /* RECODEDTYPE_H */
