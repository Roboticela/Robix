# IconButton

A round icon button. The web target follows `RefCode/Components/components/IconButton.tsx`. The accessible name is `label`. `title` defaults to that same name.

## Options

| Attribute | Values | Default | Meaning |
| --- | --- | --- | --- |
| id | string | | |
| label | string | required | `aria-label` and the tooltip |
| icon | icon pack name | | The glyph. Any pack icon. An `Icon` or `Svg` child replaces it |
| variant | ghost, subtle, outline, primary, danger | ghost | |
| size | sm, md | md | 32 px or 44 px. Also small and medium |
| badge | number or text | | Danger count at the top end. `0` is hidden. Numbers above 9 show `9+` |
| loading | true, false | false | Same as status loading when status is empty |
| status | idle, loading, success, error | | Controlled state. The glyph hides and the overlay shows |
| action | number or `error` | | Uncontrolled press. A number waits that many milliseconds, then shows success. `error` shows the error mark. Empty means the press does not change status |
| successMs | int | 1600 | How long success or error stays before idle |
| disabled | true, false | false | No pointer, 0.55 opacity |
| title | string | label | Tooltip |

## Variants

| Variant | Rest | Hover | Press | Focus |
| --- | --- | --- | --- | --- |
| ghost | Transparent, primary text | Hover surface | Active surface | Hover surface and the halo |
| subtle | Secondary surface, primary text | Hover surface | Active surface | Hover surface and the halo |
| outline | Transparent, default border, primary text | Hover surface, strong border | Active surface, primary border | Focus border and the halo |
| primary | Brand fill, on-brand text | Brand hover | Brand active | Brand hover and the halo |
| danger | Danger fill, on-brand text | Danger hover | Darker danger | Danger hover and the halo |

The shape is a full circle. Color changes take 150 ms with the cubic curve `0.28, 0.11, 0.32, 1`. Keyboard focus keeps those fills and borders, and draws a soft brand halo about 5 px outside the circle. A mouse click does not show it.

## Status

| Status | Glyph | Overlay |
| --- | --- | --- |
| idle | Visible | None |
| loading | Hidden | 14 px spinner on small, 16 px on medium. Primary and danger use a white tick. The others use a brand tick |
| success | Hidden | Check mark, 16 px or 18 px, 2 px stroke. Primary and danger use the current text color. The others use success |
| error | Hidden | Close mark, same sizes, 2 px stroke. Primary and danger use the current text color. The others use danger |

Loading sets `aria-busy` and disables the control. A controlled `status` does not run the action cycle. An uncontrolled `action` goes to loading, then success or error, then back to idle after `successMs`.

## Pointer, touch, and keyboard

The control is a real `button`. Click, tap, Enter, and Space press it. Disabled and loading do not. Tab moves focus.
