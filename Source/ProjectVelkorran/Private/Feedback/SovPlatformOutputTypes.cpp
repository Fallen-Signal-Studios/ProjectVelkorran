// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Feedback/SovPlatformOutputTypes.h"
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
