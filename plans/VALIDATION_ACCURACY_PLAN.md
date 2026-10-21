# Validation Accuracy Plan

Date: 2026-10-15

Status: planned; no slice started. Updated 2026-10-21 with GitHub issues #49 and #50, help linking, bracketed
values, Therion's line splitting (`thsplit_args`) first, and slices renumbered; (p) marks potential slices.

Scope: remove false project-validation findings so that the Validation panel can be trusted, starting with the
classes measured on a real multi-survey cave project, with tests that stop the same classes from returning.

## Why

On a local real cave project (about 180 source files, one root `thconfig` and several per-survey configs), project
validation reported 243 findings. Checked against Therion itself and the source files, about 231 were false and four
causes explain nearly all of them. Users stop reading a panel with that much noise, so real problems are missed.

## Measured Findings

| Findings | Verdict | Cause |
| --- | --- | --- |
| 59 warnings `unknown-option`, all `export ... -proj` | false | The command catalog is generated from the Therion book, which documents `-projection` only. Therion's option table (`thtt_expmap_opt` in `src/therion-core/thexpmap.h`) accepts both `-proj` and `-projection`; `-pro` is rejected, so this is an alias, not prefix matching. The same `export` catalog entry also lists Therion command-line switches (`-d`, `-q`, `-L`, `--print-symbols`, ...) that are not export options. |
| 155 errors `unknown-station-reference` "file is not included in the project source graph" | false as errors | Every one is a station point in a drawing the selected target config does not read. Some of those drawings belong to separate per-survey configs in the same folder (their `.th` inputs them; they are only commented out of the main survey); others are not read by any config. Each station of such a drawing is reported as an error. |
| 8 findings in the root `thconfig` (unclosed `layout`, unclosed `survey`, unmatched `endlayout`, 4 `invalid-command-context`) | false, one cause | A quoted `map-comment "...<br>\` value continued over several lines with `\`. The source model breaks the quoted value, so a continued line starting `Survey by ...` is read as a `survey` command and the rest follows. Therion accepts the file. |
| 9 errors `unknown-join-reference` to objects in a `-namespace off` survey | false | Already fixed on the separate `NamespaceOffBug` branch (objects in a `-namespace off` survey resolve through the survey name); waiting for its PR. |
| 1 error `missing-source-reference` | real | A per-survey config sources a file name that does not exist (a misspelling); that config would fail in Therion. |
| 12 warnings `empty-scrap-object` (`line ...` immediately followed by `endline`) | probably real | Harmless empty leftovers; a warning is appropriate. |

To collect the same report again, open the project and use `Export Markdown...` in the Validation panel (or a
scratch tool around `ProjectValidationScanner`; one is kept outside the repository).

## Principles

- Therion is the referee. A finding is an error only when Therion itself rejects the input. Therion warnings, and
  input that is very likely a mistake, are warnings. Style or heuristic notes are at most informational.
- Metadata comes from Therion's own sources. Fix catalog gaps in the generator, not by editing generated catalog
  output or adding long-lived overrides (see `AGENTS.md`, "Data Integrity Rules").
- All slices are on one branch, `ValidationAccuracy` from `main`, one commit (or a short series) per slice in number
  order, because later slices build on earlier ones (V2's checks read values through V1's splitting). Each slice
  has focused tests built from small repository fixtures and its own documentation updates (specification,
  compatibility notes, manuals, translations, WORKLOG); no local cave data in the repository, commit messages, or
  WORKLOG.
- Parser and source-model changes are high risk: they need round-trip tests and must fix the shared model, not one
  projection.

## V1 - Line Splitting Matches Therion's `thsplit_args`

Goal: Therion Studio splits a command line into values exactly as Therion does, so every later check and test reads
the same values Therion reads. GitHub issue #49 (bracketed label text).

Therion splits every command line in `thsplit_args` (`src/therion-core/thparse.cxx`, called from `thinput.cxx`) before
any keyword lookup. Its rules:

- a plain word ends at a space; `#` at the start of a word starts a comment;
- `"..."` is one value; `""` inside it is a literal quote;
- `[...]` is one value with the brackets removed; brackets nest, so `[a [b] c]` stays one value;
- a quote or bracket inside a plain word does not start a quoted value;
- an unclosed quoted or bracketed value runs to the end of the line;
- single quotes are ordinary characters.

Studio does not call or link `thsplit_args`; it returns values without their source positions and with brackets and
quotes removed, works on byte strings, and would bring in Therion's core. Studio's own tokenizer follows the same
rules, written from Therion's source; the Therion version they were taken from is recorded in
`docs/THERION_COMPATIBILITY.md`, and the table-driven test below keeps them in step. A later change in Therion is not
caught automatically (V7 would catch it).

Today the shared tokenizer (`TherionDocumentParser::tokenizeLine`) handles quotes but not brackets, so
`[Main<br>Chamber]` becomes separate tokens. Several callers rejoin them, each in its own way
(`TherionCommandLineModel`, `TherionDocumentEditor`, `TherionTokenRules`, and others); the map inspector shows only
`[Main` and editing can fail. It also treats single quotes as quoting, which Therion does not; `-text Dan's` may be
read wrongly (not yet confirmed).

- Make the tokenizer follow `thsplit_args`, returning each value as one token with its source span, then remove the
  separate rejoining code so every projection uses the one rule. Check the uses where brackets carry several numbers
  (for example scrap `-scale [...]` and `-sketch` style values) keep their numbers available.
- Editing keeps the user's form: a bracketed label stays bracketed when edited, and is not turned into quotes.
- Tests: one table-driven test with a case for each rule above (input line, expected values); round-trip of old files
  with bracketed labels (text unchanged); a bracketed multi-word label with `<br>` shows in full in the inspector and
  edits without error; existing parser and highlighting tests unchanged unless a rule above shows them wrong (then
  with the evidence and approval, as `AGENTS.md` asks).
- First, because V2's tests and checks read option values through this splitting.
- Documentation: `SPECIFICATION.md` (line splitting follows Therion; bracketed values keep their form when edited);
  `docs/THERION_COMPATIBILITY.md` (the rules above, citing `thsplit_args`). Check the user manuals' label editing text
  and update it in all four languages if it describes quoting.

## V2 - Catalog Options From Therion's Option Tables

Goal: option names, aliases, whether an option takes a value, and allowed values match what Therion accepts. GitHub
issue #50 (`-scale` rejected for labels).

- Extend `scripts/generate_therion_catalog.py` to read Therion's `thstok` tables (for example `thtt_expmap_opt`,
  `thtt_expmap_fmt` in `thexpmap.h`, and the equivalent tables for the other commands) from the same Therion checkout
  it already reads the book from, and merge them with the book-derived metadata: the tables give the exact option
  and value tokens; the book still gives summaries, syntax, and contexts.
- The generator reads Therion's source files as text when it is run; Therion code is not called or linked, and the app
  reads only the generated catalog. The Therion version is recorded in `docs/therion-source.lock` with the book files,
  and new Therion options reach Studio only when the generator is run again.
- Value arity comes from Therion, not from the book's signature. Today an option whose book heading shows no
  placeholder is generated as taking no value: `point` `-align`, `-scale`, `-text`, `-value` and `line` `-scale`.
  `Edit All Object Settings` then rejects `-scale l` (#50) and treats a multi-word `-text` as stray tokens (the other
  half of #49). Where the tables give only names, read the arity from the option's parsing code in Therion (for
  example `thpoint.cxx` `set`), or record it in the generator with a comment pointing to that code.
- Treat options that the tables do not list for a command as suspect; this removes the command-line switches that were
  attached to `export`.
- Help stays linked to the book. Option entries are matched to book text by option name; an alias (for example
  `-proj`) shows the help of its full name (`-projection`). An option Therion accepts but the book does not describe
  is still accepted and shows a short "not described in the Therion book" note.
- Have the generator write a report of differences between the book and the tables (options or aliases in one but not
  the other, and options whose book heading omits their value).
- One catalog serves every user of it (validation, highlighting, completion, context help, Blocks, the map inspector,
  and `Edit All Object Settings`), so this fixes all of them together; no per-surface option lists are added.
- Tests (no test per option):
  - one table-driven test that, for every command and option in the extracted tables, the generated catalog has the
    same name, aliases, and value arity;
  - generator unit tests on small extracted table and book snippets, including alias-to-help linking;
  - a catalog regression test that `export` accepts `-proj`, still accepts `-o`/`-output`, and no longer lists
    command-line switches, and that `point -scale l`, `-align`, and `-text` take values.
- Keep generated catalog updates in their own commit, separate from generator code (`AGENTS.md`).
- Documentation:
  - `SPECIFICATION.md`: replace "Catalog-derived zero-value option arity shall not by itself be treated as a validation
    error ..." with the rule that option names, aliases, and arity come from Therion's option tables, and describe the
    note shown for options the book does not describe;
  - `docs/THERION_COMPATIBILITY.md`, `docs/THERION_COMMAND_CATALOG_PIPELINE.md`, and
    `resources/therion_catalog/README.md`: the tables as the source of option metadata, the book as the source of
    help text, and the difference report;
  - user manuals (English, Czech, French, Slovak): context help for options the book does not describe;
  - translations for the new note.
- Upstream: send the book fixes from the difference report to the Therion project as a separate change (missing
  aliases such as `-proj`, option headings that omit their value such as `-scale`, `-text`, `-align`, `-value`).
  Therion Studio does not wait for it; help text improves as the book is fixed.

## V3 - Line Continuation Inside Quoted Values

Goal: a quoted value continued with a trailing `\` stays one value in every projection.

- Find where the shared source model (`TherionSourceText` / `TherionSourceLogicalDocument`) joins continued physical
  lines and why a quoted value spanning lines is split. Fix it there, not in validation, using V1's splitting rules.
- Tests: round-trip of a multi-line quoted `map-comment` in a `layout` block (text unchanged, one logical command,
  correct physical ranges); no `survey`/`endlayout` findings for that fixture; existing parser and highlighting tests
  unchanged.
- Documentation: `SPECIFICATION.md` (logical command lines keep a continued quoted value as one value) and
  `docs/THERION_COMPATIBILITY.md` (the continuation rule). No user manual change expected, as this is a bug fix.

## V4 - Drawings Outside The Target Config's Source Graph

Goal: no per-station errors for drawings the selected target config does not read.

- Do not validate station references in drawings outside the selected config's source graph.
- Instead report the drawing once, as an informational finding, for example "not read by the selected Target Config",
  so users who expect it to be read can see why it is not.
- Tests: a fixture with a root config, an unread drawing with station points, and a drawing read through `input`;
  only the unread drawing gets one informational finding; reachable drawings keep today's checks.
- Documentation: `SPECIFICATION.md` (project validation scope follows the target config, and the informational
  finding); `docs/THERION_COMPATIBILITY.md` (the target-config scope rule); user manuals' Validation section in all
  four languages; translations for the new finding text.

## V5 - Severity Policy

Goal: errors mean "Therion would fail".

`SPECIFICATION.md` lists what validation checks but has no rule for which findings are errors, warnings, or
informational. This slice is useful on its own: it gives one rule for every check, and applying it removes false
errors that remain after V1-V4. The referee test (V7) would only automate checking that the rule is kept.

- Write the policy (the first principle above) into `SPECIFICATION.md` next to the project validation requirements.
- Review every diagnostic code against it and adjust severities; list each change and its reason in the commit.
- Tests: the existing tests for each changed code are updated to the new severity only where the policy requires it
  (with the evidence, as `AGENTS.md` asks); a small fixture per changed class.
- Documentation: user manuals (all four languages) say what error, warning, and informational mean;
  `docs/THERION_COMPATIBILITY.md` where it names severities.

## V6 (p) - Validate Each Drawing Against The Config That Reads It

Potential, not scheduled. When another config in the project folder reads a drawing (for example a per-survey
`thconfig`), validate the drawing against that config instead of skipping it. This needs a project index per root
config (or a multi-root index), the rules for choosing the config when several read a file, and a performance check,
because each extra root multiplies index work. Revisit after V4, once the remaining false findings are known.

## V7 (p) - Therion Referee Test

Potential, not scheduled. A corpus test that runs Therion on small fixture projects and fails if Therion Studio
reports an error for a project Therion compiles without error. It would be a separate CTest executable (process and
Therion dependency) that skips with a clear message when Therion is not installed. This is a first step towards the
WORKLOG backlog item "Compiler-confirmed project-index comparison". Revisit after V5.

## Slices

| Slice | What | Issues |
| --- | --- | --- |
| V1 | Line splitting matches Therion's `thsplit_args` (brackets, quotes, comments) | #49 |
| V2 | Catalog options, aliases, and arity from Therion's `thstok` tables; help linked to the book | #50, #49 |
| V3 | Line continuation inside quoted values | |
| V4 | Drawings outside the target config's source graph | |
| V5 | Severity policy | |
| V6 (p) | Validate each drawing against the config that reads it | |
| V7 (p) | Therion referee test | |

Work in number order; (p) marks potential slices that are not scheduled. Each slice carries its own tests and
documentation, so the branch is complete after every slice.

Already done elsewhere: the `-namespace off` join references are fixed on the `NamespaceOffBug` branch.

## Open Questions

- Which Therion version should the generator read the tables from (the version already used for the book snapshot,
  or the installed Therion)? Prefer the same pinned checkout as the book (`docs/therion-source.lock`) so catalog and
  book stay consistent. V1's splitting rules should cite the same version.
- Should informational findings (V4) be visible by default, or behind a filter?
