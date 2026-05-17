# trackDbCacheBench

Benchmark `hgTracks` rendering with and without the trackDb cache, and
compare different cache directories (e.g. tmpfs vs disk-backed).

## What it measures

For each cache mode (`cached`, `no-cache`) the script does `--warmup`
discarded `hgTracks` runs followed by `--iterations` timed runs. Per run
it captures:

- **wall time** measured around the `hgTracks` subprocess
- **internal time** parsed from the `<span class='timing'>Overall total time:
  NNN millis</span>` footer when `measureTiming=1` is in the CGI args (the
  default scenario sets this)

Results print as min / median / mean / max per mode, plus a
`cached/no-cache` ratio.

## How the cache is toggled

The script sets `CACHE_TRACK_DB_DIR` in the child environment. The matching
piece of C is `trackDbCacheOn()` in `src/hg/lib/trackDbCache.c`: if that
environment variable is set (even to the empty string), its value
overrides the `cacheTrackDbDir` setting in `hg.conf`. Empty value disables
caching for that run; a non-empty value names the cache directory.

## hg.conf

By default the script picks up the `hg.conf` next to the `hgTracks` binary
(typically `/usr/local/apache/cgi-bin-$USER/hg.conf`), which is what apache
actually serves. Override with `--hg-conf`. Letting it default to your
personal `~/.hg.conf` is usually a bad idea -- if that points
`db.trackDb` at a personal trackDb table the cold trackDb load can be
orders of magnitude slower than what production sees.

## Usage

    trackDbCacheBench.py CACHE_DIR [options]

Common runs:

    # default scenario: hg38, chr1:1-1000000, hgt.trackImgOnly=1
    trackDbCacheBench.py /dev/shm/myCache

    # cached mode only, with the cache forcibly emptied first (cold rebuild)
    trackDbCacheBench.py /dev/shm/myCache --mode cached --clear-cache-before

    # custom CGI scenario
    trackDbCacheBench.py /dev/shm/myCache \
        --cgi db=hg38 --cgi position=chr12:5000000-7000000 \
        --cgi hgt.trackImgOnly=1 --cgi measureTiming=1

    # cold-from-disk: fsync + posix_fadvise(DONTNEED) on cache files before
    # each iteration; isolates real I/O cost of the cache directory
    trackDbCacheBench.py /data/myCache --evict-cache --mode cached

`--evict-cache` only meaningfully affects disk-backed cache directories.
On tmpfs (`/dev/shm`) it is essentially a no-op because tmpfs is the page
cache -- there is no underlying storage to fault back in from.
