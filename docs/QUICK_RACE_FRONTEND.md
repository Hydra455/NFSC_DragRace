# Quick Race frontend audit

## Native bindings

| Address | Role |
| --- | --- |
| 0xA590B0 | 17-entry world-map race table, stride 20 |
| 0xA590D8 | Drag entry, native type 2 |
| 0xA590E4 | Drag label hash, originally zero |
| 0x4A3380 | Existing DALLangHash race-type resolver, Drag maps to 0xE3DF9561 |
| 0x4A8F70 | Resolve event type to table index |
| 0x4A9500 | Format per-event icon name from table |
| 0x5C3B30 | Quick-list enumeration and eligibility filtering |
| 0x5C9AF0 / 0x5C9B50 | Forward/back filter search, already includes index 2 |
| 0x5F3760 | Find frontend object by hash |
| 0x5FFCD0 | Native FEImage clone |
| 0x5EC6F0 | Native object naming and name-hash update |
| 0x5EC910 | Package traversal, root list at +0x44, group child list at +0x5C |
| 0x578830 | Localized string lookup |
| 0x5A0910 | Frontend formatted string assignment |
| 0x85E5D0 | Race options setup, Drag already dispatches to SprintEtAll |

Quick-list enumeration formats the icon name and looks it up before checking event eligibility. A missing image, or an object whose type is not 1, skips the event. The filter searches for a nonempty list, so a skipped Drag event cannot make the Drag filter appear.

The fallback recognizes missing MMICON_DRAG images by indexing the icon names up to the native race database count. It locates the corresponding Sprint image, or Sprint image zero, then clones and renames it. The source remains a separate object. The clone is appended to the source's owner list, with next/previous pointers, tail and count updated. No clone pointers are retained across package loads. Existing matching objects pass through the native lookup.

The live menu must still be tested, including repeated menu entry/exit and package unload/reload. These are native-layout bindings validated by disassembly, not a recorded successful game test.

## Supplied gameplay audit

The BIN and LZC contain the same VPAK payload. The LZC uses a RAWW wrapper. Only read-only parsing was performed.

race_bin_drift/drag_test (0x7E8CB18E) inherits 0x0F6BCDE1 = drag from the drag collection. It inherits 0xB39ED8C3 = false and 0xA4E6FCFD = false from basicrace.

race_bin_quickrace/drag_test (0xBD75F6A3) inherits 0x0F6BCDE1 = p2p. Its general InitiallyUnlocked field 0xEA855EAF is true, while the separate Quick Race initial-unlock field 0xB39ED8C3 remains false.

The runtime logs loaded Drag parameter collections and their native getter flags when the Quick Race list initializes. This distinguishes data resolution from missing frontend artwork. The standalone list bypasses its Career and unlock gates for registered Drag events with valid map positions. The native Quick Race opt-out, challenge and D-Day flags, region, and race exclusions still apply.
