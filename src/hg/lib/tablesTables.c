/* tablesTables - this module deals with two types of tables SQL tables in a database,
 * and fieldedTable objects in memory. It has routines to do sortable, filterable web
 * displays on tables. */

#include "common.h"
#include "hash.h"
#include "obscure.h"
#include "linefile.h"
#include "jksql.h"
#include "jsHelper.h"
#include "sqlSanity.h"
#include "fieldedTable.h"
#include "cheapcgi.h"
#include "web.h"
#include "cart.h"
#include "tablesTables.h"

struct fieldedTable *fieldedTableFromDbQuery(struct sqlConnection *conn, char *query)
/* Return fieldedTable from a database query */
{
struct sqlResult *sr = sqlGetResult(conn, query);
char **fields;
int fieldCount = sqlResultFieldArray(sr, &fields);
struct fieldedTable *table = fieldedTableNew(query, fields, fieldCount);
char **row;
int i = 0;
while ((row = sqlNextRow(sr)) != NULL)
    fieldedTableAdd(table, row, fieldCount, ++i);
sqlFreeResult(&sr);
return table;
}

static void showTableFilterInstructionsEtc(struct fieldedTable *table, 
    char *itemPlural, struct  fieldedTableSegment *largerContext, void (*addFunc)(void))
/* Print instructional text, and basic summary info on who passes filter, and a submit
 * button just in case user needs it */
{
/* Print info on matching */
int matchCount = slCount(table->rowList);
if (largerContext != NULL)  // Need to page?
     matchCount = largerContext->tableSize;

cgiMakeButton("submit", "search");

printf("&nbsp&nbsp;");
cgiMakeOnClickButton("clearButton",
"$(':input').not(':button, :submit, :reset, :hidden, :checkbox, :radio').val('');\n"
"$('[name=cdwBrowseFiles_page]').val('1');\n"
"$('#submit').click();\n"
, "clear search");
printf("<br>");

printf("%d&nbsp;%s&nbsp;found. ", matchCount, itemPlural);

if (addFunc)
    addFunc();

printf("<BR>\n");
printf("You can further filter search results field by field below. ");    
printf("Wildcard * and ? characters are allowed in text fields. ");
printf("&GT;min or &LT;max are allowed in numerical fields.<BR>\n");
}

static void printSuggestScript(char *id, struct slName *suggestList)
/* Print out a little javascript to wrap auto-suggester around control with given ID */
{
struct dyString *dy = dyStringNew(256);
dyStringPrintf(dy,"$(document).ready(function() {\n");
dyStringPrintf(dy,"  $('#%s').autocomplete({\n", id);
dyStringPrintf(dy,"    delay: 100,\n");
dyStringPrintf(dy,"    minLength: 0,\n");
dyStringPrintf(dy,"    source: [");
char *separator = "";
struct slName *suggest;
for (suggest = suggestList; suggest != NULL; suggest = suggest->next)
    {
    dyStringPrintf(dy,"%s\"%s\"", separator, suggest->name);
    separator = ",";
    }
dyStringPrintf(dy,"]\n");
dyStringPrintf(dy,"    });\n");
dyStringPrintf(dy,"});\n");
jsInline(dy->string);
dyStringFree(&dy);
}

static void printWatermark(char *id, char *watermark)
/* Print light text filter prompt as watermark. */
{
jsInlineF(
"$(function() {\n"
"  $('#%s').watermark(\"%s\");\n"
"});\n", id, watermark);
}

static void resetPageNumberOnChange(char *id)
/* On change, reset page number to 1. */
{
jsInlineF(
"$(function() {\n"
" $('form').delegate('#%s','change keyup paste',function(e){\n"
"  $('[name=cdwBrowseFiles_page]').val('1');\n"
" });\n"
"});\n"
, id);
}


static void showTableFilterControlRow(struct fieldedTable *table, struct cart *cart, 
    char *varPrefix, int maxLenField, struct hash *suggestHash)
/* Assuming we are in table already drow control row.
 * The suggestHash is keyed by field name.  If something is there we'll assume
 * it's value is slName list of suggestion values */
{
/* Include javascript and style we need  */
webIncludeResourceFile("jquery-ui.css");
jsIncludeFile("jquery.js", NULL);
jsIncludeFile("jquery.plugins.js", NULL);
jsIncludeFile("jquery-ui.js", NULL);
jsIncludeFile("jquery.watermark.js", NULL);

int i;
printf("<TR>");
for (i=0; i<table->fieldCount; ++i)
    {
    char *field = table->fields[i];
    char varName[256];
    safef(varName, sizeof(varName), "%s_f_%s", varPrefix, field);
    webPrintLinkCellStart();

#ifdef MAKES_TOO_WIDE
    /* Print out input control.  As you can see from all the commented out bits
     * this part has been a challenge.  We'd like to make the input cell fit the
     * table size, but if we do it with style it makes whole table wider. */
    char *oldVal = cartUsualString(cart, varName, "");
    printf("<input type=\"text\" name=\"%s\" style=\"display:table-cell; width=100%%\""
	   " value=\"%s\">", varName, oldVal);
#endif /* MAKES_TOO_WIDE */

    /* Approximate size of input control in characters */
    int size = fieldedTableMaxColChars(table, i);
    if (size > maxLenField)
	size = maxLenField;

#ifdef ACTUALLY_WORKS
    /* This way does work last I checked and is just a line of code.
     * Getting an id= property on the input tag though isn't possible this way. */
    cartMakeTextVar(cart, varName, "", size + 1);
#endif

    /* Print input control getting previous value from cart.  Set an id=
     * so auto-suggest can find this control. */
    char *oldVal = cartUsualString(cart, varName, "");
    printf("<INPUT TYPE=TEXT NAME=\"%s\" id=\"%s\" SIZE=%d VALUE=\"%s\">\n",
	varName, varName, size+1, oldVal);

    /* Write out javascript to initialize autosuggest on control */
    printWatermark(varName, " filter ");

    /* Write out javascript to reset page number to 1 if filter changes */
    resetPageNumberOnChange(varName);

    if (suggestHash != NULL)
        {
	struct slName *suggestList = hashFindVal(suggestHash, field);
	if (suggestList != NULL)
	    {
	    printSuggestScript(varName, suggestList);
	    }
	}
    webPrintLinkCellEnd();
    }


printf("</TR>");
}

static void showTableSortingLabelRow(struct fieldedTable *table, struct cart *cart, char *varPrefix,
    char *returnUrl)
/* Put up the label row with sorting fields attached.  ALso actually sort table.  */
{
/* Get order var */
char orderVar[256];
safef(orderVar, sizeof(orderVar), "%s_order", varPrefix);
char *orderFields = cartUsualString(cart, orderVar, "");

char pageVar[64];
safef(pageVar, sizeof(pageVar), "%s_page", varPrefix);

/* Print column labels */
int i;
for (i=0; i<table->fieldCount; ++i)
    {
    webPrintLabelCellStart();
    printf("<A class=\"topbar\" HREF=\"");
    printf("%s", returnUrl);
    printf("&%s=1", pageVar);
    printf("&%s=", orderVar);
    char *field = table->fields[i];
    if (!isEmpty(orderFields) && sameString(orderFields, field))
        printf("-");
    printf("%s", field);
    printf("\">");
    printf("%s", field);
    if (!isEmpty(orderFields))
        {
	char *s = orderFields;
	boolean isRev = (s[0] == '-');
	if (isRev)
	    ++s;
	if (sameString(field, s))
	    {
	    if (isRev)
	        printf("&uarr;");
	    else
	        printf("&darr;");
	    }
	}
    printf("</A>");
    webPrintLabelCellEnd();
    }

/* Sort on field */
if (!isEmpty(orderFields))
    {
    boolean doReverse = FALSE;
    char *field = orderFields;
    if (field[0] == '-')
        {
	field += 1;
	doReverse = TRUE;
	}
    fieldedTableSortOnField(table, field, doReverse);
    }
}

static void showTableDataRows(struct fieldedTable *table, int pageSize, int maxLenField,
    struct hash *tagOutputWrappers, void *wrapperContext)
/* Render data rows into HTML */
{
int count = 0;
struct fieldedRow *row;
boolean isNum[table->fieldCount];
int i;
for (i=0; i<table->fieldCount; ++i)
    isNum[i] = fieldedTableColumnIsNumeric(table, i);

for (row = table->rowList; row != NULL; row = row->next)
    {
    if (++count > pageSize)
         break;
    printf("<TR>\n");
    int fieldIx = 0;
    for (fieldIx=0; fieldIx<table->fieldCount; ++fieldIx)
	{
	char shortVal[maxLenField+1];
	char *longVal = emptyForNull(row->row[fieldIx]);
	char *val = longVal;
	int valLen = strlen(val);
	if (maxLenField > 0 && maxLenField < valLen)
	    {
	    if (valLen > maxLenField)
		{
		memcpy(shortVal, val, maxLenField-3);
		shortVal[maxLenField-3] = 0;
		strcat(shortVal, "...");
		val = shortVal;
		}
	    }
	if (isNum[fieldIx])
	    webPrintLinkCellRightStart();
	else
	    webPrintLinkCellStart();
	boolean printed = FALSE;
	if (tagOutputWrappers != NULL && !isEmpty(val))
	    {
	    char *field = table->fields[fieldIx];
	    webTableOutputWrapperType *printer = hashFindVal(tagOutputWrappers, field);
	    if (printer != NULL)
		{
		printer(table, row, field, longVal, val, wrapperContext);
		printed = TRUE;
		}
	    
	    }
	if (!printed)
	    printf("%s", val);
	webPrintLinkCellEnd();
	}
    printf("</TR>\n");
    }
}

static void showTablePaging(struct fieldedTable *table, struct cart *cart, char *varPrefix,
    struct fieldedTableSegment *largerContext, int pageSize)
/* If larger context exists and is bigger than current display, then draw paging controls. */
{
/* Handle paging if any */
if (largerContext != NULL)  // Need to page?
     {
     if (pageSize < largerContext->tableSize)
	{
	int curPage = largerContext->tableOffset/pageSize;
	int totalPages = (largerContext->tableSize + pageSize - 1)/pageSize;


	char id[256];
	if ((curPage + 1) > 1)
	    {
	    // first page
	    safef(id, sizeof id, "%s_first", varPrefix);
	    printf("<a href='#' id='%s' style='font-size: 150%%;'>&#9198;</a>", id);
	    jsOnEventByIdF("click", id, 
		"$('[name=%s_page]').val('1');\n"
		"$('#submit').click();\n"
		, varPrefix);
	    printf("&nbsp;&nbsp;&nbsp;");

	    // prev page
	    safef(id, sizeof id, "%s_prev", varPrefix);
	    printf("<a href='#' id='%s' style='font-size: 150%%;'>&#9194;</a>", id);
	    jsOnEventByIdF("click", id, 
		"$('[name=%s_page]').val('%d');\n"
		"$('#submit').click();\n"
		, varPrefix, (curPage+1)-1);
	    printf("&nbsp;&nbsp;&nbsp;");
	    }


	printf("Displaying page ");

	char pageVar[64];
	safef(pageVar, sizeof(pageVar), "%s_page", varPrefix);
	cgiMakeIntVar(pageVar, curPage+1, 3);

	printf(" of %d", totalPages);

	if ((curPage + 1) < totalPages)
	    {
	    // next page
	    printf("&nbsp;&nbsp;&nbsp;");
	    safef(id, sizeof id, "%s_next", varPrefix);
	    printf("<a href='#' id='%s' style='font-size: 150%%;' >&#9193;</a>", id);
	    jsOnEventByIdF("click", id, 
		"$('[name=%s_page]').val('%d');\n"
		"$('#submit').click();\n"
		, varPrefix, (curPage+1)+1);

	    // last page
	    printf("&nbsp;&nbsp;&nbsp;");
	    safef(id, sizeof id, "%s_last", varPrefix);
	    printf("<a href='#' id='%s' style='font-size: 150%%;' >&#9197;</a>", id);
	    jsOnEventByIdF("click", id, 
		"$('[name=%s_page]').val('%d');\n"
		"$('#submit').click();\n"
		, varPrefix, totalPages);

	    }
	}
     }
}


void webFilteredFieldedTable(struct cart *cart, struct fieldedTable *table, 
    char *returnUrl, char *varPrefix,
    int maxLenField, struct hash *tagOutputWrappers, void *wrapperContext,
    boolean withFilters, char *itemPlural, 
    int pageSize, struct fieldedTableSegment *largerContext, struct hash *suggestHash, 
    void (*addFunc)(void) )
/* Show a fielded table that can be sorted by clicking on column labels and optionally
 * that includes a row of filter controls above the labels .
 * The maxLenField is maximum character length of field before truncation with ...
 * Pass in 0 for no max */
{
if (strchr(returnUrl, '?') == NULL)
     errAbort("Expecting returnUrl to include ? in showFieldedTable\nIt's %s", returnUrl);

if (withFilters)
    showTableFilterInstructionsEtc(table, itemPlural, largerContext, addFunc);

/* Set up our table within table look. */
webPrintLinkTableStart();

/* Draw optional filters cells ahead of column labels*/
if (withFilters)
    showTableFilterControlRow(table, cart, varPrefix, maxLenField, suggestHash);

showTableSortingLabelRow(table, cart, varPrefix, returnUrl);
showTableDataRows(table, pageSize, maxLenField, tagOutputWrappers, wrapperContext);

/* Get rid of table within table look */
webPrintLinkTableEnd();

if (largerContext != NULL)
    showTablePaging(table, cart, varPrefix, largerContext, pageSize);
}

void webSortableFieldedTable(struct cart *cart, struct fieldedTable *table, 
    char *returnUrl, char *varPrefix,
    int maxLenField, struct hash *tagOutputWrappers, void *wrapperContext)
/* Display all of table including a sortable label row.  The tagOutputWrappers
 * is an optional way to enrich output of specific columns of the table.  It is keyed
 * by column name and has for values functions of type webTableOutputWrapperType. */
{
webFilteredFieldedTable(cart, table, returnUrl, varPrefix, 
    maxLenField, tagOutputWrappers, wrapperContext,
    FALSE, NULL, 
    slCount(table->rowList), NULL, NULL, NULL);
}


void webTableBuildQuery(struct cart *cart, char *from, char *initialWhere, 
    char *varPrefix, char *fields, boolean withFilters, 
    struct dyString **retQuery, struct dyString **retWhere)
/* Construct select, from and where clauses in query, keeping an additional copy of where 
 * Returns the SQL query and the SQL where expression as two dyStrings (need to be freed)  */
{
struct dyString *query = dyStringNew(0);
struct dyString *where = dyStringNew(0);
struct slName *field, *fieldList = commaSepToSlNames(fields);
boolean gotWhere = FALSE;
sqlDyStringPrintf(query, "select %-s from %s", sqlCkIl(fields), from);
if (!isEmpty(initialWhere))
    {
    sqlDyStringPrintfFrag(where, " where ");
    sqlSanityCheckWhere(initialWhere, where);
    gotWhere = TRUE;
    }

/* If we're doing filters, have to loop through the row of filter controls */
if (withFilters)
    {
    for (field = fieldList; field != NULL; field = field->next)
        {
	char varName[128];
	safef(varName, sizeof(varName), "%s_f_%s", varPrefix, field->name);
	char *val = trimSpaces(cartUsualString(cart, varName, ""));
	if (!isEmpty(val))
	    {
	    if (gotWhere)
		sqlDyStringPrintf(where, " and ");
	    else
		{
	        sqlDyStringPrintf(where, " where ");
		gotWhere = TRUE;
		}
	    if (anyWild(val))
		{
		char *converted = sqlLikeFromWild(val);
		sqlDyStringPrintf(where, "%s like '%s'", field->name, converted);
		freez(&converted);
		}
	    else if (val[0] == '>' || val[0] == '<')
		{
		char *remaining = val+1;
		if (remaining[0] == '=')
		    {
		    remaining += 1;
		    }
		remaining = skipLeadingSpaces(remaining);
		if (isNumericString(remaining))
		    {
		    sqlDyStringPrintf(where, "%s ", field->name);
		    if (val[0] == '>')
			sqlDyStringPrintf(where, ">");
		    if (val[0] == '<')
			sqlDyStringPrintf(where, "<");
		    if (val[1] == '=')
			sqlDyStringPrintf(where, "=");
		    sqlDyStringPrintf(where, "%s", remaining);
		    }
		else
		    {
		    warn("Filter for %s doesn't parse:  %s", field->name, val);
		    sqlDyStringPrintf(where, "%s is not null", field->name); // Let query continue
		    }
		}
	    else
		{
		sqlDyStringPrintf(where, "%s = '%s'", field->name, val);
		}
	    }
	}
    }
sqlDyStringPrintf(query, "%-s", where->string);  // trust

/* We do order here so as to keep order when working with tables bigger than a page. */
char orderVar[256];
safef(orderVar, sizeof(orderVar), "%s_order", varPrefix);
char *orderFields = cartUsualString(cart, orderVar, "");
if (!isEmpty(orderFields))
    {
    if (orderFields[0] == '-')
	sqlDyStringPrintf(query, " order by %s desc", orderFields+1);
    else
	sqlDyStringPrintf(query, " order by %s", orderFields);
    }

// return query and where expression
*retQuery = query;
*retWhere = where;
}

void webFilteredSqlTable(struct cart *cart, struct sqlConnection *conn, 
    char *fields, char *from, char *initialWhere,  
    char *returnUrl, char *varPrefix, int maxFieldWidth, 
    struct hash *tagOutWrappers, void *wrapperContext,
    boolean withFilters, char *itemPlural, int pageSize, struct hash *suggestHash, void (*addFunc)(void) )
/* Given a query to the database in conn that is basically a select query broken into
 * separate clauses, construct and display an HTML table around results. This HTML table has
 * column names that will sort the table, and optionally (if withFilters is set)
 * it will also allow field-by-field wildcard queries on a set of controls it draws above
 * the labels. 
 *    Much of the functionality rests on the call to webFilteredFieldedTable.  This function
 * does the work needed to bring in sections of potentially huge results sets into
 * the fieldedTable. */
{
struct dyString *query;
struct dyString *where;
webTableBuildQuery(cart, from, initialWhere, varPrefix, fields, withFilters, &query, &where);

/* Figure out size of query result */
struct dyString *countQuery = sqlDyStringCreate("select count(*) from %s", from);
sqlDyStringPrintf(countQuery, "%-s", where->string);   // trust
int resultsSize = sqlQuickNum(conn, countQuery->string);
dyStringFree(&countQuery);

char pageVar[64];
safef(pageVar, sizeof(pageVar), "%s_page", varPrefix);
int page = 0;
struct fieldedTableSegment context = { .tableSize=resultsSize};
if (resultsSize > pageSize)
    {
    page = cartUsualInt(cart, pageVar, 0) - 1;
    if (page < 0)
        page = 0;
    int lastPage = (resultsSize-1)/pageSize;
    if (page > lastPage)
        page = lastPage;
    context.tableOffset = page * pageSize;
    sqlDyStringPrintf(query, " limit %d offset %d", pageSize, context.tableOffset);
    }

struct fieldedTable *table = fieldedTableFromDbQuery(conn, query->string);
webFilteredFieldedTable(cart, table, returnUrl, varPrefix, maxFieldWidth, 
    tagOutWrappers, wrapperContext, withFilters, itemPlural, pageSize, &context, suggestHash, addFunc);
fieldedTableFree(&table);

dyStringFree(&query);
dyStringFree(&where);
}

