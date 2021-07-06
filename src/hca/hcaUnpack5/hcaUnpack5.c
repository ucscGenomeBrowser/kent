/* hcaUnpack5 - Convert cellxgene hdf5 files to something cell browser and genome browser 
 * like better.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "portable.h"
#include "obscure.h"
#include "sparseMatrix.h"
#include <hdf5.h>

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hcaUnpack5 - Convert h5ad (scanpy) files to a directory filled with 3 things\n"
  "usage:\n"
  "   hcaUnpack5 input.h5ad outDir\n"
  "The output dir will be populated with exprMatrix.tsv, meta.tsv, and gene.lst\n"
  "where:\n"
  "    exprMatrix.tsv has the cell x gene matrix with cells as columns.  This includes\n"
  "             the cell names in the first row and the gene names in the first column.\n."
  "    meta.tsv has the cell-by-cell metadata.  The first row is labels, and the first\n"
  "             column corresponds with the cell names in exprMatrix\n"
  "    gene.lst has the list of genes, one per line\n"
  "options:\n"
  "   -noMatrix - skip the matrix making step\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"noMatrix", OPTION_BOOLEAN},
   {NULL, 0},
};


hid_t h5fOpen(char *fileName,  int accessType)
/* Open up file or die trying.  accessType should be 
 * H5F_ACC_RDONLY, H5F_ACC_RDWR, etc. */
{
hid_t ret = H5Fopen(fileName, accessType, H5P_DEFAULT);
if (ret < 0)
   errAbort("Couldn't open %s", fileName);
return ret;
}

hid_t  h5dOpen(hid_t parent, char *name)
/* Open up a subpart or die trying */
{
hid_t ret = H5Dopen(parent, name, H5P_DEFAULT);
H5_DLL ssize_t H5Iget_name(hid_t id, char *name/*out*/, size_t size);
if (ret < 0)
   {
   char parentName[256];
   H5Iget_name(parent, parentName, sizeof(parentName));
   errAbort("ERROR: Couldn't find dataset %s in %s", name, parentName);
   }
return ret;
}

hid_t  h5gOpen(hid_t parent, char *name)
/* Open up a subpart or die trying */
{
hid_t ret = H5Gopen(parent, name, H5P_DEFAULT);
if (ret < 0)
   {
   char parentName[256];
   H5Iget_name(parent, parentName, sizeof(parentName));
   errAbort("ERROR: Couldn't find group %s in %s", name, parentName);
   }
return ret;
}


void h5lIterate(hid_t hid, H5L_iterate_t callback, void *context)
/* Iterate over a list. Requires a callback */
{
herr_t status =  H5Literate(hid, H5_INDEX_NAME, H5_ITER_NATIVE, NULL, callback, context);
if (status < 0)
    internalErr();   // does this ever happen?
}

void h5dReadAll(hid_t hid,  hid_t type, void *buf)
/* Do the read, and pray the buf is pointing to someplace that can hold it... */
{
herr_t status = H5Dread(hid, type, H5S_ALL, H5S_ALL, H5P_DEFAULT, buf);
if (status < 0)
   {
   char name[256];
   H5Iget_name(hid, name, sizeof(name));
   errAbort("ERROR: Couldn't read from %s", name);
   }
}

void h5dRead(hid_t file,  hid_t memType, hid_t memSpace, hid_t fileSpace, hid_t pList, void *buf)
/* Do the read, and pray the buf is pointing to someplace that can hold it... */
{
herr_t status = H5Dread(file, memType, memSpace, fileSpace, pList, buf);
if (status < 0)
   {
   char name[256];
   H5Iget_name(file, name, sizeof(name));
   errAbort("ERROR: Couldn't read from %s", name);
   }
}

struct vocab
/* A controlled vocab we can refer to by short numbers instead of strings */
    {
    struct vocab *next;
    char *name;	    /* Column this vocab is associated with */
    int valCount;   /* Number of distinct values */
    char **stringVals;    /* array of strings */
    };

struct metaCol 
/* One of these for each metadata column /field */
    {
    struct metaCol *next;
    char *name;	    /* field name */
    int size;	    /* size of column, ie number of rows */
    H5T_class_t classt;	/* class */
    struct vocab *vocab;    /* Some columns have associated vocabs */
    union { int *asVocab; int *asInt; double *asDouble; char **asString;} val;
    };

struct metaCol *metaColNew(const char *name)
/* Build up a new metaCol of given name */
{
struct metaCol *col;
AllocVar(col);
col->name = cloneString(name);
return col;
}

struct hcaUnpack5
/* There is just one of these for our program - it's the context
 * we give most of our h5 callbacks */
    {
    struct hcaUnpack5 *next;
    hid_t   file;		/* Open hdf5 file */
    struct vocab *vocabList;	/* List of all restricted vocabs */
    struct hash *vocabHash;	/* Hash of vocabs keyed by vocab->name */
    struct metaCol *metaList;	/* List of all metaCols */
    struct hash *metaHash;	/* Hash of all metaCols keyed by metaCol->name */
    struct metaCol *indexCol;	/* Column that has cell IDs */
    int cellCount;		/* Number of cells - rows in meta.tsv cols-1 in exprHash */
    int geneCount;		/* Number of genes */
    };

struct vocab *vocabNew(const char *name)
/* Make a new, largely empty, vocab structure */
{
struct vocab *vocab;
AllocVar(vocab);
vocab->name = cloneString(name);
return vocab;
}

hid_t cVarString()
/* Return hid_t that'll be usual C variable length 0 terminated strings */
{
hid_t memtype = H5Tcopy (H5T_C_S1);
H5Tset_size (memtype, H5T_VARIABLE);
H5Tset_cset(memtype, H5T_CSET_UTF8) ;
return memtype;
}

void readVocab(struct vocab *vocab, hid_t hid, struct hcaUnpack5 *context)
/* Read vocab from given hid */
{
hid_t space = H5Dget_space (hid);
hsize_t     dims[1];
H5Sget_simple_extent_dims (space, dims, NULL);
vocab->valCount = dims[0];
AllocArray(vocab->stringVals, vocab->valCount);

/* Set us up for zero-terminated UTF8 (or ascii) strings */
hid_t memtype = cVarString();

/* Read it */
h5dReadAll(hid, memtype, vocab->stringVals);

/* Clean up */
H5Sclose(space);
H5Tclose(memtype);
}

int readCol(struct metaCol *col, hid_t hid, struct hcaUnpack5 *context)
/* Read metaCol from given hid */
{
hid_t space = H5Dget_space (hid);
hsize_t     dims[1];
H5Sget_simple_extent_dims (space, dims, NULL);
int size = col->size = dims[0];
hid_t type = H5Dget_type(hid);
H5T_class_t classt = col->classt = H5Tget_class(type);

struct vocab *vocab = hashFindVal(context->vocabHash, col->name);
if (vocab != NULL)
    {
    int *buf;
    AllocArray(buf, size);
    col->val.asVocab = buf;
    h5dReadAll(hid, H5T_NATIVE_INT, buf);
    }
else
    {
    switch (classt)
        {
	case H5T_INTEGER:
	    {
	    int *buf;
	    AllocArray(buf, size);
	    col->val.asInt = buf;
	    h5dReadAll(hid, H5T_NATIVE_INT, buf);
	    break;
	    }
	case H5T_FLOAT:
	    {
	    double *buf;
	    AllocArray(buf, size);
	    col->val.asDouble = buf;
	    h5dReadAll(hid, H5T_NATIVE_DOUBLE, buf);
	    break;
	    }
	case H5T_STRING:
	    {
	    char **buf;
	    AllocArray(buf, size);
	    col->val.asString = buf;
	    h5dReadAll(hid, cVarString(), buf);
	    break;
	    }
	default:
	    warn("Skipping type %d", type);
	    break;
	}
    }

/* Clean up and go home */
H5Sclose(space);
H5Tclose(type);
return size;
}


herr_t vocabCallback(hid_t loc_id, const char *name, const H5L_info_t *info,
            void *operator_data)
/* Callback from H5Iterate that we use to build up a column that has
 * an assocated vocabulary */
{
struct hcaUnpack5 *context = operator_data;
herr_t          status;
H5O_info_t      infobuf;

/*
 * Get type of the object and display its name and type.
 * The name of the object is passed to this function by
 * the Library.
 */
status = H5Oget_info_by_name(loc_id, name, &infobuf, H5P_DEFAULT);
if (status < 0)
    errAbort("missing status for %s", name);

if (infobuf.type == H5O_TYPE_DATASET)
    {
    struct vocab *vocab = vocabNew(name);
    hashAdd(context->vocabHash, vocab->name, vocab);
    slAddHead(&context->vocabList, vocab);
    }

return 0;
}

herr_t obsCallback(hid_t loc_id, const char *name, const H5L_info_t *info,
            void *operator_data)
/* Callback from H5Iterate that we use to build up a column that has
 * an assocated vocabulary */
{
struct hcaUnpack5 *context = operator_data;
herr_t          status;
H5O_info_t      infobuf;

/*
 * Get type of the object and display its name and type.
 * The name of the object is passed to this function by
 * the Library.
 */
status = H5Oget_info_by_name(loc_id, name, &infobuf, H5P_DEFAULT);
if (status < 0)
    errAbort("missing status for %s", name);


if (infobuf.type == H5O_TYPE_DATASET)
    {
    struct metaCol *col = metaColNew(name);
    hashAdd(context->metaHash, col->name, col);
    slAddHead(&context->metaList, col);
    struct vocab *vocab = hashFindVal(context->vocabHash, col->name);
    if (vocab != NULL)
        col->vocab = vocab;
    }

return 0;
}

void saveMeta(struct hcaUnpack5 *context, char *fileName)
/* Save the metaList to fileName */
{
FILE *f = mustOpen(fileName, "w");
fprintf(f, "#");
fprintf(f, "_index");
struct metaCol *col;
for (col = context->metaList; col != NULL; col = col->next)
    {
    fprintf(f, "\t%s", col->name);
    }
fprintf(f, "\n");

struct metaCol *indexCol = context->indexCol;
int colSize = context->cellCount;
int i;
for (i=0; i<colSize; ++i)
    {
    fprintf(f, "%s", indexCol->val.asString[i]);
    for (col = context->metaList; col != NULL; col = col->next)
	{
	struct vocab *vocab = col->vocab;
	if (vocab != NULL)
	    fprintf(f, "\t%s", vocab->stringVals[col->val.asVocab[i]]);
	else 
	    {
	    switch (col->classt)
	        {
		case H5T_INTEGER:
		    fprintf(f, "\t%d", col->val.asInt[i]);
		    break;
		case H5T_FLOAT:
		    fprintf(f, "\t%g", col->val.asDouble[i]);
		    break;
		case H5T_STRING:
		    fprintf(f, "\t%s", col->val.asString[i]);
		    break;
		default:
		    internalErr();
		    break;
		}
	    }
	}
    fprintf(f, "\n");
    }
carefulClose(&f);
}

int h5dReadFloatArray(hid_t file, char *path, float **retData)
/* Load up float array from file into memory */
{
hid_t hid = h5dOpen(file, path);
hsize_t space = H5Dget_space(hid);
hsize_t dims[8];
int dimCount = H5Sget_simple_extent_dims(space, dims, NULL);
if (dimCount != 1)
    errAbort("Expecting %s to be one dimensional, but it has %d dimensions", path, dimCount);
int size = dims[0];
float *array;
AllocArray(array, size);
h5dReadAll(hid, H5T_NATIVE_FLOAT, array);

/* Clean up and go home */
H5Dclose(hid);
H5Sclose(space);
*retData = array;
return size;
}

int h5dReadIntArray(hid_t file, char *path, int **retData)
/* Load up integer array from file into memory */
{
hid_t hid = h5dOpen(file, path);
hsize_t space = H5Dget_space(hid);
hsize_t dims[8];
int dimCount = H5Sget_simple_extent_dims(space, dims, NULL);
if (dimCount != 1)
    errAbort("Expecting %s to be one dimensional, but it has %d dimensions", path, dimCount);
int size = dims[0];
int *array;
AllocArray(array, size);
h5dReadAll(hid, H5T_NATIVE_INT, array);

/* Clean up and go home */
H5Dclose(hid);
H5Sclose(space);
*retData = array;
return size;
}

int h5dReadStringArray(hid_t file, char *path, char ***retData)
/* Load up string array from file into memory */
{
hid_t hid = h5dOpen(file, path);
hsize_t space = H5Dget_space(hid);
hsize_t dims[8];
int dimCount = H5Sget_simple_extent_dims(space, dims, NULL);
if (dimCount != 1)
    errAbort("Expecting %s to be one dimensional, but it has %d dimensions", path, dimCount);
int size = dims[0];
char **array;
AllocArray(array, size);
hid_t type = cVarString();
h5dReadAll(hid, type, array);

/* Clean up and go home */
H5Tclose(type);
H5Dclose(hid);
H5Sclose(space);
*retData = array;
return size;
}


void saveExprMatrix(struct hcaUnpack5 *context, char **rowLabels, char *fileName)
/* Save out expression matrix.  Just process it one line at a time so as
 * not to run out of memory */
{
/* Get column with sample names, aka indexCol */
struct metaCol *indexCol = context->indexCol;

hid_t file = context->file;

/* Figure out size of primary data - that is number of cells with data */
hid_t data = h5dOpen(file, "X/data");
hid_t dataSpace = H5Dget_space(data);
hsize_t     dataDims[4];
int dataDimCount = H5Sget_simple_extent_dims(dataSpace, dataDims, NULL);
if (dataDimCount != 1)
    internalErr();
long long dataSize = dataDims[0];
verbose(1, "Sparse matrix with %lld elements\n", dataSize);

/* Load up array that contains offsets of each column within column-major data */
int *indptr = NULL;
int indptrSize = h5dReadIntArray(file, "X/indptr", &indptr);
verbose(2, "indptr size is %d\n", indptrSize);
if (indptrSize - 1 != indexCol->size)
     errAbort("Disagreement between X/indptr size (%d) and obs/_index size (%d)",
	indptrSize - 1, indexCol->size);

hid_t indi = h5dOpen(file, "X/indices");
hsize_t indiSpace = H5Dget_space(indi);
hsize_t indiDims[4];
int indiDimCount = H5Sget_simple_extent_dims(indiSpace, indiDims, NULL);
if (indiDimCount != 1)
    internalErr();
int indiSize = indiDims[0];
verbose(2, "indi size is %d\n", indiSize);

hsize_t indiColSpace = H5Dget_space(indi);
hsize_t colOffset[1], colSize[1];
hsize_t dataColSpace = H5Dget_space(data);

int sizeAlloc = 0;
int *intiBuf = NULL;
float *fValBuf = NULL;

/* Scan through matrix a column at a time, reading it into matrix  */
struct sparseRowMatrix *matrix = sparseRowMatrixNew(context->cellCount, context->geneCount);
int colIx;
int colCount = context->cellCount;
for (colIx = 0; colIx <colCount; ++colIx)
    {
    int start = indptr[colIx];
    int end = indptr[colIx+1];
    int size = end - start;
    if (size > sizeAlloc)
        {
	freeMem(intiBuf);
	freeMem(fValBuf);
	intiBuf = needMem(size * sizeof(intiBuf[0]));
	fValBuf = needMem(size * sizeof(fValBuf[0]));
	sizeAlloc = size;
	}
    hsize_t dim1[1] = {size};
    hsize_t readMemSpace = H5Screate_simple(1, dim1, NULL);
    colOffset[0] = start;
    colSize[0] = size;
    H5Sselect_hyperslab(indiColSpace, H5S_SELECT_SET, colOffset, NULL, colSize, NULL);
    H5Sselect_hyperslab(dataColSpace, H5S_SELECT_SET, colOffset, NULL, colSize, NULL);
    h5dRead(indi, H5T_NATIVE_INT, readMemSpace, indiColSpace, H5P_DEFAULT, intiBuf);
    h5dRead(data, H5T_NATIVE_FLOAT, readMemSpace, dataColSpace, H5P_DEFAULT, fValBuf);

    int i;
    for (i=0; i<size; ++i)
        sparseRowMatrixAdd(matrix, colIx, intiBuf[i], fValBuf[i]);
    }

/* Open output and Write out header */
sparseRowMatrixSaveAsTsv(matrix, indexCol->val.asString, rowLabels, fileName);

/* Clean up and go home*/
sparseRowMatrixFree(&matrix);
freez(&intiBuf);
freez(&fValBuf);
}

void saveArrayOfStrings(char **array, int size, char *fileName)
/* Write out array of strings, pretty easy */
{
FILE *f = mustOpen(fileName, "w");
int i;
for (i=0; i<size; ++i)
    fprintf(f, "%s\n", array[i]);
carefulClose(&f);
}

void hcaUnpack5(char *input, char *output, boolean withMatrix)
/* hcaUnpack5 - Convert cellxgene hdf5 files to something cell browser and genome browser like 
 * better.. */
{
struct hcaUnpack5 *context;
AllocVar(context);
context->vocabHash = hashNew(0);
context->metaHash = hashNew(0);

hid_t file = context->file = h5fOpen(input, H5F_ACC_RDONLY);
hid_t cats = h5gOpen(file, "obs/__categories");
h5lIterate(cats, vocabCallback, context);
slReverse(&context->vocabList);

verbose(2, "And now to attach vocabs?\n");
struct vocab *vocab;
for (vocab = context->vocabList; vocab != NULL; vocab = vocab->next)
    {
    hid_t v = h5dOpen(cats, vocab->name);
    readVocab(vocab, v, context);
    verbose(2, "%s has %d items\n", vocab->name, vocab->valCount);
    int i;
    for (i=0; i<vocab->valCount; ++i)
        verbose(2, "  %s\n", vocab->stringVals[i]);
    }

verbose(2, "And now for looping through the real thingies\n");
hid_t obs = h5gOpen(file, "obs");
h5lIterate(obs, obsCallback, context);


struct metaCol *col;
int firstColSize = 0;
struct metaCol *indexCol = NULL;
for (col = context->metaList; col != NULL; col = col->next)
    {
    char *name = col->name;
    if (sameString(name, "_index"))
        indexCol = context->indexCol = col;
    if (differentString(name, "__categories"))
	{
	hid_t hid = h5dOpen(obs, col->name);
	int size = readCol(col, hid, context);
	if (firstColSize == 0)
	    firstColSize = size;
	else
	    {
	    if (size != firstColSize)
		errAbort("size mismatch %d vs %d", size, firstColSize);
	    }
	}
    }

/* Rip indexCol out of list */
if (indexCol == NULL)
    internalErr();

slRemoveEl(&context->metaList, indexCol);

/* Check all columns are same size */
int colSize = indexCol->size;
for (col = context->metaList; col != NULL; col = col->next)
    {
    if (col->size != colSize)
        errAbort("ERROR: %s has %d cells but %s has %d", 
	    col->name, col->size, indexCol->name, indexCol->size);
    }
context->cellCount = colSize;

/* Get list of gene names*/
char **geneNames;
int geneCount  = h5dReadStringArray(file, "var/_index", &geneNames);
context->geneCount = geneCount;

verbose(1, "Data on %d cells x %d genes\n", context->cellCount, context->geneCount);

/* Make output directory */
makeDirsOnPath(output);
char path[PATH_LEN];
safef(path, sizeof(path), "%s/meta.tsv", output);
saveMeta(context, path);

safef(path, sizeof(path), "%s/gene.lst", output);
saveArrayOfStrings(geneNames, geneCount, path);


if (withMatrix)
    {
    safef(path, sizeof(path), "%s/exprMatrix.tsv", output);
    saveExprMatrix(context, geneNames, path);
    }

}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
hcaUnpack5(argv[1], argv[2], !optionExists("noMatrix"));
return 0;
}
