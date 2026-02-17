#pragma once
#include <cstdint>

// GP4 "setup byte" encoding: stored value = logical + 151.
// Only this is actually used in your codebase.
inline std::uint8_t EncodeSetupByte(int v)
{
    return static_cast<std::uint8_t>(v + 151);
}
