/* annoFormatter -- aggregates, formats and writes output from multiple sources */

#include "annoFormatter.h"

struct annoOption *annoFormatterGetOptions(struct annoFormatter *self)
/* Return supported options and current settings.  Callers can modify and free when done. */
{
return annoOptionCloneList(self->options);
}

void annoFormatterSetOptions(struct annoFormatter *self, struct annoOption *newOptions)
/* Free old options and use clone of newOptions. */
{
annoOptionFreeList(&(self->options));
self->options = annoOptionCloneList(newOptions);
}

void annoFormatterFree(struct annoFormatter **pSelf)
/* Free self. This should be called at the end of subclass close methods, after
 * subclass-specific connections are closed and resources are freed. */
{
if (pSelf == NULL)
    return;
annoOptionFreeList(&((*pSelf)->options));
freez(pSelf);
}

struct annoStreamRows *annoStreamRowsNew(struct annoStreamer *streamerList)
/* Return an array of aS&R for each streamer in streamerList. */
{
int streamerCount = slCount(streamerList);
struct annoStreamRows *data = NULL;
AllocArray(data, streamerCount);
struct annoStreamer *streamer = streamerList;
int i = 0;
for (;  i < streamerCount;  i++, streamer = streamer->next)
    data[i].streamer = streamer;
return data;
}
