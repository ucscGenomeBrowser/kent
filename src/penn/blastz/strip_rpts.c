/* strip_rpts.c -- strip repeat eleements from a sequence */

#include "util.h"
#include "seq.h"

#define BUF_SIZE 1000

int main(int argc, char **argv)
{
	SEQ *sf;
	uchar *s;
	int len, b, e, i, col, old_b;
	char buf[500];
	FILE *fp;

	if (argc != 3)
		fatal("args: sequence %:repeats-file");
	sf = seq_get(argv[1]);
	s = SEQ_CHARS(sf);
	len = SEQ_LEN(sf);
	fp = ckopen(argv[2], "r");
	if (fgets(buf, BUF_SIZE, fp) == NULL ||
	    !same_string(buf, "%:repeats\n"))
		fatalf("%s is not a %:repeats file", argv[2]);
	old_b = 0;
	for (old_b = 0; fgets(buf, BUF_SIZE, fp); old_b = b) {
		if (sscanf(buf, "%d %d", &b, &e) != 2)
			fatalf("bad repeats line in %s: %s", argv[2], buf);
		if (b < old_b)
			fatalf("out-of-order line in %s: %s", argv[2], buf);
		if (b > e || b < 1 || e > len)
			fatalf("begin = %d, end = %d, seq_len = %d\n",
			  b, e, len);
		for (i = b-1; i < e; ++i)
			s[i] = 'Z';
	}
	printf(">%s, minus lineage-specific repeats\n", SEQ_HEAD(sf));
	for (i = col = 0; i < len; ++i)
		if (s[i] != 'Z') {
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
