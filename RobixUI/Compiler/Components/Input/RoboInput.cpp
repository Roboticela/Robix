#include "RoboInput.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace {

constexpr int ROBO_INPUT_MAX_SIZE = 4096;

const char* const ROBO_INPUT_ATTRIBUTES[] = {
    "id", "value", "content", "text", "placeholder", "mask", "multiline", "disabled", "cursorcolor", "cursor-color", "width", "height",
};

std::string RoboInputLower(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string RoboInputFail(const char* code, const std::string& message) {
    return std::string(code) + ": " + message;
}

bool RoboInputKnown(const std::string& key) {
    const std::string folded = RoboInputLower(key);
    for (const char* name : ROBO_INPUT_ATTRIBUTES) {
        if (folded == name) {
            return true;
        }
    }
    return false;
}

int RoboInputHex(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

bool RoboInputColor(const std::string& text, RoboU32& color) {
    if (text.size() != 7 || text[0] != '#') {
        return false;
    }
    RoboU32 value = 0;
    for (int index = 1; index <= 6; ++index) {
        const int digit = RoboInputHex(text[static_cast<std::size_t>(index)]);
        if (digit < 0) {
            return false;
        }
        value = (value << 4) | static_cast<RoboU32>(digit);
    }
    color = 0xFF000000u | value;
    return true;
}

bool RoboInputInteger(const std::string& text, int& number) {
    if (text.empty()) {
        return false;
    }
    int value = 0;
    for (char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        value = value * 10 + (ch - '0');
        if (value > ROBO_INPUT_MAX_SIZE) {
            return false;
        }
    }
    number = value;
    return true;
}

bool RoboInputFlag(const std::string& text, bool& flag) {
    const std::string folded = RoboInputLower(text);
    if (folded == "true" || folded == "yes" || folded == "on" || folded == "1") {
        flag = true;
        return true;
    }
    if (folded == "false" || folded == "no" || folded == "off" || folded == "0") {
        flag = false;
        return true;
    }
    return false;
}

int RoboInputSpan(const std::string& text, int offset) {
    if (offset < 0 || offset >= static_cast<int>(text.size())) {
        return 1;
    }
    const auto lead = static_cast<unsigned char>(text[static_cast<std::size_t>(offset)]);
    int span = 1;
    if ((lead & 0x80u) == 0) {
        span = 1;
    } else if ((lead & 0xE0u) == 0xC0u) {
        span = 2;
    } else if ((lead & 0xF0u) == 0xE0u) {
        span = 3;
    } else if ((lead & 0xF8u) == 0xF0u) {
        span = 4;
    }
    if (offset + span > static_cast<int>(text.size())) {
        return 1;
    }
    return span;
}

int RoboInputPrevious(const std::string& text, int offset) {
    if (offset <= 0) {
        return 0;
    }
    int index = offset - 1;
    while (index > 0 && (static_cast<unsigned char>(text[static_cast<std::size_t>(index)]) & 0xC0u) == 0x80u) {
        --index;
    }
    return index;
}

void RoboInputClampCaret(RoboInputNode& input) {
    if (input.caret < 0) {
        input.caret = 0;
    }
    if (input.caret > static_cast<int>(input.value.size())) {
        input.caret = static_cast<int>(input.value.size());
    }
    if (input.selectionStart < 0) {
        input.selectionStart = 0;
    }
    if (input.selectionEnd < input.selectionStart) {
        input.selectionEnd = input.selectionStart;
    }
    if (input.selectionEnd > static_cast<int>(input.value.size())) {
        input.selectionEnd = static_cast<int>(input.value.size());
    }
}

void RoboInputPlace(const std::string& text, int offset, int& column, int& row) {
    column = 0;
    row = 0;
    const int limit = std::max(0, std::min(offset, static_cast<int>(text.size())));
    for (int index = 0; index < limit;) {
        if (text[static_cast<std::size_t>(index)] == '\n') {
            ++row;
            column = 0;
            ++index;
            continue;
        }
        ++column;
        index += RoboInputSpan(text, index);
    }
}

bool RoboInputHidden(const RoboInputNode& input) {
    const std::string mask = RoboInputLower(input.mask);
    return mask == "password" || mask == "hidden" || mask == "*";
}

void RoboInputFill(RoboInputFrame& frame, int x, int y, int width, int height, RoboU32 color) {
    for (int row = y; row < y + height; ++row) {
        if (row < 0 || row >= frame.height) {
            continue;
        }
        for (int column = x; column < x + width; ++column) {
            if (column < 0 || column >= frame.width) {
                continue;
            }
            frame.pixels[static_cast<std::size_t>(row) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(column)] = color;
        }
    }
}

}  // namespace

bool RoboInputRead(const RoboXMLNode& node, RoboInputNode& input, std::string& diagnostic) {
    input = RoboInputNode{};
    diagnostic.clear();
    const std::string name = RoboInputLower(node.name);
    if (name != "input" && name != "textfield") {
        diagnostic = RoboInputFail("R8004", "'" + node.name + "' is not an Input. Use Input or TextField.");
        return false;
    }
    for (const auto& entry : node.attrs) {
        if (!RoboInputKnown(entry.first)) {
            diagnostic = RoboInputFail("R8001", "Input attribute '" + entry.first + "' is unknown. Use value, placeholder, mask, multiline, disabled, and CursorColor.");
            return false;
        }
    }
    input.value = RoboXMLAttr(node, "value");
    if (input.value.empty()) {
        input.value = RoboXMLAttr(node, "content");
    }
    if (input.value.empty()) {
        input.value = RoboXMLAttr(node, "text");
    }
    input.placeholder = RoboXMLAttr(node, "placeholder");
    input.mask = RoboXMLAttr(node, "mask");
    if (!input.mask.empty()) {
        const std::string mask = RoboInputLower(input.mask);
        if (mask != "none" && mask != "text" && mask != "password" && mask != "hidden" && mask != "*") {
            diagnostic = RoboInputFail("R8002", "Input mask '" + input.mask + "' is unknown. Use none or password.");
            return false;
        }
    }
    const std::string multiline = RoboXMLAttr(node, "multiline");
    if (!multiline.empty() && !RoboInputFlag(multiline, input.multiline)) {
        diagnostic = RoboInputFail("R8002", "Input multiline '" + multiline + "' is not true or false.");
        return false;
    }
    const std::string disabled = RoboXMLAttr(node, "disabled");
    if (!disabled.empty() && !RoboInputFlag(disabled, input.disabled)) {
        diagnostic = RoboInputFail("R8002", "Input disabled '" + disabled + "' is not true or false.");
        return false;
    }
    const std::string cursor = RoboXMLAttr(node, "cursorcolor");
    const std::string cursorValue = cursor.empty() ? RoboXMLAttr(node, "cursor-color") : cursor;
    if (!cursorValue.empty() && !RoboInputColor(cursorValue, input.cursorColor)) {
        diagnostic = RoboInputFail("R8002", "Input CursorColor '" + cursorValue + "' is not a color. Use #RRGGBB.");
        return false;
    }
    const std::string width = RoboXMLAttr(node, "width");
    const std::string height = RoboXMLAttr(node, "height");
    if (!RoboInputInteger(width, input.width) || input.width < 1) {
        diagnostic = RoboInputFail("R8003", "Input width '" + width + "' is not a size. Use a whole number of pixels.");
        return false;
    }
    if (!RoboInputInteger(height, input.height) || input.height < 1) {
        diagnostic = RoboInputFail("R8003", "Input height '" + height + "' is not a size. Use a whole number of pixels.");
        return false;
    }
    input.caret = static_cast<int>(input.value.size());
    input.selectionStart = input.caret;
    input.selectionEnd = input.caret;
    return true;
}

bool RoboInputType(RoboInputNode& input, const std::string& text) {
    if (input.disabled) {
        return false;
    }
    RoboInputClampCaret(input);
    std::string incoming = text;
    if (!input.multiline) {
        std::string flat;
        for (char ch : incoming) {
            if (ch != '\n' && ch != '\r') {
                flat.push_back(ch);
            }
        }
        if (flat.empty()) {
            return false;
        }
        incoming = flat;
    }
    if (input.selectionStart != input.selectionEnd) {
        input.value.erase(static_cast<std::size_t>(input.selectionStart), static_cast<std::size_t>(input.selectionEnd - input.selectionStart));
        input.caret = input.selectionStart;
    }
    input.value.insert(static_cast<std::size_t>(input.caret), incoming);
    input.caret += static_cast<int>(incoming.size());
    input.selectionStart = input.caret;
    input.selectionEnd = input.caret;
    return true;
}

bool RoboInputDelete(RoboInputNode& input, bool forward) {
    if (input.disabled) {
        return false;
    }
    RoboInputClampCaret(input);
    if (input.selectionStart != input.selectionEnd) {
        input.value.erase(static_cast<std::size_t>(input.selectionStart), static_cast<std::size_t>(input.selectionEnd - input.selectionStart));
        input.caret = input.selectionStart;
        input.selectionEnd = input.caret;
        return true;
    }
    if (forward) {
        if (input.caret >= static_cast<int>(input.value.size())) {
            return false;
        }
        const int span = RoboInputSpan(input.value, input.caret);
        input.value.erase(static_cast<std::size_t>(input.caret), static_cast<std::size_t>(span));
        return true;
    }
    if (input.caret <= 0) {
        return false;
    }
    const int previous = RoboInputPrevious(input.value, input.caret);
    input.value.erase(static_cast<std::size_t>(previous), static_cast<std::size_t>(input.caret - previous));
    input.caret = previous;
    input.selectionStart = input.caret;
    input.selectionEnd = input.caret;
    return true;
}

void RoboInputSelect(RoboInputNode& input, int start, int end) {
    if (start > end) {
        const int swap = start;
        start = end;
        end = swap;
    }
    input.selectionStart = start;
    input.selectionEnd = end;
    input.caret = end;
    RoboInputClampCaret(input);
}

void RoboInputPaint(const RoboInputNode& input, RoboInputFrame& frame) {
    frame = RoboInputFrame{};
    frame.width = std::max(1, input.width);
    frame.height = std::max(1, input.height);
    frame.pixels.assign(static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height), ROBO_INPUT_PLATE);
    const bool hidden = RoboInputHidden(input);
    const std::string shown = input.value.empty() ? input.placeholder : input.value;
    const bool placeholder = input.value.empty() && !input.placeholder.empty();
    const RoboU32 ink = placeholder ? 0xFF8E8E93u : (hidden ? ROBO_INPUT_BULLET : 0xFF111111u);
    int column = 0;
    int row = 0;
    int byte = 0;
    const std::string source = placeholder ? shown : input.value;
    for (int index = 0; index < static_cast<int>(source.size());) {
        const char ch = source[static_cast<std::size_t>(index)];
        const int span = ch == '\n' ? 1 : RoboInputSpan(source, index);
        const bool selected = !placeholder && byte >= input.selectionStart && byte < input.selectionEnd;
        const int x = column * ROBO_INPUT_CELL;
        const int y = row * ROBO_INPUT_LINE;
        if (ch == '\n') {
            if (selected) {
                RoboInputFill(frame, x, y, ROBO_INPUT_CELL, ROBO_INPUT_LINE, ROBO_INPUT_SELECTION);
            }
            ++row;
            column = 0;
        } else {
            if (selected) {
                RoboInputFill(frame, x, y, ROBO_INPUT_CELL, ROBO_INPUT_LINE, ROBO_INPUT_SELECTION);
            }
            if (hidden && !placeholder) {
                RoboInputFill(frame, x + 2, y + 6, 4, 4, ROBO_INPUT_BULLET);
            } else {
                RoboInputFill(frame, x + 1, y + 3, 5, 8, ink);
            }
            ++column;
        }
        index += span;
        byte += span;
    }
    if (!input.disabled) {
        int caretColumn = 0;
        int caretRow = 0;
        RoboInputPlace(input.value, input.caret, caretColumn, caretRow);
        RoboInputFill(frame, caretColumn * ROBO_INPUT_CELL, caretRow * ROBO_INPUT_LINE + 1, 2, ROBO_INPUT_LINE - 2, input.cursorColor);
    }
}

bool RoboInputPixel(const RoboInputFrame& frame, int x, int y, RoboU32& color) {
    if (x < 0 || y < 0 || x >= frame.width || y >= frame.height) {
        return false;
    }
    color = frame.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(x)];
    return true;
}
