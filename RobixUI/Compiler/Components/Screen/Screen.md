# Screen

A page on the web and a window on RoboticelaOS. The first page loads with its title, description, and theme. Later pages load when the route is opened. The same XML is the page on every platform.

A screen has no focus ring. The page itself is not a tab stop. A border appears only when XML sets `border`. Nested preview screens may set `border="1"`. The desktop page does not.

## Options

| Attribute | Values | Default | Meaning |
| --- | --- | --- | --- |
| id | string | | |
| path | string | / | Route. `route` is the same |
| name | string | | Window title and document title. `title` is the same |
| theme | dark, light | dark | `appearance` is the same |
| transparent | true, false | false | `transparency` is the same |
| place | center, top, bottom, left, right, top-left, top-right, bottom-left, bottom-right, absolute | center | Window placement |
| x, y | int | 0 | Used when place is absolute |
| width, height | int | 640, 400 | Window size. A screen's own width and height are not design-token warnings |
| minWidth, minHeight | int | 320, 200 | |
| maxWidth, maxHeight | int | 1920, 1080 | |
| minSize, maxSize | one or two numbers | | |
| radius | token | radius-lg | |
| foundation | string | | App foundation derived from RoboFoundation |
| contentWidth, gutter | token | | Layout tokens |
| border | int | 0 | Pixel width. A border is drawn only when this is set |
| background | color | | `backgroundColor` and `bg` are the same |
| borderColor | color | | |
| display | flex, grid, stack | flex | |
| direction | row, column, and reverses | column | `flexDirection` is the same |
| justify, align, gap, padding | layout | start, stretch, 0, 0 | `justifyContent` and `alignItems` match |
| columns, rows, wrap, alignContent, justifyItems | layout | | |
| position, inset, left, top, right, bottom, margin | layout | | |
| alignSelf, order, gridColumn, gridRow, grow | layout | | `flex` and `flexGrow` are `grow` |
| overflow, overflowX, overflowY, clip | see Container | visible, clip false | |
| state | normal, minimized, maximized, closed | normal | Window state |
| resizable | true, false, or side list | true | |
| resize | side list | | Sides and corners that stay resizable |
| noResize | side list | | Sides or corners to lock. `resizeDisabled` is the same |
| resizeLeft, resizeRight, resizeTop, resizeBottom, resizeTopLeft, resizeTopRight, resizeBottomLeft, resizeBottomRight | bool | | One edge or corner |
| movable | true, false | true | Drag the window |
| topmost | true, false | false | `alwaysOnTop` is the same |
| fullscreen | true, false | false | |
| zoomControls | bottom, bottom-left, bottom-right, top, top-left, top-right, left, right, center, none | bottom | Capsule shown only while pinching, Ctrl+wheel, or Ctrl+Plus, Ctrl+Minus, and Ctrl+0. `none` hides it. `zoom-controls` is the same |
| board | string | | Board id this screen displays |

Children: any component, up to 64.

## Window behavior

Drag moves the window when `movable` is true. The pointer may be mouse, touchpad, or touch. Resize uses the edges and corners that remain enabled. A locked corner does not change size. Minimized, maximized, and closed are states of the window, not of the page content. Fullscreen uses the display and ignores x and y until it leaves fullscreen.

On the web, a screen with a path is a route. The root screen is the document. Nested screens inside a page are preview surfaces and keep their own width, height, theme, and radius.

The zoom capsule uses the elevated surface, the default border, and a pill radius. Minus and plus step the zoom, the percentage is a Select with 25%, 50%, 100%, 125%, 150%, and 200%, and Reset returns to 100%. It sits on the side or corner named by `zoomControls` and leaves after the gesture. The menu stays open while it is in use. Zooming still works when the capsule is hidden.

## Keyboard and accessibility

The screen does not take arrow keys. Those belong to ScrollView or to a focused text field. The document title is `name`. The default theme is dark, and light must remain available. Do not draw a focus outline on the page. The first heading inside the screen is the page heading.
