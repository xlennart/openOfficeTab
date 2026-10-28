# Word caption-close delay investigation

## Observations · 2026-10-24

The reported symptom was two caption-close attempts taking roughly two or three minutes to finish. The existing Word process was still running the 0.1.8 DLL, although the registered next-start payload was 0.1.9. Neither a registry version nor a new installation replaces a DLL inside an already-running Office process.

The desktop-user runtime log contains two relevant sequences, at approximately 20:49 and 21:27 local time: `refresh.rejected.Item`, followed by repeated `command.rejected`, with HRESULT `0x80020009` (`DISP_E_EXCEPTION`). The latter sequence spans almost a minute. This establishes repeated object-model failures, not a measured two-minute block or a proven Office internal root cause. The old logger discarded the command name and Office's nested exception code.

No matching Word crash/hang report was found in the inspected recent event/report records. A delay that eventually resolves need not produce such a report.

After the user authorized closing the invisible background, the remaining old process was confirmed to have launched with `-Embedding`, with zero documents, protected views, native document windows, or visible top-level windows. `Application.Quit` did not remove it. Disconnecting this add-in in that empty process and calling `Quit` again also left it alive. This does not establish that the add-in was its sole cause, nor identify any remaining external COM owner. After repeating the empty-state and exact-process-identity checks, only that process was terminated. No Word process remained; the original automatic-loading setting (`LoadBehavior=3`) was restored and the local candidate registration verified.

## Targeted change

- A grouped Word caption close is posted to the add-in's dispatcher, then delivered to the corresponding native frame as `WM_CLOSE`. It no longer calls the cached Word `Window.Close` or `Window.Activate` for that operation. Excel and PowerPoint retain their existing command paths, as does Word's individual tab-close command.
- Word retains its native save and cancellation handling. The existing current/group choice, remembered preference, tab-lock boundary, and independent/group boundaries remain in effect.
- If collection refresh fails after a native Word close returns, stop the unconfirmed batch and release the pending-close gate. Do not keep blocking caption requests indefinitely or assume a close succeeded from an unavailable collection.
- Record the failed operation name and numeric Office exception code, without exception descriptions or document content. Native close entry/return timings distinguish time inside Office from other steps. Time spent in a save prompt is included and must not automatically be called a hang.

## Verification

`tests/frame-close-test.cpp` rejects cached `Close` and `Activate` calls, verifies native dispatch, remembered current/group choices, cancellation, locked-tab behavior, group boundaries, and host exit. It also rejects collection reads after a cancelled native close and checks that the batch stops and the close gate is released.

`tests/word-frame-close-runtime.ps1` opens a separate Word `/x` instance with a unique copy of the public blank fixture, checks its loaded DLL hash, and posts the native caption command. It exercises six create/close cycles, Office `BeforeClose` cancellation, group cancellation, closing a separate group while preserving the base fixture, and visible Ribbon content. This uses native messages rather than a physical mouse and does not reproduce the original user's documents or extended session history.

The first full native run passed the close regression but failed an unrelated immediate-persistence assertion in the localization suite. A standalone rerun also failed a settings write assertion. The test now retries nonblocking writes with a one-second bound, consistent with the production pending-write contract; production persistence behavior was not changed. The reason for the transient write rejection was not established.

Initial runtime-harness failures were also corrected: this machine's PowerShell dynamic binder required boxed return values from the probe, and Word can retain/reuse an SDI HWND after its document is closed. Completion is therefore checked against document/view identities and count, rather than `IsWindow` on an old number.

The final local DLL SHA-256 is `8AE68D67F21DF538126BCAB5FFB760D472543B81C3C38AB970BF177308113AC7`. All six native suites passed after the pending-close protection was added. The final independent Word run passed six caption-close cycles at 131, 104, 107, 105, 109, and 114 milliseconds, two cancellation checks, separate-group closing, and 12 visible Ribbon-content checks. These are single-machine test observations, not timing guarantees. The existing user process was preserved, and temporary close/group preferences were restored.

Further verification covered 24 consecutive new/close cycles (96–155 ms), 48 visible Ribbon-content checks, and closing the last document through the native caption command. A separate check after the automation harness ended confirmed the process exited. COM-activated `/Automation -Embedding` instances also exited without forced termination, both with the add-in connected and after disconnecting only this add-in. The observed document-close/Quit-call durations were 149/9 ms with the add-in and 357/8 ms without it; these single observations are not a comparative performance benchmark. The old long-lived instance's remaining COM owner was not identified.

For repeatable process-lifetime acceptance, run the fixture harness and the independent exit check serially. The latter checks both PID and creation time, waits at most eight seconds after the harness releases its references, and preserves a remaining process for investigation:

```powershell
powershell -NoProfile -File tests/word-frame-close-runtime.ps1 -Cycles 24
powershell -NoProfile -File tests/word-process-exit.ps1
```

This is a local candidate, not a new public release. Passing these checks does not prove that every possible native Word save/add-in delay is eliminated. A fresh recurrence requires the operation/timing log and, if still hung, a scoped stack capture to establish the exact blocked component.
