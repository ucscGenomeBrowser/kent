
#include "common.h"
#include "hash.h"
#include "hdb.h"
#include "web.h"
#include "trashDir.h"
#include "hCommon.h"
#include "hgColors.h"
#include "fieldedTable.h"
#include "tablesTables.h"
#include "facetField.h"
#include "hgConfig.h"
#include "facetedTable.h"

static struct facetedTable *facetedTableNew(char *name, char *varPrefix, char *facets)
/* Return new, mostly empty faceted table */
{
struct facetedTable *facTab;
AllocVar(facTab);
facTab->name = cloneString(name);
facTab->varPrefix = cloneString(varPrefix);
facTab->facets = cloneString(facets);
return facTab;
}

struct facetedTable *facetedTableFromTable(struct fieldedTable *table, 
    char *varPrefix, char *facets)
/* Construct a facetedTable around a fieldedTable */
{
struct facetedTable *facTab = facetedTableNew(table->name, varPrefix, facets);
facTab->table = table;
AllocArray(facTab->ffArray, table->fieldCount);
return facTab;
}


void facetedTableFree(struct facetedTable **pFt)
/* Free up resources associated with faceted table */
{
struct facetedTable *facTab = *pFt;
if (facTab != NULL)
    {
    freeMem(facTab->name);
    freeMem(facTab->varPrefix);
    freeMem(facTab->facets);
    freeMem(facTab->ffArray);
    freez(pFt);
    }
}

static void facetedTableWebInit()
/* Print out scripts and css that we need.  We should be in a page body or title. */
{
static boolean initted = FALSE;
if (initted)
    return;
initted = TRUE;
webIncludeResourceFile("facets.css");
printf("\t\t<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/1.11.0/jquery.min.js\"></script>");
printf("\t\t<link rel=\"stylesheet\" href=\"https://use.fontawesome.com/releases/v5.7.2/css/all.css\"\n"
    "\t\t integrity=\"sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr\"\n"
    "\t\t crossorigin=\"anonymous\">\n"
    "\n"
    "\t\t<!-- Latest compiled and minified CSS -->\n"
    "\t\t<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0/css/bootstrap.min.css\"\n"
    "\t\t integrity=\"sha384-Gn5384xqQ1aoWXA+058RXPxPg6fy4IWvTNh0E263XmFcJlSAwiGgFAW/dAiS6JXm\"\n"
    "\t\t crossorigin=\"anonymous\">\n"
    "\n"
    "\t\t<!-- Latest compiled and minified JavaScript -->\n"
    "\t\t<script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0/js/bootstrap.min.js\"\n"
    "\t\t integrity=\"sha384-JZR6Spejh4U02d8jOt6vLEHfe/JQGiRRSQQxSfFWpi1MquVdAyjUar5+76PVCmYl\"\n"
    "\t\t crossorigin=\"anonymous\"></script>\n"
    );
}

static char *facetedTableSelList(struct facetedTable *facTab, struct cart *cart)
/* Look up list of selected items in facets from cart */
{
char var[256];
safef(var, sizeof(var), "%s_facet_selList", facTab->varPrefix);
return cartOptionalString(cart, var);
}

static char *facetedTableSelOp(struct facetedTable *facTab, struct cart *cart)
/* Look up selOp in cart */
{
char var[256];
safef(var, sizeof(var), "%s_facet_op", facTab->varPrefix);
return cartOptionalString(cart, var);
}

static char *facetedTableSelField(struct facetedTable *facTab, struct cart *cart)
/* Look up sel field in cart */
{
char var[256];
safef(var, sizeof(var), "%s_facet_fieldName", facTab->varPrefix);
return cartOptionalString(cart, var);
}

static char *facetedTableSelVal(struct facetedTable *facTab, struct cart *cart)
/* Look up sel val in cart */
{
char var[256];
safef(var, sizeof(var), "%s_facet_fieldVal", facTab->varPrefix);
return cartOptionalString(cart, var);
}

static void facetedTableRemoveOpVars(struct facetedTable *facTab, struct cart *cart)
/* Remove sel op/field/name vars from cart */
{
char var[256];
safef(var, sizeof(var), "%s_facet_op", facTab->varPrefix);
cartRemove(cart, var);
safef(var, sizeof(var), "%s_facet_fieldVal", facTab->varPrefix);
cartRemove(cart, var);
safef(var, sizeof(var), "%s_facet_fieldName", facTab->varPrefix);
cartRemove(cart, var);
}

boolean facetedTableUpdateOnClick(struct facetedTable *facTab, struct cart *cart)
/* If we got called by a click on a facet deal with that and return TRUE, else do
 * nothing and return false */
{
char *selOp = facetedTableSelOp(facTab, cart);
if (selOp)
    {
    char *selFieldName = facetedTableSelField(facTab, cart);
    char *selFieldVal = facetedTableSelVal(facTab, cart);
    if (selFieldName && selFieldVal)
	{
	char selListVar[256];
	safef(selListVar, sizeof(selListVar), "%s_facet_selList", facTab->varPrefix);
	char *selectedFacetValues=cartUsualString(cart, selListVar, "");
	struct facetField *selList = deLinearizeFacetValString(selectedFacetValues);
	selectedListFacetValUpdate(&selList, selFieldName, selFieldVal, selOp);
	char *newSelectedFacetValues = linearizeFacetVals(selList);
	cartSetString(cart, selListVar, newSelectedFacetValues);
	facetedTableRemoveOpVars(facTab, cart);
	}
    return TRUE;
    }
else
    return FALSE;
}

struct fieldedTable *facetedTableSelect(struct facetedTable *facTab, struct cart *cart)
/* Return table containing rows of table that have passed facet selection */
{
char *selList = facetedTableSelList(facTab, cart);
return facetFieldsFromFieldedTable(facTab->table, selList, facTab->ffArray);
}

struct slInt *facetedTableSelectOffsets(struct facetedTable *facTab, struct cart *cart)
/* Return a list of row positions that pass faceting */
{
char *selList = facetedTableSelList(facTab, cart);
struct fieldedTable *ft = facTab->table;
int fieldCount = ft->fieldCount;
struct slInt *retList = NULL;
facetFieldsFromSqlTableInit(ft->fields, fieldCount,
    selList, facTab->ffArray);
struct fieldedRow *fr;
int ix = 0;
for (fr = facTab->table->rowList; fr != NULL; fr = fr->next)
    {
    if (perRowFacetFields(fieldCount, fr->row, "", facTab->ffArray))
	{
	struct slInt *el = slIntNew(ix);
	slAddHead(&retList, el);
	}
    ++ix;
    }
slReverse(&retList);
return retList;
}


void facetedTableWriteHtml(struct facetedTable *facTab, struct cart *cart,
    struct fieldedTable *selected, char *displayList, 
    char *returnUrl, int maxLenField, 
    struct hash *tagOutputWrappers, void *wrapperContext, int facetUsualSize)
/* Write out the main HTML associated with facet selection and table. */
{
facetedTableWebInit();
struct hash *emptyHash = hashNew(0);
webFilteredFieldedTable(cart, selected, 
    displayList, returnUrl, facTab->varPrefix, 
    maxLenField, tagOutputWrappers, wrapperContext,
    FALSE, NULL,
    selected->rowCount, facetUsualSize,
    NULL, emptyHash,
    facTab->ffArray, facTab->facets,
    NULL);
hashFree(&emptyHash);
}

