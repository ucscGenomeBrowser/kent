/* restore_rpts.c -- adjust blastz.out to, in effect, restore lineage-specific
*  repeats to the two sequences; this includes adjusting the header information
*
* command syntax:
     restore_rpts blastz.out orig.seq1 orig.seq2 %:repeats1 %:repeats2 [reverse]
* where blastz.out can be "-".  If the word "reverse" appears last on the
* command line, then then the second set of repeats is understood to be in
* reverse complement of orig.seq2.
*/

#include "util.h"
#include "seq.h"
#include "dna.h"

#define BUF_SIZE 1000
#define MAX_RPTS 1000000
#define MAX_ALIGN 100000
#define MIN_LEN	20	/* minimum length of printed alignment */

/* initially for locations of lineage-specific repeats, but immediately changed
   to a mapping from position in the stripped sequence to offset (amount to be
   added to get the corresponding position in the original sequence).  We add
   artificial entries at either end to keep searches in bounds */
int pos1[MAX_RPTS], offset1[MAX_RPTS], pos2[MAX_RPTS], offset2[MAX_RPTS];
int nmap1, nmap2;

/* i1 and i2 are the positions in the pos-offset arrays of the current
   alignment.  We expect them to change very little from one alignment to the
   next, so we keep them in global variables and update by linear search */
int i1, i2;

/* tell if second %:repeats file is for the reverse complement of the second
   sequence */
int reverse = 0;

/* lower-case positions are for a segment in an alignment of the stripped
   sequences */
int b1[MAX_ALIGN], b2[MAX_ALIGN], e1[MAX_ALIGN], e2[MAX_ALIGN];
/* lower-case positions are for a segment in an alignment of the original
   sequences */
int B1[MAX_ALIGN], B2[MAX_ALIGN], E1[MAX_ALIGN], E2[MAX_ALIGN];

/* the original sequences, starting with position 1, and associated info */
uchar *A, *B;
char *head1, *head2, *name1, *name2;
int len1, len2;

/* alignment scoring parameters */
ss_t ss;
int penalty_open, penalty_extend;

void restore_m_stanza(int nline) {
	int i, n, ibreak, off;

	ibreak = 0;
	for (n = i = 0; i < nline; ++n) {
		while (pos1[ibreak+1] <= b1[i])
			if (++ibreak >= nmap1 - 1) /* last entry is bogus */
				fatal("ibreak got too large");
		off = offset1[ibreak];
		if (e1[i] < pos1[ibreak+1]) {
			printf("  x %d %d\n", b1[i]+off, e1[i]+off);
			++i;
		} else {
			printf("  x %d %d\n", b1[i]+off, pos1[ibreak+1]-1+off);
			b1[i] = pos1[ibreak+1];
		}
	}
	printf("  n %d\n", n);
} 

static void print_align(int n) {
	int i, j, k, score, gap1, gap2, match, y, pct;

	if (E1[n-1] - B1[0] < MIN_LEN-1)
		return;

	/* compute the alignment's score */
	for (score = k = 0; k < n; ++k) {
		/* blastz produces an occasional length-0 line */
		if (E1[k] < B1[k] - 1 || E2[k] < B2[k] - 1)
			fatal("alignment segment cannot be printed");
		if (k > 0) {
			gap1 = B1[k] - E1[k-1] - 1;
			gap2 = B2[k] - E2[k-1] - 1;
			if (gap1 == 0 && gap2 == 0)
				fatalf("segs abut: %d %d %d %d",
				  B1[k], E1[k-1], B2[k], E2[k-1]);
			if (gap1 < 0 || gap2 < 0)
				fatalf("segs overlap: %d %d %d %d",
				  B1[k], E1[k-1], B2[k], E2[k-1]);
			if (gap1 > 0 && gap2 > 0)
				fatalf("non-touching segs: %d %d %d %d",
				  B1[k], E1[k-1], B2[k], E2[k-1]);
			score -= (penalty_open +
				  penalty_extend * MAX(gap1, gap2));
		}
		for (i = B1[k], j = B2[k]; i <= E1[k]; ++i, ++j) {
			assert(i <= len1);
			assert(j <= len2);
			score += ss[A[i]][B[j]];
		}
	}
	if (score <= 0)
		return;

	printf("a {\n  s %d\n  b %d %d\n  e %d %d\n",
	  score, B1[0], B2[0], E1[n-1], E2[n-1]);
	for (k = 0; k < n; ++k) {
		for (match = 0, i = B1[k], j = B2[k]; i <= E1[k]; ++i, ++j)
			match += (toupper(A[i]) == toupper(B[j]));
		y = E1[k] - B1[k] + 1;
		pct = (y > 0 ? (100*match+y/2)/y : 0);
		printf("  l %d %d %d %d %d\n", B1[k], B2[k], E1[k], E2[k], pct);
	}
	printf("}\n");
}

/* map coordinates in an alignment from the stripped sequence to the original
   sequence; break into two alignments at each lineage-specific repeat */
void restore_align(int nline) {
	int n, k, length, off1, off2;


	/* n = line number in alignment of stripped sequences,
	   k = line number in alignment of original sequences */
	for (k = n = 0; n < nline; ) {
		if (k == 0) {	/* starting output alignment */
			/* linear search: i1 is max s.t. pos1[i1] <= b1[n] */
			/* artificial floor and ceiling entries bound search */
			while (pos1[i1+1] <= b1[n])
				if (++i1 >= nmap1 - 1) /* last entry is bogus */
					fatal("i1 became too large");
			while (pos1[i1] > b1[n])
				if (--i1 < 0)
					fatal("i1 became negative");
			off1 = offset1[i1];
		
			/* similarly for i2 */
			while (pos2[i2+1] <= b2[n])
				if (++i2 >= nmap2 - 1)
					fatal("i2 became too large");
			while (pos2[i2] > b2[n])
				if (--i2 < 0)
					fatal("i2 became negative");
			off2 = offset2[i2];
		}
		if (pos1[i1] > b1[n] || pos1[i1+1] <= b1[n] ||
		    pos2[i2] > b2[n] || pos2[i2+1] <= b2[n])
			fatal("segment has wrong offset");

		/* if gap-free segment does not extend to the next breakpoint,
		   just stash it */
		if (e1[n] < pos1[i1+1] && e2[n] < pos2[i2+1]) {
			B1[k] = b1[n] + off1;
			B2[k] = b2[n] + off2;
			E1[k] = e1[n] + off1;
			E2[k] = e2[n] + off2;
/*
printf("mapped [%d,%d]->[%d,%d] onto [%d,%d]->[%d,%d]\n",
  b1[n], e1[n], b2[n], e2[n], B1[k], E1[k], B2[k], E2[k]);
*/
			++k;
			/* maybe there's a breakpoint before the next segment */
			if (n == nline - 1 ||
			    b1[n+1] >= pos1[i1+1] || b2[n+1] >= pos2[i2+1]) {
				print_align(k);
				k = 0;
			}
			++n;
		} else {	/* break the alignment in two at this segment */
			/* how much of this segment precedes a breakpoint? */
			length = MIN(pos1[i1+1] - b1[n], pos2[i2+1] - b2[n]);
			if (length <= 0)
				fatalf("length <= 0: [%d,%d]->[%d,%d]",
				  b1[n], e2[n], b2[n], e2[n]);
			B1[k] = b1[n] + off1;
			E1[k] = B1[k] + length - 1;
			B2[k] = b2[n] + off2;
			E2[k] = B2[k] + length - 1;
/*
fprintf(stderr,
"broken gap-free segment ends at %d,%d (%d,%d in stripped alignment)\n",
E1[k], E2[k], b1[n]+length-1, b2[n]+length-1);
*/
			print_align(k+1);
			b1[n] += length;
			b2[n] += length;
			if (b1[n] > e1[n] || b2[n] > e2[n])
				fatal("created bad aligned segment");
			k = 0;
			/* note that n isn't incremented */
		}
	}
}

void discard_lines(FILE *fp, int n) {
	char buf[BUF_SIZE];

	while (n-- > 0)
		if (fgets(buf, BUF_SIZE, fp) == NULL)
			fatal("failure when discarding lines");
}

/* make the header information in the alignment of stripped sequences look
   like an alignment of the original sequences; for each alignment, collect
   the gap-free segments and map them to original coordinates by calling
   restore_align(); process m-stanza similarly */
void restore_bz(char *filename)
{
	FILE *fp;
	char buf[BUF_SIZE], *x;
	int nline;

	fp = (same_string(filename,"-") ? stdin : ckopen(filename, "r"));
	if (fgets(buf, BUF_SIZE, fp) == NULL ||
	    !same_string(buf, "#:lav\n"))
		fatalf("%s is not a blastz output file", filename);
	printf("%s", buf);

	while (fgets(buf, BUF_SIZE, fp))
		if (same_string(buf, "d {\n")) {
			if (fgets(buf, BUF_SIZE, fp) == NULL)
				fatal("failure when tweaking d stanza");
			if (strstr(buf, "B=0") == NULL)
				fatal("blastz wasn't run with B=0");
			if (buf[2] != '"')
				fatalf("bad description line: %s", buf);
			printf("d {\n  \"restore_rpts applied to:\n  %s",buf+3);
		} else if (same_string(buf, "s {\n")) {
			discard_lines(fp, 2);
			printf("s {\n  %s\n  %s\n", name1, name2);
		} else if (same_string(buf, "h {\n")) {
			discard_lines(fp, 2);
			x = (reverse ? " (reverse complement)" : "");
			printf("h {\n   \"%s\"\n   \"%s%s\"\n", head1, head2,x);
		} else if (same_string(buf, "a {\n")) {
			discard_lines(fp, 3); /* score, beg, end */
			for (nline = 0;
			  fgets(buf, BUF_SIZE, fp) && !same_string(buf, "}\n");
			  ++nline) {
				if (nline >= MAX_ALIGN)
					fatal("Too many segments in alignment");
				if (sscanf(buf, "  l %d %d %d %d",
				  &(b1[nline]), &(b2[nline]),
				  &(e1[nline]), &(e2[nline])) != 4)
					fatalf("bad blastz line: %s", buf);
			}
			restore_align(nline);
		} else if (same_string(buf, "m {\n")) {
			printf("%s", buf);
			for (nline = 0;
			  fgets(buf, BUF_SIZE, fp) &&
			    strncmp(buf, "  x ", 4) == 0;
			  ++nline) {
				if (nline >= MAX_ALIGN)
					fatal("Too many segments in m-stanza");
				if (sscanf(buf, "  x %d %d",
				  &(b1[nline]), &(e1[nline])) != 2)
					fatalf("bad m-stanza line: %s", buf);
			}
			if (strncmp(buf, "  n ", 4) != 0)
				fatalf("confused at end of m-stanza: %s", buf);
			restore_m_stanza(nline);
		} else
			printf("%s", buf);

	if (fp != stdin)
		fclose(fp);
}

/* convert %:repeats end-points to a stripped-pos-to-offset mapping */
void rpts2map(int *pos, int *offset, int nrpts)
{
	int i, removed, len;

	removed = 0;
	for (i = 1; i < nrpts; ++i) {
		len = (offset[i] - pos[i] + 1);
		pos[i] -= removed;
		removed += len;
		offset[i] = removed;
	}
}

/* read intervals in a %:repeats file, merging pairs that overlap; call
   rpts2map to convert from original-sequence positions of repeats into a
   mapping from stripped-ssquence-positions to offsets; add artificial floor
   and ceiling entries to keep searches in bounds */
int get_mapping(char *filename, int *beg, int *end, int do_reverse, int len)
{
	FILE *fp = ckopen(filename, "r");
	char buf[BUF_SIZE];
	int b, e, nrpts, i, j, bt, et;

	if (fgets(buf, BUF_SIZE, fp) == NULL || strcmp(buf, "%:repeats\n"))
		fatalf("%s is not a %:repeats file", filename);
	nrpts = 1;	/* leave room for artifical floor */
	while (fgets(buf, BUF_SIZE, fp)) {
		if (sscanf(buf, "%d %d", &b, &e) != 2)
			fatalf("bad line in %s: %s\n", filename, buf);
		if (b < 0 || b > e || e > len)
			fatalf("in %s: repeat = %d,%d and seq len = %d\n",
			  filename, b, e, len);
		if (nrpts == 1 || b > end[nrpts-1] + 1) {
			if (nrpts >= MAX_RPTS-1)
				fatal("too many repeats");
			beg[nrpts] = b;
			end[nrpts] = e;
			++nrpts;
		} else {
			if (b < beg[nrpts-1])
				fatalf("interval %d-%d is out of order in %s",
				  b, e, filename);
			end[nrpts-1] = MAX(end[nrpts-1], e);
		}
	}

	fclose(fp);

	if (do_reverse) {
		for (i = 1, j = nrpts-1; i < j; ++i, --j) {
			bt = beg[i];
			et = end[i];
			beg[i] = len2 + 1 - end[j];
			end[i] = len2 + 1 - beg[j];
			beg[j] = len2 + 1 - et;
			end[j] = len2 + 1 - bt;
		}
		if (i == j) {
			bt = beg[i];
			beg[i] = len2 + 1 - end[i];
			end[i] = len2 + 1 - bt;
		}
	}

	rpts2map(beg, end, nrpts);

	/* add an artificial floor and ceiling entries */
	beg[0] = end[0] = 0;	/* offset is 0 before the first repeat */
	beg[nrpts] = end[nrpts] = 1000000000;	/* should never be used */
	return nrpts+1;	/* room for artifical ceiling */
}

int main(int argc, char **argv)
{
	SEQ *sf1, *sf2;
	char *s;
	int i;

	if (argc == 9 && same_string(argv[8], "reverse")) {
		reverse = 1;
		--argc;
	}
	if (argc != 8) {
		fprintf(stderr, "args: blastz.out orig_seq1 name1 ");
		fatal("orig.seq2 name2 %:repeats1 %:repeats2 [reverse]");
	}


	/* get sequence 1 */
	sf1 = seq_get(argv[2]);
	A = SEQ_CHARS(sf1) - 1;
	len1 = SEQ_LEN(sf1);
	head1 = SEQ_HEAD(sf1);
	if ((s = strstr(head1, ", minus")) != NULL)
		*s = '\0';
	name1 = argv[3];

	/* get sequence 2 */
	sf2 = seq_get(argv[4]);
	B = SEQ_CHARS(sf2) - 1;
	len2 = SEQ_LEN(sf2);
	head2 = SEQ_HEAD(sf2);
	if ((s = strstr(head2, ", minus")) != NULL)
		*s = '\0';
	name2 = argv[5];
 	if (reverse)
 		do_revcomp(B+1, len2);

	/* convert the positions of lineage-specific repeats to a mapping
	   from positions in the stripped sequence to offsets */
	nmap1 = get_mapping(argv[6], pos1, offset1, 0, len1);
	nmap2 = get_mapping(argv[7], pos2, offset2, reverse, len2);
	for (i = 1; i < nmap2; ++i)
		if (pos2[i-1] >= pos2[i])
			fatalf("bad pos2: %d >= %d\n", pos2[i-1], pos2[i]);

	/* get alignment scores */
	DNA_scores(ss);
	penalty_extend = 30;
	penalty_open = 400;

	/* convert the blastz output */
	restore_bz(argv[1]);

	return 0;
}
