/* testSearch - test the search functionality */
#include "common.h"
#include "cart.h"
#include "cheapcgi.h"
#include "hgFind.h"
#include "hdb.h"
#include "hui.h"

static char const rcsid[] = "$Id: testSearch.c,v 1.7 2008/09/03 19:21:13 markd Exp $";

/* Need to get a cart in order to use hgFind. */
struct cart *cart = NULL;
char *excludeVars[] = { NULL };

struct searchTestCase 
    {
    struct searchTestCase *next;
    char *searchTerm;
    char *database;
    int posCount;
    struct searchResults *results;
    };

struct searchResults
    {
    struct searchResults *next;
    char *tableName;
    char *chrom;
    int start;
    int end;
    char *browserName;
    };


void usage()
{
errAbort("testSearch - test search functionality.\n"
    "usage:\n"
    "  testSearch inputFile\n");
}

struct searchTestCase *readInput(char *fileName)
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
char *headerLine[3];
char *resultLine[5];
struct searchTestCase *testCase;
struct searchTestCase *testCaseList = NULL;
struct searchResults *result;
struct searchResults *resultList;
int elementCount;
int matchCount;
int pos = 0;

while (lineFileNext(lf, &line, NULL))
    {
    elementCount = chopString(line, " ", headerLine, ArraySize(headerLine));
    if (elementCount != 3)
        {
	fprintf(stderr, "Error: formatting problem, exitting\n");
	return NULL;
	}
    resultList = NULL;
    matchCount = sqlUnsigned(headerLine[2]);
    for (pos = 0; pos < matchCount; pos++)
        {
	lineFileNext(lf, &line, NULL);
	elementCount = chopString(line, " ", resultLine, ArraySize(resultLine));
	if (elementCount != 5)
	    {
	    fprintf(stderr, "Error: formatting problem, exitting\n");
	    return NULL;
	    }
	AllocVar(result);
        result->tableName = cloneString(resultLine[0]);
        result->chrom = cloneString(resultLine[1]);
	result->start = sqlUnsigned(resultLine[2]);
	result->end = sqlUnsigned(resultLine[3]);
        result->browserName = cloneString(resultLine[4]);
        slAddHead(&resultList, result);
	}
    AllocVar(testCase);
    testCase->searchTerm = headerLine[0];
    testCase->database = headerLine[1];
    testCase->posCount = matchCount;
    testCase->results = resultList;
    slAddHead(&testCaseList, testCase);
    }
return testCaseList;
}

struct searchTestCase *morph(char *database, char *searchTerm, struct hgPositions *hgpList)
/* convert an hgpList into a searchTestCase */
{
struct hgPositions *hgp = NULL;
struct hgPosTable *table = NULL;
struct hgPos *pos = NULL;
struct searchTestCase *testCase = NULL;
struct searchResults *result = NULL;
struct searchResults *resultList = NULL;
int count = 0;

AllocVar(testCase);
testCase->database = cloneString(database);
testCase->searchTerm = cloneString(searchTerm);

for (hgp = hgpList; hgp != NULL; hgp = hgp->next)
    {
    if (hgp->tableList != NULL)
        {
        for (table = hgp->tableList; table != NULL; table = table->next)
            {
	    if (table->posList != NULL)
	        {
	        for (pos = table->posList; pos != NULL; pos = pos->next)
	            {
		    count++;
		    AllocVar(result);
		    result->tableName = cloneString(table->name);
		    result->chrom = cloneString(pos->chrom);
		    result->start = pos->chromStart;
		    result->end = pos->chromEnd;
		    result->browserName = cloneString(pos->browserName);
		    slAddHead(&resultList, result);
		    }
	        }
	    }
	}
    }
testCase->posCount = count;
testCase->results = resultList;
return testCase;
}


void compareResults(struct searchTestCase *expected, struct searchTestCase *actual)
/* don't assume the results are in the same order */
/* for each excepted, just do a linear read of the actual list */
/* all of these lists should be small enough so performance isn't an issue */
{
struct searchResults *result1, *result2;

if (differentString(expected->searchTerm, actual->searchTerm))
    {
    fprintf(stderr, "Error: mismatched search terms: expected = %s, actual = %s\n", 
            expected->searchTerm, actual->searchTerm);
    return;
    }
if (differentString(expected->database, actual->database))
    {
    fprintf(stderr, "Error: mismatched databases: expected = %s, actual = %s\n", expected->database, actual->database);
    return;
    }
if (expected->posCount != actual->posCount)
    {
    fprintf(stderr, "Error: mismatched posCount: expected = %d, actual = %d\n", expected->posCount, actual->posCount);
    return;
    }

result1 = expected->results;
while (result1)
    {
    boolean matchFound = FALSE;
    result2 = actual->results;
    while (result2)
        {
	if (differentString(result1->tableName, result2->tableName)) 
	    {
	    result2 = result2->next;
	    continue;
	    }
	if (differentString(result1->chrom, result2->chrom)) 
	    {
	    result2 = result2->next;
	    continue;
	    }
	if (result1->start != result2->start)
	    {
	    result2 = result2->next;
	    continue;
	    }
	if (result1->end != result2->end)
	    {
	    result2 = result2->next;
	    continue;
	    }
	if (differentString(result1->browserName, result2->browserName)) 
	    {
	    result2 = result2->next;
	    continue;
	    }
	matchFound = TRUE;
	break;
        }
    if (!matchFound)
        {
	fprintf(stderr, "Error: no match found for expected result %s\n", result1->tableName);
        fprintf(stderr, "position = %s:%d-%d\n", result1->chrom, result1->start, result1->end);
        fprintf(stderr, "browserName = %s\n", result1->browserName);
	}
    result1 = result1->next;
    }

result1 = actual->results;
while (result1)
    {
    boolean matchFound = FALSE;
    result2 = expected->results;
    while (result2)
        {
	if (differentString(result1->tableName, result2->tableName)) 
	    {
	    result2 = result2->next;
	    continue;
	    }
	if (differentString(result1->chrom, result2->chrom)) 
	    {
	    result2 = result2->next;
	    continue;
	    }
	if (result1->start != result2->start)
	    {
	    result2 = result2->next;
	    continue;
	    }
	if (result1->end != result2->end)
	    {
	    result2 = result2->next;
	    continue;
	    }
	if (differentString(result1->browserName, result2->browserName)) 
	    {
	    result2 = result2->next;
	    continue;
	    }
	matchFound = TRUE;
	break;
        }
    if (!matchFound)
        {
	fprintf(stderr, "Error: no match found for actual result %s\n", result1->tableName);
        fprintf(stderr, "position = %s:%d-%d\n", result1->chrom, result1->start, result1->end);
        fprintf(stderr, "browserName = %s\n", result1->browserName);
	}
    result1 = result1->next;
    }

}

void searchAndCompare(struct searchTestCase *testCaseList)
/* Read testCaseList.  Execute each search.  Report unexpected results. */
{
struct hgPositions *hgpList = NULL;
struct searchTestCase *morphOutput = NULL;

while (testCaseList)
    {
    verbose(1, "database = %s, searchTerm = %s\n", testCaseList->database, testCaseList->searchTerm);
    hgpList = hgPositionsFind(testCaseList->database, testCaseList->searchTerm, "", "hgTracks", cart, FALSE);
    /* handle cases where there are no expected matches */
    if (testCaseList->posCount == 0)
        {
        if (hgpList == NULL || hgpList->posCount == 0) 
	    {
	    testCaseList = testCaseList->next;
	    continue;
	    }
	fprintf(stderr, "Error: expected no matches, got some\n");
	fprintf(stderr, "database = %s, searchTerm = %s\n", 
	        testCaseList->database, testCaseList->searchTerm);
	continue;
	}
    morphOutput = morph(testCaseList->database, testCaseList->searchTerm, hgpList);
    compareResults(testCaseList, morphOutput);
    testCaseList = testCaseList->next;
    }
}


int main(int argc, char *argv[])
{
struct searchTestCase *testCases =  NULL;

cgiSpoof(&argc, argv);
cart = cartForSession(hUserCookie(), excludeVars, NULL);

if (argc != 2)
    usage();

testCases = readInput(argv[1]);
searchAndCompare(testCases);

return 0;
}
