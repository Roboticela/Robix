# Label

A short name. Size 17, line height 1, Inter Medium, color text-primary. This is the text label, not the TextField label attribute. Use it for dock tooltips, row names, and standalone names.

## Options

| Attribute | Values | Default |
| --- | --- | --- |
| id | string | |
| tone | primary, secondary, tertiary, disabled, link, danger, success, warning, on-brand | primary |
| align | start, center, end, left, right | start |
| wrap | none, char, word | word |
| size | string | 17 |
| color | color | |
| font | string | Inter-Medium |
| content | string | |
| maxWidth | size | |
| position | relative, absolute, fixed | relative |

`Span` children may override size and color.

## Experience and accessibility

A label component is text. It does not focus and it does not toggle anything. To name a TextField, set that field's `label` attribute so the name and the control stay connected. A dock tooltip uses Label as the visible name of the tile. The tile's `label` attribute is the same string. Do not put a click handler on the text.
