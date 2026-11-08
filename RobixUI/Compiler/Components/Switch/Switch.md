# Switch

A pill switch. The web target follows `RefCode/Components/components/Switch.tsx`. `SwitchGroup` is the divided list. `SwitchItem` is one row: title, optional description, and the switch.

## Options

| Attribute | Values | Default | Meaning |
| --- | --- | --- | --- |
| id | string | | |
| label | string | | Accessible name. A `SwitchItem` title is the name of its switch |
| variant | primary, subtle, outline, ghost, danger | primary | Track color |
| size | sm, md, lg | md | Also small, medium, large |
| checked | true, false | false | On. `1`, `yes`, and `on` are the same |
| disabled | true, false | false | No press, 0.55 opacity |
| loading | true, false | false | Brand spinner in the thumb. The control cannot be pressed |

`SwitchGroup` takes `label` as the group name. `SwitchItem` takes `title`, `description`, and the same variant, size, checked, disabled, and loading values.

## Variants

| Variant | Off | On |
| --- | --- | --- |
| primary | Light uses the default border. Dark uses the active surface | Brand fill |
| subtle | Tertiary surface | Brand fill |
| outline | Transparent, 1 px default border | Brand fill and brand border |
| ghost | Transparent track. The thumb uses the elevated surface and a 1 px default border | Brand fill. The thumb is white |
| danger | Same off track as primary | Danger fill |

The thumb is white, except the ghost off state. The shape is a full pill. Color and the thumb move take 200 ms with the cubic curve `0.28, 0.11, 0.32, 1`. Reduced motion snaps.

## Sizes

| Size | Track | Thumb | Travel |
| --- | --- | --- | --- |
| sm | 44 × 26 px | 22 px | 18 px |
| md | 51 × 31 px | 27 px | 20 px |
| lg | 60 × 36 px | 32 px | 24 px |

The thumb sits 2 px inside the track.

## Press, loading, and disabled

A press stretches the thumb to 1.18 on the horizontal axis. The origin is the left edge while off and the right edge while on. The value changes on pointer down. Releasing, leaving, or canceling restores the thumb width and does not change the value again. Keyboard Space and Enter toggle on click.

Disabled uses `cursor: not-allowed` and 0.55 opacity. Loading keeps full opacity, uses `cursor: wait`, and shows the brand loader. Small uses a 12 px ring. Medium and large use 14 px. The track is tertiary at 35% and the tick is brand. Reduced motion stops the spin.

## Group

`SwitchGroup` is a full-width column. A 1 px default border separates the rows. A `SwitchItem` is at least 56 px tall, with 12 px of vertical padding and 16 px between the copy and the switch. The title is 17 px. The description is 12 px tertiary. Pressing the row toggles the switch. Pressing the switch itself toggles once. A disabled or loading row does not toggle.

## Pointer, touch, and keyboard

The control is a `button` with `role="switch"` and `aria-checked`. A click, tap, or Space and Enter toggles it. Tab moves focus. The focus ring is a 2 px brand outline, 2 px outside the track. It is not a drop shadow. Disabled and loading do not toggle.
