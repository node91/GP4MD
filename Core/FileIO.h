#pragma once
#include <cstdio>

// Simple helper for binary read-only open.
// Kept as FILE* because the rest of the code uses C stdio.
inline std::FILE* OpenFileRead(const char* path)
{
    std::FILE* f = nullptr;
    if (fopen_s(&f, path, "rb") != 0)
        return nullptr;
    return f;
}
