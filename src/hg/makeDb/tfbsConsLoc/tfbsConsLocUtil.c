/* Copyright (C) 2004 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "tfbsConsLocUtil.h"

void free_match_list (struct MATCH **matches)
{   
    struct MATCH *step, *next;

    step = *matches;
    while (step)
    {
        next = step->next;
        free (step);
        step = next;
    }
    *matches = NULL;
}


/* Clean up 2-d int array. */
void free_int_arr (int **arr, int size)
{
    int i;              /* array index */
   
    if (!arr)
        return;
   
    for (i = 0; i < size; i++)
        ckfree (arr[i]);
    ckfree (arr);
}


/* Clean up 2-d char array. */
void free_char_arr (char **arr, int size)
{
    int i;              /* array index */

    if (!arr)
        return;
   
    for (i = 0; i < size; i++)
        ckfree (arr[i]);
    ckfree (arr);
}


/* Clean up motif list. */
void free_motif_list (struct MOTIF **motif_head)
{
    int i;
    struct MOTIF *motif_step,             /* pointer to move through motif list */
                 *new_motif_step;         /* next motif to be processed */

    if (!motif_head)
        return;
   
    for (motif_step = *motif_head; motif_step; motif_step = new_motif_step)
    {
        new_motif_step = motif_step->next;
        ckfree (motif_step->name);
        ckfree (motif_step->id);
        if (motif_step->submat)
            for (i = 0; i < 4; ++i)
                ckfree (motif_step->submat[i]);
        ckfree (motif_step->submat);
        ckfree (motif_step);
    }  
    *motif_head = NULL;
}  

   
/* allocate enough memory for the substitution matrix */
void allocate_submat (struct MOTIF **new_motif)
{
    int i;      /* array index */

    (*new_motif)->submat = (int **) ckalloc (sizeof (int *) * 4);
    for (i = 0; i < 4; i++)
        (*new_motif)->submat[i] = ckalloc (sizeof (int) * MAXSUBMATLEN);
}        


struct MOTIF* copy_motif (struct MOTIF *new_motif)
{
    int i, j;
    struct MOTIF *copy;

    copy = ckalloc (sizeof (struct MOTIF));
    copy->name = (char *) ckalloc (sizeof (new_motif->name));
    copy->id = (char *) ckalloc (sizeof (new_motif->id));
    allocate_submat (&copy);

    copy->len = new_motif->len;
    strcpy (copy->name, new_motif->name);
    strcpy (copy->id, new_motif->id);
    copy->threshold = new_motif->threshold;
    copy->next = NULL;

    for (i = rowA; i <= rowT; i++)
        for (j = 0; j < MAXSUBMATLEN; j++)
            copy->submat[i][j] = new_motif->submat[i][j];

    return (copy);
}


/* add the given MOTIF struct to the MOTIF linked list */
void push_motif (struct MOTIF **motifs, struct MOTIF *new_motif)
{
    struct MOTIF *step;         /* pointer to step through linked list */

    step = *motifs;
    if (step == NULL)
        *motifs = new_motif;
    else
    {
        while (step->next != NULL)
            step = step->next;
        step->next = new_motif;
    }
}


void print5_int_arr (int *arr, int nrow)
{   
    int m;
    
    for (m = 0; m < nrow; m++)
        printf ("%5d", arr[m]);
}   


void print10_int_arr (int *arr, int nrow)
{   
    int m;
    
    for (m = 0; m < nrow; m++)
        printf ("%10d", arr[m]);
}   

   
void print_col_arr (int **col_arr, int num_motifs, int which_match)
{   
    int m;
    
    for (m = 0; m < num_motifs; m++)
        printf ("%6d", col_arr[which_match][m]);
    printf ("\n");
}   


void print_matches (struct MATCH *matches)
{   
    struct MATCH *step;
       
    step = matches;
    while (step)
    {
        printf ("%d (%d)  ", step->col, step->pos);
        step = step->next;
    }
    printf ("\n\n");
}   
