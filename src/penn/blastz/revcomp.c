/* revcomp.c -- compute the reverse complement of a fasta file */

#include "util.h"
#include "seq.h"

int main(int argc, char **argv)
{
	SEQ *sf;
	uchar *s;
	int i, len, col;

	if (argc != 2)
		fatal("arg = fasta-file");
	sf = seq_get(argv[1]);
	s = SEQ_CHARS(sf);
	len = SEQ_LEN(sf);
	do_revcomp(s, len);

	printf("%s (rev. comp.)\n", SEQ_HEAD(sf));
	for (i = col = 0; i < len; ++i) {
		putchar(s[i]);
		if (++col == 50) {
			putchar('\n');
			col = 0;
		}
	}
	if (col)
		putchar('\n');
	return 0;
}
