/* object to track refseqs that are to be retrieved */
#ifndef refSeqVerInfo_h
#define refSeqVerInfo_h
struct sqlConnection;

struct refSeqVerInfo
/* Version information for refseqs being retrieved.  Constructed from either the -accList
 * file or the all native RefSeqs in the database.
 */
{
    struct refSeqVerInfo *next;
    char *acc;            // accession without version
    int ver;              // current version in database, or 0 if not in database
    int requestVer;       // requested version from accList, or 0 if version not specified.
};

struct hash *refSeqVerInfoFromDb(struct sqlConnection *conn, boolean getNM, boolean getNR, struct refSeqVerInfo **refSeqVerInfoList);
/* load refSeqVerInfo table for all native refseqs in the database */

struct hash *refSeqVerInfoFromFile(struct sqlConnection *conn, char *accList, struct refSeqVerInfo **refSeqVerInfoList);
/* load refSeqVerInfo table for all native refseqs specified in a file, then validate it against
 * the database. */

int refSeqVerInfoGetVersion(char *acc, struct sqlConnection *conn);
/* get the version from the database, or zero if accession is not found */

#endif
