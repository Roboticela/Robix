# Button

A pill button. The web target follows `RefCode/Components/components/Button.tsx` and `ButtonLabel.tsx`. `destructive` is the same variant as `danger`.

## Options

| Attribute | Values | Default | Meaning |
| --- | --- | --- | --- |
| id | string | | |
| variant | primary, secondary, action, ghost, danger | primary | `destructive` selects danger |
| size | sm, md, lg | md | Also small, medium, large |
| fullWidth | true, false | true | Stretch to the container. `false`, `0`, `no`, and `off` keep the content width |
| loading | true, false | false | Spinner, and the control cannot be pressed |
| loadingText | string | | Replaces the label while loading |
| leading | icon pack name | | Pack icon before the label. Hidden while loading |
| trailing | icon pack name | | Pack icon after the label. Hidden while loading |
| preview | hover, focus, active | | Force that visual state |
| disabled | true, false | false | No press, 0.55 opacity |
| type | button, submit, reset | button | |
| content | string | | Label. The element's own text is the same |
| path | string | | Screen route. `route`, `href`, and `screen` are the same. On the web this is a link. Without it the control stays a button |

Any name in the icon pack works on `leading` and `trailing`. A child can be an `Icon` from that pack or an `Svg` with its own shapes. `slot`, `side`, or `place` set to `leading` / `left` / `start` or `trailing` / `right` / `end` chooses the side. Without a side, the first icon sits on the left and the next one on the right. A sided child replaces the attribute on that side.

## Variants

| Variant | Rest | Hover | Press | Focus |
| --- | --- | --- | --- | --- |
| primary | Brand fill, on-brand text, no border | Brand hover fill | Brand active fill | Soft brand halo |
| secondary | Transparent fill, brand border, brand text | Brand fill and on-brand text | Brand active fill and border | Brand border and the halo |
| action | Light uses the elevated surface and the default border. Dark uses the secondary surface and the strong border. Primary text | Hover surface, tertiary border | Active surface, primary border | Brand border and the halo |
| ghost | Transparent, tertiary text, regular weight | Brand text | Brand active text on the brand subtle surface | Brand border and the halo |
| danger | Danger fill, on-brand text | Danger hover fill | Darker danger fill | Soft brand halo |

Every variant is a fully rounded pill. Disabled and loading use 0.55 opacity and `cursor: not-allowed`. The color change takes 150 ms, linear.

## Sizes

| Size | Height | Horizontal padding | Type |
| --- | --- | --- | --- |
| sm | 32 px | 14 px | 14 px medium |
| md | 44 px | 22 px | 17 px medium |
| lg | 50 px | 24 px | 17 px medium |

Ghost uses regular weight. The other variants use medium. Icons are 16 px with an 8 px gap.

## Loading

Primary and danger spin a white tick on a `#48484A` track. The other variants spin a brand tick on a tertiary track at 35% opacity. The ring is the 16 px loader from the reference. Reduced motion stops the spin. A loading button is disabled. Leading and trailing icons are hidden. `loadingText` replaces the label when it is set.

## Pointer, touch, and keyboard

The control is a real `button` when it has no route. A click, tap, or Enter and Space runs it. Tab moves focus. Disabled and loading do not run. Keyboard focus draws a soft brand halo, about 5 px outside the pill, brighter at the inner edge. A mouse click does not show it.

On the web, `path` (or `route`, `href`, or `screen`) turns the same control into an `a`. The class, size, variant, and paint stay the same. The address is the screen route, written as `#/login` for a path of `/login`, so a crawler and a new tab can follow it. An address that starts with `http://`, `https://`, `mailto:`, or `tel:` is used as written. RoboticelaOS still paints a button.

## Web appearance

These attributes paint in the browser. The Windows and RoboticelaOS paint path keeps the existing button. `primary`, `secondary`, `action`, `ghost`, and `danger` stay the working variants. A design name such as `glass` or `neon` goes on `preset`, or on `variant` when it is not one of those five. A value you set on the button replaces the preset.

| Attribute | Meaning |
| --- | --- |
| widthMode | auto, full, fit, min, max |
| heightMode | auto, content, full |
| size | xs, sm, md, lg, xl, 2xl, 3xl |
| paddingX, paddingY, paddingTop, paddingRight, paddingBottom, paddingLeft | Inner space |
| gap, iconGap | Space between the label and the icons |
| iconPlace | top or bottom stacks the icon |
| iconColor, iconSize, textColor | Icon and label paint |
| gradient, gradientFrom, gradientTo, gradientAngle | Solid gradient fill. `radial` and `conic` select the shape |
| backgroundImage, overlay, glass, backdropBlur, texture | Image, tint, frost, and noise |
| borderStyle and per-side border width and color | Border |
| radius and radiusTopLeft, radiusTopRight, radiusBottomRight, radiusBottomLeft | Corners. Words: none, xs, sm, md, lg, full |
| shape | square, rounded, pill, circle, soft, squircle, capsule, cut, hex, diamond |
| shadow, depth, shadows, shadowX, shadowY, shadowBlur, shadowSpread, shadowColor, shadowInset, glow | Depth |
| font, fontSize, fontWeight, fontStyle, letterSpacing, textTransform, decoration | Label type |
| hoverBackground, hoverColor, hoverIcon, hoverBorder, hoverRadius, hoverShadow, hoverGlow, hoverScale, hoverX, hoverY | Hover, only when that attribute is set |
| activeBackground, activeColor, activeScale, activeShadow | Press |
| focusBackground, focusColor, focusRing, focusOffset, focusGlow | Keyboard focus |
| disabledBg, disabledColor | Disabled |
| selected, selectedBackground, selectedColor | Selected look |
| badgeColor, badgeText | Icon-button badge |
| enter | fade, slide, fade-up, zoom |
| motion | lift, pulse, shake, shimmer |
| duration, delay, easing | Transition |
| density | compact or spacious |
| theme | light, dark, neon, high-contrast |
| chrome | toolbar, titlebar, sidebar, fab, dock |
| accent | top, bottom, left |
| group | start, middle, end |
| preset | glass, flat, elevated, floating, neon, soft, brutalist, gradient, success, warning, info, outline, link, tinted, fluent, macos, material, classic, cyberpunk, luxury, chrome, retro, monochrome, pastel, corporate, industrial, transparent, circular |

Icon button, theme toggle, theme icon toggle, confirm action, and segmented control use the same attributes. On a group, the look is painted on each button.
