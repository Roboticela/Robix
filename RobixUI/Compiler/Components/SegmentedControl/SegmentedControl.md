# Segmented control

A single-choice track. The web target follows `RefCode/Components/components/SegmentedControl.tsx`. Each choice is a `Segment`. `SegmentedToolbar` lines controls up, loose or attached.

## Options

| Attribute | Values | Default | Meaning |
| --- | --- | --- | --- |
| id | string | | |
| label | string | | Caption above the track. It names the group |
| value | segment value | first segment | The selected choice |
| variant | subtle, outline, ghost, primary, pill | subtle | Track and thumb |
| size | sm, md, lg | md | Also small, medium, large |
| content | label, icon, both | label | What each segment shows |
| mode | inset, grouped | inset | Sliding thumb, or a fill on the selected segment |
| fullWidth | true, false | false | Stretch to the container |
| disabled | true, false | false | No press, 0.55 opacity |
| leading | icon pack name | | Icon before the track. A child `Icon` or `Svg` with `slot="leading"` is the same |
| trailing | icon pack name | | Icon after the track. `slot="trailing"` is the same |
| action | number or `error` | | After a choice, wait that many milliseconds, then show success. `error` shows the error mark |
| successMs | int | 1600 | How long success or error stays |

`Segment` takes `value`, `label`, `icon`, `badge`, `disabled`, `loading`, `status`, and `action`. `status` is `idle`, `loading`, `success`, or `error`. A set status does not run `action`. An `Icon` or `Svg` child replaces the pack icon. The label is required and is the accessible name when the content is icon only.

`SegmentedToolbar` takes `attached`. `true` joins the controls into one bordered bar. `false` wraps them with an 8 px gap.

## Variants

| Variant | Track | Thumb | Dividers |
| --- | --- | --- | --- |
| subtle | Tertiary surface | Elevated surface. Dark mixes the tertiary surface 72% with white 28% | Yes |
| outline | Transparent, 1 px default border | Secondary surface. Dark uses the same mix | Yes |
| ghost | Transparent | Tertiary surface. Dark uses the same mix | No |
| primary | Tertiary surface | Brand | Yes |
| pill | Tertiary surface, full pill | Same thumb as subtle, full pill | Yes |

The selected label is medium and primary text. Primary's selected label is on-brand. Idle labels are regular and secondary, and they turn primary on hover. A press that is not selected fades to 0.70. Motion is 300 ms for the thumb and 200 ms for color, with the cubic curve `0.28, 0.11, 0.32, 1`. Reduced motion snaps.

## Sizes

| Size | Height | Radius | Type | Padding |
| --- | --- | --- | --- | --- |
| sm | 36 px | 10 px, thumb 8 px | 15 px | 12 px |
| md | 44 px | 12 px, thumb 10 px | 17 px | 16 px |
| lg | 50 px | 12 px, thumb 10 px | 17 px | 20 px |

Inset mode pads the track by 2 px and slides one thumb. Grouped mode has no inset and paints the thumb fill on the selected segment. Icon-only segments are at least the track height. Labeled segments are at least 72 px unless the control is full width.

## Status

Loading, success, and error hide the label and show an overlay. The spinner is 14 px on small and 16 px on medium and large. On a selected primary segment the tick is white on `#48484A`. The other segments use a brand tick on tertiary at 35%. Success is the check mark. Error is the cross. Both are 16 px or 18 px. On a selected primary segment they use the current text color. Otherwise success is the success color and error is danger.

## Pointer, touch, and keyboard

The track is a `radiogroup`. Each segment is a `radio`. Click or tap selects it and runs `action` when one is set. Arrow keys move between enabled segments and do not scroll the page. Home and End jump to the ends. Enter and Space run the selected segment. Tab lands on the selected segment. Disabled segments are skipped. The focus ring is a 2 px brand outline, drawn 2 px inside the segment. It is not a drop shadow.
