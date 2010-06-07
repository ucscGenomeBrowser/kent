void calc(int count)
{
int s = 1;
int t = 2;
int u = 3;
int v = 4;
int x = 5;
int y = 6;
int z = 7;
int i,j;
for (i=0; i<count; ++i)
    {
    int xx = s*t + u*v+i;
    int yy = x+y+z+i;
    int sum = 0;
    for (j=0; j<count;++j)
	sum += xx + yy + j;
    _printInt(sum);
    s += 1;
    t += 1;
    u += 1;
    v += 1;
    x += 1;
    y += 1;
    z += 1;
    }
}
