/* Read in a datamatrix and make the files for loading into databse. */
#include "common.h"
#include "dMatrix.h"

void usage()
/* introduce ourselves. */
{
errAbort("dataMatrixToSql - Read in a data matrix and make an sql\n"
	 "statement for creating table and data file suitable for loading\n"
	 "directly into database.\n"
	 "usage:\n"
	 "   dataMatrixToSql inputFile tableName\n"
	 "\n"
	 "Data can then be loaded using resulting files and mysql tools:\n"
	 "mysql -A < tableName.sql\n"
	 "echo \"load data local infile 'tableName.dat' into table tableName;\" | mysql\n");
}

void dataMatrixToSql(char *fileIn, char *tableName)
/* Read in table and write out data and sql files. */
{
struct dMatrix *dM = NULL;
char sqlFile[strlen(tableName)+5];
char dataFile[strlen(tableName)+5];
warn("Reading matrix from file %s", fileIn);
dM = dMatrixLoad(fileIn);
warn("Creating sql and data files");
safef(sqlFile, strlen(tableName)+5, "%s.sql", tableName);
safef(dataFile, strlen(tableName)+5, "%s.dat", tableName);
dMatrixWriteForSql(dM, tableName, dataFile, sqlFile);
warn("Done.");
}

int main(int argc, char *argv[]) 
/* Everybody's favorite function. */
{
if(argc != 3)
    usage();
dataMatrixToSql(argv[1], argv[2]);
return 0;
}
