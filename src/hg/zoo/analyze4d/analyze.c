/* Analyze 4d - read zoo raw alignment, read 4d sites and do analysis of changes
   at 4d sites. The zoo alignments are read in using Webb's routine from read_malign.h 
   Trevor Bruen 2002 */

#include "util.h"
#include "ctype.h"
#include "string.h"
#include "read_malign.h"


#define SAME_STRING(x,y) (!strcasecmp(x,y))
#define ALPHA_SIZE 4

/* Print the totals for each type of base */
void print_totals(int, int, int***, char *);

/* Translate a base to an integer */
int translate(char );

/* Print the program usage */
void print_usage();

int main( int argc, char **argv)
{
/* Index default to -1 as flage for special human consideration */
int i,j,k,nrow, ncol, *sites, **site_members,nsites, nspecies,*table,index=-1,total_columns=0,column_differs=0,column_4d_differs=0,*spec_breakdown,same;
int **base_count,**bases_in_4d;
char **seq_name, **site_species, **species,*token,*spec_name,ref;

/* Needed for Webb's library functions */
argv0 = "analyze";
if ((argc != 3) && (argc != 4)  )
    print_usage();


/* Determine which species to use as reference */
if(argc == 4)
    {
    token = strtok(argv[3], "=");
    spec_name = strtok(NULL, " \n");
    }
else
    spec_name="human";

/* Read the alignment */
site_species = read_malign(argv[1], &nrow, &ncol, &seq_name);

/* Now read the 4d sites... */
site_members = read_sites(argv[2], &nspecies, &nsites, &species, &sites);

/* Make sure nspecies and nrow correspond -subtract one because of implicit human in 4d*/
if(nspecies != (nrow-1))
    fatal("Species in alignment must correspond to number of species in 4d sites");

/* Build conversion table */
table=ckalloc(nspecies * sizeof(int));

for(i=0;i<nspecies;i++)
    for(j=0;j<nrow;j++)
	if(SAME_STRING(species[i],seq_name[j]))
	    table[i]=j;


/* Build species breakdown table */
spec_breakdown=ckalloc(nspecies * sizeof(int));
	  
for(i=0;i<nspecies;i++)
    spec_breakdown[i]=0;

/* Find species */	   
if(!SAME_STRING(spec_name,"human")){
    for(i=0;i<nspecies;i++)
	if(SAME_STRING(species[i],spec_name))
	    index=i;

    if(index==-1)
	fatal("Species not found");

}


/* Build count tables */
base_count=ckalloc(ALPHA_SIZE * sizeof(int *));
for(i=0;i<ALPHA_SIZE;i++)
    base_count[i]=ckalloc((ALPHA_SIZE+1) * sizeof(int));

bases_in_4d=ckalloc(ALPHA_SIZE * sizeof(int *));
for(i=0;i<ALPHA_SIZE;i++)
    bases_in_4d[i]=ckalloc((ALPHA_SIZE+1) * sizeof(int)); 

/* Initialize base count arrays */
for(i=0;i<ALPHA_SIZE;i++)
    for(j=0;j<ALPHA_SIZE;j++)
	base_count[i][j]=(bases_in_4d[i][j]=0);

/* For each 4d site */
for(i=0;i<nsites;i++)
    {
    /* If the species is present... Special exception for human*/
    if(index==-1 || site_members[index][i])
	{
	/* Generate total bases */	
	
	total_columns++;
	same=1;
	
	if(index!=-1)
	    ref=toupper(site_species[table[index]][sites[i]]);
	else
	    ref=toupper(site_species[0][sites[i]]);
	
	/* Check for homogeneity */
	    for(j=0;j<nspecies;j++)
		{
		if(j==index)
		    continue;
		if(toupper(site_species[table[j]][sites[i]])!=ref)
		    same=0;
		}
	    if(!same)
		{
		column_differs++;
		/* Find mix of bases */
		for(j=0;j<nspecies;j++)
		    {
		    if(j==index)
			continue;
		    base_count[translate(ref)][translate(site_species[table[j]][sites[i]])]++;
		    }
		
		}
	    /* Check for homogeneity within 4d species */
	    same=1;
	    
	    for(j=0;j<nspecies;j++)
		{
		if(j==index)
		    continue;
		/* If species is present at site */
		if(site_members[j][i])
		    {
		    spec_breakdown[j]++;
		    if(toupper(site_species[table[j]][sites[i]]) != ref)
			{
			same=0;
			}
		    }
		}
	    if(!same)
		{
		column_4d_differs++;
		/* Find mix of bases */
		for(j=0;j<nspecies;j++)
		    {
		    if(j==index)
			continue;
		    if(site_members[j][i])
			bases_in_4d[translate(ref)][translate(site_species[table[j]][sites[i]])]++;
		    }
		
		}
	}
    }


printf("\n\n");
printf("Results of Bases Substitution at 4d sites.\n");
printf("------------------------------------------\n\n");

printf("Considering all species (%s as Reference)\n",spec_name);
printf("--------------------------------------------\n\n");
print_totals(total_columns, column_differs, &base_count, "Human");

printf("\nConsidering species which share the site (%s as Reference)\n",spec_name);
printf("-------------------------------------------------------------\n\n");
print_totals(total_columns, column_4d_differs, &bases_in_4d, "Human");
printf("\n");

/* Result of species breakdown */
for(i=0;i<nspecies;i++)
    {
    if(index==i)
	continue;
    printf("Species %-8s occured %5d (%2.2f %)\n",species[i],spec_breakdown[i],(float)(spec_breakdown[i] * 100)/total_columns);
    }
return 0;
}

/* Print the totals for each type of base */
void print_totals(int total_columns, int column_differs, int ***base_count_a, char *species){

int A,C,T,G,total;
int **base_count=(*base_count_a);

/* Very inefficient */
A=base_count[0][0]+base_count[0][1]+base_count[0][2]+base_count[0][3]+base_count[0][4];
C=base_count[1][0]+base_count[1][1]+base_count[1][2]+base_count[1][3]+base_count[1][4];
T=base_count[2][0]+base_count[2][1]+base_count[2][2]+base_count[2][3]+base_count[2][4];
G=base_count[3][0]+base_count[3][1]+base_count[3][2]+base_count[3][3]+base_count[3][4];
total= A+C+G+T;

printf("# Sites Considered: %5d\n\n",total_columns);
printf("# Homogenous Sites: %5d (%2.2f)%\t# Non Homogeneous Sites: %5d (%2.2f)%\n\n",total_columns - column_differs,(((float)((total_columns - column_differs) * 100 ))/((float)total_columns)), column_differs,(((float)(column_differs * 100 ))/((float)total_columns)));  

printf("A occurs %5d times (%2.2f %) and is replaced by: A %d (%2.2f %), C %d (%2.2f %), T %d (%2.2f %), G %d (%2.2f %), - %d (%2.2f %)\n\n",A,(float)(A * 100)/(float)(total),base_count[0][0],(float)(base_count[0][0] * 100)/(float)(A),base_count[0][1],(float)(base_count[0][1] * 100)/(float)(A),base_count[0][2],(float)(base_count[0][2] * 100)/(float)(A),base_count[0][3],(float)(base_count[0][3] * 100)/(float)(A),base_count[0][4],(float)(base_count[0][4] * 100)/(float)(A));            

printf("C occurs %5d times (%2.2f %) and is replaced by: A %d (%2.2f %), C %d (%2.2f %), T %d (%2.2f %), G %d (%2.2f %), - %d (%2.2f %)\n\n",C,(float)(C * 100)/(float)(total),base_count[1][0],(float)(base_count[1][0] * 100)/(float)(C),base_count[1][1],(float)(base_count[1][1] * 100)/(float)(C),base_count[1][2],(float)(base_count[1][2] * 100)/(float)(C),base_count[1][3],(float)(base_count[1][3] * 100)/(float)(C),base_count[1][4],(float)(base_count[1][4] * 100)/(float)(C));

printf("T occurs %5d times (%2.2f %) and is replaced by: A %d (%2.2f %), C %d (%2.2f %), T %d (%2.2f %), G %d (%2.2f %), - %d (%2.2f %)\n\n",T,(float)(T * 100)/(float)(total),base_count[2][0],(float)(base_count[2][0] * 100)/(float)(T),base_count[2][1],(float)(base_count[2][1] * 100)/(float)(T),base_count[2][2],(float)(base_count[2][2] * 100)/(float)(T),base_count[2][3],(float)(base_count[2][3] * 100)/(float)(T),base_count[2][4],(float)(base_count[2][4] * 100)/(float)(T));

printf("G occurs %5d times (%2.2f %) and is replaced by: A %d (%2.2f %), C %d (%2.2f %), T %d (%2.2f %), G %d (%2.2f %), - %d (%2.2f %)\n\n",G,(float)(G * 100)/(float)(total),base_count[3][0],(float)(base_count[3][0] * 100)/(float)(G),base_count[3][1],(float)(base_count[3][1] * 100)/(float)(G),base_count[3][2],(float)(base_count[3][2] * 100)/(float)(G),base_count[3][3],(float)(base_count[3][3] * 100)/(float)(G),base_count[3][4],(float)(base_count[3][4] * 100)/(float)(G));
}

/* Prints out proper invocation and exits */
void print_usage()
{
fprintf(stderr,
	"analyze - Analyze the substituation at 4d sites.  First run amalagam
            shell script on gff files, then use this program to check 
            the substitution that occurs at the 4d sites.  It defaults
            to using human as reference, and assumes human is the first species
            alignment.  This program is not very robust.
usage:
  analyze zooAlignmentfile 4dfile [options]

options:
  -s= species - use a different species as base.\n");
exit(1);
}


/* Translate a base to an integer */
int translate(char c)
{
char d= toupper(c);
if(d == 'A')
    return 0;
if(d == 'C')
    return 1;
if(d == 'T')
    return 2;
if(d == 'G')
    return 3;
if(d == '-')
    return 4;

return -1;
}








