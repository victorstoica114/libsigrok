# FNIRSI DLA-32 Plus

The `fnirsi-dla32` driver provides 32 physical logic inputs, Buffer and Stream
acquisition, basic and advanced parallel trigger conditions, adjustable threshold and
four independent PWM output groups. Channel names/indices follow the physical
inputs. Sparse-channel packing does not renumber enabled physical inputs.

The protocol reference includes whatshu's DLA-16 contribution
<https://github.com/sigrokproject/libsigrok/pull/306> and the GPL-3.0
TechBirdCompany overlay <https://github.com/TechBirdCompany/PXView-DLA32-overlay>.
The DLA-32 driver does not require the DLA-16 driver to be built.

## Windows transport

On Windows x64, the default transport uses a worker thread and the externally
installed WCH runtime `CH375DLL64.dll`. The FNIRSI/WCH USB driver and DLL must
already be installed. The driver loads the DLL from the FNIRSI installation
under Program Files; `FNIRSI_WCH_DLL` can select another absolute DLL path.
It does not redistribute the DLL, change Windows USB bindings or update device
firmware. Device association is intentionally restricted rather than guessing
which of several WCH devices belongs to the requested USB identity.

Linux/libusb discovery checks interface 0 for the expected bulk endpoints and
confirms the DLA-32 model with an identification request before listing a device.
The shared WCH VID/PID (`1a86:5537`) alone does not identify an analyzer. A busy
interface is skipped without detaching its driver. Install the supplied
`contrib/60-libsigrok.rules` using the distribution's libsigrok udev procedure
and reconnect the device if ordinary users lack USB access. The rule matches
the shared WCH VID/PID; the model check is performed by the driver.

An explicit connection such as `fnirsi-dla32:conn=1.49` selects a bus/address
without the discovery transaction. USB addresses can change after reconnection.
Linux/libusb acquisition has not received the equivalent hardware qualification.
An external tester has confirmed automatic discovery, a bounded Buffer transfer,
clean binary export with the companion CLI fix and automatic rate refresh on
Linux/USB 2. PWM, trigger-position and corrected CLI range display were also
confirmed there. The later continuous-Stream follow-up still needs USB 3 Linux
hardware testing. These reports do not qualify high-rate integrity or long captures.
Acceptance of the Windows transport dependency is requested for
maintainer review. This contribution does not establish vendor-independent
transport support on all platforms.

Build with the usual libsigrok configuration and `--enable-fnirsi-dla32`.
No private compile-time worker switch is needed. A physical USB 3 connection
is required for Stream. Rate/depth availability depends on mode and enabled
channels; query the driver rather than assuming all rates fit all masks.
In Buffer mode, 1-8 enabled channels allow up to 1 GS/s, 9-16 up to 500 MS/s,
and 17-32 up to 250 MS/s. USB 2 limits download throughput, not these internal
Buffer sampling rates. Stream remains restricted to USB 3.

## PWM and trigger controls

Both transports publish four PWM groups without additional analog input
channels. In PulseView, stop acquisition and open the **Channels** popup, then
find the PWM0-PWM3 groups. Set output frequency and duty cycle before enabling
the required output. Controls are 1 Hz-20 MHz and integer 0-100%. The endpoints
command constant LOW/HIGH; disabling an output has separate, unmeasured
electrical behavior. External measurements report 5 ns timing quantization,
so a requested intermediate duty is not necessarily achievable: at 20 MHz,
a 5 ns step is 10% of the period. Clocks above the rated 20 MHz have been
reported experimentally but are not enabled by this driver. Frequency/duty
accuracy remains subject to the recorded qualification scope. On Linux these
commands use libusb directly and do not require the FNIRSI application.

The device configuration popup offers the Buffer pre-trigger capture ratio
(1-90%) in Buffer mode on either transport. It is unavailable in Stream. The
ratio is a hardware setting; it does not establish an exact calibrated trigger
sample index.

### Standard AND and advanced parallel triggers

One ordinary libsigrok stage can combine conditions on multiple enabled
physical channels with AND, using the standard 0x12 command in both Buffer and
Stream. A stage can contain at most one condition for each physical channel.
Multiple conditions alone do not require the advanced trigger engine.

The driver also supports one to sixteen ordered stages in Buffer. Multiple
stages, explicit Count/Continuous qualifiers and serial-word matching require
the advanced engine. Selecting those combinations in Stream returns an
unsupported-mode error before acquisition starts.
Empty stages, duplicate conditions for one channel in a stage, disabled/foreign
channels and more than sixteen stages are rejected.

Ordinary stages default to one occurrence with continuous qualification disabled.
Optional Count/Continuous and serial-word matching are described below. Mixed
conditions and the vendor serial Start/Stop controls remain under investigation.

This fork's companion PulseView `master` provides the editor:
<https://github.com/victorstoica114/pulseview/tree/master>.
It supports stage/condition editing and setup save/restore. Existing sigrok
applications can also construct the sequence using the ordinary trigger API.
These advanced extensions are separate from the current upstream driver PR.

Windows/WCH validation includes the core/C++ tests, additional actual-driver
mapping/lifecycle probes, positive/impossible-condition cases, two/sixteen
stages, sparse masks and independent raw-byte conversion. Five voltage-gated
cases wait without data before a controlled input event, then complete their
requested sample count. A probe using production PulseView GUI objects applies,
saves/restores and acquires a real two-stage sequence. These are bounded
functional results; they do not calibrate the trigger sample index or input
threshold, and Linux advanced-trigger hardware testing remains pending.

The standard AND follow-up, prompted by @ljalves's original-application tests,
passes a two-input truth table and edge/level controls on Windows/WCH. Ten
production-API cases cover positive/negative conditions, full/sparse masks and
delayed events in both Buffer and Stream at1 MS/s. Nonempty logic output matches
independent raw-wire decoding. These bounded cases do not qualify exact trigger
position, all channel combinations, high-rate integrity or Linux behavior.

The driver also exposes a finite duration limit through libsigrok, but PulseView
does not provide a separate time-limit control. Its toolbar sets the sample
count; sampled duration is sample count divided by sample rate. For example,
5 million samples at 50 MS/s cover 100 ms. Hover over the sample-count or
sample-rate selector to see "Total sampling time". This describes the sampled
signal span, not time spent waiting for a trigger or downloading the capture.
The CLI can select a time-limited capture with `--time`; a `limit_time` readback
of zero means that time limit is inactive when a sample-count limit is used.

Both transports advertise continuous acquisition. Finite Stream ends at its
sample/time limit; continuous Stream uses the device's rolling mode and runs
until Stop. Both require USB 3. The libusb lifecycle has offline regression
coverage, but equivalent continuous hardware qualification on Linux remains
pending. USB 2 continues to offer Buffer mode only.

PulseView Stop preserves data already delivered to the application and retires
active transfers. It does not offer the vendor application's separate Buffer
choice to stop early and upload the device's partially filled memory. Failed
libusb STOP writes report an error and close the host acquisition after transfer
callbacks retire; device-side stopping is not claimed. Reopen/recover the device
before retrying if such an error occurs.

## Input threshold

The standard voltage-threshold key accepts one shared switching level from
-5 V to +5 V in 0.01 V command steps. Its low/high pair must contain equal
values: it does not configure a hysteresis band. PulseView presents the regular
list as a slider, including zero and negative voltages. The former 0.6/1.6/2.5 V
values remain available. All seven factory preset voltages select their
corresponding preset code; other values use Custom. The setup field is signed
16-bit little-endian hundredths of a volt, so -5 V is 0xfe0c, not -50.

These are protocol settings, not calibrated electrical threshold or hysteresis
specifications. Selecting a threshold with more signal margin can help examine
extra crossings, but the setting alone does not establish the cause of short
pulses. No filtering, debounce or waveform correction is applied.

The optional `FNIRSI_DLA32_WIRE_TRACE` absolute destination is a diagnostic raw
USB trace. An explicitly requested trace is created exclusively; trace-write
failure aborts acquisition. It is off by default and should remain off during
ordinary use or performance measurement.

## Acquisition outcomes

The proposed capture-status/error keys report host acquisition outcome and are
also carried in metadata and saved sessions. `complete` means the requested
sample count was delivered. It does not certify physical signal integrity,
electrical calibration or absence of device-side dropped/repeated samples.
User Stop, disconnection and errors remain distinguishable. API naming/key
allocation and saved-file representation are subject to upstream review.

The current host workaround waits up to two seconds after Stream retirement
before a following Buffer command. This is a bounded, tested mitigation; an
acknowledged USB write is not an independent device-readiness measurement.
Reconnection requires explicit Scan and device selection; there is no automatic
acquisition retry.

## Cleanup and diagnostics

Worker log retention is bounded. Dropped/truncated debug messages are counted
and reported separately; they do not stop acquisition or change its outcome.
Transport results and failed worker notifications still determine failure.

If driver cleanup cannot finish, the C API retains the context for a later
`sr_exit()` retry. The C++ last-owner deleter likewise retains the complete
owner without throwing from a destructor. `Context::retry_cleanup()` retries
retirement and reports a continuing failure as a normal `Error`; `create()`
also retries before initializing another context. Resources are released only
after successful cleanup. Unfinished retirement can retain memory/handles
until a successful retry or process exit; it does not fabricate a successful
hardware STOP.

## Validation and limits

The preceding Windows delivery was tested with bounded Buffer/Stream captures,
sparse/grouped masks, supported rates/depths, protocol decoding, PWM controls,
Stop/error handling and physical USB interruption/recovery. The independent
eight-line Kingst comparison applies to its recorded rate/threshold scope.
These results describe that delivery, not a guarantee for every device or OS.
The submitted branch's separate build/test results belong in its PR description.

Offline protocol tests use fixed command/CRC vectors and independently generated
time-domain sample expectations. They cover every physical bit, sparse masks,
fragmentation across USB read boundaries and output bounds.

Known limits remain: arbitrary-duration lossless all32 Stream at 50 MS/s /
200 MB/s is unqualified; high-rate waveform-width deviations remain recorded;
exact physical trigger position is not qualified and no precise synthetic
trigger marker is emitted. Advanced parallel stages are experimental in Buffer;
serial matching and configurable Count/Continuous use the proposed API
described below.
A large interrupted export failed during temporary-file rename
(EXP-001), and its cause remains unresolved. No filtering, sample repair,
empirical duty correction or channel remapping is used to hide these findings.

PWM settings are commanded/cache state, not electrical readback; their state
becomes unknown after reopening. Threshold presets have functional validation,
not calibrated hysteresis/threshold specifications for every channel.

## Experimental trigger options

This fork's `master` additionally proposes
`SR_CONF_TRIGGER_OPTIONS` (`trigger_options`), using an `a{sv}` dictionary.
This extends configuration without changing the public `sr_trigger` structures.
It is a proposed API, separate from the existing upstream PR. Pair it with
<https://github.com/victorstoica114/pulseview/tree/master>
for the matching editor. Existing upstream PulseView builds do not expose these fields.

SET replaces the complete dictionary. GET returns the commanded configuration;
LIST describes the available modes and bounds. Invalid, unknown, duplicate or
overflowing fields are rejected without changing the previous configuration.
The three accepted forms, in GVariant text notation, are:

```
{'mode': <'default'>}
{'mode': <'parallel'>, 'stages': <[(uint32 4, false), (1, true)]>}
{'mode': <'serial'>, 'clock': <uint32 17>, 'data': <uint32 18>, 'edge': <'rising'>, 'bits': <uint32 8>, 'value': <uint32 165>}
```

Default mode retains the normal session trigger model. Parallel mode supplies
one `(uint32 Count, boolean Continuous)` tuple per ordinary trigger stage, in
stage order. There must be 1–16 stages and Count must be 1–255. Count is an edge
occurrence count for edge conditions. For level conditions it is nominal time
in nanoseconds: Continuous requires uninterrupted matching time, while the
other setting permits accumulation over separate matching intervals. Mixed
edge/level qualifiers remain experimental; calibrated timing is not claimed.

Serial mode uses distinct enabled physical logic channels for clock and data,
a rising/falling clock edge, a width of 1–16 bits and an unsigned value fitting
that width. It matches the latest sampled bits, MSB first. It does not implement
SPI chip select, UART framing, a protocol decoder or serial Start/Stop controls.
The ordinary session trigger must be cleared in serial mode. Serial and qualified
parallel triggers require Buffer mode; Stream and continuous acquisition reject
these extensions. Configuration changes are rejected during acquisition.

Windows/WCH hardware checks include rising/falling/either-edge Counts 1–6,
100/500 ns HIGH and LOW pulses, both Continuous settings, 50/250 MS/s controls,
SPI words of 1/8/16 bits and both clock edges, an absent-word negative control,
and a three-channel sparse-mask serial capture. The new public API also passed
independent raw-to-logic comparison. PulseView's production toolbar/editor
passed real Count and SPI captures, configuration persistence and Cancel checks.
Linux validation of these new options is still requested.

Controlled SPI words sent after arming and allowing prehistory were found within
5–8 samples of the nominal midpoint at 25 MS/s. This does not qualify exact
trigger position for arbitrary traffic. A previous free-running serial test
had a positioning discrepancy; its cause remains unresolved. Intermittent
either-edge Count behavior reported externally was not reproduced in the
bounded controlled-start series. No FPGA/controller firmware is changed.
