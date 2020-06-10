
struct assoc
{
struct assoc *next;
struct hash *settings;
char *table;
char *idSql;
char *url;
char *shortLabel;
char *longLabel;
};

char *assocGetRefSeq(struct sqlConnection *conn, char *id);
char *getAssoc(struct sqlConnection *conn, char *table, char *field,  char *whereField, char *whereValue);
struct assoc *readAssoc(struct trackDb *tdb);
void doAssociations(struct sqlConnection *conn, struct trackDb *tdb, char *geneId);

