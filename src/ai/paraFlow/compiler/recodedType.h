/* recodedType - Types in form that can be used at run time. */

#ifndef RECODEDTYPE_H
#define RECODEDTYPE_H

void encodeType(struct pfType *type, struct dyString *dy);
/* Encode type into dy. */

int recodedTypeId(struct pfCompile *pfc, struct pfType *type);
/* Return run time type ID associated with type */

void printModuleTypeTable(struct pfCompile *pfc, FILE *f);
/* Print out type table for one module. */

#endif /* RECODEDTYPE_H */
