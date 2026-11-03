# Cached Bluetooth layout diagnostic

`make bt-layout PS5_PAYLOAD_SDK=/path/to/sdk BUILD_ID=<source-commit>` builds
`dist/bt-layout.elf`, separate from the controller bridge. It is a firmware
layout diagnostic, not Wii controller support.

It opens `/dev/ugen0.2` read-only, checks MediaTek `0e8d:3603`, copies cached
device/configuration descriptors and lists complete HCI functions. A bounded
4096-byte buffer and strict descriptor length/stream checks reject truncated
or malformed output before endpoint parsing. Output goes to stdout and
`/data/prospero-win/wii-bt-layout.log` (the existing directory must be writable).

There are no HCI commands, USB bus requests, USB_FS transfers, endpoint reads,
pairing, firmware uploads, credential changes, virtual pads or UI registration.
Only the diagnostic log is written. The running system Bluetooth driver stays
attached. A successful report describes endpoints; it does not prove they are
free to use or safe to open alongside the system driver.

The operator launches it through the console payload loader and captures its
log/kernel output. Expect `BT_LAYOUT result=ok` plus complete configuration hex
and HCI endpoints. Denied access, changed identity or incomplete descriptors
are inconclusive. A complete descriptor with no HCI function is a negative
layout result. Record controller/Home responsiveness separately. Do not follow
this diagnostic with radio activity until the result has been reviewed.

## External TP-Link cached layout

`make bt-adapter-layout ...` builds `dist/bt-adapter-layout.elf` from the same
cached-only producer and parser. It writes `/data/prospero-win/wii-bt-adapter-layout.log`
with `BT_ADAPTER_LAYOUT` markers. It enumerates at most4096 /dev entries and32
strict ugen basenames, excludes root hubs and internalugen0.2, opens external
candidates read-only and copies cached device descriptors. Exactly one2357:0604
match is required; only that adapter receives a cached full-descriptor query.
No endpoint nodes, radio commands, firmware uploads, transfers or pairing.
Malformed/traversal/alias/overflow node names are rejected by portable host tests.
Successful cached layout does not prove HCI command/event or Realtek readiness.
