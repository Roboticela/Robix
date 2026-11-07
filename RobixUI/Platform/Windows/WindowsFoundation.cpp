#include "WindowsFoundation.hpp"

namespace Robix::UI::Platform::Windows {

bool ApplyFoundation(FoundationPaint& paint, bool dark, std::string& error) {
    return LoadFoundation(paint, dark, error);
}

}
