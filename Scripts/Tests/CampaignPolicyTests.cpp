// Portable regression scenarios use the production campaign policy without Unreal headers.
#include "Campaign/SovCampaignPolicy.h"
#include <cassert>
#include <iostream>
int main()
{
	using namespace SovCampaignPolicy;
	assert(CompleteBeat(false,true,false,true,true,true,false,false,false,false)==Result::Invalid);
	assert(CompleteBeat(true,false,false,true,true,true,false,false,false,false)==Result::Invalid);
	assert(CompleteBeat(true,true,true,false,false,false,true,true,false,true)==Result::Duplicate);
	assert(CompleteBeat(true,true,false,false,true,true,false,false,false,false)==Result::Prerequisite);
	assert(CompleteBeat(true,true,false,true,false,true,false,false,false,false)==Result::Knowledge);
	assert(CompleteBeat(true,true,false,true,true,false,false,false,false,false)==Result::Protected);
	assert(CompleteBeat(true,true,false,true,true,true,true,true,false,false)==Result::NotViewed);
	assert(CompleteBeat(true,true,false,true,true,true,true,true,true,true)==Result::NotViewed);
	assert(CompleteBeat(true,true,false,true,true,true,true,false,true,false)==Result::NotViewed);
	assert(CompleteBeat(true,true,false,true,true,true,true,true,true,false)==Result::Allowed);
	assert(CompleteBeat(true,true,false,true,true,true,false,true,false,true)==Result::Allowed);
	assert(CanHandoff(true,false,true,true,true,true,true));
	assert(!CanHandoff(false,false,true,true,true,true,true));
	assert(!CanHandoff(true,true,true,true,true,true,true));
	assert(!CanHandoff(true,false,false,true,true,true,true));
	assert(!CanHandoff(true,false,true,false,true,true,true));
	assert(!CanHandoff(true,false,true,true,false,true,true));
	assert(!CanHandoff(true,false,true,true,true,false,true));
	assert(!CanHandoff(true,false,true,true,true,true,false));
	assert(MayCommitAsync(7,7,true,true));
	assert(!MayCommitAsync(0,0,true,true));
	assert(!MayCommitAsync(8,7,true,true));
	assert(!MayCommitAsync(7,7,false,true));
	assert(!MayCommitAsync(7,7,true,false));
	std::cout << "24 campaign policy assertions passed\n";
}
