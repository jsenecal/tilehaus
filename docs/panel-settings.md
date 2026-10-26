# Panel settings

Each panel's behaviour is set from Home Assistant, not from the deck: the
layout is per deck, but brightness, timeouts and night mode belong to the
panel on the wall. The ESPHome integration adds these entities to the panel's
device page. Every setting survives reboots and reflashes.

The panel's device page also has a **Visit** link, which opens its
configurator in the browser. The firmware's `tilehaus_web` component reports
the configurator's port to Home Assistant. Don't add ESPHome's own
`web_server`: the configurator already holds port 80, and the build refuses
the two together.

## Screen and idle

| Entity | Default | What it does |
|---|---|---|
| **Page timeout** | 0 s (off) | After this long untouched, the panel closes any open dialog and returns to the first page. |
| **Dim timeout** | 0 s (off) | After this long untouched, the screen drops to **Dim brightness**. |
| **Sleep timeout** | 0 s (off) | After this long untouched, the screen drops to **Sleep brightness**, and the screensaver shows if one is set. |
| **Brightness** | 100 % | The awake level. |
| **Dim brightness** | 25 % | The dimmed level. |
| **Sleep brightness** | 0 % | The asleep level; 0 turns the backlight off. |
| **Screensaver** | None | **Clock** shows a full-screen clock while asleep. |
| **Wake dimmed** (switch) | off | A touch on a dark screen brings it up at **Dim brightness** instead of full, and the next touch goes to full. Useful at night. Needs a Dim brightness above 0. |
| **Presence sensor** | empty | An occupancy sensor that wakes the panel. See [Presence wake](#presence-wake). |
| **Wake** (button) | | Wakes the screen and returns to the first page. |
| **In use** (sensor) | | On while the panel has been touched within the dim timeout. |

The three timeouts are independent. Each one counts from the last touch, so a
panel with a 30 s dim and a 120 s sleep dims at 30 s and sleeps at 120 s. A
touch on a dark screen or the screensaver only wakes it; it doesn't press the
tile underneath. A dimmed screen takes the touch as usual.

## Presence wake

Set **Presence sensor** to the entity id of an occupancy or motion sensor in
the room, such as `binary_sensor.office_presence`, then press the panel's
**Restart** button. The panel reads the setting when it starts, so a change
takes effect after the restart.

With a sensor set:

- **Arriving wakes the screen.** It comes back to full brightness and hides
  the screensaver, without changing the page.
- **While the sensor reads on, the panel neither dims nor sleeps.** The page
  timeout still applies.
- **Leaving starts the countdown.** The dim and sleep timeouts count from the
  moment the sensor cleared, not from the last touch.

Only `on` counts as present. A sensor that goes `unavailable` or `unknown`
releases the screen rather than holding it awake. Leave the field empty to
turn presence wake off.

## Night mode

The **Night mode** switch darkens the page behind the tiles and lightens its
text; the tiles themselves are unchanged. It applies at once, without a
reboot.

Tilehaus has no built-in schedule: Home Assistant already has one. A single
automation can switch every panel, for example at sunset and sunrise:

```yaml
triggers:
  - trigger: sun
    event: sunset
    id: sunset
  - trigger: sun
    event: sunrise
    id: sunrise
actions:
  - choose:
      - conditions: [{ condition: trigger, id: sunset }]
        sequence:
          - action: switch.turn_on
            target:
              entity_id: [switch.office_panel_night_mode, switch.lobby_panel_night_mode]
      - conditions: [{ condition: trigger, id: sunrise }]
        sequence:
          - action: switch.turn_off
            target:
              entity_id: [switch.office_panel_night_mode, switch.lobby_panel_night_mode]
```

Use your panels' own switch ids. A time trigger or a **Schedule** helper works
the same way. The same automation can also lower **Brightness** at night.

## Other entities

| Entity | What it does |
|---|---|
| **Accent** (light) | Overrides the deck's accent colour live. Off returns to the deck's own colour. |
| **Configurator** (switch) | Allows saving from the configurator; see [Config API](config-api.md). |
| **Configurator status** | Who holds the configurator session. |
| **Guest Wi-Fi name** / **password** | The network a Wi-Fi share tile shows as a QR code. |
| **Current page** | The page the panel is showing. |
| **Restart** (button) | Restarts the panel. |

The `show_page` action (`esphome.<panel>_show_page`, with a `page` name)
switches the panel to a page by name. It counts as activity, so it also wakes
the screen.
