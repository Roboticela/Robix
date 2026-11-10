# Text

Rich text. The words are the element content or `content`. A `Span` child keeps its own size and color. The same roles, tones, and wraps are used on RoboticelaOS and on the web.

Text does not take keyboard focus. It is selected with the platform text selection when the surface allows it. It has no border and no hover chrome.

## Options

| Attribute | Values | Default | Meaning |
| --- | --- | --- | --- |
| id | string | | |
| variant | display, h1, h2, h3, h4, h5, h6, subhead, content, content-sm, body, body-sm, caption, eyebrow, label | content | Role. Prefer Heading, Body, and the other named tags |
| tone | primary, secondary, tertiary, disabled, link, danger, success, warning, on-brand | from the role | Color token |
| align | start, center, end, left, right | start | left is start, right is end |
| wrap | none, nowrap, char, word, balance, pretty | word | `textWrap` is the same attribute |
| size | px, sm, md, or a layout size | from the role | |
| color | color | | Overrides tone |
| font | face | from the role | Inter faces: sans, sans-medium, sans-semibold, sans-bold, sans-italic |
| content | string | | Used when the element has no text child |
| maxWidth, minWidth, minHeight, maxHeight, minSize, maxSize | size | | |
| x, y, width, height | int | | |
| position, inset, left, top, right, bottom, margin | layout | | |
| flex, flexShrink, alignSelf, alignItems, order | layout | | |
| display, direction, justify, justifyItems, columns, rows, gap, gridColumn, gridRow | layout | | |

`Span` children, up to 48. A span may set the same web type properties as the parent. A property set on a parent Text, Heading, Paragraph, Body, Caption, Eyebrow, Label, Subhead, or Container passes down to nested text. A child that sets the same property keeps its own value. Role size and weight stay in place until `fontSize`, `fontWeight`, `letterSpacing`, or `lineHeight` is set.

## Web

The web renderer and the platform window use the same wrapping attributes. `wrap` chooses the break, `whiteSpace` keeps or collapses spaces and newlines, `lines` limits the block, and `textOverflow` chooses the ellipsis or a hard clip.

| Group | Attributes |
| --- | --- |
| Family and weight | `font` (`system`, `sans`, `serif`, `mono`, `display`, `handwriting`, `condensed`, or a custom stack), `fontWeight` (`thin` through `black`, or 100–900), `fontStyle` (`normal`, `italic`, `oblique`) |
| Size | `fontSize` (`xs` through `7xl`, `fluid`, or a length), `fontSizeMin` and `fontSizeMax`, `fontSizeSm`, `fontSizeLg` |
| Spacing | `letterSpacing` (`tighter` through `widest`), `wordSpacing`, `lineHeight` (`compact`, `normal`, `relaxed`, `loose`), `paragraphSpacing`, `textIndent` |
| Alignment and flow | `textAlign` (`start`, `center`, `end`, `justify`), `verticalAlign`, `wrap` (`word`, `char`, `nowrap`, `balance`, `pretty`), `lines`, `textOverflow` (`ellipsis`, `clip`), `fade`, `direction` (`ltr`, `rtl`, `auto`), `writingMode` (`vertical`) |
| Color and fill | `color`, `headingColor`, `paragraphColor`, `linkColor`, `strongColor`, `mutedColor`, `gradient`, `gradientFrom`, `gradientTo`, `gradientAngle`, `backgroundImage`, `effect` (`glow`, `neon`, `gold`, `silver`, `chrome`, `holographic`, `glass`, `emboss`, `engrave`, `retro`, `3d`) |
| Decoration | `decoration` (`underline`, `overline`, `line-through`, `wavy`, `dotted`, `dashed`, `double`), `decorationColor`, `decorationThickness`, `decorationOffset`, `textTransform`, `smallCaps` |
| Depth | `shadow` (`soft`, `hard`, `long`, `glow`, `3d`), `shadowX`, `shadowY`, `shadowBlur`, `shadowColor`, `shadows`, `outline`, `outlineColor` |
| Highlight | `mark` (`soft`, `pill`, `dark`, `block`, `underline`), `markColor`, `paddingX`, `paddingY`, `radius` |
| States | `hoverColor`, `hoverBackground`, `hoverWeight`, `hoverTracking`, `hoverDecoration`, `hoverShadow`, `hoverGlow`, `focusColor`, `focusOutline`, `activeColor`, `activeScale`, `disabled`, `selected`, `selectionColor` |
| Motion and presets | `enter` (`fade`, `fade-up`, `fade-down`, `slide`, `zoom`, `blur`, `typewriter`), `motion` (`pulse`, `shimmer`, `float`, `blink`), `duration`, `delay`, `easing`, `preset` (`quote`, `code`, `lead`, `footnote`, `numeric`, `link`), `density`, `theme`, `dropCap`, `ligatures`, `numerals`, `features` |

The same attributes on a Container set inherited type for the text inside it. `headingColor` reaches headings. `paragraphColor` reaches paragraphs and body copy. A nested Heading can still set its own `color` or `fontSize`.

## Roles

| Role | Size | Face | Tone |
| --- | --- | --- | --- |
| display | 80 | SemiBold | primary |
| h1 | 56 | SemiBold | primary |
| h2 | 40 | SemiBold | primary |
| h3 | 28 | SemiBold | primary |
| h4 | 22 | SemiBold | primary |
| h5 | 19 | SemiBold | primary |
| h6 | 17 | SemiBold | primary |
| subhead | 24 | Regular | secondary |
| content, body | 19 | Regular | secondary |
| content-sm, body-sm | 17 | Regular | secondary |
| caption | 12 | Regular | tertiary |
| eyebrow | 12, tracked, uppercase | Regular | tertiary |
| label | 17, line height 1 | Medium | primary |

Heading tracking and leading are the level defaults in `Heading.md`.

## Pointer, keyboard, accessibility

Clicks pass through unless the text is a link tone inside an actual link. There is no shortcut. Use the named heading tags so the page outline matches the visual levels. Do not skip from a page title to a low heading. Tone `disabled` is visual only. It does not remove the text from the page. A link tone still needs a real link target.
