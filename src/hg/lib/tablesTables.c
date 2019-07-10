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
#include "htmshell.h"
#include "web.h"
#include "cart.h"
#include "facetField.h"
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

struct fieldedTable *fieldedTableAndCountsFromDbQuery(struct sqlConnection *conn, char *query, int limit, int offset, 
    char *selectedFields, struct facetField ***pFfArray, int *pResultCount)
/* Return fieldedTable from a database query and also fetch use and select counts */
{
struct sqlResult *sr = sqlGetResult(conn, query);
char **fields;
int fieldCount = sqlResultFieldArray(sr, &fields);
struct facetField **ffArray;
AllocArray(ffArray, fieldCount);
struct fieldedTable *table = fieldedTableNew(query, fields, fieldCount);

struct facetField *ffList = facetFieldsFromSqlTableInit(fields, fieldCount, selectedFields, ffArray);

char **row;
int i = 0;
int id = 0;
char *nullVal = "n/a"; 
/* Scan through result saving it in list. */
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (perRowFacetFields(fieldCount, row, nullVal, ffArray))
	{
	if ((i >= offset) && (i < offset+limit))
	    fieldedTableAdd(table, row, fieldCount, ++id);
	++i;
	}
    }
facetFieldsFromSqlTableFinish(ffList, facetValCmpSelectCountDesc);
sqlFreeResult(&sr);
*pFfArray = ffArray;
*pResultCount = i;
return table;
}

static void showTableFilterInstructionsEtc(struct fieldedTable *table, 
    char *itemPlural, struct  fieldedTableSegment *largerContext, void (*addFunc)(int),
    char *visibleFacetList)
/* Print instructional text, and basic summary info on who passes filter, and a submit
 * button just in case user needs it */
{
/* Print info on matching */
int matchCount = slCount(table->rowList);
if (largerContext != NULL)  // Need to page?
     matchCount = largerContext->tableSize;

printf("<input class='btn btn-secondary' type='submit' name='submit' id='submit' value='Search'>");

printf("&nbsp&nbsp;");
printf("<input class='btn btn-secondary' type='button' id='clearButton' VALUE=\"Clear Search\">");
jsOnEventById("click", "clearButton",
    "$(':input').not(':button, :submit, :reset, :hidden, :checkbox, :radio').val('');\n"
    "$('[name=cdwBrowseFiles_page]').val('1');\n"
    "$('[name=clearSearch]').val('1');\n"
    "$('#submit').click();\n");

printf("<br>");

printf("%d&nbsp;%s&nbsp;found. ", matchCount, itemPlural);

if (addFunc)
    addFunc(matchCount);

if (!visibleFacetList)
    {
    printf("<BR>\n");
    printf("You can further filter search results field by field below. ");    
    printf("Wildcard * and ? characters are allowed in text fields. ");
    printf("&GT;min or &LT;max are allowed in numerical fields.<BR>\n");
    }
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

#ifdef NOT_CURRENTLY_USED
static void printWatermark(char *id, char *watermark)
/* Print light text filter prompt as watermark. */
{
jsInlineF(
"$(function() {\n"
"  $('#%s').watermark(\"%s\");\n"
"});\n", id, watermark);
}
#endif

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
printf("<link rel='stylesheet' href='//code.jquery.com/ui/1.12.1/themes/base/jquery-ui.css'>\n");
printf("<script src='https://code.jquery.com/ui/1.12.1/jquery-ui.js'></script>\n");

int i;
printf("<tr>");
for (i=0; i<table->fieldCount; ++i)
    {
    char *field = table->fields[i];
    char varName[256];
    safef(varName, sizeof(varName), "%s_f_%s", varPrefix, field);
    printf("<td>");

    /* Approximate size of input control in characters */
    int size = fieldedTableMaxColChars(table, i);
    if (size > maxLenField)
	size = maxLenField;

    /* Print input control getting previous value from cart.  Set an id=
     * so auto-suggest can find this control. */
    char *oldVal = cartUsualString(cart, varName, "");
    printf("<INPUT TYPE=TEXT NAME=\"%s\" id=\"%s\" SIZE=%d",
	varName, varName, size+1);
    if (isEmpty(oldVal))
        printf(" placeholder=\" filter \">\n");
    else
        printf(" value=\"%s\">\n", oldVal);

    /* Write out javascript to reset page number to 1 if filter changes */
    resetPageNumberOnChange(varName);

    /* Set up the auto-suggest list for this filter */
    if (suggestHash != NULL)
        {
	struct slName *suggestList = hashFindVal(suggestHash, field);
	if (suggestList != NULL)
	    {
	    printSuggestScript(varName, suggestList);
	    }
	}
    printf("</td>\n");
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
    printf("<td>");
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
    printf("</td>\n");
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
	if (isNum[fieldIx]) // vacuous, but left it just in case we want to do different stuff to numbers later
            printf("<td>");
        else
            printf("<td>");
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
	printf("</td>\n");
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
	    printf("<a href='#' id='%s'>&#9198;</a>", id);
	    jsOnEventByIdF("click", id, 
		"$('[name=%s_page]').val('1');\n"
		"$('#submit').click();\n"
		, varPrefix);
	    printf("&nbsp;&nbsp;&nbsp;");

	    // prev page
	    safef(id, sizeof id, "%s_prev", varPrefix);
	    printf("<a href='#' id='%s'>&#9194;</a>", id);
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
	    printf("<a href='#' id='%s'>&#9193;</a>", id);
	    jsOnEventByIdF("click", id, 
		"$('[name=%s_page]').val('%d');\n"
		"$('#submit').click();\n"
		, varPrefix, (curPage+1)+1);

	    // last page
	    printf("&nbsp;&nbsp;&nbsp;");
	    safef(id, sizeof id, "%s_last", varPrefix);
	    printf("<a href='#' id='%s'>&#9197;</a>", id);
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
    struct facetField **ffArray, char *visibleFacetList,
    void (*addFunc)(int))
/* Show a fielded table that can be sorted by clicking on column labels and optionally
 * that includes a row of filter controls above the labels .
 * The maxLenField is maximum character length of field before truncation with ...
 * Pass in 0 for no max */
{
if (strchr(returnUrl, '?') == NULL)
     errAbort("Expecting returnUrl to include ? in showFieldedTable\nIt's %s", returnUrl);

if (withFilters || visibleFacetList)
    showTableFilterInstructionsEtc(table, itemPlural, largerContext, addFunc, visibleFacetList);

printf("<div class='row'>\n"); // parent container

if (visibleFacetList)
    {
    // left column
    printf("<div class='col-xs-6 col-sm-4 col-md-4 col-lg-3 col-xl-3'>\n");

    // reset all facet value selections
    char *op = "resetAll";
    htmlPrintf("<a class='btn btn-secondary' href='../cgi-bin/cdwWebBrowse?%s=%s|url|&cdwCommand=browseFiles"
	    "&browseFiles_facet_op=%s|url|"
	    "&browseFiles_facet_fieldName=%s|url|"
	    "&browseFiles_facet_fieldVal=%s|url|"
	    "&cdwBrowseFiles_page=1' "
	    ">%s</a><br><br>\n",
	cartSessionVarName(), cartSessionId(cart),
	op, "", "",
	"Clear All"
	);

    struct slName *nameList = slNameListFromComma(visibleFacetList);
    int f;
    for (f = 0; f < table->fieldCount; ++f) 
	{
	struct facetField *field = ffArray[f];
	if (slNameInListUseCase(nameList, field->fieldName)) // i.e. is this field a visible facet?
	    {
            htmlPrintf("<div class='card facet-card'><div class='card-body'>\n");
            htmlPrintf("<h6 class='card-title'>%s</h6><dl>\n", field->fieldName);
	    struct facetVal *val;

	    if (!field->allSelected)  // add reset facet link
		{
		char *op = "reset";
		htmlPrintf("<dd><a class='btn btn-secondary' href='../cgi-bin/cdwWebBrowse?%s=%s|url|&cdwCommand=browseFiles"
			"&browseFiles_facet_op=%s|url|"
			"&browseFiles_facet_fieldName=%s|url|"
			"&browseFiles_facet_fieldVal=%s|url|"
			"&cdwBrowseFiles_page=1' "
			">%s</a></dd>\n",
		    cartSessionVarName(), cartSessionId(cart),
		    op, field->fieldName, "",
		    "Clear"
		    );
		}

	    int valuesShown = 0;
	    int valuesNotShown = 0;
	    if (field->showAllValues)  // Sort alphabetically if they want all values 
	        {
		slSort(&field->valList, facetValCmp);
		}
	    for (val = field->valList; val; val=val->next)
		{
		boolean specificallySelected = (val->selected && !field->allSelected);
		if ((val->selectCount > 0 && (field->showAllValues || valuesShown < FacetFieldLimit))
		    || specificallySelected)
		    {
		    ++valuesShown;
		    char *op = "add";
		    if (specificallySelected)
			op = "remove";
		    printf("<dd class=\"facet\">\n");
		    htmlPrintf("<input type=checkbox value=%s class=cdwFSCheckBox %s>&nbsp;",
			specificallySelected ? "true" : "false", 
			specificallySelected ? "checked" : "");
		    htmlPrintf("<a href='../cgi-bin/cdwWebBrowse?%s=%s|url|&cdwCommand=browseFiles"
			    "&browseFiles_facet_op=%s|url|"
			    "&browseFiles_facet_fieldName=%s|url|"
			    "&browseFiles_facet_fieldVal=%s|url|"
                            "&cdwBrowseFiles_page=1' "
			    ">",
			cartSessionVarName(), cartSessionId(cart),
			op, field->fieldName, val->val
                        );
		    htmlPrintf("%s (%d)</a>", val->val, val->selectCount);
		    printf("</dd>\n");
		    }
		else if (val->selectCount > 0)
		    {
		    ++valuesNotShown;
		    }
		}

	    // show "See More" link when facet has lots of values
	    if (valuesNotShown > 0)
		{
		char *op = "showAllValues";
		htmlPrintf("<dd><a href='../cgi-bin/cdwWebBrowse?%s=%s|url|&cdwCommand=browseFiles"
			"&browseFiles_facet_op=%s|url|"
			"&browseFiles_facet_fieldName=%s|url|"
			"&browseFiles_facet_fieldVal=%s|url|"
			"&cdwBrowseFiles_page=1' "
			">See %d More</a></dd>\n",
		    cartSessionVarName(), cartSessionId(cart),
		    op, field->fieldName, "", valuesNotShown 
		    );
		}

	    // show "See Fewer" link when facet has lots of values
	    if (field->showAllValues && valuesShown >= FacetFieldLimit)
		{
		char *op = "showSomeValues";
		htmlPrintf("<dd><a href='../cgi-bin/cdwWebBrowse?%s=%s|url|&cdwCommand=browseFiles"
			"&browseFiles_facet_op=%s|url|"
			"&browseFiles_facet_fieldName=%s|url|"
			"&browseFiles_facet_fieldVal=%s|url|"
			"&cdwBrowseFiles_page=1' "
			">%s</a></dd>\n",
		    cartSessionVarName(), cartSessionId(cart),
		    op, field->fieldName, "",
		    "See Fewer"
		    );
		}
            htmlPrintf("</div></div>\n");
	    }
	}
    printf("</div>\n");
    // Clicking a checkbox is actually a click on the following link
    jsInlineF(
	"$(function () {\n"
	"  $('.cdwFSCheckBox').click(function() {\n"
	"    this.nextSibling.nextSibling.click();\n"
	"  });\n"
	"});\n");
    }

// start right column, if there are two columns
if (visibleFacetList)
    printf("<div class='col-xs-6 col-sm-8 col-md-8 col-lg-9 col-xl-9'>\n");
else
    printf("<div class='col-12'>\n");
    
printf("  <div>\n");
printf("    <table class=\"table table-striped table-bordered table-sm text-nowrap\">\n");

/* Draw optional filters cells ahead of column labels*/
printf("<thead>\n");
if (withFilters)
    showTableFilterControlRow(table, cart, varPrefix, maxLenField, suggestHash);
showTableSortingLabelRow(table, cart, varPrefix, returnUrl);
printf("</thead>\n");

printf("<tbody>\n");
showTableDataRows(table, pageSize, maxLenField, tagOutputWrappers, wrapperContext);
printf("</tbody>\n");
printf("</table>\n");
printf("</div>");

if (largerContext != NULL)
    showTablePaging(table, cart, varPrefix, largerContext, pageSize);

if (visibleFacetList) // close right column, if there are two columns
    printf("</div>");

printf("</div>\n"); //close parent container
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
    slCount(table->rowList), NULL, NULL, NULL, NULL, NULL);
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
sqlDyStringPrintf(query, "select %-s from %-s", sqlCkIl(fields), sqlCkIl(from));
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
    boolean withFilters, char *itemPlural, int pageSize, struct hash *suggestHash, char *visibleFacetList,
    void (*addFunc)(int) )
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

char *selectedFacetValues=cartUsualString(cart, "cdwSelectedFieldValues", "");

struct facetField **ffArray = NULL;
struct fieldedTable *table = NULL;

char pageVar[64];
safef(pageVar, sizeof(pageVar), "%s_page", varPrefix);
int page = 0;
struct fieldedTableSegment context;
page = cartUsualInt(cart, pageVar, 0) - 1;
if (page < 0)
    page = 0;
context.tableOffset = page * pageSize;

if (visibleFacetList)
    {
    table = fieldedTableAndCountsFromDbQuery(conn, query->string, pageSize, context.tableOffset, selectedFacetValues, &ffArray, &context.tableSize);
    }
else
    {
    /* Figure out size of query result */
    struct dyString *countQuery = sqlDyStringCreate("select count(*) from %-s", sqlCkIl(from));
    sqlDyStringPrintf(countQuery, "%-s", where->string);   // trust
    context.tableSize = sqlQuickNum(conn, countQuery->string);
    dyStringFree(&countQuery);
    }

if (context.tableSize > pageSize)
    {
    int lastPage = (context.tableSize-1)/pageSize;
    if (page > lastPage)
        page = lastPage;
    context.tableOffset = page * pageSize;
    }

if (!visibleFacetList)
    {
    sqlDyStringPrintf(query, " limit %d offset %d", pageSize, context.tableOffset);
    table = fieldedTableFromDbQuery(conn, query->string);
    }

webFilteredFieldedTable(cart, table, returnUrl, varPrefix, maxFieldWidth, 
    tagOutWrappers, wrapperContext, withFilters, itemPlural, pageSize, &context, suggestHash, ffArray, visibleFacetList, addFunc);
fieldedTableFree(&table);

dyStringFree(&query);
dyStringFree(&where);
}

