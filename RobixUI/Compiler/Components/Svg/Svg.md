# Svg

A drawing written in XML. The build stores it in the icon pack and the app refers to it by name. Use `Svg` for a one-off mark. Use `Icon` for a mark that already lives in `Assets/Icons`.

An inline id must not match an asset icon name. Two inline drawings in one app must not share an id.

## Options

| Attribute | Values | Default | Meaning |
| --- | --- | --- | --- |
| id | string | | Pack name when `name` is empty |
| name | string | | Pack name |
| size | int | 24 | |
| width, height | size | | |
| viewBox | string | | SVG view box |
| minWidth, minHeight, maxWidth, maxHeight | size | | |
| x, y | int | 0 | |
| opacity | int | | 0 through 100 |
| fill, stroke, accent, color | color | | Same slots as Icon |
| position, inset, margin, flex, alignSelf, order, gridColumn, gridRow | layout | | |

Children are SVG shapes: `path`, `circle`, `rect`, `line`, `polyline`, `polygon`, `ellipse`, and `g`. A drawing with no shapes warns.

## Experience

The shapes keep the attributes written on them. `fill`, `stroke`, and `accent` on the `Svg` element recolor the Robo slots. The result is a square icon of `size` pixels. It does not scroll and it does not theme itself beyond the color slots and the current light or dark tokens used in those colors.

## Pointer, keyboard, accessibility

The drawing is decorative and not a tab stop. Pointer events belong to the parent control. Give the parent a text name. Do not put instructions into the SVG title and expect the field or tile to repeat them.
