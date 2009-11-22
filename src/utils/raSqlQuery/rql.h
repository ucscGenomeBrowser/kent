
enum rqlOp
    {
    rqlOpUnknown,	/* Should not occur */
    rqlOpLiteral,        /* Literal string or number. */
    rqlOpSymbol,	/* A symbol name. */
    rqlOpEq,	/* An equals comparison */
    rqlOpNe,	/* A not equals comparison */

    rqlOpStringToBoolean,
    rqlOpIntToBoolean,
    rqlOpDoubleToBoolean,
    rqlOpStringToInt,
    rqlOpStringToDouble,
    rqlOpBooleanToInt,
    rqlOpBooleanToDouble,
    rqlOpIntToDouble,

    rqlOpUnaryMinusDouble,

    rqlOpGt,  /* Greater than comparison. */
    rqlOpLt,  /* Less than comparison. */
    rqlOpGe,  /* Greater than or equals comparison. */
    rqlOpLe,  /* Less than or equals comparison. */
    rqlOpLike, /* SQL wildcard compare. */

    rqlOpAnd,     /* An and */
    rqlOpOr,      /* An or */
    rqlOpNot,	  /* A unary not. */
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
    struct rqlParse *whereClause;	/* Where clause if any - tokenized. */
    };

void rqlValDump(union rqlVal val, enum rqlType type, FILE *f);
/* Dump out value to file. */

void rqlParseDump(struct rqlParse *p, int depth, FILE *f);
/* Dump out rqlParse tree and children. */

struct rqlParse *rqlParseExpression(struct tokenizer *tkz);
/* Parse out a clause, usually a where clause. */

struct rqlStatement *rqlStatementParse(struct lineFile *lf);
/* Parse an RQL statement out of text */

void rqlStatementDump(struct rqlStatement *rql, FILE *f);
/* Print out statement to file. */

struct rqlEval rqlEvalOnRecord(struct rqlParse *p, struct raRecord *ra);
/* Evaluate self on ra. */

struct rqlEval rqlEvalCoerceToBoolean(struct rqlEval r);
/* Return TRUE if it's a nonempty string or a non-zero number. */
