#pragma once

#include "../Icon/IconPlatform.hpp"

/*
 * Svg has no second paint path. The build stores the drawing in the icon pack,
 * and IconPlatform draws it on every platform except the web.
 */
