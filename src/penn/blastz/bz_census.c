#include "util.h"
#include "seq.h"
#include "bz_all.h"
#include "bz_main.h"
#include "bz_census.h"

census_t *new_census(int n)
{
	return ckallocz((n+2)*sizeof(census_t));
	    // 1 based, plus 1 for off by one errors
}

static void inc(census_t census[], int n, int i)
{
	assert(i<=n);
	if (census[i] < 255) ++census[i];
}

void msp_census(census_t census[], int n, msp_table_t *mt)
{
	msp_t *p;
	int i, j;

	for (p = MSP_TAB_FIRST(mt); MSP_TAB_MORE(mt,p); p = MSP_TAB_NEXT(p))
		for (i=p->pos1, j=p->pos1+p->len; i < j; ++i)
			inc(census,n,i);

}

int census_mask_align(align_t *a, int n, uchar *fwd, uchar *rev, census_t *census, int thresh)
{
	int cnt = 0;
        for (; a; a = a->next_align) {
		int i,j;
		for (i=a->beg1, j=a->end1; i<=j; ++i)
			inc(census,n,i);
		cnt += census_mask_interval(
			n, fwd, rev, a->beg1, a->end1, census, thresh);
        }
	return cnt;
}

int census_mask_gapped_align(align_t *a, int n, uchar *fwd, uchar *rev, census_t *census, int thresh)
{
	int i, j;
	int cnt;
	unsigned int k;

	cnt = 0;
        for (; a; a = a->next_align) {
		int x = a->beg1;
		int y = a->beg2;
//fprintf(stderr, "== %d %d -> %d %d\n", a->beg1, a->beg2, a->end1, a->end2);
		for (k = 0; k < a->script->num; ++k) {
			edit_op_t op = a->script->op[k];
			int len = edit_val_get(op);
			switch (edit_opc_get(op)) {
			case EDIT_OP_INS:
				y += len;
				break;
			case EDIT_OP_DEL:
				x += len;
				break;
			case EDIT_OP_REP:
//fprintf(stderr, "%d %d -> ", x, y);
				for (i=x, j=x+len-1; i <= j; ++i)
					inc(census,n,i);
				cnt += census_mask_interval(n, fwd, rev, x, j, census, thresh);
				x += len;
				y += len;
//fprintf(stderr, "%d %d\n", x-1, y-1);
				assert(x <= a->end1+1);
				assert(y <= a->end2+1);
				break;
			default:
				abort();
			}
        	}
        }
	return cnt;
}


// mask seq in [a,z] where census[] >= k
// mask rev in the other direction
// msps are 1 based
// census is 1 based
// seq is 0 based
int census_mask_interval(int n, uchar *fwd, uchar *rev, int a, int z, census_t census[], int k)
{
	int i;
	int cnt;

	cnt = 0;
	//fprintf(stderr, "range %d %d\n", a, z);
	for (i=a; i<=z; ++i) {
		assert(0 < i);
		assert(i <= n);
		assert(0 <= n-i);
		//fprintf(stderr, "census[%d]: %d\n", i, census[i]);
		if (k > 0 && census[i] >= k) {
			//fprintf(stderr, "masking: %d %d\n", a, z);
			if (fwd) fwd[i-1] = 'x';
			if (rev) rev[n-i] = 'x';
			++cnt;
		}
	}
	return cnt;
}

void print_census(FILE *fp, uchar census[], int n, int k)
{
	int i;
	fprintf(fp, "Census {\n");
	for (i=1; i<=n; ++i) {
		if (census[i] >= k)
		    fprintf(fp, "%d %d\n", i, census[i]);
	}
	fprintf(fp, "}\n");
}


int census_map_intervals(uchar census[], int n,
		int t, int(*fn)(int,int,void*), void *p)
{
	int i=0, k=0;
	int count=0;
	int in=1;

	for (i=1; i<=n; ++i) {
		if (census[i] < t) {
			if (in == 1) {
				in = 0;
				if (k > 0) {
					fn(k, i-1, p); // XXX - check return
					++count;
				}
			}
		} else {
			if (in == 0) {
				in = 1;
				k = i;
			}
		}
	}
	if (in && k==0) {
		fn(1,n,p);
		++count;
	}
	return count;	
}

static int pr2i(int a, int z, void *fp)
{
	return fprintf(fp, "  x %d %d\n", a, z);
}

void print_intervals(FILE *fp, uchar census[], int n, int t)
{
	int i;
	fprintf(fp, "m {\n");
	i = census_map_intervals(census, n, t, pr2i, fp);
	fprintf(fp, "  n %d\n", i);
	fprintf(fp, "}\n");
}
