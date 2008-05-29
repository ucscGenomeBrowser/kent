/* broadData - stuff to help with data broadcasting. */

#include "paraCommon.h"
#include "broadData.h"


int bdSendTo(int sd, struct bdMessage *m, bits32 destIp, int port)
/* Send message out socket.  Returns error code if any or 0 for success. */
{
struct sockaddr_in sai;
int err;
bits32 machine;
bits32 id;		/* Message id. */
bits16 type;	/* One of the bdMessageTypes above. */
bits16 size;	/* Size of the data part of this message */

/* Save binary non-byte parts of message and convert them
 * to network format. */
machine = m->machine;
m->machine = htonl(machine);
id = m->id;
m->id = htonl(id);
type = m->type;
m->type = htons(type);
size = m->size;
m->size = htons(size);

/* Send message */
ZeroVar(&sai);
sai.sin_addr.s_addr = htonl(destIp);
sai.sin_family = AF_INET;
sai.sin_port = htons(port);
// uglyf("bdSendTo destIp = %x, port %d, machine %x size %d\n", destIp, port, machine, size);
err =  sendto(sd, m, bdHeaderSize + size, 0, (struct sockaddr *)&sai, sizeof(sai));
if (err < 0) warn(" sendto err %d, errno %d %s", err, errno, strerror(errno));

/* Restore message to host format. */
m->machine = machine;
m->id = id;
m->type = type;
m->size = size;
return err;
}

int bdReceive(int sd, struct bdMessage *m, bits32 *retSource)
/* Get message from socket.  Data should be maxDataSize long. 
 * Returns error code if any or 0 (bdGood) for success. */
{
struct sockaddr_in sai;
unsigned int saiSize = sizeof(sai);
int err;
ZeroVar(&sai);
sai.sin_family = AF_INET;
err = recvfrom(sd, m, sizeof(*m), 0, (struct sockaddr *)&sai, &saiSize);
if (err >= 0)
    {
    m->machine = ntohl(m->machine);
    m->id = ntohl(m->id);
    m->type = ntohs(m->type);
    m->size = ntohs(m->size);
    if (retSource != NULL)
	*retSource = ntohl(sai.sin_addr.s_addr);
    }
return err;
}

off_t bdBlockOffset(bits32 sectionIx, bits32 blockIx)
/* Find start byte position of this block in this section. */
{
off_t startOffset = sectionIx;
startOffset *= bdSectionBlocks;
startOffset += blockIx;
startOffset *= bdBlockSize;
return startOffset;
}

void bdInitMessage(struct bdMessage *m, bits32 machine, bits32 messageId, bits16 type, 
	bits16 dataSize)
/* Initialize a message. */
{
m->machine = machine;
m->id = messageId;
m->type = type;
m->size = dataSize;
}

static char *addLongData(char *data, bits32 val)
/* Add val to data stream, converting to network format.  Return
 * new position of data stream. */
{
val = htonl(val);
memcpy(data, &val, sizeof(val));
return data + sizeof(val);
}

static bits32 getLongData(char **pData)
/* Get val from data stream, converting from network to host format.
 * Update data stream position. */
{
bits32 val;
memcpy(&val, *pData, sizeof(val));
*pData += sizeof(val);
val = ntohl(val);
return val;
}

void bdMakeAckMessage(struct bdMessage *m, bits32 machine, bits32 messageId, signed32 err)
/* Create acknowledgement message. */
{
char *data = m->data;
bdInitMessage(m, machine, messageId, bdmAck, sizeof(err));
data = addLongData(data, err);
}

void bdParseAckMessage(struct bdMessage *m, signed32 *retErr)
/* Parse an acknowledgement message. */
{
char *data = m->data;
*retErr = getLongData(&data);
}
	

static void makeOpenCloseMessage(struct bdMessage *m, bits16 type, bits32 machine,
	bits32 messageId, bits32 fileId, char *fileName)
/* Create a file open or close message depending on type. */
{
char *data = m->data;
int len = strlen(fileName);
bdInitMessage(m, machine, messageId, type, len + 1 + sizeof(fileId));
data = addLongData(data, fileId);
memcpy(data, fileName, len+1);
}

void bdMakeFileOpenMessage(struct bdMessage *m, bits32 machine,
	bits32 messageId, bits32 fileId, char *fileName)
/* Create a file open message. */
{
makeOpenCloseMessage(m, bdmFileOpen, machine, messageId, fileId, fileName);
}

void bdParseFileOpenMessage(struct bdMessage *m, bits32 *retFileId, char **retFileName)
/* Parse out the specific parts of a file open message. */
{
char *data = m->data;
*retFileId = getLongData(&data);
*retFileName = data;
}

void bdMakeFileCloseMessage(struct bdMessage *m, bits32 machine,
	bits32 messageId, bits32 fileId, char *fileName)
/* Create a file open message. */
{
makeOpenCloseMessage(m, bdmFileClose, machine, messageId, fileId, fileName);
}

void bdParseFileCloseMessage(struct bdMessage *m, bits32 *retFileId, char **retFileName)
/* Parse out the specific parts of a file open message. */
{
char *data = m->data;
*retFileId = getLongData(&data);
*retFileName = data;
}

void bdMakeBlockMessage(struct bdMessage *m, bits32 machine, bits32 messageId, 
	bits32 fileId, bits32 sectionIx, bits32 blockIx, 
	int size, void *fileData)
/* Create a message describing part of a file. */
{
int dataSize = size + 3*sizeof(bits32);
char *data = m->data;
assert(size <= bdBlockSize);
bdInitMessage(m, machine, messageId, bdmBlock, dataSize);
data = addLongData(data, fileId);
data = addLongData(data, sectionIx);
data = addLongData(data, blockIx);
memcpy(data, fileData, size);
}

void bdParseBlockMessage(struct bdMessage *m, bits32 *retFileId,
	bits32 *retSectionIx, bits32 *retBlockIx, int *retSize, void **retFileData)
/* Parse out the specific parts of a file open message. */
{
int size = m->size - 3 * sizeof(bits32);
char *data = m->data;
*retFileId = getLongData(&data);
*retSectionIx = getLongData(&data);
*retBlockIx = getLongData(&data);
*retFileData = data;
*retSize = size;
}

void bdMakeSectionQueryMessage(struct bdMessage *m, bits32 machine,
	bits32 messageId, bits32 fileId,
	bits32 sectionIx, bits32 blockCount, unsigned char md5[16])
/* Create a message to query about status of file section.  Block count should
 * be bdSectionSize or less */
{
char *data = m->data;
assert(blockCount <= bdSectionBlocks);
bdInitMessage(m, machine, messageId, bdmSectionQuery, 3*sizeof(bits32) + 16);
data = addLongData(data, fileId);
data = addLongData(data, sectionIx);
data = addLongData(data, blockCount);
memcpy(data, md5, 16);
}

void bdParseSectionQueryMessage(struct bdMessage *m, bits32 *retFileId,
	bits32 *retSectionIx, bits32 *retBlockCount, unsigned char **retMd5)
/* Parse out the specific parts of a section query message. */
{
char *data = m->data;
*retFileId = getLongData(&data);
*retSectionIx = getLongData(&data);
*retBlockCount = getLongData(&data);
*retMd5 = (unsigned char*)data;
}

void bdMakeMissingBlocksMessage(struct bdMessage *m, bits32 machine,
	bits32 messageId, bits32 fileId,
	bits32 missingCount, bits32 *missingList)
/* Create a message listing missing blocks. */
{
char *data = m->data;
int i;
assert(missingCount <= bdSectionBlocks);
bdInitMessage(m, machine, messageId, bdmMissingBlocks, (missingCount+2)*sizeof(bits32));
data = addLongData(data, fileId);
data = addLongData(data, missingCount);
for (i=0; i<missingCount; ++i)
    {
    data = addLongData(data, missingList[i]);
    }
}

void bdParseMissingBlocksMessage(struct bdMessage *m, bits32 *retFileId, 
	bits32 *retMissingCount, bits32 **retMissingList)
/* Parse out the specific parts of message listing missing blocks. */
{
char *data = m->data;
bits32 *ml;
int i, missingCount;
*retFileId = getLongData(&data);
missingCount = *retMissingCount = getLongData(&data);
*retMissingList = ml = (bits32 *)data;
for (i=0; i<missingCount; ++i)
    {
    ml[i] = ntohl(ml[i]);
    }
}

void bdMakeSectionDoneMessage(struct bdMessage *m, bits32 machine,
	bits32 messageId, bits32 fileId, bits32 sectionIx, bits32 blockCount)
/* Send message saying section is done. */
{
char *data = m->data;
bdInitMessage(m, machine, messageId, bdmSectionDone, 3*sizeof(bits32));
data = addLongData(data, fileId);
data = addLongData(data, sectionIx);
data = addLongData(data, blockCount);
}

void bdParseSectionDoneMessage(struct bdMessage *m, bits32 *retFileId,
	bits32 *retSectionIx, bits32 *retBlockCount)
/* Parse out section done message. */
{
char *data = m->data;
*retFileId = getLongData(&data);
*retSectionIx = getLongData(&data);
*retBlockCount = getLongData(&data);
}
