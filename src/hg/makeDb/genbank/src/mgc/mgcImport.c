/* mgcImport - convert tables dumps from NCBI */

#include "common.h"
#include "options.h"
#include "mgcClone.h"
#include "mgcFullength.h"
#include "mgcLibrary.h"
#include "mgcStage1.h"
#include "mgcStatusTbl.h"
#include "linefile.h"
#include "gbFileOps.h"

static char const rcsid[] = "$Id: mgcImport.c,v 1.9 2006/09/14 16:42:42 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mgcImport - import MGC table dumps from NCBI\n"
  "\n"
  "Usage:\n"
  "   mgcImport [options] tableDir mgcStatusTab mgcFullStatusTab\n"
  "     tableDir - directory containing the table files:\n"
  "                clone.gz, fullength.gz, library.gz, stage1.gz\n"
  "     mgcStatusTab - tab file with status for all MGC clones. Compressed\n"
  "                    if ends in .gz.\n"
  "     mgcFullStatusTab - tab file with status for full-CDS MGC clones.\n"
  "                        Compressed if ends in .gz.\n"
  "Options:\n"
  "\n"
  );
}

struct mgcStatusType *fullengthToStatus(struct mgcFullength *fullength)
/* convert the value in the full_length column of the fullength table to a
 * mgcStatusType */
{
switch (fullength->full_length)
    {
    case 0:
        if (strlen(fullength->seq_back) == 0)
            return &MGC_NOT_BACK;
        else
            return &MGC_NO_DECISION;
    case 1:
        return &MGC_INCOMPLETE;
    case 2:
        return &MGC_FULL_LENGTH;
    case 3:
        return &MGC_CHIMERIC;
    case 4:
        return &MGC_FRAME_SHIFTED;
    case 5:
        return &MGC_CONTAMINATED;
    case 6:
        return &MGC_RETAINED_INTRON;
    case 8:
        return &MGC_FULL_LENGTH_SHORT;
    case 9:
        return &MGC_FULL_LENGTH_SYNTHETIC;
    case 100 :
        return &MGC_MIXED_WELLS;
    case 101 :
        return &MGC_NO_GROWTH;
    case 102 :
        return &MGC_NO_INSERT;
    case 103 :
        return &MGC_NO_5_EST_MATCH;
    case 104 :
        return &MGC_MICRODELETION;
    case 105 :
        return &MGC_LIBRARY_ARTIFACTS;
    case 106 :
        return &MGC_NO_POLYA_TAIL;
    case 107 :
        return &MGC_CANT_SEQUENCE;
    case 108 :
        return &MGC_INCONSISTENT_WITH_GENE;
    case 110 :
        return &MGC_PLATE_CONTAMINATED;
    default  :
        errAbort("unknown value for column full_length: \"%d\" in fullength "
                 "table for id_image %d",
                 fullength->full_length, fullength->id_parent); 
    }
return NULL;
}

boolean haveErrorStatus(struct mgcStatus *mgcStatus)
/* check if any of the entries in a list have an error status */
{
for (; mgcStatus != NULL; mgcStatus = mgcStatus->next)
    {
    if (mgcStatus->status->state == MGC_STATE_PROBLEM)
        return TRUE;
    }
return FALSE;
}

boolean haveStatus(struct mgcStatus *mgcStatus,
                   struct mgcStatusType* status)
/* check if any of the entries in a list have the specified status */
{
for (; mgcStatus != NULL; mgcStatus = mgcStatus->next)
    {
    if (mgcStatus->status == status)
        return TRUE;
    }
return FALSE;
}

void processFullengthRow(struct mgcStatusTbl *mgcStatusTbl, char **row)
/* Process a row parsed from the fullength table */
{
struct mgcFullength fullength;
struct mgcStatus *mgcStatus;
struct mgcStatusType *status;

mgcFullengthStaticLoad(row, &fullength);
status = fullengthToStatus(&fullength);

/* Get the list of entries for this image id */
mgcStatus = mgcStatusTblFind(mgcStatusTbl, fullength.id_parent);


/* We save all rows with accessions.  Things are a little more complex for
 * entries without accessions.  Here we save only one that is marked with an
 * error status, or once of each of the other non-error status (although there
 * is probably only one). */
if ((mgcStatus == NULL) || (strlen(fullength.gb_acc) > 0)
    || ((status->state == MGC_STATE_PROBLEM) && !haveErrorStatus(mgcStatus))
    || (!haveStatus(mgcStatus, status)))
    mgcStatusTblAdd(mgcStatusTbl, fullength.id_parent, status,
                    fullength.gb_acc,
                    mgcOrganismNameToCode(fullength.organism,
                                          "fullength table"),
                    fullength.genesymbol);
}

void processFullength(struct mgcStatusTbl *mgcStatusTbl, char *tabFile)
/* Process the fullength table dump. Call this first. */
{
struct lineFile *lf = gzLineFileOpen(tabFile);
char *row[MGCFULLENGTH_NUM_COLS];

while (lineFileNextRowTab(lf, row, MGCFULLENGTH_NUM_COLS))
    processFullengthRow(mgcStatusTbl, row);

gzLineFileClose(&lf);
}

void processStage1Row(struct mgcStatusTbl *mgcStatusTbl, char **row)
/* Process a row from a stage1 table dump. */
{
struct mgcStage1 stage1;
mgcStage1StaticLoad(row, &stage1);

/*
 * Determine if it's a candidate or pick
 *
 * (stage1.suppress == 1) && (stage1.live == 2)
 *    - if a valid entry
 * (stage1.currpick == 2) && (stage1.picked == 1)
 *    - candidate picks, are waiting on a full plate
 * (stage1.currpick == 1) && (stage1.picked == 2)
 *    - picked,sent to LLNL for rearraying 
 */
if ((stage1.suppress == 1) && (stage1.live == 2)
    && ((stage1.currpick == 2) || (stage1.picked == 2))
    && (mgcStatusTblFind(mgcStatusTbl, stage1.id_clone) == NULL))
    {
    struct mgcStatusType* status;
    if (stage1.currpick == 2)
        status = &MGC_CANDIDATE;
    else
        status = &MGC_PICKED;
    mgcStatusTblAdd(mgcStatusTbl, stage1.id_clone, status, NULL,
                    mgcOrganismNameToCode(stage1.organism,
                                          "stage1 table"),
                    NULL);
    }
}

void processStage1(struct mgcStatusTbl *mgcStatusTbl, char *tabFile)
/* Process the stage1 table dump. */
{
struct lineFile *lf = gzLineFileOpen(tabFile);
char *row[MGCSTAGE1_NUM_COLS];

/* we always add picked, even if we have other status */
while (lineFileNextRowTab(lf, row, MGCSTAGE1_NUM_COLS))
    processStage1Row(mgcStatusTbl, row);
gzLineFileClose(&lf);
}

void processCloneRow(struct mgcStatusTbl *mgcStatusTbl, 
                     struct mgcLibraryTbl *mgcLibraryTbl,
                     char **row)
/* Process a row from the clone table dump. */
{
struct mgcClone clone;
mgcCloneStaticLoad(row, &clone);
if (mgcStatusTblFind(mgcStatusTbl, clone.id_clone) == NULL)
    {
    struct mgcLibrary *library = mgcLibraryTblFind(mgcLibraryTbl,
                                                   clone.id_lib);
    if (library == NULL)
        errAbort("no library %d referenced in clone table, clone id %d",
                 clone.id_lib, clone.id_clone);
    mgcStatusTblAdd(mgcStatusTbl, clone.id_clone,
                    &MGC_UNPICKED, NULL,
                    mgcOrganismNameToCode(library->organism,
                                          "library table"),
                    NULL);
    }
}

void processClone(struct mgcStatusTbl *mgcStatusTbl,
                  struct mgcLibraryTbl *mgcLibraryTbl, char *tabFile)
/* Process the clone table dump. */
{
struct lineFile *lf = gzLineFileOpen(tabFile);
char *row[MGCCLONE_NUM_COLS];

/* add ones in clone table if not already there */
while (lineFileNextRowTab(lf, row, MGCCLONE_NUM_COLS))
    processCloneRow(mgcStatusTbl, mgcLibraryTbl, row);
gzLineFileClose(&lf);
}

void mgcImport(char *libraryTab, char *fullengthTab, char *stage1Tab,
               char *cloneTab, char *mgcStatusTab, char *mgcFullStatusTab)
/* Convert MGC table dumps into a mgcStatus table tab file. */
{
FILE *outFh;
struct mgcStatusTbl *mgcStatusTbl = mgcStatusTblNew(mgcStatusImageIdHash);
struct mgcLibraryTbl *mgcLibraryTbl = mgcLibraryTblLoad(libraryTab);

/* must parse in this order */
processFullength(mgcStatusTbl, fullengthTab);
processStage1(mgcStatusTbl, stage1Tab);
processClone(mgcStatusTbl, mgcLibraryTbl, cloneTab);

/* Output table with all clone status */
outFh = gzMustOpen(mgcStatusTab, "w");
mgcStatusTblTabOut(mgcStatusTbl, outFh);
gzClose(&outFh);

/* Output table with only full-length mRNA status */
outFh = gzMustOpen(mgcFullStatusTab, "w");
mgcStatusTblFullTabOut(mgcStatusTbl, outFh);
gzClose(&outFh);

mgcLibraryTblFree(&mgcLibraryTbl);
mgcStatusTblFree(&mgcStatusTbl);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *tableDir;
char libraryTab[PATH_LEN];
char fullengthTab[PATH_LEN];
char stage1Tab[PATH_LEN];
char cloneTab[PATH_LEN];
char *mgcStatusTab, *mgcFullStatusTab;

setlinebuf(stdout);
setlinebuf(stderr);

optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage();
tableDir = argv[1];
mgcStatusTab = argv[2];
mgcFullStatusTab = argv[3];

safef(libraryTab, sizeof(libraryTab), "%s/library.gz", tableDir);
safef(fullengthTab, sizeof(fullengthTab), "%s/fullength.gz", tableDir);
safef(stage1Tab, sizeof(stage1Tab), "%s/stage1.gz", tableDir);
safef(cloneTab, sizeof(cloneTab), "%s/clone.gz", tableDir);

mgcImport(libraryTab, fullengthTab, stage1Tab, cloneTab,
          mgcStatusTab, mgcFullStatusTab);

return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

