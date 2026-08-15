// Copyright Narrative Tools 2025.


#include "NarrativePoseSearchFeatureChannel_Climb.h"

#include "PoseSearch/PoseSearchContext.h"

void UNarrativePoseSearchFeatureChannel_Climb::BuildQuery(UE::PoseSearch::FSearchContext& SearchContext) const
{
		using namespace UE::PoseSearch;

	if (bUseBlueprintQueryOverride)
	{	

		UAnimInstance* AnimInst = Cast<UAnimInstance>(SearchContext.GetContext(SampleRole)->GetFirstObjectParam());

		if (!AnimInst)
		{
			check(false);
			return;
		}

		const FVector BonePositionWorld = BP_GetLocalPosition(AnimInst);
		//const FVector BonePosition = SearchContext.GetSamplePosition(SampleTimeOffset, OriginTimeOffset, SchemaBoneIdx, SchemaOriginBoneIdx, SampleRole, OriginRole, PermutationTimeType, &BonePositionWorld);
  		FFeatureVectorHelper::EncodeVector(SearchContext.EditFeatureVector(), ChannelDataOffset, BonePositionWorld, ComponentStripping, false);
		return;
	}

	// trying to get the BuildQuery data from another schema UPoseSearchFeatureChannel_Position already cached in the SearchContext
	if (SearchContext.IsUseCachedChannelData())
	{
		// composing a unique identifier to specify this channel with all the required properties to be able to share the query data with other channels of the same type
		uint32 UniqueIdentifier = GetClass()->GetUniqueID();
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, GetTypeHash(SampleRole));
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, GetTypeHash(OriginRole));
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, GetTypeHash(SamplingAttributeId));
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, GetTypeHash(SampleTimeOffset));
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, GetTypeHash(OriginTimeOffset));
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, GetTypeHash(SchemaBoneIdx));
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, GetTypeHash(SchemaOriginBoneIdx));
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, GetTypeHash(InputQueryPose));
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, GetTypeHash(ComponentStripping));
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, GetTypeHash(PermutationTimeType));

		TConstArrayView<float> CachedChannelData;
		if (const UPoseSearchFeatureChannel* CachedChannel = SearchContext.GetCachedChannelData(UniqueIdentifier, this, CachedChannelData))
		{
#if DO_CHECK
			const UPoseSearchFeatureChannel_Position* CachedPositionChannel = Cast<UPoseSearchFeatureChannel_Position>(CachedChannel);
			check(CachedPositionChannel);
			check(CachedPositionChannel->GetChannelCardinality() == ChannelCardinality);
			check(CachedChannelData.Num() == ChannelCardinality);

			// making sure there were no hash collisions
			check(CachedPositionChannel->SampleRole == SampleRole);
			check(CachedPositionChannel->OriginRole == OriginRole);
			check(CachedPositionChannel->SamplingAttributeId == SamplingAttributeId);
			check(CachedPositionChannel->SampleTimeOffset == SampleTimeOffset);
			check(CachedPositionChannel->OriginTimeOffset == OriginTimeOffset);
			check(CachedPositionChannel->SchemaBoneIdx == SchemaBoneIdx);
			check(CachedPositionChannel->SchemaOriginBoneIdx == SchemaOriginBoneIdx);
			check(CachedPositionChannel->InputQueryPose == InputQueryPose);
			check(CachedPositionChannel->ComponentStripping == ComponentStripping);
			check(CachedPositionChannel->PermutationTimeType == PermutationTimeType);
#endif //DO_CHECK

			// copying the CachedChannelData into this channel portion of the FeatureVectorBuilder
			FFeatureVectorHelper::Copy(SearchContext.EditFeatureVector().Slice(ChannelDataOffset, ChannelCardinality), 0, ChannelCardinality, CachedChannelData);
			return;
		}
	}

	const bool bCanUseCurrentResult = SearchContext.CanUseContinuingPoseValues(); //SearchContext.CanUseCurrentResult();
	const bool bSkip = InputQueryPose != EInputQueryPose::UseCharacterPose && bCanUseCurrentResult && SampleRole == OriginRole;
	const bool bIsRootBone = SchemaBoneIdx == RootSchemaBoneIdx;
	if (bSkip || (/*!SearchContext.ArePoseHistoriesValid() && deprecated as not needed */!bIsRootBone))
	{
		if (bCanUseCurrentResult)
		{
			FFeatureVectorHelper::Copy(SearchContext.EditFeatureVector(), ChannelDataOffset, ChannelCardinality, SearchContext.GetContinuingPoseValues()/*SearchContext.GetCurrentResultPoseVector()*/);
			return;
		}

		// we leave the SearchContext.EditFeatureVector() set to zero since the SearchContext.PoseHistory is invalid and it'll fail if we continue
		
		return;
	}
	
	// calculating the BonePosition in root bone space for the bone indexed by SchemaBoneIdx
	const FVector BonePosition = SearchContext.GetSamplePosition(SampleTimeOffset, OriginTimeOffset, SchemaBoneIdx, SchemaOriginBoneIdx, SampleRole, OriginRole, PermutationTimeType);
	FFeatureVectorHelper::EncodeVector(SearchContext.EditFeatureVector(), ChannelDataOffset, BonePosition, ComponentStripping, false);
}
