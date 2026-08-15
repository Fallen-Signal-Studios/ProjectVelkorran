
// Copyright Narrative Tools 2025.

#include "Camera/NarrativeSettingsFOV.h"
#include "Core/CameraEvaluationContext.h"
#include "Core/CameraParameterReader.h"
#include "GameplayCameras.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"
#include "UnrealFramework/NarrativePlayerController.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(NarrativeSettingsFOV)

namespace UE::Cameras
{

	class FNarrativeFOVCameraNodeEvaluator: public FCameraNodeEvaluator
	{
		UE_DECLARE_CAMERA_NODE_EVALUATOR(NARRATIVEARSENAL_API, FNarrativeFOVCameraNodeEvaluator)

	protected:

		virtual void OnInitialize(const FCameraNodeEvaluatorInitializeParams& Params, FCameraNodeEvaluationResult& OutResult) override;
		virtual void OnRun(const FCameraNodeEvaluationParams& Params, FCameraNodeEvaluationResult& OutResult) override;

	private:

		TCameraParameterReader<float> FieldOfViewReader;
	};

	UE_DEFINE_CAMERA_NODE_EVALUATOR(FNarrativeFOVCameraNodeEvaluator)

	void FNarrativeFOVCameraNodeEvaluator::OnInitialize(const FCameraNodeEvaluatorInitializeParams& Params, FCameraNodeEvaluationResult& OutResult)
	{
		const UNarrativeSettingsFOVCameraNode* FieldOfViewNode = GetCameraNodeAs<UNarrativeSettingsFOVCameraNode>();
		//FieldOfViewReader.Initialize(FieldOfViewNode->FieldOfViewPct);
	}

	void FNarrativeFOVCameraNodeEvaluator::OnRun(const FCameraNodeEvaluationParams& Params, FCameraNodeEvaluationResult& OutResult)
	{

		//if (Params.EvaluationContext)
		//{
		//	if (ANarrativePlayerController* PC = Cast<ANarrativePlayerController>(Params.EvaluationContext->GetPlayerController()))
		//	{
		//		const float DesiredFOV = PC->GetDesiredFOV();

		//		OutResult.CameraPose.SetFieldOfView(DesiredFOV);
		//		OutResult.CameraPose.SetFocalLength(-1);
		//	}
		//}
	}

}  // namespace UE::Cameras

UNarrativeSettingsFOVCameraNode::UNarrativeSettingsFOVCameraNode(const FObjectInitializer& ObjectInit)
{
	//FieldOfViewPct = 1.f;
}

FCameraNodeEvaluatorPtr UNarrativeSettingsFOVCameraNode::OnBuildEvaluator(FCameraNodeEvaluatorBuilder& Builder) const
{
	using namespace UE::Cameras;
	return Builder.BuildEvaluator<FNarrativeFOVCameraNodeEvaluator>();
}
