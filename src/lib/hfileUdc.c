/* hfileUdc -- hfile backend that routes HTTP/HTTPS/FTP through UDC.
 *
 * Registers as an hfile scheme handler at priority 60 (above libcurl's 50),
 * so all remote file access via htslib automatically uses UDC caching.
 *
 * Replaces the legacy knetUdc.c / knetfile hook approach.
 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "udc.h"
#include "hfileUdc.h"

#include "htslib/hfile.h"
#include "hfile_internal.h"

#include <errno.h>

typedef struct {
    hFILE base;              /* must be first */
    struct udcFile *udc;     /* UDC file handle */
    bits64 fileSize;         /* cached file size for SEEK_END */
} hFILE_udc;

/* --- backend operations --- */

static ssize_t udc_read(hFILE *fpv, void *buffer, size_t nbytes)
{
hFILE_udc *fp = (hFILE_udc *) fpv;
bits64 n = udcRead(fp->udc, buffer, (bits64)nbytes);
return (ssize_t)n;
}

static ssize_t udc_write(hFILE *fpv, const void *buffer, size_t nbytes)
{
(void)fpv; (void)buffer; (void)nbytes;
errno = EROFS;
return -1;
}

static off_t udc_seek(hFILE *fpv, off_t offset, int whence)
{
hFILE_udc *fp = (hFILE_udc *) fpv;
bits64 abspos;

switch (whence)
    {
    case SEEK_SET:
        abspos = offset;
        break;
    case SEEK_CUR:
        abspos = udcTell(fp->udc) + offset;
        break;
    case SEEK_END:
        abspos = fp->fileSize + offset;
        break;
    default:
        errno = EINVAL;
        return -1;
    }

udcSeek(fp->udc, abspos);
return (off_t)udcTell(fp->udc);
}

static int udc_close(hFILE *fpv)
{
hFILE_udc *fp = (hFILE_udc *) fpv;
udcFileClose(&fp->udc);
return 0;
}

static const struct hFILE_backend udc_backend =
{
    udc_read, udc_write, udc_seek, NULL, udc_close
};

/* --- scheme handler open --- */

static hFILE *hopen_udc(const char *url, const char *mode)
{
if (!(sameOk((char *)mode, "r") || sameOk((char *)mode, "rb")))
    {
    errno = EROFS;
    return NULL;
    }

struct udcFile *udcf = udcFileMayOpen((char *)url, NULL);
if (udcf == NULL)
    {
    errno = ENOENT;
    return NULL;
    }

hFILE_udc *fp = (hFILE_udc *) hfile_init(sizeof(hFILE_udc), mode, 0);
if (fp == NULL)
    {
    udcFileClose(&udcf);
    return NULL;
    }

fp->udc = udcf;
fp->fileSize = udcFileOpenSize(udcf);
fp->base.backend = &udc_backend;
return &fp->base;
}

static const struct hFILE_scheme_handler udc_handler =
{
    hopen_udc, hfile_always_remote, "UCSC UDC", 60
};

/* --- public registration --- */

void hfileUdcInstall()
/* Install UDC as the hfile backend for HTTP/HTTPS/FTP URLs.
 * Call once at startup; replaces legacy knetUdcInstall(). */
{
hfile_add_scheme_handler("http",  &udc_handler);
hfile_add_scheme_handler("https", &udc_handler);
hfile_add_scheme_handler("ftp",   &udc_handler);
}