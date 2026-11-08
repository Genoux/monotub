# AGENTS.md

Notes on why monotub is built the way it is. Record what was measured or
debugged, and the mistake each decision guards against. The shared rules
(git, commits, releases, local-only demos) are in jb-toolkit's `RULES.md` and
are not repeated here; anything below that departs from them says why.

Read `PRODUCT.md` first.

## Stack

- none — why: _to fill in when the stack is chosen_.

## Layout

- `scripts/check` — format, lint, types, tests. CI runs exactly this.

## Decisions

<!-- - YYYY-MM-DD: decision. Measured: number, how. Guards against: mistake. -->

## Project rules

<!-- Rules that apply only here, or a deliberate exception to RULES.md with the reason. -->
