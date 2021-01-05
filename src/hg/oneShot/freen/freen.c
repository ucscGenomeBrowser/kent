/* freen - My Pet Freen.  A pet freen is actually more dangerous than a wild one. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "portable.h"
#include "obscure.h"
#include "localmem.h"
#include "csv.h"
#include "tokenizer.h"
#include "strex.h"
#include "hmac.h"
#include "hdf5.h"

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}

#define FULLDATASET         "obs/__categories/cell_type"
#define DIM0            4

hid_t  h5dOpen(hid_t parent, char *name)
/* Open up a subpart or die trying */
{
hid_t ret = H5Dopen(parent, name, H5P_DEFAULT);
if (ret < 0)
   errAbort("Couldn't find %s in parent", name);
return ret;
}

hid_t h5fOpen(char *fileName,  int accessType)
{
hid_t ret = H5Fopen(fileName, accessType, H5P_DEFAULT);
if (ret < 0)
   errAbort("Couldn't open %s", fileName);
return ret;
}

void freen(char *fileName)
/* Test something */
{

/*
 * Open file, dataset, and attribute.
 */
hid_t file = h5fOpen (fileName, H5F_ACC_RDONLY);
hid_t dset = h5dOpen (file, "obs/__categories");

/*
 * Get the datatype.
 */
hid_t filetype = H5Dget_type (dset);
uglyf("filetype = %d\n", filetype);

/*
 * Get dataspace and allocate memory for read buffer.
 */
hid_t space = H5Dget_space (dset);
hsize_t     dims[1] = {DIM0};
int ndims = H5Sget_simple_extent_dims (space, dims, NULL);
uglyf("Got %d ndims\n", ndims);
char **rdata = (char **) malloc (dims[0] * sizeof (char *));

/*
 * Create the memory datatype.
 */
hid_t memtype = H5Tcopy (H5T_C_S1);
herr_t status = H5Tset_size (memtype, H5T_VARIABLE);
status = H5Tset_cset(memtype, H5T_CSET_UTF8) ;

/*
 * Read the data.
 */
status = H5Dread (dset, memtype, H5S_ALL, H5S_ALL, H5P_DEFAULT, rdata);
if (status < 0)
    errAbort("status %d, rats", status);

/*
 * Output the data to the screen.
 */
int i;
for (i=0; i<dims[0]; i++)
    printf ("%s[%d]: %s\n", "cell_type", i, rdata[i]);


/* Close the dataset. */
/*
 * Close and release resources.  Note that H5Dvlen_reclaim works
 * for variable-length strings as well as variable-length arrays.
 * Also note that we must still free the array of pointers stored
 * in rdata, as H5Tvlen_reclaim only frees the data these point to.
 */
status = H5Dvlen_reclaim (memtype, space, H5P_DEFAULT, rdata);
free (rdata);
status = H5Dclose (dset);
status = H5Sclose (space);
status = H5Tclose (filetype);
status = H5Tclose (memtype);
status = H5Fclose (file);
if (status < 0)
   warn("status %d", status);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}

