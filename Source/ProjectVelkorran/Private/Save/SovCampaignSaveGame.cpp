// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Save/SovCampaignSaveGame.h"
#include "Misc/Crc.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

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
