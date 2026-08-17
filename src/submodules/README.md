# submodules

Submodules used by the UCSC Browser source are cloned here.
`src/makefile` invokes `src/submoduleSetup`, which runs
`git submodule update --init --recursive` when needed, configures
zlib-ng, and errors out if a stale `src/htslib` directory is still
present.

## htslib

Fork of https://github.com/samtools/htslib. The only UCSC patch
is a CRAM reference existence check that returns a status instead
of triggering a download. The browser drops a marker file and
informs the user to check back later; a cron job downloads the
reference asynchronously.

The remained of UCSC support, include UDC file access, goes through the
standard htslib interfaces.

Canonical repo: `/data/git/htslib.git` (UCSC internal).
Mirror: https://github.com/ucscgenomebrowser (htslib repo).

UCSC changes live on branch `ucsc-browser-support`, rebased onto
each upstream release. The branch point is marked with a tag of
the form `ucsc-browser-support_<upstream-version>`, e.g.
`ucsc-browser-support_1.23.1`.

### Updating to a new htslib release

1. In the submodule: fetch upstream, rebase `ucsc-browser-support`
   onto the new upstream tag.
2. Tag `ucsc-browser-support_<ver>` and push the branch and tag.
3. In kent: bump the submodule pointer and commit.

## zlib-ng

Mirror of https://github.com/zlib-ng/zlib-ng, currently at release
2.3.3. There are no UCSC patches.

zlib-ng is a drop-in replacement for zlib. We build it with
`--zlib-compat`, so the header and the symbol names are the ordinary
zlib ones and nothing that calls zlib has to change, and with
`--static`, so only `libz.a` is built. `inc/common.mk` points `ZLIB`
at `submodules/zlib-ng/libz.a`, and every binary links it statically,
the same way htslib is linked.

It is worth the extra dependency because it is much faster at the two
things the browser does most with zlib. Writing the hgTracks PNG is
about three times faster, and reading a bigBed data block about twice
as fast. Measured over the eight Recommended Track Set pages, hgTracks
uses 29 percent less processor time and 26 percent less wall clock,
with a pixel-identical image. See Redmine #38125.

`configure` compiles every SIMD variant and chooses at run time, so
one build covers the AVX-512 and AVX2 machines, and on ARM it picks up
NEON and the ARMv8 CRC instructions. There is no need for a per-machine
build. `submoduleSetup` runs `configure` once, serially, because the
parallel build must not race on generating the Makefile.

Do not replace this with a system zlib-ng package. The EPEL package
builds the *native* zlib-ng API, with `zng_` prefixed symbols and a
`zlib-ng.h` header, which is not a drop-in, and it ships no static
library.

Canonical repo: `/data/git/zlib-ng.git` (UCSC internal).

The `ucsc-browser-support` branch sits on the upstream release tag, and
the branch point is marked `ucsc-browser-support_<upstream-version>`,
e.g. `ucsc-browser-support_2.3.3`, the same convention as htslib. The
branch exists so a patch has somewhere to live if we ever need one.

### Updating to a new zlib-ng release

1. In the submodule: fetch upstream, move `ucsc-browser-support`
   to the new upstream release tag.
2. Tag `ucsc-browser-support_<ver>` and push the branch and tag.
3. In kent: bump the submodule pointer and commit.
