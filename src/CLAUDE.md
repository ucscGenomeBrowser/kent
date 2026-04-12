# CLAUDE.md — UCSC Genome Browser Kent Source Tree

AI assistant guidelines for modifying the kent codebase.

## Key Rules

- Always search `src/inc/` and `src/lib/` for existing utility functions before writing inline parsing, conversion, or data manipulation code. The kent tree has 222+ headers covering most common operations (e.g., `htmlColor.h` for color parsing, `obscure.h` for misc utilities, `hash.h` for hash tables). Writing a new implementation when one exists leads to code review failures.
- Use `sqlSafef()` or `sqlDyStringPrintf()` for ALL SQL query construction — never `safef()` or `dyStringPrintf()`.
- Use `safef()`, `safecpy()`, `safecat()` instead of `sprintf()`, `strcpy()`, `strcat()`.
- Use `needMem()` / `AllocVar()` instead of `malloc()` — all kent code assumes zeroed memory.
- Struct `next` must be the first member for any struct used in singly-linked lists.
- Make the smallest change that achieves the goal. Do not restructure surrounding code.
- Preserve existing patterns even when a "cleaner" design is conceivable.
- After building, run binaries from `~/bin/x86_64/`, not bare command name (system PATH resolves to production binaries).
- Never use inline event handlers (`onclick`, `onchange`, `oninput`) in HTML output — CSP blocks them. Use `jsInlineF()` with `addEventListener` instead, which emits JavaScript inside a nonce-tagged script block.
- Deploy CSS and static files by running `make` in the appropriate `htdocs/` subdirectory (e.g., `cd src/hg/htdocs/style && make`). Never copy files directly to `/usr/local/apache/`.
