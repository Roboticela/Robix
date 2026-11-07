# Container

The layout box. It is the same flex, grid, stack, and canvas model on every platform. It draws no border and no focus ring unless the XML sets a border or a background.

## Options

| Attribute | Values | Default | Meaning |
| --- | --- | --- | --- |
| id | string | | |
| display | flex, grid, stack, canvas | flex | stack is a column. canvas uses x and y |
| direction | row, column, row-reverse, column-reverse | column | `flexDirection` is the same |
| justify | start, center, end, space-between, space-around, space-evenly | start | `justifyContent` is the same |
| align | start, center, end, stretch | stretch | `alignItems` is the same |
| alignContent | start, center, end, space-between, space-around, space-evenly | start | |
| justifyItems | start, center, end, stretch | stretch | |
| wrap | wrap, nowrap | nowrap | `flexWrap` is the same |
| columns, rows | int | | Grid |
| gap, rowGap, columnGap | length | gap 0 | `colGap` is `columnGap` |
| padding, paddingTop, paddingRight, paddingBottom, paddingLeft | length | 0 | Longhands override |
| width, height | size | auto | Exact px, percent, or auto |
| minWidth, minHeight, maxWidth, maxHeight | size | | |
| minSize, maxSize | two lengths, or one for both | | |
| size | exact, auto, dynamic, fill, min, max | | exact uses width and height. auto fits content. dynamic grows. fill is 100% |
| x, y | int | 0 | |
| position | relative, absolute, fixed | relative | |
| inset | one to four lengths | | CSS inset |
| left, top, right, bottom, margin, marginTop, marginRight, marginBottom, marginLeft | length | | |
| flex | int | | `flexGrow` and `grow` are the same |
| flexShrink | number | | `shrink` is the same. `0` stays unitless |
| alignSelf | auto, start, center, end, stretch | auto | |
| order | int | 0 | |
| gridColumn, gridRow | string | | `auto`, `span N`, or `start / end` |
| overflow | visible, hidden, clip, scroll, auto | visible | Both axes |
| overflowX, overflowY | same | | Override the shorthand. Hyphenated names match |
| clip | true, false | false | true means overflow clip |
| background | color | | `backgroundColor` and `bg` are the same |
| fill | true, false | false | `fillParent` is the same |

Children: any component, up to 64.

Color, background, border color, type size, weight, tracking, leading, gap, padding, margin, radius, content width, and gutter use RoboFoundation tokens. A direct value still compiles and the compiler warns. `id` is not part of that warning.

## Web

These attributes paint in the browser only. RoboticelaOS keeps the layout box above and does not draw the extra surface, motion, or chrome.

| Group | Attributes |
| --- | --- |
| Size | `widthMode` (`fixed`, `auto`, `full`, `fit`, `min`, `max`, `content`, `viewport`), `heightMode` (same, viewport is `100vh`), `aspectRatio` (`1:1`, `4:3`, `3:2`, `16:9`, `21:9`, `9:16`, or a custom ratio), `sizing` (`content`, `equal`, `intrinsic`, `fluid`, `proportional`) |
| Space | `marginX`, `marginY`, `paddingX`, `paddingY`, `spacing` (gap when `gap` is empty). Negative lengths are allowed |
| Layout | `display` also accepts `block`, `inline`, `inline-flex`, `horizontal`, `vertical`, `overlay`, `layered`, `absolute`. `place` (`top-left` through `bottom-right`), `reverse`, `layout`, `columns`, `rows` |
| Background | `gradient` (`linear`, `radial`, `conic`, or a CSS gradient), `gradientFrom`, `gradientTo`, `gradientAngle`, `gradientStops`, `backgroundImage`, `backgroundSize`, `backgroundPosition`, `backgroundRepeat`, `backgroundVideo`, `backgroundOpacity`, `overlay`, `glass`, `frost`, `backdropBlur`, `texture` (`noise`, `pattern`, `mesh`) |
| Border and shape | `borderStyle` (`none`, `solid`, `dashed`, `dotted`, `double`, `groove`, `ridge`, `inset`, `outset`, `gradient`), per-side `borderTopWidth` and `borderTopColor` (right, bottom, left too), `borderGradient`, `radiusTopLeft`, `radiusTopRight`, `radiusBottomLeft`, `radiusBottomRight`, `shape` (`square`, `rounded`, `pill`, `circle`, `squircle`, `cut`, `notch`), `clipPath` |
| Depth | `shadow` (`none`, `subtle`, `soft`, `medium`, `large`, `xl`, `floating`, `ambient`, `inner`, `glow`, `neon`), `shadowX`, `shadowY`, `shadowBlur`, `shadowSpread`, `shadowColor`, `shadowInset`, `shadows`, `depth`, `glow`, `innerGlow` |
| Effects | `brightness`, `contrast`, `saturation`, `grayscale`, `sepia`, `hue`, `invert`, `foregroundBlur`, `filter`, `blend`, `opacity` |
| Type | `textColor`, `headingColor`, `paragraphColor`, `linkColor`, `fontSize`, `fontWeight`, `lineHeight`, `letterSpacing`, `textAlign`, `textDecoration`, `textTransform`, `textWrap`, `textOverflow`, `selectionColor`, `placeholderColor` |
| States | `hoverBackground`, `hoverBorderColor`, `hoverBorderWidth`, `hoverRadius`, `hoverShadow`, `hoverGlow`, `hoverOpacity`, `hoverBrightness`, `hoverBlur`, `hoverScale`, `hoverRotate`, `hoverX`, `hoverY`, `hoverColor`, `focusBorderColor`, `focusOutline`, `focusOffset`, `focusGlow`, `focusShadow`, `focusBackground`, `activeBackground`, `activeBorder`, `activeShadow`, `activeScale`, `activeOpacity`, `disabled`, `selected` |
| Motion | `enter` (`fade`, `fade-up`, `fade-down`, `fade-left`, `fade-right`, `zoom`, `zoom-out`, `slide`, `flip`, `rotate`, `bounce`, `blur`), `motion` (`lift`, `scale`, `tilt`, `rotate`, `glow`, `pulse`, `shake`, `float`), `duration`, `delay`, `easing`, `iteration`, `transition` |
| Presets | `preset` (`minimal`, `elevated`, `glass`, `dark`, `neumorphic`, `brutalist`, `neon`, `organic`, `material`, `fluent`, `windows`, `macos`, `classic`, `acrylic`, `mica`, `liquid`, `modern`, `luxury`, `editorial`, `retro`, `cyberpunk`, `industrial`, `soft`, `flat`, `skeuomorphic`, `monochrome`, `pastel`, `gradient`, `corporate`), `theme` (`light`, `dark`), `density` (`compact`, `comfortable`, `spacious`) |
| Desktop chrome | `chrome` (`window`, `panel`, `floating`, `card`, `sidebar`, `docked`, `split`), `titlebarColor`, `accent` (`top`, `bottom`, `left`, `right`), `accentColor`, `scrollbarWidth`, `scrollbarThumb`, `scrollbarTrack`, `scrollbarHover` |

An author value on the element wins over a preset. Hover, focus, and active styles apply only for the attributes that are set. Focus uses `:focus-visible`.

## Experience

Padding is inside the border box. Gap is between children and is not added outside the first or last child. A fixed child is taken out of the flow and is not clipped when the container clips. Overflow `scroll` and `auto` clip on RoboticelaOS. The matching scroll feel, thin bar, and keyboard rules live on ScrollView.

## Pointer, keyboard, accessibility

The container is not a control. It is not a tab stop. Mouse, touchpad, and touch hit the child under the pointer. There is no keyboard shortcut. Do not put `tabIndex` on it. If the box is only for layout, leave it without a name. If it groups a form, the label belongs on the field, not on the box.
