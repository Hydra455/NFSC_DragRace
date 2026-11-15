## NFSC_DragRace

Renames the solution, project, and plugin. Removes file-based diagnostics
and uses the previous default settings. The four bundled races retain their
fallback display names.

## v0.15.1

Fixes duplicate Drag rows in Quick Race. Each list build tracks the Drag
events accepted by Carbon's Career pass and skips only their standalone
duplicates. Area filters and list probes use independent build state.

## v0.15.0

Uses Carbon's race database and native Drag type for event discovery and Career
availability. Sizes the missing-icon lookup from the native race count. Adds
per-event visibility diagnostics and moves optional display names into an INI.

## v0.14.9

Adds missing-name fallbacks for University Drag and the two new rival events.
Adds Angie and Darius race scripts with a shared install and uninstall menu.

## v0.14.8

Adds a scoped Career availability fallback and Quick Race eligibility exception
for the renamed Hotels race. Reports whether the runtime loaded its race
record, Career track, and map location.

## v0.14.7

Adds the renamed Hotels collection to the display-name fallback.

## v0.14.6

Uses Carbon's native track preview streamer for both Hotels Drag and University
Drag. Removes the bundled Hotels preview registration and synchronous texture
switch from the World Map UI. Keeps the Hotels display name fallback.

## v0.14.5

Matches the nine-character race index used by the shared Career and Quick Race preview code. Includes a corrected copy of the supplied track map archive keyed by `TRACK_MAP_DRAG_EXAM`.

## v0.14.4

Binds the registered Hotels Drag route texture directly to the Career preview image. This skips the stock streaming lookup for the bundled texture. Other event previews use the native streamer.

## v0.14.3

Connects the bundled Hotels Drag route texture to the Career event panel. Supplies the Hotels Drag display name when the event's localization entry is missing.

## v0.14.2

Hides roadside engage icons and particle effects using the active Drag event in both Quick Race and Career. Restores native free-roam visibility when the event clears.

## v0.14.1

Uses Carbon's native Quick Race mode check before applying the main-menu Career Drag shortcut. Preserves the selected Quick Race event for car selection.

## v0.14.0

Launches Drag from the main-menu Career world map through the front-end race loader. Repairs a blown player engine on the return to free roam. Hides the Drag shift icon outside the shift windows and recolors the four countdown animations yellow.

## v0.13.9

Hides the active Career Drag race's roadside icon and clears its particle effects through Carbon's native icon update. Restores normal icon visibility in free roam.

## v0.13.8

Restores the Drag map image fallback at Carbon's Career world-map call sites. Returns free-roam markers to the v0.13.6 particle path and keeps the v0.13.7 HUD and Quick Race options.

## v0.13.7

Restores the native world Drag icon path, enables point-to-point Quick Race options for Drag, and reskins the native fullscreen Drag package in Carbon colors.

## v0.13.6

Ports the original Drag menu and world textures from Most Wanted. Restores Carbon's dormant world emitter and releases the Drag HUD on race completion and return to roaming. Includes an exact Hotels Career gameplay backup and an editable VltEd export.

## v0.13.5

Uses the Most Wanted mask rotation origin for the nitrous, Speedbreaker and heat gauges. Uses the Direct3D back-buffer aspect ratio to spread the two HUD groups. Runs authored Career Drag launches through the same startup tracing and fallback used for Quick Race. Includes a repaired gameplay pair for the Hotels territory example.

## v0.13.4

Suppresses the MW HUD during intro NIS playback. Connects nitrous and Speedbreaker to Carbon vehicle charge, drives the over-rev heat meter, and activates the original MW shift scripts.

## v0.13.3

Restores authored Quick Race Drag detection on first launch. Rebinds the native HUD on a restart when the live race still resolves as Drag.

## v0.13.2

Rebinds the native MW Drag HUD after a same-launch restart and restores speed-unit and countdown text. Hides unused MW text objects.

## v0.13.1

Corrects the Direct3D format offset in all 28 Carbon texture records. Checks GPU handles before activating the original MW FNG and logs each native HUD startup step.

## v0.13

Loads the original Most Wanted Drag FNG through Carbon FEng with converted texture resources and live gauge bindings. Suppresses the stock race package during supported Quick Race Drag events. Uses the MW body font for speed digits. Fullscreen testing is pending.

## v0.12.1

Reduces the fullscreen gauge footprint to clear the adjacent native nitrous and Speedbreaker meters.

## v0.12.0

Registers a Carbon-native HUD_Drag.fng package for authored Quick Race Drag events. Hides its stock tachometer group and draws a cyan fullscreen gauge with speed, RPM, gear, shift timing and heat.

## v0.11.9

Draws the embedded Drag gauge inside Carbon’s final Direct3D 9 frame for exclusive fullscreen support. The previous layered window is removed.

## v0.11.8

Embeds the four required Most Wanted gauge textures and logs initial HUD presentation.

## v0.11.7

Adds a Drag HUD overlay driven by live RPM, gear, shift and heat data with original Most Wanted gauge textures. The original texture bank is loaded from beside the ASI or from GLOBAL.

## v0.11.6

Makes authored Drag events visible through the native Quick Race list without the Extra Options ShowHiddenRaces setting.

## v0.11.5

Adds one-row placement for three-car Quick Race Drag events at the native start position restore.

## v0.11.4

Adds a one-time native music completion fallback for stalled Quick Race Drag startup and records the initialized racer count.

## v0.11.3

Traces the music transition request, completion messages and intro NIS invocation during race startup.

## v0.11.2

Adds observation of the final native LoadingComplete gate values, notification requests and event construction, FinishedGameLoading entry/return, native HUD filename selection and resource readiness. No gate is forced and no gameplay database is modified. This is a diagnostic build; the loading freeze remains unconfirmed as fixed.

# v0.11.1

Adds loading-stage entry/return records, launch identification and a five-second diagnostic heartbeat. Observes native audio/model/stock-car loading results without overriding them. Drag activation and gameplay behavior retain v0.11 logic.

The freeze remains unresolved. This build is intended to capture the native stage needed for a targeted fix.
