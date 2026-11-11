# Paragraph

The same component as Body. The tags `Paragraph` and `P` both compile to body text. `size="md"` is 19 px. `size="sm"` is 17 px. Face is Inter Regular. Color is text-secondary.

## Options

| Attribute | Values | Default |
| --- | --- | --- |
| id | string | |
| size | md, sm, medium, small | md |
| tone | primary, secondary, tertiary, disabled, link, danger, success, warning, on-brand | secondary |
| align | start, center, end, left, right | start |
| wrap | none, char, word | word |
| color | color | |
| font | string | Inter-Regular |
| content | string | |

`Span` children may set size and color.

## Experience and accessibility

One paragraph is one block. Margins come from the parent gap, not from a built-in indent. It does not take focus or shortcuts. On the web it is a `p`. On RoboticelaOS it paints with the body metrics. Do not nest a heading inside it.
