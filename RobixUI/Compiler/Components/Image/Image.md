# Image

A picture on the web page. The Windows window does not paint it.

`Source` is the file or address. A path is read next to the `.rbxu` document and copied into the web package as `images/<file>`. An `http`, `https`, `data`, or `blob` address is used as written. A missing file is left as written and the compiler warns.

## Options

| Attribute | Values | Default | Meaning |
| --- | --- | --- | --- |
| id | string | | |
| source | path or URL | | `src` is the same |
| alt | string | empty | `description` is the same. Empty alt is decorative |
| title | string | | Tooltip |
| caption | string | | Line under the picture. `label` is the same |
| path | route | | Wraps the picture in a link. `href` is the same. A path of `/products` becomes `#/products` |
| srcSet, sizes | string | | Responsive sources |
| load | content, ready, lazy | content | `content` fetches the picture with the page. `ready` waits until the page has finished loading. `lazy` waits until the picture is near the viewport. `when` is the same |
| resize | true, false | true | During `robixc` web compilation, a local picture is also written at 320, 480, 640, 768, 960, 1280, 1600, and 1920 pixels wide when the source is wider than that size. The browser picks one. `false`, `off`, or `none` keeps the original file only. The interpreter copies the original and does not resize |
| decoding | async, sync, auto | async | |
| fetchPriority | high, low, auto | | `priority` is the same |
| crossOrigin | anonymous, use-credentials | | |
| referrerPolicy | string | | |
| draggable | true, false | | |
| fit | contain, cover, fill, none, scale-down | contain | `objectFit` is the same |
| objectPosition | center, edges, corners, or a CSS position | | `position` is the layout position when it is relative, absolute, fixed, or sticky. Use `objectPosition` for the picture's anchor |
| aspectRatio | 1:1, 4:3, 3:2, 16:9, 21:9, 9:16, or a custom ratio | | `ratio` is the same |
| width, height, minWidth, minHeight, maxWidth, maxHeight | size | | The frame. The picture fills that frame |
| radius | none, xs, sm, md, lg, xl, full, or a length | | Corners of the picture. `borderRadius` is the same. Each corner can be set with `radiusTopLeft`, `radiusTopRight`, `radiusBottomRight`, and `radiusBottomLeft` |
| shape | square, rounded, pill, circle, squircle | | Used when `radius` is empty |
| border, borderColor, borderStyle | same as Container | | A border around the picture. Each side has `borderTopWidth` and `borderTopColor`, and the same for right, bottom, and left |
| shadow | none, subtle, soft, medium, large, xl, floating, glow, or a CSS shadow | | `depth` is the same. `shadowX`, `shadowY`, `shadowBlur`, `shadowSpread`, `shadowColor`, and `shadows` work the same way as on a Container |
| background | color | | Shown behind a transparent picture. `gradient`, `gradientFrom`, and `gradientTo` also work |
| opacity | number | | |
| filter | CSS filter | | Also `brightness`, `contrast`, `saturation`, `grayscale`, `sepia`, `hue`, `blur`, and `invert` |
| hoverRadius, hoverBorderColor, hoverShadow, hoverScale, hoverOpacity, hoverRotate, hoverX, hoverY | | | The same hover surface as a Container. `hoverBrightness` and the other filter names with a `hover` prefix work too |
| disabled | true, false | false | 0.55 opacity and no pointer |
| layout | margin, padding, flex, order, display, x, y | | Same box attributes as a container |

Screen-size suffixes work on these properties: `Width.sm="100%"`, `Fit.mx="cover"`, `Radius.735="24"`.

Motion attributes work here too: `Animate`, `Initial`, `Exit`, `WhileHover`, `WhileTap`, `WhileFocus`, `WhileInView`, `Transition`, `Drag`.

## Pointer, keyboard, accessibility

The picture is not a control. A `path` makes the whole frame a link, and Tab reaches that link. `alt` is the accessible name. Leave it empty only when the picture adds nothing a nearby sentence does not already say. `disabled` removes the pointer. It does not remove the picture from the page.
