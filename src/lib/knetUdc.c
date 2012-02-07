/* knetUdc -- install udc i/o functions in knetfile interface in samtools. */
/* As of 2/23/10, the KNETFILE_HOOKS extension is a UCSC-local modification of samtools. */

#if (defined USE_BAM && defined KNETFILE_HOOKS)

#include "common.h"
#include "udc.h"
#include "knetUdc.h"
#include "knetfile.h"


struct knetFile_s {
    struct udcFile *udcf;
}; // typedef'd to knetFile in knetfile.h

static char *udcCacheDir = NULL;

static knetFile *kuOpen(const char *filename, const char *mode)
/* Open the given filename with mode which must be "r". */
{
if (!sameOk((char *)mode, "r"))
    errAbort("mode passed to kuOpen must be 'r' not '%s'", mode);
struct udcFile *udcf = udcFileMayOpen((char *)filename, udcCacheDir);
if (udcf == NULL)
    return NULL;
knetFile *kf = NULL;
AllocVar(kf);
kf->udcf = udcf;
verbose(2, "kuOpen: returning %lu\n", (unsigned long)(kf->udcf));
return kf;
}

static knetFile *kuDopen(int fd, const char *mode)
/* Open from a file descriptor -- not necessary for our use of samtools. */
{
errAbort("kuDopen not implemented");
return NULL;
}

static off_t kuRead(knetFile *fp, void *buf, off_t len)
/* Read len bytes into buf, return amount actually read. */
{
verbose(2, "udcRead(%lu, buf, %lld)\n", (unsigned long)(fp->udcf), (long long)len);
return (off_t)udcRead(fp->udcf, buf, (int)len);
}

static off_t kuSeek(knetFile *fp, int64_t off, int whence)
/* Seek to off according to whence (but don't waste time with samtools' SEEK_END to
 * check empty record at end of file.  Don't be fooled by the off_t return type --
 * it's 0 for OK, non-0 for fail. */
{
bits64 offset;
if (whence == SEEK_SET)
    offset = off;
else if (whence == SEEK_CUR)
    offset = off+ udcTell(fp->udcf);
else
    return -1;
verbose(2, "udcSeek(%lu, %lld)\n", (unsigned long)(fp->udcf), offset);
udcSeek(fp->udcf, offset);
return 0;
}

static off_t kuTell(knetFile *fp)
/* Tell current offset in file. */
{
verbose(2, "udcTell(%lu)\n", (unsigned long)(fp->udcf));
return udcTell(fp->udcf);
}

static int kuClose(knetFile *fp)
/* Close and free fp->udcf. */
{
verbose(2, "udcClose(%lu)\n", (unsigned long)(fp->udcf));
udcFileClose(&(fp->udcf));
return 0;
}

void knetUdcInstall()
/* install udc i/o functions in knetfile interface in Heng Li's samtools lib. */
{
// maybe init udcCacheDir from hg.conf?
knet_init_alt(kuOpen, kuDopen, kuRead, kuSeek, kuTell, kuClose);
}

#endif//def USE_BAM && KNETFILE_HOOKS
