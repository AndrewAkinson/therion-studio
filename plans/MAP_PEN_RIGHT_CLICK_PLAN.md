# Map Pen Right Click Plan

Date: 2026-10-21

Status: plan only, for later. Branch to be made from `main` as `fix/map-pen-right-click`.

Scope: a right click with the pen's side button on the map canvas opens the map context menu, as a mouse right
click does.

## Why

On Linux a pen side-button click on the map does nothing; the mouse right click works, and the same pen works in
other programs such as a web browser.

Cause, from reading the code (not yet traced with a tablet):

- `MapEditorViewportInputController` turns every tablet press, move and release into a mouse event itself and
  accepts the tablet event (`4247bef fix(map): handle stylus viewport input`, so Windows pen editing does not rely
  on platform mouse synthesis).
- Because the tablet event is accepted, Qt does not make its own mouse events, so the `QEvent::ContextMenu` that
  opens the menu for a mouse right click is never sent.
- The forwarded right press starts a pan; the pan release only opens the menu for the Ctrl+click route, not for
  the right button.

## Behaviour

- Pen tip input (drawing, selecting, dragging) is unchanged.
- The pen side button follows Qt's normal handling, the same route as the mouse: a still click opens the context
  menu, a drag pans the map.

## Implementation Outline

1. In the tablet forwarding (`MapEditorViewportInputController.cpp`, `TabletPress` / `TabletMove` /
   `TabletRelease`), leave events whose button is `Qt::RightButton` (or whose buttons include it) unaccepted and
   unforwarded, so Qt synthesises the mouse events and the context menu event.
2. Make sure the synthetic mouse suppression (`hasRecentHandledTabletEvent`) does not swallow those synthesised
   right-button events: the tablet timestamp is only set for handled events, so check this holds.
3. Confirm by turning on the existing map input trace and clicking with the pen side button.

## Tests

- `tests/MapEditorViewportTabletInputTest.cpp`: a tablet press with `Qt::RightButton` is not handled by the
  controller (left to Qt), and a following synthesised right-button mouse press is not suppressed.
- The existing tablet tests still pass.

## Risks

- Windows pen side-button behaviour may differ; ask the maintainer to check a pen right click on Windows in the PR.
- If pen jitter exceeds the 4 pixel pan threshold, a click may still count as a pan. If so, a larger threshold
  for pen input is a possible second step.

## Documentation

- User manuals: pen side button opens the map context menu (if the manuals mention pen input).
- `WORKLOG.md`: entry while in progress.
