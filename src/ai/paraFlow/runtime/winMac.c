/* This, GHU help us, is the Mac Windows Library. */

#include "common.h"
#include "../compiler/pfPreamble.h"

struct win
    {
    int _pf_refCount;			     	/* Number of references. */
    void (*_pf_cleanup)(struct win *obj, int id); /* Called when refCount <= 0 */
    _pf_polyFunType *_pf_polyFun;	/* Polymorphic functions. */
    struct _pf_string *name;		/* The only exported field. */
    };

static void winCleanup(struct win *win, int typeId)
/* Clean up string->s and string itself. */
{
uglyf("winCleanup()\n");
_pf_class_cleanup((struct _pf_object *)win, typeId);
}

void pf_winOpen(_pf_Stack *stack)
{
int width = stack[0].Int;
int height = stack[1].Int;
struct win *win;
struct _pf_string *name = stack[2].String;
uglyf("winOpen(%d,%d,%s);\n", width, height, name->s);
AllocVar(win);
win->_pf_refCount = 1;
win->_pf_cleanup = winCleanup;
win->name = name;
stack[0].v = win;
}

void _pf_cm_win_clear(_pf_Stack *stack)
{
struct win *win = stack[0].v;
uglyf("clear();\n");
}

void _pf_cm_win_plotDot(_pf_Stack *stack)
{
struct win *win = stack[0].v;
int x = stack[1].Int;
int y = stack[2].Int;
uglyf("plotDot(%d,%d);\n", x, y);
}

void _pf_cm_win_circle(_pf_Stack *stack)
{
struct win *win = stack[0].v;
int xCen = stack[1].Int;
int yCen = stack[2].Int;
int radius = stack[3].Int;
uglyf("circle(%d,%d,%d);\n", xCen, yCen, radius);
}

void _pf_cm_win_line(_pf_Stack *stack)
{
struct win *win = stack[0].v;
int x1 = stack[1].Int;
int y1 = stack[2].Int;
int x2 = stack[3].Int;
int y2 = stack[4].Int;
uglyf("line(%d,%d,%d,%d);\n", x1,y1,x2,y2);
}
