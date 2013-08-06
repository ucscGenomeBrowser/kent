
struct raToStructReader *raToStructReaderNew(char *name,  int fieldCount, char **fields,  
    int requiredFieldCount, char **requiredFields)
/* Create a helper object for parsing an ra file into a C structure.  This structure will
 * contain */
{
struct raToStructReader *reader;
AllocVar(reader);
reader->name = cloneString(name);
reader->fieldCount = fieldCount;
reader->fields = fields;
reader->requiredFieldCount = requiredFieldCount;
reader->requiredFields = requiredFields;
struct hash *fieldIds = reader->fieldIds = hashNew(4);
int i;
for (i=0; i<fieldCount; ++i)
    hashAddInt(fieldIds, fields[i], i);
if (requiredFieldCount > 0)
    {
    AllocArray(reader->requiredFieldIds, requiredFieldCount);
    for (i=0; i<requiredFieldCount; ++i)
        {
	char *required = requiredFields[i];
	struct hashEl *hel = hashLookup(fieldIds, required);
	if (hel == NULL)
	    errAbort("Required field %s is not in field list", required);
	reader->requiredFieldIds[i] = ptToInt(hel->val);
	}
    }
AllocArray(reader->fieldsObserved, fieldCount);
return reader;
}

void raToStructReaderFree(struct raToStructReader **pReader)
/* Free up memory associated with reader. */
{
struct raToStructReader *reader = *pReader;
if (reader != NULL)
    {
    freeMem(reader->name);
    freeHash(&reader->fieldIds);
    freeMem(reader->fieldIds);
    freeMem(reader->fieldsObserved);
    freez(pReader);
    }
}

void raToStructReaderCheckRequiredFields(struct raToStructReader *reader, struct lineFile *lf)
/* Make sure that all required files have been seen in the stanza we just parsed. */
{
int *requiredFieldIds = reader->requiredFieldIds;
bool *fieldsObserved = reader->fieldsObserved;
int i;
for (i=0; i<reader->requiredFieldCount; ++i)
    {
    if (!fieldsObserved[requiredFieldIds[i]])
	{
	errAbort("Required field %s not found line %d of %s", reader->requiredFields[i],
	    lf->lineIx, lf->fileName);
	}
    }
}

