// NO CLUSTERING VERSION OF TFCLUSTER

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "tfbsConsLoc.h"

typedef unsigned char uchar;
extern void do_revcomp(uchar *s, int len);
extern void free_match_list (struct MATCH **matches);
extern void free_motif_list (struct MOTIF **motif_head);
extern void allocate_submat (struct MOTIF **new_motif);

void print_strand (const char *strand, int dir, int nrow)
{
    if (dir == FORWARD)
    {
	printf ("%c", strand[0]);
    } else {
        if (strand[0] == '+')
	    putchar ('-');
        else
	    putchar ('+');
    }
}

void output_matches (struct MATCH *matches, char *strand, char *id, int nrow, struct MOTIF *motif)
{
    int lowest_cutoff;
    struct MATCH *step;

    //'chr22',50427,50436,'GATA-1','+++',0.87,'$V_GATA-1_01'
    step = matches;
    while (step)
    {
    	lowest_cutoff = (int) ((floor ((step->lowest_score - motif->min) / ((float) (motif->max - motif->min)) * 100) / 100) * 1000);
	if (lowest_cutoff < 0)
	    lowest_cutoff = -1;        
	printf ("chr?	%d	%d	", step->pos, (step->pos + motif->len - 1));
	printf ("%s	%d	", motif->id, lowest_cutoff);
        print_strand (strand, step->dir, nrow);
     	putchar ('\n');
        step = step->next;
    }
//printf ("---NEXT---\n");
}   

int get_pos (int start, int col, char strand, int size, char *text, int motif_len, int dir)
{
    int num_gaps = 0,
	i;

    if (dir == FORWARD) {
    	for (i = 0; i < col; i++)
{
//printf ("IT %c\n", text[i]);
            if (text[i] == '-')
	    	num_gaps++;
}
 
    	return (start + col - num_gaps + 1);

    } else {
        for (i = size - 1; i > col + motif_len - 1; i--)
{
//printf ("IT %c\n", text[i]);
            if (text[i] == ' ')
                num_gaps++;
}

        return (start + (size - col) - num_gaps + 1 - motif_len);
    }
}

void add_to_matches (struct MATCH **matches, int col, 
			    int seq, int *score, int nrow, struct mafAli *ali, int motif_len, int dir,
			    int do_order)
{
    int i;
    struct MATCH *match_ptr;
    struct MATCH *newMatch, *temp;
    struct mafComp *comp = ali->components;

    newMatch = ckalloc (sizeof (struct MATCH));
    newMatch->lowest_score = INT_MAX;
    newMatch->pos = get_pos (comp->start, col, comp->strand, ali->textSize, comp->text, motif_len, dir);
    newMatch->col = col;
    newMatch->dir = dir;

    for (i = 0; comp; comp = comp->next, i++)
        if (score[i] < newMatch->lowest_score)
            newMatch->lowest_score = score[i];

    newMatch->next = NULL;

    /* slower, but prints in order */
    if (do_order) {
    	match_ptr = *matches;
    	if (match_ptr == NULL) {
//printf ("NULL!\n");
	    *matches = newMatch;
    	} else if (newMatch->pos < match_ptr->pos) {
	    newMatch->next = match_ptr;
	    *matches = newMatch;
    	} else {
//printf ("SECOND?\n");
            while ((match_ptr->next != NULL) && (match_ptr->next->pos < newMatch->pos))
{
//printf ("MOVING ON list: %d new: %d\n", match_ptr->pos, newMatch->pos);
             	match_ptr = match_ptr->next;
}
	    temp = match_ptr->next;
            match_ptr->next = newMatch;
	    newMatch->next = temp;
    	}

    /* faster, pays no regard to ordering */
    } else {
    	match_ptr = *matches;
    	if (match_ptr == NULL)
            *matches = newMatch;
    	else
    	{
            while (match_ptr->next != NULL)
            	match_ptr = match_ptr->next;
            match_ptr->next = newMatch;
    	}   
    }
}

/* Given the sequence char and motif we are currently on, return corresponding score. */
int get_score (int **submat, char seq_char, int col)
{
    if (seq_char == 'A') return (submat[rowA][col]);
    if (seq_char == 'C') return (submat[rowC][col]);
    if (seq_char == 'G') return (submat[rowG][col]);
    if (seq_char == 'T') return (submat[rowT][col]);
    return (INT_MIN);
}

int motif_hit (char *text, int col, struct MOTIF *motif, int *score)
{
    int i;

    *score = 0;
    for (i = 0; i < motif->len; i++)
        *score += get_score (motif->submat, text[col + i], i);
	
    if (*score >= motif->threshold)
	return 1;
    else 
	return 0;
}

void get_matches (struct MATCH **matches, int dir,
			 struct mafAli *ali, int nrow, struct MOTIF *motif, int do_order, int misses_allowed)
{
    int col,
	seq,
	misses_so_far = 0,
  	*score = NULL;
    struct mafComp *comp = NULL;

    score = (int *) ckalloc (sizeof (int) * nrow);

    for (col = 0; col < (ali->textSize - motif->len + 1); col++)
    {
	misses_so_far = 0;
    	for (comp = ali->components, seq = 0; comp; comp = comp->next, seq++)
	{
//printf ("MO %s seq %d char %c\n", motif->name, seq, comp->text[col]);
	    if (!motif_hit (comp->text, col, motif, &(score[seq])))
		misses_so_far++;
	}

    	if (misses_so_far <= misses_allowed) 
            add_to_matches (matches, col, seq, score, nrow, ali, motif->len, dir, do_order);
    }

    free (score);
}
 
/* Compute the minimum possible score that a certain motif can achieve. */
int get_min_score( int **submat, int length)
{
    int row, pos,               /* current row and position in matrix */
    	min,                    /* minimum value at one position */
        tot_min = 0;            /* total of all minimum values across all positions */
    
    for (pos = 0; pos < length; pos++)
    {   
        min = INT_MAX;
        for (row = rowA; row <= rowT; row++)
            if (submat[row][pos] < min)
                min = submat[row][pos];
        tot_min += min;
    }

    return (tot_min);
}

/* Compute the maximum possible score that a certain motif can achieve. */
int get_max_score( int **submat, int length )
{
    int row, pos,               /* current row and position in matrix */
    	max,                    /* maximum value at one position */
        tot_max = 0;            /* total of all maximum values across all positions */
    
    for (pos = 0; pos < length; pos++)
    {
        max = INT_MIN;
        for (row = rowA; row <= rowT; row++)
            if (submat[row][pos] > max)
                max = submat[row][pos];
        tot_max += max;
    }
    
    return (tot_max);
}   

void init_motif (struct MOTIF **motif, char *fileName, float cutoff,
			 char *id)
{
    char chr,
	 *cur_id = NULL,
	 *na = NULL,
	 *buf = NULL;
    int i, 
	pos,
	found_id = 0,
	fA, fC, fG, fT;
    FILE *fp = ckopen (fileName, "r");

    buf = (char *) ckalloc (BUFSIZE);
    cur_id = (char *) ckalloc (STRSIZE);
    na = (char *) ckalloc (STRSIZE);

    while (fgets( buf, BUFSIZE, fp ))
    {
  	if (sscanf (buf, "ID %s", cur_id) == 1) {
	    if (strcmp (cur_id, id) == 0)
	    {
		found_id = 1;
    	    	*motif = ckalloc (sizeof (struct MOTIF));
    	    	(*motif)->id = (char *) ckalloc ((strlen (id) + 1) * sizeof (char));
                strcpy ((*motif)->id, id);
	    } 

	} else if ((found_id) && sscanf (buf, "NA %s", na) == 1) {
            (*motif)->name = (char *) ckalloc ((strlen (na) + 1) * sizeof (char));
            strcpy ((*motif)->name, na);  

	} else if ((found_id) && strncmp (buf, "P0", 2) == 0) {
            allocate_submat (motif);
	    pos = 0;
            while ((fgets (buf, BUFSIZE, fp)) && (buf[0] != 'X'))
            {
                if ((sscanf (buf,"%d %d %d %d %d %c", &i, &fA, &fC, &fG, &fT, &chr)) !=6)
                    fatal ("Invalid transfac matrix file format.");
                (*motif)->submat[rowA][pos] = fA;
                (*motif)->submat[rowC][pos] = fC;
                (*motif)->submat[rowG][pos] = fG;
                (*motif)->submat[rowT][pos] = fT;
		pos++;
            }

	    (*motif)->min = get_min_score ((*motif)->submat, pos);
            (*motif)->max = get_max_score ((*motif)->submat, pos);

            /* equation taken from the MOTIF program (http://motif.genome.ad.jp) */
	    (*motif)->threshold = (cutoff) * (*motif)->max + (1 - cutoff) * (*motif)->min;

	    (*motif)->len = pos;
	    (*motif)->next = NULL;

	    break;
        } 
    }

    if (!found_id)
        fatalf ("*** ERROR: did not find id number '%s' in transfac file. ***\n\n", id);

    free (cur_id);
    free (na);
    free (buf);
}

void get_args (int argc, char **argv, int *output_mode, struct mafFile **file, char **id,
		      struct MOTIF **motif, int *do_order, int *nrow, float *cutoff, int *misses_allowed)
{
    int i;

    if (argc < 5 || argc > 12)
        fatalf (SYNTAX);

    if (strcmp (argv[1], "-verbose") == 0)
        *output_mode = VERBOSE;
    else if (strcmp (argv[1], "-gala") == 0)
        *output_mode = GALA;
    else if (strcmp (argv[1], "-hgb") == 0)
        *output_mode = HGB;
    else
        fatalf (SYNTAX);

    *file = mafOpen (argv[2]);
    strcpy (*id, argv[4]);

    i = 5;
    argc -= 5;
    while (argc)
    {
        if (strcmp (argv[i], "-order") == 0)
            *do_order = 1;
        else if (strcmp (argv[i], "-nseq") == 0) {
            i++;
            argc--;
            if (argc == 0)
                fatal (SYNTAX);
            *nrow = atoi (argv[i]);
            if (*nrow < 2)
                fatal ("Number of sequences must be at least 2.\n");
        } else if (strcmp (argv[i], "-cut") == 0) {
            i++;
            argc--;
            if (argc == 0)
                fatal (SYNTAX);
            *cutoff = atof (argv[i]);
            if (*cutoff < 0.0 || *cutoff > 1.0)
                fatal ("Cutoff value must be between 0 and 1.\n");
        } else if (strcmp (argv[i], "-nm") == 0) {
            i++;
            argc--;
            if (argc == 0)
                fatal (SYNTAX);
            *misses_allowed = atoi (argv[i]);
            if (*misses_allowed < 0)
                fatal ("Number of misses cannot be negative.\n");
        } else
            fatalf (SYNTAX);
        argc--;
        i++;
    }
    init_motif (motif, argv[3], *cutoff, *id);
}


int main (int argc, char **argv)
{
	int nrow = DEFAULT_NROW,
	which_seq = 0,
	do_order = 0,
	misses_allowed = 0,
	output_mode = -1;
    float cutoff = DEFAULT_CUTOFF;
    char *strand = NULL,
	 *id = NULL;
    struct mafFile *file = NULL;
    struct mafAli *ali = NULL;
    struct mafComp *comp = NULL;
    struct MOTIF *motif = NULL;
    struct MATCH *matches = NULL;

    id = ckalloc (STRSIZE);
    get_args (argc, argv, &output_mode, &file, &id, &motif, &do_order, &nrow, &cutoff, &misses_allowed);
    strand = ckalloc (sizeof (char) * (nrow + 1));

    while (NULL != (ali = mafNext (file)))
    {
        for (comp = ali->components, which_seq = 0; comp; comp = comp->next, which_seq++)
            strand[which_seq] = comp->strand;
        strand[which_seq] = '\0';

	/* skip blocks that don't have all seqs in them */
 	if (which_seq != nrow)
	{
            mafAliFree (&ali);
	    continue;
	}

	/* forward strand */
	get_matches (&matches, FORWARD, ali, nrow, motif, do_order, misses_allowed);

	/* reverse strand */
        for (comp = ali->components; comp; comp = comp->next)
            do_revcomp((uchar *)comp->text, ali->textSize );

        get_matches (&matches, REVERSE, ali, nrow, motif, do_order, misses_allowed);

	/* output matches */
        if (matches)
            output_matches (matches, strand, id, nrow, motif);
        free_match_list (&matches);

        mafAliFree (&ali);
    }

    mafFileFree (&file);
    free (strand);
    free_motif_list (&motif);
    free (id);

    return 0;
}
