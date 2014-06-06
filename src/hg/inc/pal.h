/* Copyright (C) 2008 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */


void palOptions(struct cart *cart, 
	struct sqlConnection *conn, void (*addButtons)(), 
	char *extraVar);
/* output the options dialog (select MAF file, output options */

int palOutPredsInBeds(struct sqlConnection *conn, struct cart *cart,
    struct bed *beds, char *table);
/* output the alignments whose names and coords match a bed*/

int palOutPredList(struct sqlConnection *conn, struct cart *cart,
    struct genePred *list);
/* output a list of genePreds in pal format */
