# Wii Remote red-Sync pairing boundary

Based on the target-qualified full upstream33fa2fe internal-controller startup.
No USB transport, native startup, virtual-pad or LE source change is required.

Before a new Classic connection, cancel inquiry as before and ask its remote
name using the inquiry's scan mode/clock offset. Wait up to3s; error or unavailable
name falls back to the existing connection/pairing policy. Exact names Nintendo
RVL-CNT-01 and Nintendo RVL-CNT-01-TR select Wii legacy bonding. Other controllers
keep ASCII0000 PIN fallback and existing Secure Simple Pairing behavior.

For the Wii Remote's red Sync button, send six binary host Bluetooth address
bytes in HCI wire order (already the reverse of display order), including any
zero byte, padded to the standard16-byte PIN field. Refuse an unknown all-zero
host address. Logs identify selection/length/auth status without PIN/key bytes.
Temporary1+2 pairing is not implemented in this boundary.

This does not add a Wii guitar profile or deliver input to YARG. After successful
PIN/authentication/key storage/SDP IDs, generic HID rejection is currently expected.
The next change will initialize and parse the guitar extension, retaining its
instrument identity for the prospero-win XInput integration rather than treating
it as a generic DualSense.

Protocol reference: https://wiibrew.org/wiki/Wiimote#Bluetooth_Pairing
