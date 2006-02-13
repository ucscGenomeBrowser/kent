/****************************************************
** 
** FILE:   	jp2Dec.c  (derived from ecw jp2 sdk example4.c)
** CREATED:	2006-02-10
** AUTHOR: 	Galt Barber
**
** PURPOSE:	Decode jp2 file (e.g. from Allen Brain Atlas) one row at a time through callback. 
**              It is just set up to open and read one jp2 file once.  Returns width and height.
**              Uses a 24-bit RGB view on the jp2.
**
*******************************************************/

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "NCSECWClient.h"
#include "NCSErrors.h"

#include "jp2Dec.h"

/*----------------------- */
/*
** Define a basic struct to hold our FILE pointer
*/
typedef struct {
	FILE *pFile;
} MyDataStruct;

/*
** Open File CB - open the file, fill in MyDataStruct
*/
static NCSError NCS_CALL FileOpenCB(char *szFileName, void **ppClientData)
{
	MyDataStruct *pData;
	if(NULL != (pData = (MyDataStruct*)malloc(sizeof(MyDataStruct)))) {
		pData->pFile = fopen(szFileName, "rb");
		if(pData->pFile) {
			*ppClientData = (void*)pData;
			return(NCS_SUCCESS);
		}
		return(NCS_FILE_OPEN_FAILED);
	}
	return(NCS_COULDNT_ALLOC_MEMORY);
}

/*
** Close File CB - close file, free MyDataStruct
*/
static NCSError NCS_CALL FileCloseCB(void *pClientData)
{
	MyDataStruct *pData = (MyDataStruct*)pClientData;
	if(pData) {
		if(pData->pFile) {
			fclose(pData->pFile);
		}
		free((void*)pData);
		return(NCS_SUCCESS);
	}
	return(NCS_INVALID_PARAMETER);
}

/*
** Read File CB - read given length from current offset into buffer
*/
static NCSError NCS_CALL FileReadCB(void *pClientData, void *pBuffer, UINT32 nLength)
{
	MyDataStruct *pData = (MyDataStruct*)pClientData;
	if(pData && pData->pFile) {
		size_t nRead;
		nRead = fread(pBuffer, 1, nLength, pData->pFile);
		if(nRead == nLength) {
			return(NCS_SUCCESS);
		}
		return(NCS_FILEIO_ERROR);
	}
	return(NCS_INVALID_PARAMETER);
}

/*
** Seek File CB - seek file to given offset
*/
static NCSError NCS_CALL FileSeekCB(void *pClientData, UINT64 nOffset)
{
	MyDataStruct *pData = (MyDataStruct*)pClientData;
	if(pData && pData->pFile) {
		int nRet;
		nRet = fseek(pData->pFile, (long)nOffset, SEEK_SET);
		if(nRet == 0) {
			return(NCS_SUCCESS);
		}
		return(NCS_FILEIO_ERROR);
	}
	return(NCS_INVALID_PARAMETER);
}

/*
** Tell File CB - get the current file offset
*/
static NCSError NCS_CALL FileTellCB(void *pClientData, UINT64 *pOffset) 
{
	MyDataStruct *pData = (MyDataStruct*)pClientData;
	if(pData && pData->pFile && pOffset) {
		long nOffset;
		nOffset = ftell(pData->pFile);
		if(nOffset >= 0) {
			*pOffset = (UINT64)nOffset;
			return(NCS_SUCCESS);
		}
		// Error
		*pOffset = 0;
		if(errno == EBADF || errno == EINVAL) {
			return(NCS_INVALID_PARAMETER);
		} 
		return(NCS_FILEIO_ERROR);
	}
	return(NCS_INVALID_PARAMETER);	
}


NCSFileView *pNCSFileView;
NCSFileViewFileInfo	*pNCSFileInfo;
NCSError eError = NCS_SUCCESS;

UINT32 Bands[3] = { 0, 1, 2 };
INT32 nBands;
UINT8 *pRGBTriplets;
INT32 nTLX, nTLY;
INT32 nBRX, nBRY;
INT32 nWidth, nHeight;

static void jp2ErrAbort(char *errMsg)
/* print err msg to stderr and exit with non-zero exit code */
{
/* Abort on error */
fprintf(stderr,"%s",errMsg);
exit(1);
}

void jp2DecInit(char *jp2Path, int *retWidth, int *retHeight)
/* initialize jp2 decoder */
{

/*
**  Initialize library explicitly in case of static linkage
*/
NCSecwInit();

/*
** Set up the callbacks
*/
NCSecwSetIOCallbacks(FileOpenCB, FileCloseCB, FileReadCB, FileSeekCB, FileTellCB);
/*
**	Open the input NCSFileView
*/
eError = NCScbmOpenFileView(jp2Path, &pNCSFileView, NULL);

if(eError != NCS_SUCCESS) 
    jp2ErrAbort("Error opening jp2 file view.\n");

/* Get the file info (width, height etc) */
NCScbmGetViewFileInfo(pNCSFileView, &pNCSFileInfo);

/* 3 bands only RGB */
nBands = 3; /* pNCSFileInfo->nBands */

/* Setup view dimensions */
nTLX = 0;
nTLY = 0;
nBRX = pNCSFileInfo->nSizeX - 1;
nBRY = pNCSFileInfo->nSizeY - 1;
nWidth = (INT32)pNCSFileInfo->nSizeX;
nHeight = (INT32)pNCSFileInfo->nSizeY;

*retWidth = nWidth;
*retHeight = nHeight;

/* Allocate scanline RGB triplet buffer */
pRGBTriplets = (UINT8 *) malloc(nWidth*3);		/* RGB triplet buffer */

if(!pRGBTriplets) 
    jp2ErrAbort("Error allocating pRGBTriplets memory buffer.\n");
   
/* Set the view, using dimensions calculated above */
eError = NCScbmSetFileView(pNCSFileView, 
		nBands, Bands,
		nTLX, nTLY, 
		nBRX, nBRY,
		nWidth, nHeight);

if(eError != NCS_SUCCESS) 
    jp2ErrAbort("Error setting file view params.\n");

}

unsigned char *jp2ReadScanline()
/* read the next RGB scanline */
{
NCSEcwReadStatus eStatus = NCScbmReadViewLineRGB(pNCSFileView, pRGBTriplets);
if(eStatus != NCSECW_READ_OK) 
    {
    jp2ErrAbort("Error decoding jp2.\n");
    }
return (unsigned char *)pRGBTriplets;    
}

void jp2Destroy()
/* close all resources */
{
free(pRGBTriplets);
NCScbmCloseFileView(pNCSFileView);
NCSecwShutdown();
}

