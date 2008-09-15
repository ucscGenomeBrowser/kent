
void palOptions(struct cart *cart, 
	struct sqlConnection *conn, void (*addButtons)(), 
	char *extraVar);
/* output the options dialog (select MAF file, output options */

int palOutPredsInHash(struct sqlConnection *conn, struct cart *cart,
    struct hash *hash, char *table );
/* output the alignments who's names match strings in hash */
