// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Save/SovCampaignSaveGame.h"
#include "Misc/Crc.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

namespace
{
    uint32 CalculateEnvelopeChecksum(const USovCampaignSaveGame& Save, bool bCanonicalLabel)
    {
        TArray<uint8> Bytes;
        FMemoryWriter Writer(Bytes, bCanonicalLabel);
        FObjectAndNameAsStringProxyArchive Ar(Writer, false);
        FSovSaveSlotHeader Copy = Save.Header;
        if (bCanonicalLabel)
        {
            // Persistent editor archives assign keyless FText a new localization
            // GUID on each memory write. Hash its invariant source meaning instead;
            // preserve the original localized text in the actual save envelope.
            Copy.MissionLabel = FText::AsCultureInvariant(Save.Header.MissionLabel.BuildSourceString());
        }
        FSovSaveSlotHeader::StaticStruct()->SerializeItem(Ar, &Copy, nullptr);
        for (const FSoftObjectPath& Asset : Save.RequiredAssets)
        { FString Path = Asset.ToString(); Ar << Path; }
        uint32 Checksum = FCrc::MemCrc32(Bytes.GetData(), Bytes.Num());
        Checksum = FCrc::MemCrc32(Save.NarrativePayload.GetData(), Save.NarrativePayload.Num(), Checksum);
        return FCrc::MemCrc32(Save.PortableSettings.GetData(), Save.PortableSettings.Num(), Checksum);
    }
}

uint32 USovCampaignSaveGame::CalculateChecksum() const
{
    return CalculateEnvelopeChecksum(*this, true);
}

bool USovCampaignSaveGame::HasValidIntegrity() const
{
    if (NarrativePayload.IsEmpty() || Header.Generation <= 0) { return false; }
    // Preserve previously valid schema-1 banks whose localized label already had
    // a stable key. New writes always use the canonical, repeatable checksum.
    return IntegrityChecksum == CalculateChecksum()
        || IntegrityChecksum == CalculateEnvelopeChecksum(*this, false);
}
