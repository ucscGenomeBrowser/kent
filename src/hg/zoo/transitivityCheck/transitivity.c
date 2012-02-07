/* This file takes two sequences in unpacked psl format and finds the implied
   transitivity.  The implied transitivity is outputted in a similar format to
   the unpacked psl files.  Note this only works for psl files in which the
   blocks don't contain other blocks (i.e. a block from 1 to 10 and a block
   from 2 to 8).  This is possible but requires a greater time and 
   computational complexity.

   Copyright Trevor Bruen 2002 */  

#include <stdlib.h>
#include <stdio.h>
#include "misc.h"


#define MARKER -77

struct Pair {
    int begin;
    int end;
};
typedef struct Pair pair;

struct Node 
{
    pair *from;
    pair *to;
    struct Node *next;
    struct Node *previous;
};


typedef struct Node node;


pair *new_pair(int left, int right)
{
pair *p;

p = safe_alloc(sizeof(pair));
p->begin = left;
p->end = right;

return p;
}

/* Initialize Roots nodes */
void initialize(node **root)
{
(*root) = safe_alloc(sizeof(node));
(*root) -> previous = NULL;
(*root) -> next = NULL;
(*root) -> from = new_pair(MARKER,MARKER);
(*root) -> to = new_pair(MARKER,MARKER);
}

void print_usage(){
fprintf(stderr,
"transitivity - Create an inferred map from two unpacked psl alignments.  
               These two alignments should be (seq A aligned to B) and
               (seq B aligned to C).  The output then displays the implied
               alignment of seq A to C.  It is output in the unpacked psl
               format.
usage:
  transitivity alignmentAtoB alignmentBtoC
");
exit(1);
}

node *create_node(pair *left, pair *right)
{
node *new_node;

new_node = safe_alloc(sizeof(node));

new_node -> from = left;
new_node -> to = right;
new_node -> previous = NULL;
new_node -> next = NULL;
return new_node;
}

void print_list(node *root){
node *current_node = root->next;

while(current_node != NULL)
    {
    fprintf(stdout,"%d %d %d %d\n",current_node->from->begin,current_node->from->end,current_node->to->begin,current_node->to->end);
    current_node = current_node->next;
    }
}
/* Returns the node "before" or at the position to add stuff */
node *find_node(node *root, pair *left)
{
node *current_node = root->next, *previous_node = root;

while(current_node  != NULL)
    {
    if((current_node->from == NULL) || (current_node->from)->begin > left->begin)
	return previous_node;
    previous_node = current_node;
    current_node = current_node->next;
    }
return previous_node;
}

node *add(node *root, pair *left, pair *right)
{
node *locator = find_node(root, left), *new_node;
new_node = create_node(left,right);


/* Check if before or after */
if(locator->from->begin > left->begin)
    {
    new_node->next = locator;
    new_node->previous = locator->previous;
    new_node->previous->next = new_node;
    locator->previous = new_node;
    return new_node;
    }
else
    {
    new_node->next = locator->next;
    new_node->previous = locator;
    locator->next = new_node;
    return locator;
    }

return NULL;
}

node *infer_alignment(node *root_left,node *root_right)
{
node *left=root_left->next, *right=root_right->next, *new_list;
int x1,x2,y1,y2,diff;

initialize(&new_list);

while(left!= NULL && right!=NULL)
    {
    if(left->to->end < right->from->begin)
	left=left->next;
    else if(right->from->end < left->to->begin)
	right=right->next;
    else
	{
	/* We're creating an inferred node */
	
	/* For the beginning */
	if((diff = left->to->begin - right->from->begin) > 0)
	    {
	    x1=left->from->begin;
	    /* Shift down the beginning */
	    y1=right->to->begin + diff;
	    }
	else
	    {
	    /* Otherwise shift down left side */
	    x1=left->from->begin - diff;
	    y1=right->to->begin;
	    }
	
	/* For the end */
	if((diff = left->to->end - right->from->end) > 0)
	    {
	    /* i.e. The right one ends first - so advance it */
	    x2=left->from->end - diff;
	    y2=right->to->end;
	    
	    right=right->next;
	    }
	else
	    {
	    x2=left->from->end;
	    y2=right->to->end + diff;
	    left=left->next;
	    }
	add(new_list, new_pair(x1,x2),new_pair(y1,y2));	
	}
    }


return new_list;
}


int main (int argc, char **argv)
{
node * left_list = NULL,  *right_list, *current=NULL, *inferred_list;
FILE *in_left, *in_right;
int x1,x2,y1,y2;

/* Check argc */
if(argc != 3)
    print_usage();

/* Initialize lists */
initialize(&left_list);
initialize(&right_list);

/* Open files */
in_left = safe_open(argv[1],"r");
in_right = safe_open(argv[2],"r");

/* Read in pairs */
while(!feof(in_left))
    {
    fscanf(in_left,"%d%d%d%d\n",&x1,&x2,&y1,&y2);
    add(left_list, new_pair(x1,x2),new_pair(y1,y2));
    }

/* Print out list */
print_list(left_list);

/* Read in pairs */
while(!feof(in_right))
    {
    fscanf(in_right,"%d%d%d%d\n",&x1,&x2,&y1,&y2);
    add(right_list, new_pair(x1,x2),new_pair(y1,y2));
    }

/* Print out list */
print_list(right_list);

/* Infer alignment */
inferred_list = infer_alignment(left_list,right_list);

fprintf(stdout,"\nInferred list\n");
print_list(inferred_list);
return 0;
}











