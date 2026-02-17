#pragma once
#include <vector>
#include <string>
#include <cstdint>

namespace MagicData
{
    std::string GetGP4RootFolder();

    bool LoadDatFile(int trackIndex, std::vector<std::uint8_t>& out);

    std::uint8_t* FindMagicDataInDat(std::vector<std::uint8_t>& dat,
        std::size_t& outSize);

    void CopyDatMagicDataToGP4(std::uint8_t* src,
        std::size_t size,
        std::uint8_t* dst,
        int trackIndex);

    bool ExtractLapsFromDat(const std::vector<std::uint8_t>& dat,
        int& outLaps);
}
