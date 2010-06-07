/* Function type */
typedef double (*funct_t)(double);

/* Data type for links in chain of symbols. */
struct symrec
    {
    struct symrec *next;	/* link field. */
    char *name;	/* name of symbol */
    int type;	/* type of symbol (VAR or FNCT) */
    union
       {
       double var;	/* value of a VAR. */
       funct_t fnctptr;	/* value of a FNCT */
       } value;
    };

typedef struct symrec symrec;

/* The symbol table: a chain of struct symrec'. */
extern symrec *sym_table;

symrec *putsym(char const *, int);
symrec *getsym(char const *);

void init_table();
/* Put arithmetic functions in table. */
