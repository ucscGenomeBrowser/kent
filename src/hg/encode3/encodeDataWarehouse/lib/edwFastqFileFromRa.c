/* edwFastqFileFromRa - fill in a edwFastqFile from a .ra file.  This file largely 
 * generated automatically with raToStructGen. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "obscure.h"
#include "ra.h"
#include "raToStruct.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "edwFastqFileFromRa.h"

struct raToStructReader *edwFastqFileRaReader()
/* Make a raToStructReader for edwFastqFile */
{
static char *fields[] = {
    "id",
    "fileId",
    "sampleCount",
    "basesInSample",
    "sampleFileName",
    "readCount",
    "baseCount",
    "readSizeMean",
    "readSizeStd",
    "readSizeMin",
    "readSizeMax",
    "qualMean",
    "qualStd",
    "qualMin",
    "qualMax",
    "qualType",
    "qualZero",
    "atRatio",
    "aRatio",
    "cRatio",
    "gRatio",
    "tRatio",
    "nRatio",
    "qualPos",
    "aAtPos",
    "cAtPos",
    "gAtPos",
    "tAtPos",
    "nAtPos",
    };
static char *requiredFields[] = {
    "sampleCount",
    "basesInSample",
    "readCount",
    "baseCount",
    "readSizeMean",
    "readSizeStd",
    "readSizeMin",
    "readSizeMax",
    "qualMean",
    "qualStd",
    "qualMin",
    "qualMax",
    "qualType",
    "qualZero",
    "atRatio",
    "aRatio",
    "cRatio",
    "gRatio",
    "tRatio",
    "nRatio",
    "qualPos",
    "aAtPos",
    "cAtPos",
    "gAtPos",
    "tAtPos",
    "nAtPos",
    };
return raToStructReaderNew("edwFastqFile", 29, fields, 26, requiredFields);
}


struct edwFastqFile *edwFastqFileFromNextRa(struct lineFile *lf, struct raToStructReader *reader)
/* Return next stanza put into an edwFastqFile. */
{
if (!raSkipLeadingEmptyLines(lf, NULL))
    return NULL;

struct edwFastqFile *el;
AllocVar(el);

bool *fieldsObserved = reader->fieldsObserved;
bzero(fieldsObserved, reader->fieldCount);

char *tag, *val;
while (raNextTagVal(lf, &tag, &val, NULL))
    {
    struct hashEl *hel = hashLookup(reader->fieldIds, tag);
    if (hel != NULL)
        {
	int id = ptToInt(hel->val);
	if (fieldsObserved[id])
	     errAbort("Duplicate tag %s line %d of %s\n", tag, lf->lineIx, lf->fileName);
	fieldsObserved[id] = TRUE;
	switch (id)
	    {
	    case 0:
	        {
	        el->id = sqlUnsigned(val);
		break;
	        }
	    case 1:
	        {
	        el->fileId = sqlUnsigned(val);
		break;
	        }
	    case 2:
	        {
	        el->sampleCount = sqlLongLong(val);
		break;
	        }
	    case 3:
	        {
	        el->basesInSample = sqlLongLong(val);
		break;
	        }
	    case 4:
	        {
	        el->sampleFileName = cloneString(val);
		break;
	        }
	    case 5:
	        {
	        el->readCount = sqlLongLong(val);
		break;
	        }
	    case 6:
	        {
	        el->baseCount = sqlLongLong(val);
		break;
	        }
	    case 7:
	        {
	        el->readSizeMean = sqlDouble(val);
		break;
	        }
	    case 8:
	        {
	        el->readSizeStd = sqlDouble(val);
		break;
	        }
	    case 9:
	        {
	        el->readSizeMin = sqlDouble(val);
		break;
	        }
	    case 10:
	        {
	        el->readSizeMax = sqlDouble(val);
		break;
	        }
	    case 11:
	        {
	        el->qualMean = sqlDouble(val);
		break;
	        }
	    case 12:
	        {
	        el->qualStd = sqlDouble(val);
		break;
	        }
	    case 13:
	        {
	        el->qualMin = sqlDouble(val);
		break;
	        }
	    case 14:
	        {
	        el->qualMax = sqlDouble(val);
		break;
	        }
	    case 15:
	        {
	        el->qualType = cloneString(val);
		break;
	        }
	    case 16:
	        {
	        el->qualZero = sqlSigned(val);
		break;
	        }
	    case 17:
	        {
	        el->atRatio = sqlDouble(val);
		break;
	        }
	    case 18:
	        {
	        el->aRatio = sqlDouble(val);
		break;
	        }
	    case 19:
	        {
	        el->cRatio = sqlDouble(val);
		break;
	        }
	    case 20:
	        {
	        el->gRatio = sqlDouble(val);
		break;
	        }
	    case 21:
	        {
	        el->tRatio = sqlDouble(val);
		break;
	        }
	    case 22:
	        {
	        el->nRatio = sqlDouble(val);
		break;
	        }
	    case 23:
	        {
                int qualPosSize;
		sqlDoubleDynamicArray(val, &el->qualPos, &qualPosSize);
                if (!fieldsObserved[10])
                     errAbort("readSizeMax must precede qualPos");
                if (qualPosSize != el->readSizeMax)
                     errAbort("qualPos has wrong count");
		break;
	        }
	    case 24:
	        {
                int aAtPosSize;
		sqlDoubleDynamicArray(val, &el->aAtPos, &aAtPosSize);
                if (!fieldsObserved[10])
                     errAbort("readSizeMax must precede aAtPos");
                if (aAtPosSize != el->readSizeMax)
                     errAbort("aAtPos has wrong count");
		break;
	        }
	    case 25:
	        {
                int cAtPosSize;
		sqlDoubleDynamicArray(val, &el->cAtPos, &cAtPosSize);
                if (!fieldsObserved[10])
                     errAbort("readSizeMax must precede cAtPos");
                if (cAtPosSize != el->readSizeMax)
                     errAbort("cAtPos has wrong count");
		break;
	        }
	    case 26:
	        {
                int gAtPosSize;
		sqlDoubleDynamicArray(val, &el->gAtPos, &gAtPosSize);
                if (!fieldsObserved[10])
                     errAbort("readSizeMax must precede gAtPos");
                if (gAtPosSize != el->readSizeMax)
                     errAbort("gAtPos has wrong count");
		break;
	        }
	    case 27:
	        {
                int tAtPosSize;
		sqlDoubleDynamicArray(val, &el->tAtPos, &tAtPosSize);
                if (!fieldsObserved[10])
                     errAbort("readSizeMax must precede tAtPos");
                if (tAtPosSize != el->readSizeMax)
                     errAbort("tAtPos has wrong count");
		break;
	        }
	    case 28:
	        {
                int nAtPosSize;
		sqlDoubleDynamicArray(val, &el->nAtPos, &nAtPosSize);
                if (!fieldsObserved[10])
                     errAbort("readSizeMax must precede nAtPos");
                if (nAtPosSize != el->readSizeMax)
                     errAbort("nAtPos has wrong count");
		break;
	        }
	    default:
	        internalErr();
		break;
	    }
	}
    }

if (reader->requiredFieldIds != NULL)
    raToStructReaderCheckRequiredFields(reader, lf);
return el;
}

struct edwFastqFile *edwFastqFileOneFromRa(char *fileName)
/* Return list of all edwFastqFiles in file. */
{
struct raToStructReader *reader = edwFastqFileRaReader();
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct edwFastqFile *fqf = edwFastqFileFromNextRa(lf, reader);
if (fqf == NULL)
     errAbort("No data in %s", fileName);
raToStructReaderFree(&reader);
return fqf;
}


