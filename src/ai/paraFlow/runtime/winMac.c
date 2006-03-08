/* This, GHU help us, is the Mac Windows Library. */

#include <Carbon/Carbon.h>
#include "common.h"
#include "portable.h"
#include "../compiler/pfPreamble.h"
#include "pfString.h"

struct winApp
    {
    int _pf_refCount;			     	/* Number of references. */
    void (*_pf_cleanup)(struct winApp *obj, int id); /* Called when refCount <= 0 */
    _pf_polyFunType *_pf_polyFun;	/* Polymorphic functions. */
    };

/* Order of functions:
    open()
    close()
    keyPress(string key)
    mouse(int x,y, short buttons,lastButtons)
 */

void pf_winSurrender(_pf_Stack *stack)
/* Surrender control to windowing system.  Hope they call us back. */
{
struct winApp *app = stack[0].v;
int mouseX = 300, mouseY=200;
_pf_nil_check(app);

stack[0].v = app;
app->_pf_polyFun[0](stack);
for (;;)
    {
    char c = rawKeyIn();
    boolean gotMouse = FALSE;
    if (c == 'q')
        break;
    else if (c == '+')
        {
	mouseX += 1;
	gotMouse = TRUE;
	}
    else if (c == '-')
        {
	mouseX -= 1;
	gotMouse = TRUE;
	}
    if (gotMouse)
        {
	stack[0].v = app;
	stack[1].Int = mouseX;
	stack[2].Int = mouseY;
	stack[3].Short = 0;
	stack[4].Short = 0;
	app->_pf_polyFun[3](stack);
	}
    else
        {
	stack[0].v = app;
	stack[1].String = _pf_string_new(&c, 1);
	app->_pf_polyFun[2](stack);
	}
    }
stack[0].v = app;
app->_pf_polyFun[1](stack);

if (--app->_pf_refCount <= 0)
    app->_pf_cleanup(app, 0);
}

