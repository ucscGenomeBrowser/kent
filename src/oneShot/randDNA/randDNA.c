#include <stdio.h>
#include <stdlib.h>
static char nuc[] = {'A', 'C', 'T', 'G'};

int main(int argc, char *argv[]) {
    int j;
    if (argc != 2) {printf("usage: %s <N>\n\tOutputs N random nucleotides\n",
	argv[0]); return 255;}
    for(j = 1; j <= atoi(argv[1]); ++j) {
	putchar(nuc[rand()%4]);
	(j%60) ? (j%10) ? 0 : putchar(' ') : putchar('\n');
    }
    (atoi(argv[1])%60) ? putchar('\n') : 0;
    return 0;
}
