#pragma once

#include <cstddef>

#include "core/Service.h"

namespace Kernel {

void Boot(const Service* const* services, std::size_t count);

}
