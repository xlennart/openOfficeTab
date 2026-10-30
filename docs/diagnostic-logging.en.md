# Diagnostic logging

Use **Diagnostics & repair → Export diagnostics and logs** when reporting a problem. Include reproduction steps, Office version and architecture, file size, and a video when useful. Exports exclude document names, paths, contents, INI history and legacy raw logs. Inspect attachments before posting.

Native JSONL records include UTC and monotonic times, process/thread/run IDs and sequence numbers. Host codes 0/1/2 mean Word/Excel/PowerPoint; 3 is the desktop service. `bits` describes the recording process, not proof of the loaded Office DLL.

From 0.2.3-beta.1, `traceId`, `parentTrace`, `origin` and `jobId` connect plugin operations, callbacks and layout jobs. Origins: 1 tab command, 2 layout execution, 3 refresh, 4 native window message, 5 painting, 6 geometry application, 7 restoration. Parent IDs describe observed call context; they do not prove exclusive causation. External activation cannot always be attributed to the user, Office or Windows.

Window API codes: 1 active window, 2 foreground window, 3 position, 4 show state, 5 COM activation, 6 placement. Begin/end/error records include process-local HWNDs and observed state. An accepted API request is not visual acceptance. Layout cancellation reasons: 1 replacement, 2 explicit selection, 3 detach, 4 destruction; deferral reasons: 1 Excel editing, 2 busy window state.

Paint samples record selected/hovered IDs, actual drawing counts, DPI and geometry, at most once per 250 ms per tab bar. State snapshots are limited to once per second and 32 views; layout deferrals to once per 500 ms. Suppressed and truncated counts expose these limits. Hover is counted separately from selection.

Callbacks queue up to 64 KiB in memory without disk I/O; idle work flushes it. Recent history holds about 128 KiB. Each process retains four files of at most 1 MiB. Export reads at most eight files and 4096 events and reports skipped, malformed or truncated data. Congestion can drop records; abrupt termination can lose the final unflushed buffer. Logs improve diagnosis but cannot guarantee complete evidence or a root cause for every failure.
