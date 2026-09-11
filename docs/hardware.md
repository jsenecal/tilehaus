# Hardware

## Supported panels

Tilehaus targets **ESP32-P4** touchscreen panels. The primary, tested target
is the **Guition JC1060P470** — a 1024×600 capacitive touchscreen with a GT911
touch controller. Hardware bring-up for this panel lives in
`firmware/hardware.yaml`.

## Wi-Fi is hosted over SDIO

The ESP32-P4 has no native Wi-Fi radio. On the JC1060P470, Wi-Fi and
Bluetooth LE are provided by an on-board **ESP32-C6 co-processor**, wired to
the P4 over SDIO and driven through ESPHome's `esp32_hosted` component. This
is required for the `wifi:` component to work at all on esp32p4 — there's no
way to run Tilehaus on this hardware without it.

Practical implications:

- Wi-Fi quality depends on the companion radio and its SDIO link, not just
  antenna placement.
- Power-save behavior on the hosted radio can introduce latency on Home
  Assistant commands; the shipping panel config disables power save
  (`power_save_mode: none`) and uses `fast_connect` on hidden networks to
  avoid slow reconnects after a deauth.

## Power and USB

The panel is powered over USB-C. The same USB-C port is used for the initial
firmware flash (serial). After that first flash, all further updates go over
the network via OTA — you don't need to keep the panel connected to a
computer.

## First flash vs. OTA

- **First flash (USB):** required once, to get any Tilehaus firmware onto a
  blank panel. Uses a serial connection over USB-C.
- **OTA (Wi-Fi):** every flash after that, once the panel has joined your
  network. No cable needed.

See [Flashing](flashing.md) for the exact commands for both paths, including
the post-OTA safety window.
