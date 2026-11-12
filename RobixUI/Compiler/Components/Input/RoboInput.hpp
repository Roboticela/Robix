#pragma once

#include "../../RoboXML.hpp"
#include "../../../RoboShared/RoboTypes.hpp"

#include <string>
#include <vector>

/* Stage C11. Input is the editing surface. TextField is the same primitive. */

constexpr RoboU32 ROBO_INPUT_PLATE = 0xFFFFFFFFu;
constexpr RoboU32 ROBO_INPUT_SELECTION = 0xFFCCE0FFu;
constexpr RoboU32 ROBO_INPUT_BULLET = 0xFF222222u;
constexpr int ROBO_INPUT_CELL = 8;
constexpr int ROBO_INPUT_LINE = 16;

struct RoboInputNode {
    std::string value;
    std::string placeholder;
    std::string mask;
    bool multiline = false;
    bool disabled = false;
    RoboU32 cursorColor = 0xFF111111u;
    int width = 0;
    int height = 0;
    int caret = 0;
    int selectionStart = 0;
    int selectionEnd = 0;
};

struct RoboInputFrame {
    int width = 0;
    int height = 0;
    std::vector<RoboU32> pixels;
};

/* Accepts value, placeholder, mask, multiline, disabled, and CursorColor. TextField is read as Input. */
bool RoboInputRead(const RoboXMLNode& node, RoboInputNode& input, std::string& diagnostic);

/* Inserts text at the caret. A selection is replaced. Disabled input refuses the edit. */
bool RoboInputType(RoboInputNode& input, const std::string& text);

/* Backspace removes the selection or the character before the caret. Delete removes the character after it. */
bool RoboInputDelete(RoboInputNode& input, bool forward);

/* Selects a byte range. The caret sits at the end of the range. */
void RoboInputSelect(RoboInputNode& input, int start, int end);

/* Paints the plate, the selection, the value or placeholder, and the caret. */
void RoboInputPaint(const RoboInputNode& input, RoboInputFrame& frame);

/* Returns false when x or y is outside the frame. */
bool RoboInputPixel(const RoboInputFrame& frame, int x, int y, RoboU32& color);
