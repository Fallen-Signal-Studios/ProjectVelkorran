// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Misc/Crc.h"

/**
 * The outer frame around every stored campaign envelope.
 *
 * Unreal's save-game loader trusts the bytes it is given: a damaged length field inside them can ask for an
 * allocation of gigabytes, and the envelope's own checksum can only be computed after that load. The frame's
 * magic, length and CRC are checked on raw bytes first, so damaged storage is rejected before any of it
 * reaches the loader. Envelopes written before the frame existed are still accepted, as "legacy", and are
 * framed the next time they are written.
 */
namespace SovSaveEnvelopeFrame
{
    inline constexpr uint32 Magic = 0x45564F53;       // "SOVE"
    inline constexpr uint32 Version = 1;
    inline constexpr int32 HeaderBytes = 16;
    inline constexpr int32 MaximumPayloadBytes = 64 * 1024 * 1024;
    inline constexpr uint32 LegacyEngineTag = 0x53415647; // "GVAS", Unreal's own save-game file tag
    inline constexpr int32 LegacyEngineVersion = 3;

    enum class EDecode : uint8 { Framed, Legacy, Rejected };

    inline uint32 ReadUInt32(const uint8* At) { return uint32(At[0]) | uint32(At[1]) << 8 | uint32(At[2]) << 16 | uint32(At[3]) << 24; }
    inline void WriteUInt32(TArray<uint8>& Out, uint32 Value)
    { for (int32 Shift = 0; Shift < 32; Shift += 8) { Out.Add(uint8(Value >> Shift)); } }

    inline bool Encode(const TArray<uint8>& Payload, TArray<uint8>& OutFramed)
    {
        OutFramed.Reset();
        if (Payload.IsEmpty() || Payload.Num() > MaximumPayloadBytes) { return false; }
        OutFramed.Reserve(HeaderBytes + Payload.Num());
        WriteUInt32(OutFramed, Magic); WriteUInt32(OutFramed, Version);
        WriteUInt32(OutFramed, uint32(Payload.Num())); WriteUInt32(OutFramed, FCrc::MemCrc32(Payload.GetData(), Payload.Num()));
        OutFramed.Append(Payload);
        return true;
    }

    /** OutPayload receives the bytes to hand to the loader: the frame's payload, or the whole legacy envelope. */
    inline EDecode Decode(const TArray<uint8>& Stored, TArray<uint8>& OutPayload)
    {
        OutPayload.Reset();
        if (Stored.Num() < 8 || Stored.Num() > HeaderBytes + MaximumPayloadBytes) { return EDecode::Rejected; }
        const uint8* Data = Stored.GetData();
        if (ReadUInt32(Data) == Magic)
        {
            if (Stored.Num() < HeaderBytes || ReadUInt32(Data + 4) != Version) { return EDecode::Rejected; }
            const uint32 Length = ReadUInt32(Data + 8);
            if (Length == 0 || int64(Length) != int64(Stored.Num()) - HeaderBytes
                || FCrc::MemCrc32(Data + HeaderBytes, int32(Length)) != ReadUInt32(Data + 12)) { return EDecode::Rejected; }
            OutPayload.Append(Data + HeaderBytes, int32(Length));
            return EDecode::Framed;
        }
        if (ReadUInt32(Data) != LegacyEngineTag || int32(ReadUInt32(Data + 4)) != LegacyEngineVersion) { return EDecode::Rejected; }
        OutPayload = Stored;
        return EDecode::Legacy;
    }
}
