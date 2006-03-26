void calc()
{
int s = 88;
int t = 81;
int u = 19;
int v = 21;
int x = 23;
int y = 33;
int z = 44;
int i = 0;
for (i=0; i<10; ++i)
    {
    int xx = s*t + u*v+i;
    int yy = x+y+z+i;
    _printInt(xx+yy);
    s += 1;
    t += 1;
    u += 1;
    v += 1;
    x += 1;
    y += 1;
    z += 1;
    }
}
