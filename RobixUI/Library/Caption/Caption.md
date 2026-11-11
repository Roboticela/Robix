# Caption

Small supporting text. Size 12, Inter Regular, leading 1.33, color text-tertiary. Used for section names, hints, and metadata. Same on every platform.

## Options

| Attribute | Values | Default |
| --- | --- | --- |
| id | string | |
| tone | primary, secondary, tertiary, disabled, link, danger, success, warning, on-brand | tertiary |
| align | start, center, end, left, right | start |
| wrap | none, char, word | word |
| size | string | 12 |
| color | color | |
| font | string | Inter-Regular |
| content | string | |
| maxWidth | size | |
| position | relative, absolute, fixed | relative |

`Span` children may override size and color.

## Experience and accessibility

Caption is not a heading and not a label for a field. A field hint belongs on TextField `hint`, which uses this tone. Do not use caption as the only name of a control. It does not take pointer or keyboard input. Keep contrast by using the tertiary token rather than a lighter raw color.
