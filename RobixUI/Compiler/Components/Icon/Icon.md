# Icon

An SVG from `Assets/Icons`. The app pack contains only the names the app uses. The tag can be the icon name, as in `<Home />`, or `<Icon name="Settings" />`. Both forms are the same component on every platform.

## Options

| Attribute | Values | Default | Meaning |
| --- | --- | --- | --- |
| id | string | | |
| name | string | the tag name | Required when the tag is `Icon` |
| size | int | 24 | Width and height |
| width, height | size | | Override size |
| minWidth, minHeight, maxWidth, maxHeight | size | | |
| x, y | int | 0 | |
| opacity | int | | 0 through 100 |
| color | color | | Alias of fill |
| fill | color | | Replaces `#ROBO_FILL` |
| stroke | color | | Replaces `#ROBO_STROKE` |
| accent | color | | Replaces `#ROBO_ACCENT` |
| strokeWidth | float | | Reserved. The cooked SVG keeps its own width |
| position, inset, left, top, right, bottom, margin | layout | | |
| flex, alignSelf, order | layout | | |
| display, direction, justify, alignItems, justifyItems, columns, rows, gap, gridColumn, gridRow | layout | | |

No children. Drawing belongs in the SVG asset, or in an `Svg` element.

## Experience

The icon is a square of `size` pixels. Fill, stroke, and accent are the only recolor slots. Missing slots keep the cooked color. The graphic is crisp at the stated size and does not animate unless a parent, such as the Dock, scales the tile.

## Pointer, keyboard, accessibility

An icon is decorative. It does not take focus, clicks, or keys. The control that contains it supplies the name: a dock tile uses `label`, a pane row uses `label`, a field uses its label. Do not use an icon as the only name of a button. If the icon is the whole control, the parent still needs a text name.
