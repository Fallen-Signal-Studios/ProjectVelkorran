// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cstddef>
#include <cstdint>
#include <array>

/** The outer frame is deliberately independent of UObject serialization and allocation. */
namespace SovSaveFramePolicy
{
constexpr std::uint32_t Magic = 0x32465653u; // SVF2, little endian
constexpr std::uint32_t Version = 1;
constexpr std::size_t HeaderBytes = 16;
constexpr std::size_t MaximumBytes = 64u * 1024u * 1024u;
constexpr std::uint32_t UnrealMagic = 0x53415647u; // GVAS, genuine pre-frame UE saves
enum class Format { Invalid, LegacyUnreal, Framed };
inline std::uint32_t Read32(const std::uint8_t* Bytes)
{ return std::uint32_t(Bytes[0]) | (std::uint32_t(Bytes[1]) << 8) | (std::uint32_t(Bytes[2]) << 16) | (std::uint32_t(Bytes[3]) << 24); }
inline void Write32(std::uint8_t* Bytes, std::uint32_t Value)
{ for (unsigned Index = 0; Index < 4; ++Index) { Bytes[Index] = static_cast<std::uint8_t>(Value >> (Index * 8)); } }
constexpr std::array<std::uint32_t, 256> BuildChecksumTable()
{
    std::array<std::uint32_t, 256> Table{};
    for (std::size_t Index = 0; Index < Table.size(); ++Index)
    {
        auto Value = static_cast<std::uint32_t>(Index);
        for (unsigned Bit = 0; Bit < 8; ++Bit) { Value = (Value >> 1) ^ (0xedb88320u & (0u - (Value & 1u))); }
        Table[Index] = Value;
    }
    return Table;
}
inline std::uint32_t Checksum(const std::uint8_t* Bytes, std::size_t Count)
{
    static constexpr auto Table = BuildChecksumTable();
    std::uint32_t Value = ~0u;
    for (std::size_t Index = 0; Index < Count; ++Index) { Value = (Value >> 8) ^ Table[(Value ^ Bytes[Index]) & 255u]; }
    return ~Value;
}
inline Format Inspect(const std::uint8_t* Bytes, std::size_t Count, std::size_t& Offset, std::size_t& Length)
{
    Offset = Length = 0;
    if (!Bytes || Count < 4 || Count > MaximumBytes) { return Format::Invalid; }
    if (Read32(Bytes) == UnrealMagic)
    { Offset = 0; Length = Count; return Format::LegacyUnreal; }
    if (Count < HeaderBytes || Read32(Bytes) != Magic || Read32(Bytes + 4) != Version) { return Format::Invalid; }
    const std::size_t Payload = Read32(Bytes + 8);
    if (Payload < 4 || Payload != Count - HeaderBytes || Read32(Bytes + 12) != Checksum(Bytes + HeaderBytes, Payload))
    { return Format::Invalid; }
    Offset = HeaderBytes; Length = Payload; return Format::Framed;
}
}
