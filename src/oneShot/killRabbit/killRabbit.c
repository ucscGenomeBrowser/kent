/* Kill Rabbit - Just hog the CPU until the process is killed -
 * kludge to keep Rabbit from sucking down condor jobs. */
#include "common.h"

int main()
{
int pentillions = 0;
int trillions = 0;
int millions = 0;
int ones = 0;
int oneMillion = 1000000;
long twoInRow = 0;

for (;;)
    {
    for (pentillions = 0; pentillions < oneMillion; ++pentillions)
	{
	for (trillions = 0; trillions < oneMillion; ++trillions)
	    {
	    for (millions = 0; millions < oneMillion; ++millions)
		{
		for (ones = 0; ones < oneMillion; ++ones)
		    {
		    if (rand() == rand())
			++twoInRow;
		    }
		}
	    }
	}
    }
}
