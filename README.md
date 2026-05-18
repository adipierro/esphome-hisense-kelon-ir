# ESPHome Hisense/Kelon AC IR control

External ESPHome climate component for Hisense/Kelon air conditioners using the Kelon168 IR protocol.

This component is implemented against ESPHome's `remote_transmitter`, `remote_receiver`, and `remote_base` APIs. It does not use Arduino-only IR libraries.

⚠️ there are currently some issues with swing mode control and follow-me (iFeel). I plan to fix those before June 2026.

## Acknowledgements
- ESPhome community for ESPhome project
- Leonardo Ascione, David Conran & Davide Depau for [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266/blob/master/src/ir_Kelon.h) which I used for protocol reference
- [@ezhevita](https://github.com/ezhevita) for additional research

## Limitations
Kelon/Hisense protocol does not allow explicitly setting air conditioner's power state. Because of this, `ensure_power` workaround is used – it sends a command that should set the AC to `on` state, so that we could ensure it is on or off by sending another command shortly after.

## Example

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/adipierro/esphome-hisense-kelon-ir
    components: [hisense_kelon_ir]

remote_transmitter:
  id: ir_tx
  pin: GPIOXX
  carrier_duty_percent: 50%

remote_receiver:
  id: ir_rx
  pin:
    number: GPIOYY
    inverted: true
    mode:
      input: true
      pullup: true
  tolerance: 55%
  dump:
    - hisense_kelon_ir
    - raw

climate:
  - platform: hisense_kelon_ir
    id: ac
    name: "Hisense AC"
    transmitter_id: ir_tx
    receiver_id: ir_rx
    ensure_power: smart
```

## Options

- `ensure_power`: `smart`, `super`, or `none`. Defaults to `smart`.
- `receiver_id`: optional remote receiver used to sync state from a physical remote.

## Actions

```yaml
- hisense_kelon_ir.follow_me:
    id: ac
    temperature: !lambda "return x;"
    enabled: true
```

```yaml
- hisense_kelon_ir.display_off:
    id: ac
```

Display-off is intentionally an action, not a persistent entity, because the unit can turn the display back on after any other command.

## Capture Fixtures

Use `scripts/kelon168_fixture.py` to validate decoded 21-byte captures. Fixtures must explicitly declare `bit_order: lsb_first` or `bit_order: msb_first`.
