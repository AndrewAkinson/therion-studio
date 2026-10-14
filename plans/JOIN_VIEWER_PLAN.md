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
- Possible later upgrade: use Therion-computed coordinates from a `.lox` (already loadable by
  `ThreeDViewerLoxLoader`) for true cross-survey placement. Not in the first slice because it needs a fresh compile.
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
  `Remove Join` per joined drawing.

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
  the existing separate map window.
- A Settings option reverses the two: double-click opens with a separate window, and the context menu offers
  `Open Join in Tabs`. Stored through `ISessionStore` like the other Settings options.

### B. Show that drawings are linked

- While a join's views are linked, both document tabs show a link icon (Lucide `link`, added to the bundled icon set
  under the existing Lucide licence notice). The separate map window shows the same icon in its title area. The icon
  goes when the link is turned off or the join removed.

### C. Therion `join` awareness

- Therion `join` connects scraps (`join CenturySP1@Century TickleWestSP4@TickleWest`) or line ends and marked line
  points (`join Wall1@TickleWest:Left Wall2@TickleWest:0`). The project index already parses join references for
  validation; it shall also keep the join commands with their resolved scraps/lines.
- An equate row shall be shown green when its two stations are drawn in scraps that a `join` connects (directly, or
  through joined lines in those scraps) in the selected projection.
- An option on the Joins page shall list `join` commands as well, marking which are backed by an equate between the
  joined drawings.
- Equates without a join, and joins without an equate, are legitimate and shall not be shown as problems.

### Open questions for phase 2

1. Which drawing goes in the separate window: always the second (right-hand) side of the row, or a choice?
2. Green means "a `join` connects the two scraps that draw the equated stations". Should a join that connects other
   scraps of the same two surveys also count?
3. "Stations can also be joined by no survey": does this mean equates with no survey shot between the stations, joins
   where no survey data connects the drawings, or something else?
4. Should the join list be a mode of the Joins page (`Equates | Joins | Both`) or a separate checkbox filter?

## Risks

- Same file in two buffers: avoided by keeping normal tabs (no editor reuse needed).
- Drawings whose station points are not snapped to the XVI stations give a less exact fit; the fit residual is shown in
  the pane tooltip.

## Verification

- Core QTests for slices 1–2.
- Manual: LongwoodValleySink, `0@BoldStep = 36@TridentLeft` in plan and elevation 315; `13` four-way equate; edit and
  save each pane separately and confirm only that file changes (`git diff`) and undo stays per pane.
