#include "uxs/optional.h"

const char* est::bad_optional_access::what() const noexcept { return "bad optional access"; }
