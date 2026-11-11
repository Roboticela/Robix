# RobixUI — Component Plan

A screen is a small set of primitives the compiler paints, plus library components written as `.rxu` documents on top of those primitives. `Button`, `Card`, `Checkbox`, `Dropdown`, `Modal`, and `Navbar` are library components. They are never compiler primitives.

Every component has its own attributes. It also inherits the attributes of the components and parts it is built from, and that inheritance is transitive. A screen built from `TextField` can set `CursorColor` because `TextField` is built from `Input`, and `Input` owns the caret.

A checkbox is filled only when that item compiles, runs, and its test was executed. A header, a schema file, or a document does not count.

Targets are the native window host, the RoboticelaOS framebuffer (`RoboGPU`), and the web. The web target emits semantic HTML from the same component tree. There is no `WebView` primitive and no second set of web components.

Document tags stay the short names below (`Screen`, `View`, `Button`). C++ types use the `Robo` prefix (`RoboScreen`, `RoboView`, `RoboButton`). Compiler units live in `RobixUI/Compiler/Components/<Tag>/`. Library units live in `RobixUI/Library/`. Every new source file has a test beside it.

---

## 1. Three tiers

The compiler already records this split in `RobixUI/Compiler/ComponentTier.hpp`.

| Tier | Where it lives | What it is |
| :--- | :--- | :--- |
| Compiler | C++ in `RobixUI/Compiler` | A primitive. It paints, hits, and talks to input and the framebuffer. |
| Part | Inside one primitive | A piece that has no tag of its own. `Span` belongs to `Text`. The caret belongs to `Input`. The thumb belongs to `ScrollView`. |
| Library | `.rxu` under `RobixUI/Library` | A component expanded by `RoboUICompose` before paint. It is built from primitives and from library components of earlier stages. |

A new tag is a library component. A new compiler primitive is added only when no combination of the primitives below can do the job. Adding the primitive comes first, in its own stage, with a test. The library component that needed it comes after.

`RoboUICompose` already expands `Import`, `Component`, `Slot`, `Data`, `Let`, `For`, `If`, and `{expressions}`. Library stages use that path. They do not add a C++ class per control.

---

## 2. What the compiler owns

### 2.1 Visual primitives

These are the only tags the painter knows.

| Tag | C++ type | Job |
| :--- | :--- | :--- |
| `Screen` | `RoboScreen` | Root screen or page. `Window` and `Page` are names for the same primitive. |
| `View` | `RoboView` | The box. Every layout property hangs on this tag. |
| `Text` | `RoboText` | Glyphs. |
| `Image` | `RoboImage` | A decoded picture. |
| `Video` | `RoboVideo` | A decoded picture sequence and its clock. |
| `Audio` | `RoboAudio` | A decoded sound and its clock. |
| `Icon` | `RoboIcon` | One glyph from the icon pack. |
| `Svg` | `RoboSvg` | Vector paths. |
| `Canvas` | `RoboCanvas` | Immediate 2D drawing. |
| `View3D` | `RoboView3D` | One `.rx3d` scene in a rectangle. The scene file stays free of RobixUI tags. |
| `Input` | `RoboInput` | A value the user edits: text, number, or a hidden buffer. |
| `ScrollView` | `RoboScrollView` | A `View` whose content moves and whose clip is the viewport. |
| `List` | `RoboList` | Repeated children from a collection, with stable keys. |

`Container` is the name in the tree today. It becomes `View`. Until documents move, `Container` is an alias of `View`, not a second primitive.

### 2.2 Layout is properties of `View`

These are not tags. The layout engine reads them from `View` and from `Screen`. A later library stage may offer `Row` as sugar that expands to `View`. The compiler still lays out a `View`.

| Property | What it sets |
| :--- | :--- |
| `Row` | `Direction="row"` |
| `Column` | `Direction="column"` |
| `Stack` | Children occupy the same cell, later children above earlier ones |
| `Grid` | `Columns`, `Rows`, `GridColumn`, `GridRow` |
| `Position` | `relative`, `absolute` inside the parent |
| `Padding` | Inner inset, including the four edges |
| `Margin` | Outer inset, including the four edges |
| `Alignment` | Main axis, cross axis, and self |
| `Spacing` | Gap between children, including row and column gap |
| `Size` | Width, height, min, max, fill, and content size |
| `Constraints` | Min and max size the parent must honor |
| `Overflow` | `visible`, `hidden`, `clip`, `scroll` |
| `Flex` | Grow, shrink, order |
| `AspectRatio` | Width divided by height when one side is free |

### 2.3 Events

One event record reaches every primitive. A library component binds the same names. Names match the `.rxu` attributes.

| Category | Events |
| :--- | :--- |
| Pointer | `OnClick`, `OnDoubleClick`, `OnHover`, `OnMouseDown`, `OnMouseUp`, `OnContextMenu` |
| Touch | `OnTap`, `OnLongPress`, `OnSwipe`, `OnDrag`, `OnDrop`, `OnPinch`, `OnRotate` |
| Keyboard | `OnKeyDown`, `OnKeyUp`, `OnFocus`, `OnBlur`, `OnSubmit` |
| Value | `OnChange`, `OnInput`, `OnValueChange`, `OnSelectionChange` |
| Lifecycle | `OnMount`, `OnUnmount`, `OnUpdate` |
| Layout | `OnResize`, `OnLayout`, `OnVisibilityChange` |
| Scroll | `OnScroll`, `OnScrollStart`, `OnScrollEnd` |
| Media | `OnLoad`, `OnError`, `OnPlay`, `OnPause`, `OnEnd`, `OnTimeUpdate` |

### 2.4 Actions

The behavior engine runs these. Library components call them. They are not components.

`State`, `SetState`, `Conditional`, `Show`, `Hide`, `Enable`, `Disable`, `Toggle`, `Animate`, `Transition`, `Navigate`, `Focus`, `Blur`, `ScrollTo`, `Validate`, `Submit`, `Reset`, `Execute`, `Delay`, `Sequence`, `Parallel`.

`Conditional` is the runtime form of `If`. `Show` and `Hide` write visibility. `Navigate` changes the `Screen` path inside the window.

### 2.5 Behavior the runtime keeps

| Piece | Role |
| :--- | :--- |
| Events | Deliver the table in §2.3 to the hit target, then to parents |
| State | A named value on a component, read by bindings |
| Actions | The verbs in §2.4 |
| Conditions | Choose a child or a property from state |
| Animations | Interpolate a property over time |
| Gestures | Recognize tap, long press, swipe, drag, pinch, and rotate from the pointer stream |
| Bindings | `{Name}` and two-way `Bind` between state and a property |
| Lifecycle | Mount, update, unmount, in that order |
| Layout engine | Flex, grid, stack, position, constraints, spacing, alignment |

### 2.6 Own attributes and inherited attributes

Each component publishes one schema with two lists.

| List | What it contains |
| :--- | :--- |
| Own | Names this component defines. `TextField` owns `Label`, `Placeholder`, and `Error`. The template places those values. |
| Inherited | Every attribute of every component and part inside the template, including attributes those pieces inherited. Events from §2.3 travel with the piece that fires them. |

A primitive's schema is its own list. Parts are included. `Input` owns `Value`, `Placeholder`, `Mask`, and `CursorColor`. `CursorColor` is the color of the caret part. Descendants of `Input` inherit `CursorColor` without declaring it again.

`SearchField` is built from `TextField` and an `Icon`. Its own attributes are the search-specific ones, such as the icon name. Its inherited attributes are everything `TextField` owns and everything `TextField` inherited, plus the `Icon` attributes. This document is legal:

```xml
<SearchField
  Label="Find"
  Placeholder="Robots"
  Icon="search"
  CursorColor="#0E7AFF"
  OnChange="QueryChanged"/>
```

`Label`, `Placeholder`, and `Icon` are own attributes along the chain. `CursorColor` was never declared on `SearchField` or on `TextField`. Compose copies it onto the `Input` inside `TextField`. `OnChange` follows the same path because only that `Input` fires it.

Forwarding rules, applied at compose time:

1. An own name is consumed by this component. The template decides where it goes. A use-site value replaces the template default.
2. A name that matches exactly one inner attribute is copied onto that inner node. The match walks through nested library components until it reaches the primitive or part that defined the name.
3. A name that matches two or more inners is written with a qualifier: `Label.Color`, `Input.Color`, `Confirm.Background`. The qualifier is the inner tag, or the inner `Name` when two children share a tag. A path is allowed when the short name would have been enough: `Input.CursorColor`, `TextField.Input.CursorColor`.
4. The outer box is `Root`. `Root.Padding` sets the root. An unqualified `Padding` is legal when only one box in the expansion has `Padding`.
5. A name that is neither own nor inherited is a diagnostic. The message names the component and the attribute.

A component may also take an inherited name as its own, to give it a default or to point it at a chosen child. `TextField` can own `CursorColor` and write `CursorColor="{CursorColor}"` on its `Input`. If it does not, rule 2 still forwards `CursorColor`.

The published schema for a library component lists both groups, and each inherited name shows the path it forwards to. That list is what the editor and `robix ui validate` use.

### 2.7 Web face

The `.rxu` component is the source of truth on every target. Compose runs first, including attribute inheritance. The native painter then draws the expanded tree. The web compiler asks each node for its web face and emits HTML. The framebuffer never reads the face.

A web face has three pieces. The first is required. The other two are optional.

| Piece | Where it lives | What it does |
| :--- | :--- | :--- |
| Semantic tag | `Web` on the component | The element used when nobody overrides it. |
| `<Web>` template | Inside the component | The markup when one component is more than one element, plus the attribute map. |
| Custom code | `Robo<Name>.web.js` beside the `.rxu` file | Browser-only behavior. The component still works when this file is absent. |

`As` chooses a different element. A value on the use site wins over a value on the definition, which wins over the default tag.

```xml
<Component Name="Button" Web="button">
  <View>...</View>
</Component>

<Button As="a" Path="/robots" Text="Open"/>
<SearchField As="site-search" CursorColor="#0E7AFF"/>
```

`As="a"` emits an anchor. `As="site-search"` emits `<site-search>`. The page, or the component's custom code, registers that element. `As` is a face attribute. It is not forwarded as a visual attribute of `View` or `Input`.

When the assigned element replaces the inner tree, the face maps Robix attributes onto it:

```xml
<Web As="site-search">
  <Attribute Name="caret-color" From="CursorColor"/>
  <Attribute Name="placeholder" From="Placeholder"/>
  <Event Name="input" To="OnChange"/>
</Web>
```

`From` and `To` are Robix attributes, own or inherited. `Name` is a DOM attribute, a CSS property, or an event. An assigned element with no map still receives the Robix attributes as `data-*` attributes. A name is not dropped silently.

Default tags for the primitives:

| Robix tag | Element |
| :--- | :--- |
| `Screen` | `main` |
| `View` | `div` |
| `Text` | `p`, or `span` when it is nested |
| `Image` | `img` |
| `Video` | `video` |
| `Audio` | `audio` |
| `Icon` | `span` with `aria-hidden` |
| `Svg` | `svg` |
| `Canvas` | `canvas` |
| `View3D` | `canvas` |
| `Input` | `input`, or `textarea` when multiline |
| `ScrollView` | `div` |
| `List` | `ul`, or `ol` when numbered |

Library components replace those defaults with the semantic element. Layout sugar (`Row`, `Column`, `Stack`, `Grid`, `Spacer`, `Center`, `Absolute`, `AspectBox`) stays a `div`. The other defaults:

| Component | Element |
| :--- | :--- |
| `Heading` | `h1`–`h6` from its level |
| `Subhead` | `h2` |
| `Body`, `Paragraph`, `Caption`, `Eyebrow` | `p` |
| `Label` | `label` |
| `Code` | `code` |
| `Kbd` | `kbd` |
| `Divider` | `hr` |
| `Button`, `IconButton`, `CloseButton` | `button` |
| `Link` | `a` |
| `Checkbox`, `Radio` | `input` inside `label` |
| `Switch` | `button` with `role="switch"` and `aria-checked` |
| `TextField`, `PasswordField`, `SearchField`, `NumberField` | `label` and `input` |
| `TextArea` | `label` and `textarea` |
| `Slider`, `RangeSlider` | `input` |
| `Select` | `select` |
| `Table` | `table` |
| `Navbar` | `nav` |
| `NavItem`, `Tab` | `a` or `button` |
| `Dialog`, `AlertDialog`, `Modal` | `dialog` |
| `Form` | `form` |
| `VideoPlayer` | `video` plus its controls |
| `AudioPlayer` | `audio` plus its controls |

A component not in that table uses the tag of its root primitive. `Card` is a `View`, so it emits a `div` until its own face names a better element.

Semantics that are not tags live in the map. `TextField` pairs `label for` with `input id`. `Icon` sets `aria-hidden="true"` unless it has a label. `Input` writes `CursorColor` as `caret-color`. Those mappings belong to the face of the component that owns the attribute. Descendants inherit the attribute. They do not restate the mapping.

Custom code is for behavior the tag does not provide: a focus trap, a calendar grid, an existing design-system element. The script receives the host element, the component state, and the events from §2.3. It may render into the element. A new visual property is still an attribute on the schema, so the native host and later components inherit it. The web compiler loads the script only when that component is in the document. The framebuffer never loads it.

The web compiler does this, in order:

1. Compose the document, including inherited attributes.
2. Pick the element: use-site `As`, then the component face, then the default tag.
3. Write attributes through the map. Events become listeners that call the Robix function.
4. Emit ARIA from the face.
5. Attach `Robo<Name>.web.js` when that file exists.

Hand-written emitters such as `ButtonWeb.cpp` are the old shape. Stage C21 replaces them with the face. A new component does not add a C++ web file.

---

## 3. How a stage is built

Compiler stages come first, in the order below. Each one depends only on earlier compiler stages.

A library stage starts only after every compiler stage it lists is checked. Inside the library, a component may use primitives and library components from earlier letters only. `Select` waits for `Popover` and `List`. `Popover` waits for `View` and focus. `Button` waits for `View`, `Text`, `Icon`, pointer events, and state.

Each compiler primitive closes with the same four proofs:

1. The schema accepts the component's own attributes, including part attributes such as `CursorColor` on `Input`, and rejects an unknown one with a diagnostic. Those own attributes are what later components inherit.
2. The native host paints it into a `Frame`.
3. A hit, when the primitive is interactive, returns that primitive.
4. A `.test` file asserts pixels, the box, or the event. `robix test` runs it.

Each library component closes with four proofs:

1. The `.rxu` file contains only primitives and earlier library components.
2. The schema lists its own attributes and the inherited attributes, each inherited name with the path it forwards to.
3. Compose expands it to those tags. The test sets one own attribute and one inherited attribute the component did not declare, and asserts both landed on the right inner node.
4. The native host paints the expansion. The test asserts one box and one pixel.
5. The web face names its tag, or a `<Web>` template when the component is more than one element. After C21, the web test asserts that element and one mapped attribute. This proof waits on C21. The native proofs do not.

The compiler has no `Button`. Stage L3 builds it from `View`, `Text`, and `Icon`.

---

## 4. Compiler stages

### Stage C1 — Screen

The root. One window, one theme, one route, one child tree.

Depends on: nothing.

`RoboScreenRead` accepts the schema and rejects an unknown attribute, a bad theme, place, state, color, or size, and a minimum above the maximum. `Window` and `Page` resolve to `Screen`. `robix ui window` and `robix ui validate` use that read. The host clears the framebuffer to the screen color. `RoboScreenPaint` fills a frame with that color.

- [x] `Screen` schema: title, size, min, max, theme, background, place, window state
- [x] `Window` and `Page` resolve to `Screen`
- [x] Native host opens a window from the document
- [x] Paint fills the framebuffer with the screen color
- [x] Test asserts the frame size and the origin pixel

`Tests/UI/RoboScreenTests.cpp` passed, 104 checks. The paint check is an 84×44 frame filled with `#0E7AFF`. The origin pixel and the far corner match. A pixel outside the frame is rejected.

### Stage C2 — View and the layout engine

The box every later primitive sits in. Layout from §2.2 is implemented here, on `View`.

Depends on: C1.

`RoboViewRead` accepts the schema and rejects an unknown attribute, a bad direction, size, or tag, and more than 64 children. `Container` and `Box` resolve to `View`. `RoboViewLayout` places children. `RoboViewPaint` fills a frame. A clipped child does not paint or hit outside its parent.

- [x] `View` schema
- [x] `Container` resolves to `View`
- [x] Row and column, including reverse
- [x] Stack
- [x] Grid, including span
- [x] Position, including absolute children
- [x] Padding
- [x] Margin
- [x] Alignment
- [x] Spacing
- [x] Size, including min, max, fill, and content
- [x] Constraints
- [x] Overflow, including clip
- [x] Flex grow, shrink, and order
- [x] Aspect ratio
- [x] Test lays out a row of three boxes and asserts each origin
- [x] Test clips a child that overflows

`Tests/UI/RoboViewTests.cpp` passed, 101 checks. The row places three 20×10 boxes at x 0, 20, and 40. The clip test paints a child through x 39 and leaves x 45 on the parent.

### Stage C3 — Text

Glyphs. `Span` is a colored run inside one `Text`.

Depends on: C2.

`RoboTextRead` accepts value, family, size, weight, color, wrap, and align, and rejects an unknown attribute, a bad weight, wrap, align, size, or color, a child that is not a `Span`, and more than 48 spans. `RoboTextMeasure` reads the cooked font pack. `RoboTextPaint` writes the glyph coverage in the run color.

- [x] `Text` schema: value, family, size, weight, color, wrap, align
- [x] Part: `Span` for a colored run inside one `Text`
- [x] Glyphs from the cooked font pack
- [x] Measure width and height for the layout engine
- [x] Test asserts the ink pixel of a known glyph and the measured width

`Tests/UI/RoboTextTests.cpp` passed, 105 checks. The paint check is a cooked 4×8 glyph for `A`. The origin pixel is `#FF0000`. The pixel beside it stays clear. The measured width is 4.

### Stage C4 — Image

Depends on: C2.

- [ ] `Image` schema: source, fit, alt
- [ ] Decode into the framebuffer
- [ ] `OnLoad` and `OnError` fire
- [ ] Test asserts a known pixel of a fixture image

### Stage C5 — Icon

Depends on: C3.

- [ ] `Icon` schema: name, size, color
- [ ] Read one glyph from the icon pack
- [ ] Test asserts the glyph box

### Stage C6 — Svg

Depends on: C2.

- [ ] `Svg` schema: paths, fill, stroke, view box
- [ ] Rasterize into the element box
- [ ] Test asserts a pixel on a fixture path and a pixel outside it

### Stage C7 — Canvas

Depends on: C2.

- [ ] `Canvas` schema: width, height, and a draw list
- [ ] Draw rect, disc, line, and text into the box
- [ ] Test asserts a pixel on the rect and a pixel outside it

### Stage C8 — View3D

Depends on: C2. The scene raster from the OS plan supplies the pixels. The `.rx3d` file stays free of RobixUI tags.

- [ ] `View3D` schema: source scene, camera
- [ ] `Scene3D` resolves to `View3D`
- [ ] Paint one frame into the element box
- [ ] Test asserts a body pixel and the clear color

### Stage C9 — Video

Depends on: C2, C4. A frame is an image with a clock. No page loader.

- [ ] `Video` schema: source, time, playing
- [ ] Decode a frame into the box
- [ ] Clock reports time
- [ ] Test asserts a pixel at a fixed timestamp

### Stage C10 — Audio

Depends on: C1. Sound has no box. Transport state is still a component so library players can bind it.

- [ ] `Audio` schema: source, time, playing, volume
- [ ] Decode and play through the host output
- [ ] Test asserts duration and the time after a fixed seek

### Stage C11 — Input

Depends on: C2, C3. This is the editing surface. Checkbox, switch, and slider are not this primitive.

- [ ] `Input` schema: value, placeholder, mask, multiline, disabled, `CursorColor`
- [ ] Part: caret. `CursorColor` paints this part
- [ ] Part: selection
- [ ] Keyboard inserts and deletes
- [ ] Test types three characters and asserts the value

### Stage C12 — ScrollView

Depends on: C2. Scroll bars are parts of this primitive.

- [ ] `ScrollView` schema: axis, offset
- [ ] Clip to the viewport
- [ ] Part: track
- [ ] Part: thumb
- [ ] Drag and wheel change the offset
- [ ] Test scrolls by a known delta and asserts the child origin

### Stage C13 — List

Depends on: C2, C12. Repetition of a template over a collection. The template is any earlier primitive.

- [ ] `List` schema: items, key, template
- [ ] Build one child per item
- [ ] Recycle children when the collection changes
- [ ] Test asserts three children and their keys

### Stage C14 — Events

Depends on: C2. The hit target is the topmost primitive under the point.

- [ ] Pointer: `OnClick`
- [ ] Pointer: `OnDoubleClick`
- [ ] Pointer: `OnHover`
- [ ] Pointer: `OnMouseDown`
- [ ] Pointer: `OnMouseUp`
- [ ] Pointer: `OnContextMenu`
- [ ] Touch: `OnTap`
- [ ] Touch: `OnLongPress`
- [ ] Touch: `OnSwipe`
- [ ] Touch: `OnDrag`
- [ ] Touch: `OnDrop`
- [ ] Touch: `OnPinch`
- [ ] Touch: `OnRotate`
- [ ] Keyboard: `OnKeyDown`
- [ ] Keyboard: `OnKeyUp`
- [ ] Keyboard: `OnFocus`
- [ ] Keyboard: `OnBlur`
- [ ] Keyboard: `OnSubmit`
- [ ] Value: `OnChange`
- [ ] Value: `OnInput`
- [ ] Value: `OnValueChange`
- [ ] Value: `OnSelectionChange`
- [ ] Lifecycle: `OnMount`
- [ ] Lifecycle: `OnUnmount`
- [ ] Lifecycle: `OnUpdate`
- [ ] Layout: `OnResize`
- [ ] Layout: `OnLayout`
- [ ] Layout: `OnVisibilityChange`
- [ ] Scroll: `OnScroll`
- [ ] Scroll: `OnScrollStart`
- [ ] Scroll: `OnScrollEnd`
- [ ] Media: `OnLoad`
- [ ] Media: `OnError`
- [ ] Media: `OnPlay`
- [ ] Media: `OnPause`
- [ ] Media: `OnEnd`
- [ ] Media: `OnTimeUpdate`
- [ ] Test plays a click script and asserts the hit id

### Stage C15 — State, conditions, and actions

Depends on: C14.

- [ ] `State`
- [ ] `SetState`
- [ ] `Conditional`
- [ ] `Show`
- [ ] `Hide`
- [ ] `Enable`
- [ ] `Disable`
- [ ] `Toggle`
- [ ] `Navigate`
- [ ] `Focus`
- [ ] `Blur`
- [ ] `ScrollTo`
- [ ] `Validate`
- [ ] `Submit`
- [ ] `Reset`
- [ ] `Execute`
- [ ] `Delay`
- [ ] `Sequence`
- [ ] `Parallel`
- [ ] Test sets state from a click and asserts the new value
- [ ] Test runs a sequence of two actions in order

### Stage C16 — Gestures

Depends on: C14. Recognition feeds the touch events. It is not a separate set of tags.

- [ ] Tap
- [ ] Long press
- [ ] Swipe
- [ ] Drag, including axis lock
- [ ] Drop target
- [ ] Pinch
- [ ] Rotate
- [ ] Test feeds three pointer samples and asserts one swipe

### Stage C17 — Animation

Depends on: C2, C15.

- [ ] `Animate` from one property value to another
- [ ] `Transition` duration, delay, and ease
- [ ] Enter and leave
- [ ] Test samples a fade at the midpoint and asserts the opacity

### Stage C18 — Bindings and lifecycle

Depends on: C15.

- [ ] One-way `{Name}`
- [ ] Two-way `Bind`
- [ ] Mount, update, and unmount run in order
- [ ] Test binds a field to state, edits it, and asserts both sides

### Stage C19 — Theme tokens

Depends on: C1, C3. Library colors, space, type, radius, and elevation come from here. A library file reads a token. It does not embed a raw palette.

- [ ] Color tokens for light and dark
- [ ] Space, type, radius, and elevation tokens
- [ ] A document selects `dark` or `light`
- [ ] Test resolves one color token in each theme

### Stage C20 — Attribute inheritance

Compose forwards inherited attributes. Library components rely on this stage, so every library stage depends on it.

Depends on: C1 through C19. The forwarding test uses `Input.CursorColor` and a `View` attribute, so C2 and C11 have to be in place. The other primitive schemas are what make the inherited lists complete.

- [ ] Each primitive schema is the own-attribute list, including part attributes
- [ ] A library schema stores own attributes and inherited attributes with forward paths
- [ ] A unique inherited name is copied onto the inner node that defined it
- [ ] An own name stays on the component and is placed by the template
- [ ] An ambiguous name without a qualifier is a diagnostic
- [ ] `Root.Padding` and `Input.CursorColor` resolve through nested components
- [ ] An unknown name is a diagnostic that names the component
- [ ] Test builds `TextField` from `Label`, `Input`, and `Caption`, then builds `SearchField` from `TextField`
- [ ] That test sets `CursorColor` on `SearchField` and asserts the expanded `Input` received it
- [ ] That test sets `Label.Color` and `Input.Color` to different values and asserts each text run

### Stage C21 — Web face

The same composed tree emits semantic HTML. This stage adds no primitive and no library component.

Depends on: C20. Emission of a primitive's tag waits until that primitive's schema exists. The face mechanism itself is this stage.

- [ ] Default tag for `Screen`, `View`, `Text`, `Image`, `Video`, `Audio`, `Icon`, `Svg`, `Canvas`, `View3D`, `Input`, `ScrollView`, and `List`
- [ ] `Web` on a component definition
- [ ] `<Web>` template for a component that is more than one element
- [ ] Attribute map: `From` a Robix attribute, `Name` a DOM attribute, CSS property, or event
- [ ] Use-site `As` overrides the definition, which overrides the default tag
- [ ] `As` is not forwarded as a visual attribute
- [ ] An assigned element with no map receives `data-*` attributes
- [ ] `Input` maps `CursorColor` to `caret-color`
- [ ] `OnClick` and `OnChange` become listeners that call the Robix function
- [ ] ARIA from the face: `Icon` is `aria-hidden` unless labeled, `Switch` is `role="switch"`
- [ ] `Robo<Name>.web.js` loads only when the component is in the document and the file exists
- [ ] The component still emits its semantic tag when the script is absent
- [ ] Test emits `SearchField` with `CursorColor` and asserts an `input` whose `caret-color` is that value
- [ ] Test emits `Button As="a"` and asserts an anchor
- [ ] Test emits `SearchField As="site-search"` and asserts the custom element received `CursorColor`
- [ ] Hand-written `*Web.cpp` emitters are retired for components whose face covers them

Compiler stages C1 through C21 are the foundation. Library stages start after the stages they name are checked, and after C20. The web proof of a library component also waits on C21.

---

## 5. Library stages

Files live in `RobixUI/Library/<stage>/`. Each component is `Robo<Name>.rxu` with `Robo<Name>.test.rx`. The test covers the expanded tree, one own attribute, one inherited attribute, and one painted box. Every stage below depends on C20 as well as the stages named on its `Depends on` line. Each component also declares the web tag from §2.7, or a `<Web>` template. Optional custom code is `Robo<Name>.web.js` in the same folder. The web proof waits on C21.

### Stage L0 — Layout sugar

Expands to `View` with layout properties set. The compiler does not grow a new tag.

Depends on: C2.

- [ ] `Row`
- [ ] `Column`
- [ ] `Stack`
- [ ] `Grid`
- [ ] `Spacer`
- [ ] `Center`
- [ ] `Absolute`
- [ ] `AspectBox`

### Stage L1 — Text roles

Built from `Text` and `Span`.

Depends on: C3, C19.

- [ ] `Heading`
- [ ] `Subhead`
- [ ] `Body`
- [ ] `Paragraph`
- [ ] `Caption`
- [ ] `Eyebrow`
- [ ] `Label`
- [ ] `Code`
- [ ] `Kbd`
- [ ] `Divider` — a `View` with a fixed cross size and a token color

### Stage L2 — Marks

Built from `View`, `Text`, `Icon`, and `Image`.

Depends on: L1, C4, C5.

- [ ] `IconLabel`
- [ ] `Avatar`
- [ ] `Badge`
- [ ] `Tag`
- [ ] `Chip`

### Stage L3 — Pressables

Built from `View`, `Text`, `Icon`, pointer events, focus, and state. The compiler no longer has a `Button`.

Depends on: L1, L2, C14, C15, C19.

- [ ] `Button`
- [ ] `IconButton`
- [ ] `CloseButton`
- [ ] `Link` — focusable `View` that calls `Navigate`
- [ ] `ButtonGroup`
- [x] The compiler has no `Button`. `RoboButton.rxu` is still the library control to build

### Stage L4 — Choice

Built from L3 plus state. A check mark is an `Icon`. The track of a switch is a `View`.

Depends on: L3.

- [ ] `Checkbox`
- [ ] `CheckboxGroup` — `Column` plus `Checkbox`
- [ ] `Radio`
- [ ] `RadioGroup`
- [ ] `Switch`
- [ ] `SwitchGroup`
- [ ] `Toggle`
- [ ] `SegmentedControl`

### Stage L5 — Fields

Built from `Input`, `Label`, `Caption`, `Icon`, and `IconButton`. `TextField` owns `Label`, `Placeholder`, and `Error`. It inherits `CursorColor`, `OnChange`, and the rest of `Input`. `SearchField` inherits those again, so a screen can set the caret color without `SearchField` declaring it.

Depends on: L1, L2, L3, C11, C18.

- [ ] `TextField`
- [ ] `TextArea`
- [ ] `PasswordField`
- [ ] `SearchField`
- [ ] `NumberField`
- [ ] `Field` — label, control slot, and error caption

### Stage L6 — Surfaces

Built from `View`, text roles, and `Image`.

Depends on: L0, L1, C4, C19.

- [ ] `Paper`
- [ ] `Section`
- [ ] `Panel`
- [ ] `Card`
- [ ] `Collapse`
- [ ] `Accordion` — several `Collapse` blocks with one open key

### Stage L7 — Feedback

Built from `View`, `Text`, `Icon`, `Svg`, and animation. These sit in the document flow.

Depends on: L2, C6, C17.

- [ ] `Spinner`
- [ ] `Skeleton`
- [ ] `ProgressBar`
- [ ] `ProgressRing`
- [ ] `Meter`

### Stage L8 — Overlays

A floating `View` with position, focus, and stack order. `MenuItem` is a `Button`. The rest of the menu is this stage.

Depends on: L3, L6, C15, C16.

- [ ] `Overlay`
- [ ] `Popover`
- [ ] `Tooltip`
- [ ] `MenuItem`
- [ ] `Menu`
- [ ] `ContextMenu`
- [ ] `Toast`
- [ ] `Drawer`
- [ ] `Sheet`
- [ ] `Dialog`
- [ ] `AlertDialog`
- [ ] `Modal`

### Stage L9 — Value controls that need a drag or a popover

`Slider` is a `View` with drag. `Select` is a `Field` that opens a `List` inside a `Popover`.

Depends on: L5, L8, C12, C13, C16.

- [ ] `Slider`
- [ ] `RangeSlider`
- [ ] `Select`
- [ ] `MultiSelect`
- [ ] `ComboBox`

### Stage L10 — Collections

Built from `List`, `ScrollView`, `Card`, `Checkbox`, and text roles.

Depends on: L4, L6, C12, C13.

- [ ] `ListItem`
- [ ] `EmptyState`
- [ ] `TableCell`
- [ ] `TableRow`
- [ ] `Table`
- [ ] `TreeItem`
- [ ] `Tree`

### Stage L11 — Navigation

Built from `Row`, `Column`, `Button`, `Icon`, `List`, and `Link`.

Depends on: L0, L3, L10.

- [ ] `NavItem`
- [ ] `Navbar`
- [ ] `Sidebar`
- [ ] `Tab`
- [ ] `Tabs`
- [ ] `Toolbar`
- [ ] `Breadcrumb`
- [ ] `Pagination`
- [ ] `Stepper`

### Stage L12 — Forms

Built from `Field`, `Button`, `Validate`, `Submit`, and `Reset`.

Depends on: L5, L9, C15.

- [ ] `FormError`
- [ ] `SubmitBar`
- [ ] `Form`

### Stage L13 — Media chrome

The primitive plays. This stage draws the controls around it.

Depends on: L3, L9, C9, C10.

- [ ] `AudioPlayer`
- [ ] `VideoPlayer`

### Stage L14 — Data graphics

`Chart` and `Sparkline` draw through `Canvas`. `Calendar` is a `Grid` of buttons. No new primitive.

Depends on: L0, L3, C7.

- [ ] `Sparkline`
- [ ] `Chart`
- [ ] `KeyValue`
- [ ] `Timeline`
- [ ] `Calendar`

### Stage L15 — Window chrome

Built from `Screen` parts plus navigation and overlays. The host still owns the real window frame from C1. These components are the document that fills it.

Depends on: L8, L9, L11.

- [ ] `WindowControls`
- [ ] `TitleBar`
- [ ] `StatusBar`
- [ ] `SplitView`
- [ ] `CommandPalette`

---

## 6. Order

```text
C1 Screen
  C2 View + layout
    C3 Text → C5 Icon
    C4 Image
    C6 Svg
    C7 Canvas
    C8 View3D
    C9 Video
    C10 Audio
    C11 Input
    C12 ScrollView → C13 List
  C14 Events → C15 State and actions → C16 Gestures
            → C17 Animation
            → C18 Bindings
  C19 Theme
  C20 Attribute inheritance
  C21 Web face

L0 layout sugar     after C2, C20, C21 for the web proof
L1 text roles       after C3, C19, C20
L2 marks            after L1, C4, C5, C20
L3 pressables       after L1, L2, C14, C15, C19, C20
L4 choice           after L3, C20
L5 fields           after L1, L2, L3, C11, C18, C20
L6 surfaces         after L0, L1, C4, C19, C20
L7 feedback         after L2, C6, C17, C20
L8 overlays         after L3, L6, C15, C16, C20
L9 slider, select   after L5, L8, C12, C13, C16, C20
L10 collections     after L4, L6, C12, C13, C20
L11 navigation      after L0, L3, L10, C20
L12 forms           after L5, L9, C15, C20
L13 media chrome    after L3, L9, C9, C10, C20
L14 data graphics   after L0, L3, C7, C20
L15 window chrome   after L8, L9, L11, C20
```

Parallel work is safe across branches that do not share a dependency. `Image` and `Text` can land together. `Select` waits until both the field branch and the overlay branch are checked. Every library stage's web proof also waits on C21. The native proofs of that stage do not.

---

## 7. Already in the tree

These files exist. Their boxes above stay open until the stage test has been run on the native host.

| Path | What it is today |
| :--- | :--- |
| `RobixUI/Compiler/Components/Screen/` | `Screen` compiler unit. Stage C1 `RoboScreen` reads the schema, resolves `Window` and `Page`, and paints the screen color. |
| `RobixUI/Compiler/Components/Container/` | The old container name. Stage C2 resolves it to `View`. |
| `RobixUI/Compiler/Components/View/` | Stage C2 layout engine. `RoboView` places and paints the box. |
| `RobixUI/Compiler/Components/Text/` | Stage C3. `RoboText` reads the schema, measures the cooked font pack, and paints the glyph. `Span` is a part. |
| `RobixUI/Compiler/Components/Image/` | Decode helpers. The native window still has to paint the bitmap. |
| `RobixUI/Compiler/Components/Icon/` | `Icon` compiler unit |
| `RobixUI/Compiler/Components/Svg/` | `Svg` compiler unit |
| `RobixUI/Compiler/ComponentTier.cpp` | Lists `View3D` as a compiler tag. The paint unit is still to build. |
| `RobixUI/Compiler/Compose.cpp` | Expands library components. Library stages use it. |
| `RobixUI/Compiler/Components/*/*Web.cpp` | Hand-written HTML emitters for the plan primitives. Stage C21 replaces them with the web face. |

`Canvas`, `Video`, `Audio`, `Input`, `ScrollView`, and `List` have no compiler unit yet. They begin at their stage.
