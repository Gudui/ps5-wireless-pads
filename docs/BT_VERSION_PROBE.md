# Single command/event transport diagnostic

`make bt-version PS5_PAYLOAD_SDK=/path/to/sdk BUILD_ID=<source-commit>` builds
`dist/bt-version.elf`. It is separate from the controller bridge's startup.

The diagnostic checks MediaTek0e8d:3603 and a complete cached configuration,
then requires interface0 event endpoint81 with packet size16. It opens one
USB_FS interrupt-IN transfer and sends one Read Local Version Information
command1001 without parameters or retry. The restricted transport rejects all
other commands and exposes no ACL send/receive operations. Up to60 pumps of50ms
and64 events per pump bound the query. A matching Command Complete must have
the expected14-byte layout and status0; unrelated events cannot make it pass.

Every owned transfer/file is stopped, uninitialized and closed, with errors
reported. It does not reset, scan, pair, write keys, load firmware, detach a
driver, create virtual controllers or change credentials. A single reader can
lose an event to the system driver; timeout does not prove the radio is broken.

Output appends to `/data/prospero-win/wii-bt-version.log`. Record the exact begin
source/pid, Complete fields, terminal result/query_status/commands/cleanup,
kernel exit and operator DualSense/Home responsiveness. Data-only query status
numbers are defined in `src/version_query.h`. A failed transport setup does
not submit a version command. A cleanup error cannot produce result=ok.

Host tests cover matching/foreign/malformed/truncated events, explicit errors,
transport loss, no response and an event flood. They do not qualify USB access.
Protocol: Bluetooth Core, Vol4 PartE7.4.1:
https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-60/out/en/host-controller-interface/host-controller-interface-functional-specification.html

The completion-observation revision also logs actual control-request length and
counters for existing COMPLETE calls (successful, EBUSY, EAGAIN and other errors),
invalid indices and the last completion metadata/first16 bytes. A snapshot before
cleanup records read counts, pending state and assembler partial/dropped counts.
This adds no ioctl or transport policy change. Invariants: polls equals the sum
of ok/busy/again/other; reads_done plus bad_index equals ok for the one-slot query.
A zero-event timeout alone does not distinguish a pending USB read from failed
completions or partial event bytes. These counters do not qualify pairing.
