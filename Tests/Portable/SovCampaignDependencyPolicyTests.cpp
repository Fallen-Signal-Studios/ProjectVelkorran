#include "Validation/SovCampaignDependencyPolicy.h"
#include <cassert>
#include <iostream>
#include <vector>
int main()
{
	using Graph = std::vector<std::vector<std::size_t>>;
	using SovCampaignDependencyPolicy::IsMandatoryPredecessor;
	const Graph Linear{{1}, {2}, {}};
	assert(IsMandatoryPredecessor(Linear, 0, 2, true));
	assert(IsMandatoryPredecessor(Linear, 1, 2, true));
	assert(!IsMandatoryPredecessor(Linear, 1, 2, false));
	assert(!IsMandatoryPredecessor(Linear, 2, 2, true));
	assert(!IsMandatoryPredecessor(Linear, 2, 0, true));
	assert(!IsMandatoryPredecessor(Linear, 7, 2, true));
	const Graph Bypass{{1, 2}, {3}, {3}, {}};
	assert(!IsMandatoryPredecessor(Bypass, 1, 3, true));
	assert(IsMandatoryPredecessor(Bypass, 0, 3, true));
	const Graph MultipleEntries{{2}, {2}, {3}, {}};
	assert(!IsMandatoryPredecessor(MultipleEntries, 0, 3, true));
	assert(IsMandatoryPredecessor(MultipleEntries, 2, 3, true));
	const Graph RootlessCycle{{1}, {2}, {0}};
	assert(!IsMandatoryPredecessor(RootlessCycle, 0, 2, true));
	const Graph RootedCycle{{1}, {2}, {1, 3}, {}};
	assert(IsMandatoryPredecessor(RootedCycle, 1, 3, true));
	const Graph Disconnected{{1}, {}, {3}, {}};
	assert(!IsMandatoryPredecessor(Disconnected, 0, 3, true));
	const Graph BadEdge{{1, 99}, {}};
	assert(!IsMandatoryPredecessor(BadEdge, 0, 1, true));
	assert(!IsMandatoryPredecessor({}, 0, 1, true));
	std::cout << "Campaign prerequisite graph: 15 assertions passed\n";
}
