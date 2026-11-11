# Subhead

The line under a title. Size 24, Inter Regular, leading 1.33, color text-secondary. Same metrics on every platform.

## Options

| Attribute | Values | Default |
| --- | --- | --- |
| id | string | |
| tone | primary, secondary, tertiary, disabled, link, danger, success, warning, on-brand | secondary |
| align | start, center, end, left, right | start |
| wrap | none, char, word | word |
| size | string | 24 |
| color | color | |
| font | string | Inter-Regular |
| content | string | |
| maxWidth | size | |
| position | relative, absolute, fixed | relative |
| flex, alignSelf | layout | |

`Span` children may change size and color for part of the line.

## Experience and accessibility

Subhead is body-weight text, not a heading level. Do not use it as the page title. It does not take focus, clicks, or shortcuts. Place it directly under the heading it supports. Wrapping is by word. `wrap="none"` stays on one line and may clip at `maxWidth`.
