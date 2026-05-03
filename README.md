# ESPHome Hisense/Kelon IR

External ESPHome climate component for Hisense/Kelon air conditioners using the Kelon168 IR protocol.

This component is implemented against ESPHome's `remote_transmitter`, `remote_receiver`, and `remote_base` APIs. It does not use Arduino-only IR libraries.

## Example

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/dipierro/esphome-hisense-kelon-ir
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
  dump: raw

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
