// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cstddef>
#include <vector>

namespace SovCampaignDependencyPolicy
{
inline bool CanReach(const std::vector<std::vector<std::size_t>>& Edges, std::size_t Start,
	std::size_t Target, std::size_t Excluded)
{
	if (Start >= Edges.size() || Target >= Edges.size()) { return false; }
	std::vector<bool> Visited(Edges.size(), false);
	std::vector<std::size_t> Pending{Start};
	while (!Pending.empty())
	{
		const std::size_t Current = Pending.back(); Pending.pop_back();
		if (Current >= Edges.size() || Current == Excluded || Visited[Current]) { continue; }
		Visited[Current] = true;
		if (Current == Target) { return true; }
		for (const std::size_t Next : Edges[Current]) { Pending.push_back(Next); }
	}
	return false;
}
/** Every declared entry path must traverse this mandatory strict-predecessor writer. */
inline bool IsMandatoryPredecessor(const std::vector<std::vector<std::size_t>>& Edges,
	std::size_t Writer, std::size_t Target, bool Mandatory)
{
	if (!Mandatory || Edges.empty() || Edges.size() > 4096 || Writer >= Edges.size()
		|| Target >= Edges.size() || Writer == Target) { return false; }
	std::vector<bool> HasPredecessor(Edges.size(), false);
	for (const auto& Successors : Edges)
	{
		if (Successors.size() > 4096) { return false; }
		for (const std::size_t Next : Successors)
		{ if (Next >= Edges.size()) { return false; } HasPredecessor[Next] = true; }
	}
	if (!CanReach(Edges, Writer, Target, Edges.size())) { return false; }
	bool ReachableEntry = false;
	for (std::size_t Entry = 0; Entry < Edges.size(); ++Entry)
	{
		if (HasPredecessor[Entry]) { continue; }
		ReachableEntry |= CanReach(Edges, Entry, Target, Edges.size());
		if (CanReach(Edges, Entry, Target, Writer)) { return false; }
	}
	return ReachableEntry;
}
}
