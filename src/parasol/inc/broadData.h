/* broadData - stuff to help with data broadcasting. */

#ifndef BROADDATA_H
#define BROADDATA_H

/* Files are transmitted a block at a time.  This block and surrounding
 * data has to be less than 64k to fit in a datagram.
 *
 * Blocks are grouped together into sections of 8k blocks.  After a section
 * is transmitted then the round robin is polled to see what if any
 * blocks need to be retransmitted.  The system does not go on to the
 * next section until either each nodes on the round robin has
 * indicated full reciept of it, or the node has been marked as dead.
 */

#define bdBlockSize (1024)	        /* The block size of file data to send. */
#define bdSectionBlocks (bdBlockSize/4)	/* The number of blocks in a section. */
#define bdSectionBytes (bdBlockSize * bdSectionSize) /* Number of bytes in a section */

enum bdMessageTypes 
/* Defines various types of messages. */
    {
    bdmAck = 0,		/* Acknowledge a message */
    bdmQuit = 1,	/* Tell node daemon to quit. */
    bdmFileOpen = 2,	/* Starting to transmit a file. */
    bdmBlock = 3,	/* Transmit part of a file. */
    bdmFileClose = 4,	/* Finish transmitting file. */
    bdmPing = 5,	/* Ask node daemon to show it's alive. */
    bdmSectionQuery = 6,  /* Ask node about status of file section */
    bdmMissingBlocks = 7, /* Reply about file section status */
    };


#define bdMaxDataSize (bdBlockSize + 3*sizeof(bits32))
#define bdGood 0
#define bdHubInPort 17780	/* Default input port for hub (output for node) */
#define bdNodeInPort 17781	/* Default input port for node (input for hub) */

struct bdMessage
/* The shared header for all messages */
    {
    in_addr_t machine;	/* Machine hub is specifically talking to with this message. 
			 * For broadcast messages the machine that should send a
			 * response.  For a response the originating machine. */
    bits32 id;		/* Message id. */
    bits16 type;	/* One of the bdMessageTypes above. */
    bits16 size;	/* Size of the data part of this message */
    char data[bdMaxDataSize];  /* The data part of message. */
    };

#define bdHeaderSize 12

int bdSendTo(int sd, struct bdMessage *m, bits32 dest, int port);
/* Send message out socket.  Returns error code if any or 0 for success. */

int bdReceive(int sd, struct bdMessage *m, bits32 *retSource);
/* Get message from socket.  Data should be maxDataSize long. 
 * Returns error code if any or 0 (bdGood) for success. */

off_t bdBlockOffset(bits32 sectionIx, bits32 blockIx);
/* Find start byte position of this block in this section. */

void bdInitMessage(struct bdMessage *m, bits32 machine, bits32 messageId, bits16 type, 
	bits16 dataSize);
/* Initialize a message. */

void bdMakeAckMessage(struct bdMessage *m, bits32 machine, bits32 messageId, signed32 err);
/* Create acknowledgement message. */

void bdParseAckMessage(struct bdMessage *m, signed32 *retErr);
/* Parse an acknowledgement message. */
	
void bdMakeFileOpenMessage(struct bdMessage *m, bits32 machine, bits32 messageId, 
	bits32 fileId, char *fileName);
/* Create a file open message. */

void bdParseFileOpenMessage(struct bdMessage *m, bits32 *retFileId, char **retFileName);
/* Parse out the specific parts of a file open message. */

void bdMakeBlockMessage(struct bdMessage *m, bits32 machine, bits32 messageId, 
	bits32 fileId, bits32 sectionIx, bits32 blockIx, 
	int size, void *fileData);
/* Create a message describing part of a file. */

void bdParseBlockMessage(struct bdMessage *m, bits32 *retFileId,
	bits32 *retSectionIx, bits32 *retBlockIx, int *retSize, void **retFileData);
/* Parse out the specific parts of a file open message. */

void bdMakeFileCloseMessage(struct bdMessage *m, bits32 machine, bits32 messageId, 
	bits32 fileId, char *fileName);
/* Create a file open message. */

void bdParseFileCloseMessage(struct bdMessage *m, bits32 *retFileId, char **retFileName);
/* Parse out the specific parts of a file open message. */

void bdMakeSectionQueryMessage(struct bdMessage *m, bits32 machine,
	bits32 messageId, bits32 fileId,
	bits32 sectionIx, bits32 blockCount);
/* Create a message to query about status of file section.  Block count should
 * be bdSectionSize or less, and firstBlock should start on a section boundary. */

void bdParseSectionQueryMessage(struct bdMessage *m, bits32 *retFileId,
	bits32 *retSectionIx, bits32 *retBlockCount);
/* Parse out the specific parts of a section query message. */

void bdMakeMissingBlocksMessage(struct bdMessage *m, bits32 machine,
	bits32 messageId, bits32 fileId,
	bits32 missingCount, bits32 *missingList);
/* Create a message listing missing blocks. */

void bdParseMissingBlocksMessage(struct bdMessage *m, bits32 *retFileId, 
	bits32 *retMissingCount, bits32 **retMissingList);
/* Parse out the specific parts of message listing missing blocks. */

#endif /* BROADDATA_H */

