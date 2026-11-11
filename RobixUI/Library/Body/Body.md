# Body

Reading text. `size="md"` is 19 px with leading 1.42. `size="sm"` is 17 px with leading 1.47. Face is Inter Regular. Color is text-secondary.

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
| maxWidth | size | |
| position | relative, absolute, fixed | relative |
| flex, alignSelf | layout | |

`Span` children keep their own size and color.

## Experience and accessibility

Body is a paragraph of running text. It is not a heading and not a control. Mouse, touchpad, and touch do not activate it. There is no shortcut. Keep line length comfortable with `maxWidth` and a content-width token. A link inside the text uses a `Span` with the link tone only when that span is a real link.
