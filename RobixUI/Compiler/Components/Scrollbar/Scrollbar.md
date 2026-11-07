# Scrollbar

The thin bar drawn by ScrollView. It is not a separate scroller. Set `arrows`, `autoHide`, `overlay`, and the input sources on ScrollView. The same bar is used on RoboticelaOS and on the web.

## Options

| Attribute | Values | Default | Meaning |
| --- | --- | --- | --- |
| orientation | vertical, horizontal | vertical | Axis this chrome belongs to |
| arrows | off, hover, always | hover | Owned by the parent ScrollView |
| autoHide | true, false | true | Fade when scrolling stops |
| overlay | true, false | true | Paint over the content |
| thickness | int | 6 | Fixed. The bar does not animate its size |
| thumbColor, trackColor | color | | |
| position, margin, flex, alignSelf, order, gridColumn, gridRow | layout | | |

Putting chrome attributes on the Scrollbar tag warns. The parent ScrollView is the owner.

## Experience

Thickness stays 6 px while idle, while dragging, and while fading. The only motion is opacity. The bar is shown while content is moving, while the thumb is dragged, or while auto-hide is off. Hover does not reveal it.

The vertical bar sits on the inline end. The horizontal bar sits on the bottom. Arrow buttons nudge one step. A track click pages toward the pointer. Dragging the thumb moves the content with the pointer and stops any coast.

## Pointer, keyboard, accessibility

Mouse uses the thumb, track, and arrows when ScrollView `pointer` is on. Touch and pen use them when `touch` is on. The bar has no keys of its own. Keyboard scrolling is on the ScrollView. The thumb does not need a separate name. Hiding the bar must not remove the keyboard path.
