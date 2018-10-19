/* rqlToSql - convert queries in the sql-like rql query language into sql
 * statements that can be passed to a MySQL (or similar) server. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "dystring.h"
#include "rql.h"

char *rqlParseToSqlWhereClause (struct rqlParse *p, boolean inComparison)
/* Recursively convert an rqlParse into an equivalent SQL where clause (without the "where").
 * The RQL construction of just naming a field is converted to a SQL "is not NULL".
 * Leaks memory from a series of constructed and discarded strings. */
{
struct dyString *sqlClause = dyStringNew(0);
struct rqlParse *nextPtr = NULL;
switch (p->op)
    {
    case rqlOpUnknown:
        errAbort("Unknown operation in parse tree - can't translate to SQL");
        break;
    // Type casts (ignore)
    case rqlOpStringToBoolean:
    case rqlOpIntToBoolean:
    case rqlOpDoubleToBoolean:
    case rqlOpStringToInt:
    case rqlOpDoubleToInt:
    case rqlOpBooleanToInt:
    case rqlOpStringToDouble:
    case rqlOpBooleanToDouble:
    case rqlOpIntToDouble:
        return rqlParseToSqlWhereClause(p->children, inComparison);
        break;
    // Comparison Ops
    case rqlOpEq:
        dyStringPrintf(sqlClause, "(%s = %s)", rqlParseToSqlWhereClause(p->children, TRUE),
                rqlParseToSqlWhereClause(p->children->next, TRUE));
        break;
    case rqlOpNe:
        dyStringPrintf(sqlClause, "(%s != %s)", rqlParseToSqlWhereClause(p->children, TRUE),
                rqlParseToSqlWhereClause(p->children->next, TRUE));
        break;
    case rqlOpGt:
        dyStringPrintf(sqlClause, "(%s > %s)", rqlParseToSqlWhereClause(p->children, TRUE),
                rqlParseToSqlWhereClause(p->children->next, TRUE));
        break;
    case rqlOpLt:
        dyStringPrintf(sqlClause, "(%s < %s)", rqlParseToSqlWhereClause(p->children, TRUE),
                rqlParseToSqlWhereClause(p->children->next, TRUE));
        break;
    case rqlOpGe:
        dyStringPrintf(sqlClause, "(%s >= %s)", rqlParseToSqlWhereClause(p->children, TRUE),
                rqlParseToSqlWhereClause(p->children->next, TRUE));
        break;
    case rqlOpLe:
        dyStringPrintf(sqlClause, "(%s <= %s)", rqlParseToSqlWhereClause(p->children, TRUE),
                rqlParseToSqlWhereClause(p->children->next, TRUE));
        break;
    case rqlOpLike:
        dyStringPrintf(sqlClause, "(%s like %s)", rqlParseToSqlWhereClause(p->children, TRUE),
                rqlParseToSqlWhereClause(p->children->next, TRUE));
        break;
    case rqlOpAdd:
        dyStringPrintf(sqlClause, "(%s + %s)", rqlParseToSqlWhereClause(p->children, TRUE),
                rqlParseToSqlWhereClause(p->children->next, TRUE));
        break;
    case rqlOpSubtract:
        dyStringPrintf(sqlClause, "(%s - %s)", rqlParseToSqlWhereClause(p->children, TRUE),
                rqlParseToSqlWhereClause(p->children->next, TRUE));
        break;
    case rqlOpMultiply:
        dyStringPrintf(sqlClause, "(%s * %s)", rqlParseToSqlWhereClause(p->children, TRUE),
                rqlParseToSqlWhereClause(p->children->next, TRUE));
        break;
    case rqlOpDivide:
        dyStringPrintf(sqlClause, "(%s / %s)", rqlParseToSqlWhereClause(p->children, TRUE),
                rqlParseToSqlWhereClause(p->children->next, TRUE));
        break;

    // Logical Ops
    case rqlOpAnd:
        dyStringPrintf(sqlClause, "(%s", rqlParseToSqlWhereClause(p->children, FALSE));
        nextPtr = p->children->next;
        while (nextPtr != NULL)
            {
            dyStringPrintf(sqlClause, " and %s", rqlParseToSqlWhereClause(nextPtr, FALSE));
            nextPtr = nextPtr->next;
            }
        dyStringPrintf(sqlClause, ")");
        break;
    case rqlOpOr:
        dyStringPrintf(sqlClause, "(%s", rqlParseToSqlWhereClause(p->children, FALSE));
        nextPtr = p->children->next;
        while (nextPtr != NULL)
            {
            dyStringPrintf(sqlClause, " or %s", rqlParseToSqlWhereClause(nextPtr, FALSE));
            nextPtr = nextPtr->next;
            }
        dyStringPrintf(sqlClause, ")");
        break;
    case rqlOpNot:
        dyStringPrintf(sqlClause, "(not %s)", rqlParseToSqlWhereClause(p->children, FALSE));
        break;
    // Leading minus
    case rqlOpUnaryMinusInt:
    case rqlOpUnaryMinusDouble:
        dyStringPrintf(sqlClause, "(-%s)", rqlParseToSqlWhereClause(p->children, inComparison));
        break;
    // Array index
    case rqlOpArrayIx:
        dyStringPrintf(sqlClause, "(%s[%s])", rqlParseToSqlWhereClause(p->children, inComparison),
                rqlParseToSqlWhereClause(p->children->next, inComparison));
        break;
    // Symbols and literals
    case rqlOpLiteral:
        if (p->type == rqlTypeBoolean)
            dyStringPrintf(sqlClause, "%s", (p->val.b ? "true" : "false"));
        else if (p->type == rqlTypeString)
            dyStringPrintf(sqlClause, "\"%s\"", p->val.s);
        else if (p->type == rqlTypeInt)
            dyStringPrintf(sqlClause, "%lld", p->val.i);
        else if (p->type == rqlTypeDouble)
            dyStringPrintf(sqlClause, "%lf", p->val.x);
        if (!inComparison)
            dyStringPrintf(sqlClause, " is not NULL");
        break;
    case rqlOpSymbol:
        dyStringPrintf(sqlClause, "%s", p->val.s);
        if (!inComparison)
            dyStringPrintf(sqlClause, " is not NULL");
        break;
    default:
        errAbort("Unrecognized rql operator: %d", p->op);
    }
return dyStringCannibalize(&sqlClause);
}

char *rqlStatementToSql (struct rqlStatement *rql, struct slName *allFields)
/* Convert an rqlStatement into an equivalent SQL statement.  This requires a list
 * of all fields available, as RQL select statements can include wildcards in the field
 * list (e.g., 'select abc* from table' retrieves all fields whose names begin with abc). */
{
struct dyString *sqlClause = dyStringNew(0);
if (sameString(rql->command, "select"))
    {
    if (slNameInList(rql->fieldList, "*"))
        dyStringPrintf(sqlClause, "select %s", slNameListToString(allFields, ','));
    else
        {
        struct slName *returnedFields = wildExpandList(allFields, rql->fieldList, TRUE);
        dyStringPrintf(sqlClause, "select %s", slNameListToString(returnedFields, ','));
        slNameFreeList(&returnedFields);
        }
    }
else if (sameString(rql->command, "count"))
    dyStringPrintf(sqlClause, "select count(*)");
else
    errAbort("Unrecognized command %s in rql statement", rql->command);

dyStringPrintf(sqlClause, " from %s", slNameListToString(rql->tableList, ','));

if (rql->whereClause != NULL)
    dyStringPrintf(sqlClause, " where %s", rqlParseToSqlWhereClause(rql->whereClause, FALSE));
return dyStringCannibalize(&sqlClause);
}
