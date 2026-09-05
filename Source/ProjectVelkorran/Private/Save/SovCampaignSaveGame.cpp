// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Save/SovCampaignSaveGame.h"
#include "Misc/Crc.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/MemoryReader.h"
#include "Kismet/GameplayStatics.h"
#include "Save/SovSaveFramePolicy.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    // Bound the genuine UE5 SaveGame header before StripSaveGameHeader performs string/array allocation.
    // Unsupported engine layouts fail with retained source bytes; they are never guessed or rewritten.
    bool ReadKnownHeader(const TArray<uint8>& Bytes, const FString& ExpectedClass, int64& BodyOffset)
    {
        int64 Position = 0;
        auto Take = [&](int64 Count)
        { if (Count < 0 || Position > Bytes.Num() - Count) { return false; } Position += Count; return true; };
        auto ReadInt = [&](uint32& Value)
        { if (!Take(4)) { return false; } Value = SovSaveFramePolicy::Read32(Bytes.GetData() + Position - 4); return true; };
        auto ReadString = [&](FString& Value, int32 Limit)
        {
            uint32 Encoded = 0; if (!ReadInt(Encoded)) { return false; }
            const int32 Signed = static_cast<int32>(Encoded);
            if (Signed == MIN_int32) { return false; }
            const int64 Count = Signed < 0 ? -int64(Signed) : int64(Signed);
            const int64 Width = Signed < 0 ? 2 : 1;
            if (Count > Limit || !Take(Count * Width)) { return false; }
            Value.Reset(); if (!Count) { return true; }
            const uint8* Data = Bytes.GetData() + Position - Count * Width;
            Value.Reserve(static_cast<int32>(Count - 1));
            for (int64 Index = 0; Index < Count; ++Index)
            {
                const uint16 Character = Data[Index * Width] | (Width == 2 ? uint16(Data[Index * Width + 1]) << 8 : 0);
                if (Index + 1 == Count) { if (Character != 0) { return false; } }
                else { if (!Character) { return false; } Value.AppendChar(static_cast<TCHAR>(Character)); }
            }
            return true;
        };
        uint32 Magic = 0, Format = 0, CustomFormat = 0, Versions = 0;
        if (!ReadInt(Magic) || Magic != SovSaveFramePolicy::UnrealMagic || !ReadInt(Format) || Format != 3
            || !Take(8 + 6 + 4)) { return false; } // UE4/UE5 package versions, engine major/minor/patch/changelist
        FString Branch, Class;
        if (!ReadString(Branch, 4096) || !ReadInt(CustomFormat) || CustomFormat != 3
            || !ReadInt(Versions) || Versions > 4096 || !Take(int64(Versions) * 20)
            || !ReadString(Class, 4096) || Class != ExpectedClass || Position >= Bytes.Num()) { return false; }
        BodyOffset = Position; return true;
    }
    class FBoundedSaveArchive final : public FObjectAndNameAsStringProxyArchive
    {
    public:
        explicit FBoundedSaveArchive(FArchive& Inner) : FObjectAndNameAsStringProxyArchive(Inner, false), Source(Inner)
        { ArMaxSerializeSize = static_cast<int64>(SovSaveFramePolicy::MaximumBytes); }
        void Serialize(void* Data, int64 Count) override
        {
            if (Count < 0 || Source.Tell() < 0 || Count > Source.TotalSize() - Source.Tell()) { SetError(); Source.SetError(); return; }
            FObjectAndNameAsStringProxyArchive::Serialize(Data, Count);
        }
    private:
        FArchive& Source;
    };
}

USaveGame* USovCampaignSaveGame::DecodeKnownSave(const TArray<uint8>& Bytes, UClass* ExpectedClass, UObject* Outer, FString& Error)
{
    int64 BodyOffset = 0;
    if (!ExpectedClass || !ExpectedClass->IsChildOf(USaveGame::StaticClass()) || Bytes.IsEmpty()
        || Bytes.Num() > static_cast<int64>(SovSaveFramePolicy::MaximumBytes)
        || !ReadKnownHeader(Bytes, ExpectedClass->GetPathName(), BodyOffset))
    { Error = TEXT("Save header, configured class, engine format or size is unsupported. Source bytes are retained."); return nullptr; }
    FMemoryReader Reader = UGameplayStatics::StripSaveGameHeader(Bytes);
    if (Reader.IsError() || Reader.Tell() != BodyOffset)
    { Error = TEXT("Save header disagrees with the current engine decoder. Source bytes are retained."); return nullptr; }
    TStrongObjectPtr<USaveGame> Save(NewObject<USaveGame>(Outer ? Outer : GetTransientPackage(), ExpectedClass));
    FBoundedSaveArchive Archive(Reader);
    Save->Serialize(Archive);
    if (Archive.IsError() || Reader.IsError() || Reader.Tell() != Reader.TotalSize())
    { Error = TEXT("Save archive was truncated, oversized or contained trailing data."); return nullptr; }
    return Save.Get();
}

bool USovCampaignSaveGame::EncodeFramed(USovCampaignSaveGame* Save, TArray<uint8>& Bytes, FString& Error)
{
    Bytes.Reset(); TArray<uint8> Payload;
    if (!Save || Save->GetClass() != StaticClass() || !UGameplayStatics::SaveGameToMemory(Save, Payload)
        || Payload.IsEmpty() || Payload.Num() > static_cast<int64>(SovSaveFramePolicy::MaximumBytes - SovSaveFramePolicy::HeaderBytes))
    { Error = TEXT("Campaign envelope could not be serialized within its size budget."); return false; }
    Bytes.SetNumUninitialized(static_cast<int32>(SovSaveFramePolicy::HeaderBytes) + Payload.Num());
    SovSaveFramePolicy::Write32(Bytes.GetData(), SovSaveFramePolicy::Magic);
    SovSaveFramePolicy::Write32(Bytes.GetData() + 4, SovSaveFramePolicy::Version);
    SovSaveFramePolicy::Write32(Bytes.GetData() + 8, static_cast<uint32>(Payload.Num()));
    SovSaveFramePolicy::Write32(Bytes.GetData() + 12, SovSaveFramePolicy::Checksum(Payload.GetData(), Payload.Num()));
    FMemory::Memcpy(Bytes.GetData() + SovSaveFramePolicy::HeaderBytes, Payload.GetData(), Payload.Num());
    Error.Reset(); return true;
}

USovCampaignSaveGame* USovCampaignSaveGame::DecodeFramed(const TArray<uint8>& Bytes, FString& Error, bool* bLegacy)
{
    std::size_t Offset = 0, Length = 0;
    const auto Format = SovSaveFramePolicy::Inspect(Bytes.GetData(), Bytes.Num(), Offset, Length);
    if (bLegacy) { *bLegacy = Format == SovSaveFramePolicy::Format::LegacyUnreal; }
    if (Format == SovSaveFramePolicy::Format::Invalid)
    { Error = TEXT("Save raw frame is corrupt, incomplete or exceeds its size budget. Source banks are retained."); return nullptr; }
    TArray<uint8> Payload; Payload.Append(Bytes.GetData() + Offset, static_cast<int32>(Length));
    auto* Save = Cast<USovCampaignSaveGame>(DecodeKnownSave(Payload, StaticClass(), GetTransientPackage(), Error));
    if (Save) { Save->bReadLegacyUnframed = Format == SovSaveFramePolicy::Format::LegacyUnreal; }
    return Save;
}

uint32 USovCampaignSaveGame::CalculateChecksum() const
{
    TArray<uint8> Bytes;
    FMemoryWriter Writer(Bytes);
    FObjectAndNameAsStringProxyArchive Ar(Writer, false);
    FSovSaveSlotHeader Copy = Header;
    FSovSaveSlotHeader::StaticStruct()->SerializeItem(Ar, &Copy, nullptr);
    for (const FSoftObjectPath& Asset : RequiredAssets)
    { FString Path = Asset.ToString(); Ar << Path; }
    uint32 Checksum = FCrc::MemCrc32(Bytes.GetData(), Bytes.Num());
    Checksum = FCrc::MemCrc32(NarrativePayload.GetData(), NarrativePayload.Num(), Checksum);
    return FCrc::MemCrc32(PortableSettings.GetData(), PortableSettings.Num(), Checksum);
}
bool USovCampaignSaveGame::HasValidIntegrity() const
{
    return NarrativePayload.Num() > 0 && Header.Generation > 0 && IntegrityChecksum == CalculateChecksum();
}
