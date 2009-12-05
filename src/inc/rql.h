/* rql.h - interface to RQL (Ra Query Language - similar in syntax to SQL) parser and
 * interpreter. */

#ifndef RQL_H
#define RQL_H

#ifndef TOKENIZER_H
#include "tokenizer.h"
#endif

enum rqlOp
/* An operation in the parse tree. */
    {
    rqlOpUnknown,	/* Should not occur */
    rqlOpLiteral,        /* Literal string or number. */
    rqlOpSymbol,	/* A symbol name. */

    /* Type casts. */
    rqlOpStringToBoolean,	
    rqlOpIntToBoolean,
    rqlOpDoubleToBoolean,
    rqlOpStringToInt,
    rqlOpDoubleToInt,
    rqlOpBooleanToInt,
    rqlOpStringToDouble,
    rqlOpBooleanToDouble,
    rqlOpIntToDouble,

    /* Comparisons. */
    rqlOpEq,	/* An equals comparison */
    rqlOpNe,	/* A not equals comparison */
    rqlOpGt,  /* Greater than comparison. */
    rqlOpLt,  /* Less than comparison. */
    rqlOpGe,  /* Greater than or equals comparison. */
    rqlOpLe,  /* Less than or equals comparison. */
    rqlOpLike, /* SQL wildcard compare. */

    /* Logic ops. */
    rqlOpAnd,     /* An and */
    rqlOpOr,      /* An or */
    rqlOpNot,	  /* A unary not. */

    /* Leading minus. */
    rqlOpUnaryMinusInt,
    rqlOpUnaryMinusDouble,

    /* Fancy ops to fetch sub-parts. */
    rqlOpArrayIx,	/* An array with an index. */
    };

char *rqlOpToString(enum rqlOp op);
/* Return string representation of parse op. */

enum rqlType
/* A type */
    {
    rqlTypeBoolean = 1,
    rqlTypeString = 2,
    rqlTypeInt = 3,
    rqlTypeDouble = 4,
    };

union rqlVal
/* Some value of arbirary type that can be of any type corresponding to rqlType */
    {
    boolean b;
    char *s;
    int i;
    double x;
    };

struct rqlEval
/* Result of evaluation of parse tree. */
    {
    enum rqlType type;
    union rqlVal val;
    };

struct rqlParse
/* A rql parse-tree. */
    {
    struct rqlParse *next;	/* Points to younger sibling if any. */
    struct rqlParse *children;	/* Points to oldest child if any. */
    enum rqlOp op;		/* Operation at this node. */
    enum rqlType type;		/* Return type of this operation. */
    union rqlVal val;		/* Return value of this operation. */
    };

struct rqlStatement
/* A parsed out RQL statement */
    {
    char *next;		/* Next in list */
    char *command;	/* Generally the first word in the statement. */
    struct slName *fieldList;	/* List of fields if any. */
    struct slName *tableList;	/* List of tables if any. */
    struct rqlParse *whereClause;	/* Where clause if any - in parse tree. */
    struct slName *whereVarList;	/* List of variables used in where clause. */
    int limit;		/* If >= 0 then limits # of records returned. */
    };

void rqlValDump(union rqlVal val, enum rqlType type, FILE *f);
/* Dump out value to file. */

void rqlParseDump(struct rqlParse *p, int depth, FILE *f);
/* Dump out rqlParse tree and children. */

struct rqlParse *rqlParseExpression(struct tokenizer *tkz);
/* Parse out a clause, usually a where clause. */

struct rqlStatement *rqlStatementParse(struct lineFile *lf);
/* Parse an RQL statement out of text */

void rqlStatementFree(struct rqlStatement **pRql);
/* Free up an rql statement. */

void rqlStatementDump(struct rqlStatement *rql, FILE *f);
/* Print out statement to file. */

typedef char* (*RqlEvalLookup)(void *record, char *key);
/* Callback for rqlEvalOnRecord to lookup a variable value. */

struct rqlEval rqlEvalOnRecord(struct rqlParse *p, void *record, RqlEvalLookup lookup,
	struct lm *lm);
/* Evaluate parse tree on record, using lm for memory for string operations. */

struct rqlEval rqlEvalCoerceToBoolean(struct rqlEval r);
/* Return TRUE if it's a nonempty string or a non-zero number. */

#endif /* RQL_H */

