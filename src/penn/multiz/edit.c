#include "util.h"
#include "seq.h"
#include "edit.h"
#include <assert.h>

static const char rcsid[]=
"$Id: edit.c,v 1.1 2002/09/24 18:04:17 kent Exp $";

static edit_op_t *ed_ops_realloc(edit_op_t *op, unsigned int n);
static edit_op_t edit_op_cons(unsigned int op, unsigned int val);
static edit_op_t edit_op_inc(edit_op_t op, unsigned int n);
static edit_op_t edit_op_inc_last(edit_script_t *es, unsigned int n);
static edit_script_t *edit_script_concat(edit_script_t *es, edit_script_t *et);
static edit_script_t *edit_script_copy(edit_script_t *);
static edit_script_t *edit_script_fin(edit_script_t *data);
static edit_script_t *edit_script_init(edit_script_t *es);
static int edit_script_more(edit_script_t *data, unsigned int op, unsigned int k);
static int edit_script_put(edit_script_t *es, unsigned int op, unsigned int n);
static int edit_script_ready(edit_script_t *es, unsigned int n);
static int edit_script_readyplus(edit_script_t *es, unsigned int n);
static void edit_script_prnt(edit_script_t *es);

unsigned int edit_opc_get(edit_op_t op)
{
	return op & EDIT_OP_MASK;
}

unsigned int edit_val_get(edit_op_t op)
{
	return op >> 2;
}

static edit_op_t edit_op_cons(unsigned int op, unsigned int val)
{
	return (val << 2) | (op & EDIT_OP_MASK);
}

static edit_op_t edit_op_inc(edit_op_t op, unsigned int n)
{
	return edit_op_cons(edit_opc_get(op), edit_val_get(op) + n);
}

static edit_op_t edit_op_inc_last(edit_script_t *es, unsigned int n)
{
	edit_op_t *last;
	assert (es->num > 0);
	last = &(es->op[es->num-1]);
	*last = edit_op_inc(*last, n);
	return *last;
}

static void edit_script_prnt(edit_script_t *es)
{
	unsigned int i;

	printf("num=%d size=%d\n", es->num, es->size);
	for (i = 0; i < es->num; ++i) {
		edit_op_t this = es->op[i];
		assert(this != 0);
		printf("%d:%c:%d ", i, "?IDR"[edit_opc_get(this)],
		  edit_val_get(this));
		if (i%8 == 7) putchar('\n');
	}
	putchar('\n');
}

static edit_script_t *edit_script_init(edit_script_t *es)
{
	es->op = 0;
	es->size = es->num = 0;
	es->last = 0;
	edit_script_ready(es, 8);
	return es;
}

static edit_op_t *ed_ops_realloc(edit_op_t *op, unsigned int n)
{
	return ckrealloc(op, n*sizeof(edit_op_t)); 
	/* XXX - assumes posix realloc */
}

static int edit_script_ready(edit_script_t *es, unsigned int n)
{
	edit_op_t *p;
	unsigned int m = n + n/2;

	if (es->size <= n) {
		p = ed_ops_realloc(es->op, m);
		if (p == 0) {
			return 0;
		} else {
			es->op = p;
			es->size = m;
		}
	}
	return 1;
}

static int edit_script_readyplus(edit_script_t *es, unsigned int n)
{
	if (es->size - es->num <= n)
		return edit_script_ready(es, n + es->num);
	return 1;
}

static int edit_script_put(edit_script_t *es, unsigned int op, unsigned int n)
{
	if (!edit_script_readyplus(es, 2))
		return 0;
	es->last = op;
	assert(op != 0);
	es->op[es->num] = edit_op_cons(op, n);
	es->num += 1;
	es->op[es->num] = 0; /* sentinal */
	return 1;
}

static edit_script_t *edit_script_fin(edit_script_t *es)
{
	edit_op_t *p = ed_ops_realloc(es->op, es->num);
	if (!p)
		return 0;
	es->op = p;
	es->size = es->num;
	return es;
}

static int edit_script_more(edit_script_t *data, unsigned int op, unsigned int k)
{
	if (op == EDIT_OP_ERR)
		fatalf("edit_script_more: bad opcode %d:%d", op, k);
	if (edit_opc_get(data->last) == op)
		edit_op_inc_last(data, k);
	else
		edit_script_put(data, op, k);
	return 0;
}

int edit_script_del(edit_script_t *data, unsigned int k)
{
	return edit_script_more(data, EDIT_OP_DEL, k);
}

int edit_script_ins(edit_script_t *data, unsigned int k)
{
	return edit_script_more(data, EDIT_OP_INS, k);
}

int edit_script_rep(edit_script_t *data, unsigned int k)
{
	return edit_script_more(data, EDIT_OP_REP, k);
}

edit_script_t *edit_script_reverse_inplace(edit_script_t *es)
{
	unsigned int i;
	const unsigned int num = es->num;
	const unsigned int mid = num/2;
	const unsigned int end = num-1;

	for (i = 0; i < mid; ++i) {
		const edit_op_t t = es->op[i];
		es->op[i] = es->op[end-i];
		es->op[end-i] = t;
	}
	return es;
}

edit_script_t *edit_script_new(void)
{
	edit_script_t *es = ckallocz(sizeof(*es));
	if (!es)
		return 0;
	return edit_script_init(es);
}

edit_script_t *edit_script_free(edit_script_t *es)
{
	if (es) {
		if (es->op)
			ckfree(es->op);
		memset(es, 0, sizeof(*es));
		ckfree(es);
	}
	return 0;
}

/* deep copy of es */
static edit_script_t *edit_script_copy(edit_script_t *es)
{
	edit_script_t *nes = ckallocz(sizeof(*nes));
	unsigned int size = sizeof(*nes->op) * es->num;
	nes->op = ckallocz(size);
	if (!nes->op)
		return 0; /* XXX - leak */
	memcpy(nes->op, es->op, size);
	nes->size = nes->num = es->num;
	nes->last = 0;
	return nes;
}

edit_op_t *edit_script_first(edit_script_t *es)
{
	return es->num > 0 ? &es->op[0] : 0;
}

edit_op_t *edit_script_next(edit_script_t *es, edit_op_t *op)
{
	/* XXX - assumes flat address space */
	if (&es->op[0] <= op && op < &es->op[es->num-1])
		return op+1;
	else
		return 0;
}

/* build a new script from es and et */
static edit_script_t *edit_script_concat(edit_script_t *es, edit_script_t *et)
{
	edit_op_t *op;
	edit_script_t *eu;

	if (!(eu = edit_script_new()))
		return 0;
	for (op = edit_script_first(es); op; op = edit_script_next(es, op))
		edit_script_more(eu, edit_opc_get(*op), edit_val_get(*op));
	for (op = edit_script_first(et); op; op = edit_script_next(et, op))
		edit_script_more(eu, edit_opc_get(*op), edit_val_get(*op));
	return edit_script_fin(eu);
}

/* add et to es */
edit_script_t *edit_script_append(edit_script_t *es, edit_script_t *et)
{
	edit_op_t *op;

	for (op = edit_script_first(et); op; op = edit_script_next(et, op))
		edit_script_more(es, edit_opc_get(*op), edit_val_get(*op));
	return es;
}

int es_rep_len(edit_script_t *S, int *n, const uchar *p, const uchar *q, int *match)
{
	int len;

	len = *match = 0;
	while ((*n < S->num) && (edit_opc_get(S->op[*n]) == EDIT_OP_REP)) {
		int num = edit_val_get(S->op[*n]);
		len += num;
		while (num-- > 0)
		    /* masked regions are lower case */
                    if (toupper(*p++) == toupper(*q++)) ++(*match);
		*n += 1;
	}
	return len;
}

int es_indel_len(edit_script_t *S, int *n, int *i, int *j)
{
	if (S->num <= *n)
		return 0;
	else {
		edit_op_t op = S->op[*n];
		int len = edit_val_get(op);

		switch (edit_opc_get(op)) {
		case EDIT_OP_INS:
			*j += len;
			break;
		case EDIT_OP_DEL:
			*i += len;
			break;
		default:
			fatalf("es_indel_len: cannot happen!");
		}
		*n += 1;
		return len;
	}
}
  

#ifdef TESTING
int main()
{
	int i;
	edit_script_t *es, *et, *eu;

	es = edit_script_new();
	for (i = 0; i < 13; ++i) {
		unsigned int r = random() % 4;
		unsigned int s = random() % 100;
		if (1 <= r && r <= 3) edit_script_more(es, r, s);
	}
	edit_script_prnt(es);
	et = edit_script_reverse_inplace(edit_script_copy(es));
	edit_script_prnt(et);
	eu = edit_script_concat(es, et);
	edit_script_prnt(eu);
	eu = edit_script_append(eu, es);
	edit_script_prnt(eu);

	es = edit_script_free(es);
	et = edit_script_free(et);
	eu = edit_script_free(eu);
	exit(0);
}
#endif
