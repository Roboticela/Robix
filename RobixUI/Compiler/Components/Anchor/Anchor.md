# Anchor

A text link. It takes the same destination, icons, disabled state, and preview state as a button. The face is the WebRef anchor, not a filled pill.

Without `Path` it is a button that looks like a link. With `Path` it is a real link. `href`, `route`, and `screen` are the same as `Path`.

## Options

| Attribute | Values | Default | Meaning |
| --- | --- | --- | --- |
| variant | inline, subtle, underline, plain, standalone, arrow, more | inline | Inline sits in a sentence. Subtle is tertiary until hover. Underline stays drawn. Plain uses the primary text color. Standalone is a medium trail with a chevron. Arrow is the circular chevron. More is the home text link: 17px medium brand text, a 6px gap, and a sliding arrow at 0.72em. Hover shifts the color and slides the arrow. It does not underline |
| size | inherit, sm, md, lg | inherit | Inherit matches the surrounding text. Small is 12px, medium is 17px, large is 17px and 19px from 735px |
| tone | brand, danger | brand | Brand uses the link blue. Danger uses the danger color |
| arrow | end, start | | Draws the circular chevron and selects the arrow face. End points forward. Start points back |
| external | auto, true, false | auto | Auto adds the launch mark for an http address or `target="_blank"`. True also opens a new window |
| target, rel | string | | Passed through. A new window gets `noopener noreferrer` when `rel` is empty |
| leading, trailing | icon name | | Same icon slots as Button. A trailing icon replaces the automatic chevron or launch mark |
| preview | hover, focus, active, visited | | Paints that state without the pointer |
| disabled | true, false | false | Inert text. It is not focused and does not navigate |
| type | button, submit, reset | button | Used only when there is no path |
| path | route or URL | | `/products` becomes `#/products` |

Screen-size suffixes and the same motion attributes as Button work here.

## Pointer, keyboard, accessibility

Tab reaches the link. Enter follows it. A disabled anchor is removed from the tab order. An external link that opens a new window adds the words "opens in a new window" for assistive technology. The Windows window paints the words in the link color. The circle, the sliding arrow, and the launch mark are on the web page.
