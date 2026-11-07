# Board

An empty canvas that a screen shows. Nothing is drawn until children are added. The screen is the window. The board is the layout inside that window.

Connect them with `board="id"` on Screen, or `screen="id"` (also `screenId` or `path`) on Board. RoboticelaOS and the web use the same connection and the same layout attributes.

## Options

| Attribute | Values | Default | Meaning |
| --- | --- | --- | --- |
| id | string | | Name the screen points at |
| screen | string | | Screen id or path |
| display | canvas, flex, grid, stack | canvas | canvas places children by x and y. The others use Container rules |
| direction | row, column, row-reverse, column-reverse | column | Main axis. `flexDirection` is the same |
| justify | start, center, end, space-between, space-around, space-evenly | start | Main-axis alignment. `justifyContent` is the same |
| align | start, center, end, stretch | start | Cross-axis alignment. `alignItems` is the same |
| wrap | wrap, nowrap | nowrap | `flexWrap` is the same |
| gap, rowGap, columnGap | length | 0 | Space between children. `colGap` is `columnGap` |
| padding and paddingTop, paddingRight, paddingBottom, paddingLeft | length | 0 | Inner space. Longhands override the shorthand |
| width, height, minWidth, minHeight, maxWidth, maxHeight | size | | Exact, percent, or auto |
| size | exact, auto, dynamic, fill, min, max | | exact uses width and height. auto fits content. dynamic grows. fill is 100% |
| x, y | int | 0 | Canvas origin |
| columns, rows | int | columns 1 | Grid track counts |
| alignContent, justifyItems | same as Container | start, stretch | Extra grid and wrap alignment |
| position | relative, absolute, fixed | relative | |
| inset, left, top, right, bottom, margin | length | | Outer placement |
| flex | int | | `flexGrow` and `grow` are the same |
| alignSelf | auto, start, center, end, stretch | auto | |
| order | int | 0 | |
| gridColumn, gridRow | string | | `auto`, `span N`, or `start / end` |
| overflow | visible, hidden, clip, scroll, auto | visible | Shorthand sets both axes |
| overflowX, overflowY | same | | Longhands override the shorthand. `overflow-x` and `overflow-y` are the same |
| clip | true, false | false | true is overflow clip |
| background | color | | `backgroundColor` and `bg` are the same |
| fill | true, false | false | Fill the parent. `fillParent` is the same |

Children: any component, up to 64. The board starts empty.

## Overflow

`visible` paints past the board. `hidden` and `clip` cut children to the board. Fixed children still paint outside the clip. `scroll` and `auto` clip on RoboticelaOS. On the web, `scroll` shows a native scrollport. Use ScrollView when the scrollbar, glide, and keyboard rules must match.

## Pointer, keyboard, accessibility

The board does not take focus and has no border unless `background` or a child draws one. Clicks, touch, and the touchpad hit the topmost child. Arrow keys do nothing on the board. If the board clips, put a ScrollView inside it for keyboard scrolling. Give grouped content a heading child so the structure is readable without the canvas.
