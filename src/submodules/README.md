# submodules

Submodules used by the UCSC Browser source are cloned here.
`src/makefile` invokes `src/submoduleSetup`, which runs
`git submodule update --init --recursive` when needed and
errors out if a stale `src/htslib` directory is still present.

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
