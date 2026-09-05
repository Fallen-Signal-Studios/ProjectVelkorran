// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Feedback/SovPlatformOutputTypes.h"
#include "Settings/SovDisplayPolicy.h"

bool FSovHDRCalibration::IsValid() const
{
	return SovDisplayPolicy::ValidCalibration(BlackFloorNits, PaperWhiteNits, UIWhiteNits);
}
bool FSovHDRCalibration::Equals(const FSovHDRCalibration& Other) const
{
	return FMath::IsNearlyEqual(BlackFloorNits, Other.BlackFloorNits, 1.e-7f)
		&& FMath::IsNearlyEqual(PaperWhiteNits, Other.PaperWhiteNits, .01f)
		&& FMath::IsNearlyEqual(UIWhiteNits, Other.UIWhiteNits, .01f);
}
#include "Feedback/SovHapticPolicy.h"
float FSovHapticSettings::Scale(ESovHapticChannel Channel) const
{
	switch (Channel)
	{
	case ESovHapticChannel::Combat: return Combat;
	case ESovHapticChannel::Interaction: return Interaction;
	case ESovHapticChannel::Cinematic: return Cinematic;
	case ESovHapticChannel::Ambience: return Ambience;
	case ESovHapticChannel::UI: return UI;
	default: return 0.f;
	}
}
bool FSovHapticSettings::IsValid() const
{
	return SovHapticPolicy::Unit(Master) && SovHapticPolicy::Unit(Combat) && SovHapticPolicy::Unit(Interaction)
		&& SovHapticPolicy::Unit(Cinematic) && SovHapticPolicy::Unit(Ambience) && SovHapticPolicy::Unit(UI);
}
