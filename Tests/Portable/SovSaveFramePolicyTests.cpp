#include "Save/SovSaveFramePolicy.h"
#include <cassert>
#include <iostream>
#include <vector>

int main()
{
    using namespace SovSaveFramePolicy;
    const std::uint8_t Known[] = {'1','2','3','4','5','6','7','8','9'};
    assert(Checksum(Known, sizeof Known) == 0xcbf43926u);
    assert(Checksum(nullptr, 0) == 0u);
    std::vector<std::uint8_t> Bytes(HeaderBytes + 1024);
    Write32(Bytes.data(), Magic); Write32(Bytes.data() + 4, Version);
    Write32(Bytes.data() + 8, static_cast<std::uint32_t>(Bytes.size() - HeaderBytes));
    for (std::size_t I = HeaderBytes; I < Bytes.size(); ++I) { Bytes[I] = static_cast<std::uint8_t>(I); }
    Write32(Bytes.data() + 12, Checksum(Bytes.data() + HeaderBytes, Bytes.size() - HeaderBytes));
    std::size_t Offset = 0, Length = 0;
    assert(Inspect(Bytes.data(), Bytes.size(), Offset, Length) == Format::Framed && Offset == HeaderBytes && Length == 1024);
    for (std::size_t End = 0; End < Bytes.size(); ++End)
    { assert(Inspect(Bytes.data(), End, Offset, Length) == Format::Invalid); }
    for (std::size_t Index = 0; Index < Bytes.size(); ++Index)
    {
        Bytes[Index] ^= 0x80u;
        assert(Inspect(Bytes.data(), Bytes.size(), Offset, Length) == Format::Invalid);
        Bytes[Index] ^= 0x80u;
    }
    auto Bad = Bytes; Write32(Bad.data() + 8, 0xffffffffu);
    assert(Inspect(Bad.data(), Bad.size(), Offset, Length) == Format::Invalid);
    assert(Inspect(Bytes.data(), MaximumBytes + 1, Offset, Length) == Format::Invalid);
    Bad.push_back(0);
    assert(Inspect(Bad.data(), Bad.size(), Offset, Length) == Format::Invalid);
    const std::uint8_t Legacy[] = {'G','V','A','S',3,0,0,0};
    assert(Inspect(Legacy, sizeof Legacy, Offset, Length) == Format::LegacyUnreal);
    // Legacy recognition is not acceptance: production must still validate exact engine/class header and envelope integrity.
    std::cout << "PASS: raw save frame checksum, every truncation/byte mutation, lengths, and genuine GVAS recognition\n";
}
