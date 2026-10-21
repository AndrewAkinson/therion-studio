# Join Viewer Plan

Date: 2026-10-13

Branch: `JoinViewer`

Scope: add a `Joins` page to the project navigation sidebar that lists `equate` links between different surveys, and a
Join view that opens the two `.th2` drawings containing the equated stations side by side, aligned on the join station
through the centreline, each fully editable and saved independently, with a faded read-only outline of the partner
drawing in each pane.

Status: phase 1 implemented (slices 1–6 below). Phase 2 (options) agreed in outline on 2026-10-14; open questions are
listed in that section. Specification, architecture, user manual, worklog and translations are updated in the same
changes that implement each slice. Not archived until phase 2 is done.

## Current State

- The project navigation sidebar (`src/app/MainWindowSidebar.cpp`) hosts a segmented control `Files | Survey | Map`
  over a `QStackedWidget`; Survey and Map share one `QTreeView` that is re-parented between pages.
- `ProjectStructureIndex` already parses `equate` commands and resolves station references to survey namespaces
  (including `-namespace off` surveys) for validation diagnostics, but keeps no equate link data in
  `ProjectIndexSnapshot`.
- Map editors can center on a source line (`MapEditorTab::selectMapLine(line, centerOnSelection)`).
- A `.th2` file is open in at most one editor (attached tab or detached window, keyed by canonical path).
- `TherionXviParser` already reads XVI grid origin/vectors and station positions; `TherionBackgroundMetadata` already
  reads `##XTHERION## xth_me_image_insert` placement.

## Observations From Test Data (`CheddarCatchment/LongwoodValleySink`)

- Each survey has one `.th2` per projection (`*.plan.th2`, `*.extended.th2`, `*E315.th2`, ...). A join pairs drawings of
  the same projection.
- The `##XTHERION##` header lines carry editor state only (`xth_me_area_adjust` canvas extent, `xth_me_area_zoom_to`
  view zoom) and the background placement (`xth_me_image_insert`). The metric grid lives in the referenced XVI file
  (`set XVIgrids {1 m}`, `set XVIgrid {originX originY dx 0 0 dy nx ny}`), together with the centreline station
  positions (`set XVIstations`). 108 of 111 drawings reference an XVI.
- Scrap `-scale` is set on 203 of 212 scraps but is not reliable: Therion derives scrap scale/rotation from two or more
  station points matched to the centreline, so `-scale` is often absent or nominal. It is not used as the primary
  alignment source.
- Equates may link more than two stations (`equate 13 14@TennisBallRift 2@TickleCheck 2@TickleWest`) and may reference
  stations of a `-namespace off` survey from the parent (`equate 9 9@Triassic`). Equates inside one survey and
  commented-out equates are not joins.

## Alignment Method

Mirror Therion: place each drawing by matching its station points to centreline positions, then link the two drawings
at the equated station.

1. For each drawing, collect matched pairs: `point x y station -name N` (drawing coordinates) and the centreline
   position of `N` in metres.
2. Centreline positions, in order of preference:
   - the XVI referenced by `xth_me_image_insert` (the one matching most station names): XVI station positions
     converted to metres with the XVI grid (`XVIgrid` vector length per `XVIgrids` unit). The XVI placement offset
     is not needed because the fit maps drawing points straight onto XVI coordinates;
   - fallback when no usable XVI exists: scrap `-scale` (if set), translation-only at the join station;
   - otherwise the pair is listed but marked "no scale"; the panes open centred on the station without linking.
3. With two or more matched stations, fit a 2D similarity transform (scale, rotation, translation; least squares)
   from drawing coordinates to survey metres. With one station, use the grid scale and no rotation.
4. Each survey's XVI has its own origin, so the cross-survey offset is taken from the equated station: translate so
   the two join stations coincide.
5. Pane transform: `partnerToThis = inverse(fitThis) ∘ translateJoin ∘ fitPartner`. Pan/zoom in one pane is mapped
   through this transform to the other.

Notes:

- Exact for plan and fixed-angle elevation projections. Extended elevations are drawn independently per survey; the
  join station coincides but passage direction may differ.
- Later upgrade: phase 2 stage C4 places drawings from Therion-computed station positions (SQL database export).
- No external library: the fit is a closed-form 2D similarity. Core stays QtCore-only, so it uses a small
  `Th2SimilarityTransform` value type; the app layer converts it to `QTransform` for `QGraphicsView`.

## Sidebar: Joins Page

- A fourth segment `Joins` is added to the existing `Files | Survey | Map` control (same segmented style, last-segment
  property moves to `Joins`), backed by a new page in `structureViewStack_` and a new `StructurePanelJoinsPage` value.
- Page content, top to bottom:
  - projection selector (combo): projections found in the project, e.g. `plan`, `extended`, `elevation 315`;
    default `plan`; remembered per session;
  - filter field (survey or station text);
  - tree:
    - top level: one row per cross-survey `equate`, e.g. `0@BoldStep = 36@TridentLeft`; tooltip shows the
      source file and line of the `equate`;
    - two-station equates open directly on activation;
    - multi-station equates expand to one child row per survey pair;
    - rows are dimmed with a tooltip reason when either survey has no drawing for the selected projection or the
      station point is not drawn.
- Activation (double-click / Enter) opens the Join view. Context menu: `Open Join View`, `Go to Equate in Source`,
  `Open <survey> drawing` for each side.
- The Joins tree uses its own model and view (it is a flat/grouped list, not a projection of the Survey tree), filled
  from the same background project-index snapshot that drives Survey/Map, so it refreshes on save/edit without UI
  blocking.
- Structure remains navigation-only: unresolved equates stay reported in Validation, not in Joins.

## Join View (as built)

- Hosting: no join tab. The two drawings open as normal document tabs, and the user can show either map in the
  existing separate map window (`MapEditorTab::toggleMapPaneWindow`). Both editors therefore stay visible to save,
  close, and session handling, which a tab holding two editors would not be without a `MainWindow` refactor (46 code
  paths enumerate attached tabs or detached windows separately).
- `MapEditorTab` exposes a narrow view-linking API: view centre and scale in drawing units, `mapViewChanged`,
  `centrelineFitForSourceLine`, `stationPointSourcePosition`, outline path and source-to-scene transform, and adding a
  display-only overlay item.
- `MapEditorJoinViewLink` keeps both views on the same real point at the same real-world scale; views agree to within
  about 3 screen pixels because each view scrolls in whole pixels. A linked view widens its own scroll area so a join
  at a drawing edge can be centred.
- `MapEditorJoinOutline` shows the partner as a faded, display-only outline (lines and station points) placed through
  the join transform. It rebuilds after any partner source change (debounced 150 ms, because some map edits skip the
  scene refresh) and re-adds its item after host scene refreshes.
- `MapEditorJoinPairs` opens, lists, and closes a join (fit, outlines both ways, one link) and zooms both views to about
  30 m across the first view. A `Joined: <drawing>` status-bar indicator offers `Link Views`, `Show Outline`, and
  `Remove Join` per joined drawing. Changed 2026-10-15 after review: `Link Views` removed (joined drawings always move
  together), and the View menu entry renamed `Linked Drawings`.

## Changes From The Original Plan

- Join tab with a splitter replaced by normal tabs plus the existing separate map window (data-safety reason above).
- Faded outline implemented before wiring the link, at the user's suggestion; it made a two-editor host unnecessary.
- Alignment uses XVI station fits; scrap `-scale` is only a fallback (scale often unset; Therion fits from stations).
- Side fixes kept out of this branch: `fix/french-3d-inspector-strings`, `chore/refresh-translation-locations`, and
  `XVIrootStation` (`xth_me_image_insert` root station `0` was ignored, misplacing the XVI background).

## Implementation Slices

1. Core equate links: `ProjectEquateLink` data in `ProjectIndexSnapshot` (stations, resolved survey namespaces, source
   file/line), cross-survey filter, plus station-point lookup per survey/projection (`.th2` file, scrap, line).
   QTest cases: pairs, multi-station, `-namespace off`, same-survey exclusion, commented-out equates.
2. Core alignment: `Th2CentrelineAlignment` (pure functions): XVI grid scale, station matching, similarity fit,
   join transform. Extend `TherionXviParser` to read `XVIgrids` units if needed. QTest cases with fixtures taken from
   LongwoodValleySink.
3. Joins sidebar page (`ProjectJoinsPanel` over the pure `ProjectJoinList`), opening the two drawings centred on the
   station. Done.
4. View-linking API and `MapEditorJoinViewLink`. Done.
5. Faded partner outline (`MapEditorJoinOutline`). Done.
6. `Open Join` links the views (`MapEditorJoinPairs`) with the `Joined` indicator. Done.

Slices 1 and 2 are also done.

Each slice: update `SPECIFICATION.md`, `ARCHITECTURE.md` (UI surface ownership: Joins page), `docs/USER_MANUAL.md`,
`WORKLOG.md`, translation catalogs; run `scripts/check_structure_constraints.py`; stop for user review against
LongwoodValleySink.

## Phase 2: Options (requested 2026-10-14)

### A. Open one view in a separate window

- Default: activating a join (double-click / Enter) opens both drawings as tabs, as now.
- Joins row context menu gains `Open Join with Separate Window`: as `Open Join`, then shows the second drawing's map in
  the existing separate map window. Always the second (right-hand) drawing of the row; choosing a side is not worth
  the extra complexity.
- A Settings option reverses the two: double-click opens with a separate window, and the context menu offers
  `Open Join in Tabs`. Stored through `ISessionStore` like the other Settings options.

### B. Show that drawings are linked

- While a join's views are linked, both document tabs show a link icon (Lucide `link`, added to the bundled icon set
  under the existing Lucide licence notice). The separate map window shows the same icon in its title area. The icon
  goes when the link is turned off or the join removed.

### C. Therion `join` and `nosurvey` awareness

Background (Therion book, `join`):

- `join` works in two modes. **Scrap join**: two scrap IDs; Therion joins the closest scrap ends (passage walls only).
  `-count N` asks Therion to join the scraps in N places/passages. **Point/line join**: IDs of points or lines,
  optionally with a line point mark (`id:mark`, `:0`, `:end`, or an index); several items in one command join them all
  (`join a b c`). A passage usually needs two line joins; in extreme cases single points are joined, which can leave
  points on one side and a line on the other.
- `data nosurvey <from> <to>`: the network is computed from the other data and the listed stations are then connected
  by a line, so the two stations have no measured relation of their own.

One list, coloured (no separate modes): the Joins page lists every connection between drawings of different surveys:

- equate rows (as now);
- `nosurvey` rows: a `data nosurvey` leg whose two stations are in different survey namespaces;
- join-only rows: a `join` between drawings of different surveys with no equate or `nosurvey` leg between them.

Colours (three greens, light and dark theme tokens, with a legend tooltip):

1. **Bold green**: automatic scrap join (no `-count`) and an equate or `nosurvey` leg anywhere between the two
   surveys (decided 2026-10-14; to be reviewed once it is in use).
2. Green (second shade): scrap join with `-count N`.
3. Green (third shade): point/line join.
4. Plain: equate or `nosurvey` leg with no `join`. Legitimate; never shown as a problem.

A `nosurvey` leg counts exactly as an equate: it records a definite link, so it qualifies for bold green.

Join rules between one pair of drawings (decided 2026-10-14): the three join kinds are mutually exclusive. A scrap
join (automatic or `-count N`) is a single `join` command. Point/line joins need at least two `join` commands (a
passage needs both walls joined). Breaking these rules is an input error:

- more than one join kind between the same pair of drawings;
- more than one scrap-join command between the same pair;
- only one point/line join between a pair.

These are reported as Validation warnings naming the `join` lines (problem reporting stays in the Validation panel),
and the affected Joins row carries a warning marker that points to them.

Join-only rows use the colour of their join type and say "no equate" in the tooltip. Connections with neither a `join`
nor an equate are legitimate and simply not listed.

Staged implementation:

- **C1 Index.** Record `join` commands (mode, `-count`, each item resolved to its scrap or line and the line's scrap,
  marks) by extending the existing join-reference validation scan rather than scanning again, and record
  cross-survey `nosurvey` legs from the centreline scan. Drawing points gain their scrap's identity so equates and
  joins can be matched. Core tests only; no UI change.
- **C2 Classification and colours.** `ProjectJoinList` classifies each row (equate / `nosurvey` / join-only, and join
  type) for the selected projection; `ProjectJoinsPanel` shows the colours, legend, and join file/line in tooltips.
  Core and panel tests.
- **C3 Join rule checks.** Add the three join-rule warnings to project validation (same index data as C2), with
  focused false-positive tests, and the warning marker on Joins rows.
- **C4 Computed centreline placement.** Therion computes the station network; the app asks Therion for it rather than
  using the `.lox` (which also carries walls and surfaces). A background Therion run, on its own
  `TherionRunnerService` instance so the Compiler panel is not blocked, uses a temporary config that sources the
  project root and only exports the database (`export database -fmt sql`): surveys, stations with computed X/Y/Z,
  shots, and flags. The app already imports this format for its SQL report tab, so loading and station lookup reuse
  that code. The export is regenerated when missing or older than the sources. While it runs, equate rows open at
  once with the existing XVI anchor; when it is ready, every row (equate, `nosurvey`, join-only) is placed by fitting
  each drawing's station points to the computed positions projected into its projection (plan, or elevation at its
  angle), so both drawings share one frame and no anchor is needed.
  C4 details (decided 2026-10-15, to confirm in review):
  - Temporary config in an app cache folder (never in the project): `encoding utf-8`, one `source` line per `source`
    of the target config (absolute paths), and `export database -fmt sql`. Therion runs with that folder as its working
    directory, so `therion.log` also stays out of the project. About 0.5 s on the test project.
  - Station positions keyed like the index (`name@namespace`, namespace innermost first, matching the SQL
    `SURVEY.FULL_NAME`); every station point of every scrap is kept per scrap (line number and station key).
  - Projection into drawing metres: plan (X, Y); `elevation θ` (degrees) (X cos θ - Y sin θ, Z). Checked on 114
    scraps of a real project: 0.00-0.54 m RMS; the mirrored sign gives metres. Extended and other elevations: none.
  - Each scrap with two or more matched station points gets a similarity fit; partner-to-this is partner fit then
    the inverse of this fit, with no anchor. Otherwise the XVI anchor (equate rows) is used as now.
  - Run policy: one run at a time on its own runner; a request while running is queued once. Starts when the Joins
    page gets a project index and when a join opens while the export is missing or older than the project sources
    (saved files; Therion reads the disk). The status bar shows the run; a failed run leaves the XVI behaviour and says why.
  - Join-only rows open both drawings aligned when both fits exist, centred between the two scraps.
  - Slices: C4a core (positions, projection, fit, index station points), C4b service (export run, cache, load),
    C4c wiring (join open, join-only rows, status).
- **C5 Fallback without computed positions** — dropped (2026-10-15). Much of Therion Studio already needs Therion
  installed, so a second, approximate placement for when it cannot run is not worth its maintenance. Without computed
  positions, equate rows keep the XVI/`-scale` fit at the join station and join-only rows open unaligned with a
  status-bar reason. (The idea was: anchor point/line joins on the closest joined points and scrap joins on the
  closest wall-line ends.)
- **Extended elevations** have no common frame: each survey's extended layout is independent. Equate rows keep the
  equate anchor; rows with no equate open both drawings unlinked and say why, until a way to line them up is found.
  Note (2026-10-15): nothing to do at the moment; revisit only if a way to give extended elevations a shared frame
  appears.

### D. Fixes found in use (2026-10-21)

Reported while testing on a real project. Status: planned, no slice started. D1 comes first because it may
explain D3, and its result may change D2-D4.

- **D1 Alignment is wrong even with a shared point.** Equate rows, where both drawings draw the shared station, do not
  line up correctly. Investigate before changing anything else. Measure where the shared station point lands in each
  pane in the common frame and how far apart it is. Then find which placement was used (computed fit or the XVI
  anchor, see D3) and check each step. That means station keys matched to the SQL `FULL_NAME`, plan and elevation
  projection and its sign, the per-scrap similarity fit against a drawing with several scraps, and the
  partner-to-this transform (partner fit, then the inverse of this fit). Record the cause here with its numbers before
  the fix is agreed.
- **D2 Station not drawn in the view.** Some rows of the form `<station>@<survey> = <station>@<other survey>` are
  dimmed with "not drawn in plan view", although the survey has a drawing in that view. Only the join station's point
  is missing. This happens in both plan and elevation views. The point is needed only for the XVI anchor. With
  computed positions, open the survey's drawing in that view whose scrap lies nearest the station's computed position,
  place both drawings by their fits and centre both views on the station's computed position. Without computed
  positions keep the dimmed row but word the reason exactly, e.g. "<station>@<survey> has no station point in
  <survey>'s plan drawing". Rows where the survey reads no drawing at all stay as they are.
- **D3 Join-only rows open unaligned.** Probable cause, still to be confirmed in the app because the test harness has
  no such timing:
  - the positions count as out of date when any source is newer, `.th2` drawings included, so every drawing save
    starts a new Therion run;
  - while a run is going the placement ignores the previous positions, so a join-only row opens unaligned. The same
    happens when a join is opened within about a second of the Joins page appearing.

  Fix: only `.th` files and the config make positions stale; keep using the previous positions while a run is going;
  when fresh positions arrive, re-place joins that opened without them (only those).
- **D4 Centre on where the drawings meet.** Join-only rows are centred between the middles of the two scraps, which can
  be well away from where they meet. Instead find the closest pair of station points, one in each scrap, using their
  computed positions, and centre both views halfway between them (the user's suggestion). Placement itself stays on
  the fit to all station points, which also gets rotation and scale right.

Each slice is a separate commit on `JoinViewer` with tests, doc and translation updates, measured on the test project
before and after, and checked in the app.

### Open questions for phase 2

Answered (2026-10-14):

- An equate or `nosurvey` leg anywhere between the two surveys is enough for bold green (to be reviewed in use).
- Join kinds are exclusive per pair of drawings; point/line joins need at least two commands, scrap joins exactly one;
  violations are Validation warnings.
- Computed station positions are produced by the app through Therion when needed, and used for all rows once
  available.
- A `nosurvey` leg counts as an equate.
- Join-rule errors: Validation warnings plus a warning marker on the Joins row (confirmed).
- Extended elevations without an equate are left unlinked for now.
- Computed positions come from Therion's SQL database export, not the `.lox`.
- The background Therion run starts automatically when a join needs computed positions and they are missing or older
  than the sources; the status bar shows that it is running.

Still open:

1. Check during C4 whether `export database` can leave out the shot tables; only the station table is read either way.

## Risks

- Same file in two buffers: avoided by keeping normal tabs (no editor reuse needed).
- Drawings whose station points are not snapped to the XVI stations give a less exact fit; the fit residual is shown in
  the pane tooltip.

## Verification

- Core QTests for slices 1–2.
- Manual: LongwoodValleySink, `0@BoldStep = 36@TridentLeft` in plan and elevation 315; `13` four-way equate; edit and
  save each pane separately and confirm only that file changes (`git diff`) and undo stays per pane.
