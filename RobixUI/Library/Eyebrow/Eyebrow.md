# Eyebrow

The small uppercase line above a title. Size 12, tracking +0.05 em, uppercase, Inter Regular, color text-tertiary.

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

`Span` children may override size and color. Uppercase still applies to the whole line.

## Experience and accessibility

Eyebrow is a visual kicker. The heading under it is the accessible name of the section. Do not replace the heading with an eyebrow. It is not a button, has no hover, and has no shortcut. Write the source in normal case. The component draws the uppercase.
