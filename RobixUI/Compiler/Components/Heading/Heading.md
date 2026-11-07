# Heading

A title. `level` is `display` or 1 through 6. Weight is Inter SemiBold. Color is text-primary. The web element matches the level: display and level 1 are `h1`, then `h2` through `h6`. RoboticelaOS paints the same sizes.

## Options

| Attribute | Values | Default | Meaning |
| --- | --- | --- | --- |
| id | string | | |
| level | display, 1, 2, 3, 4, 5, 6, h1 through h6 | 1 | |
| tone | primary, secondary, tertiary, disabled, link, danger, success, warning, on-brand | primary | |
| align | start, center, end, left, right | start | |
| wrap | none, char, word | word | |
| size | string | the level size | Overrides the level |
| color | color | | Overrides tone |
| font | string | Inter-SemiBold | |
| content | string | | Or the element text |
| maxWidth, minWidth, minHeight, maxHeight | size | | |
| position | relative, absolute, fixed | relative | |
| flex, alignSelf, order, gridColumn, gridRow | layout | | |

`Span` children may override size and color for part of the line.

## Levels

| Level | Size | Tracking | Leading |
| --- | --- | --- | --- |
| display | 80 | -15 | 1050 |
| 1 | 56 | -15 | 1070 |
| 2 | 40 | -10 | 1100 |
| 3 | 28 | -5 | 1140 |
| 4 | 22 | 0 | 1220 |
| 5 | 19 | 0 | 1260 |
| 6 | 17 | 0 | 1290 |

Tracking and leading are the foundation type numbers for that level. The face stays SemiBold.

## Pointer, keyboard, accessibility

A heading is not a button and is not a tab stop. Do not attach click behavior to it. The first heading on a screen names that screen. Levels stay in order. Display is the largest title and still occupies the `h1` slot on the web, so a screen has one display or one level-1 heading, not both.
