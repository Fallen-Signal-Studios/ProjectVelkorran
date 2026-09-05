// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cstddef>
#include <vector>

namespace SovNarrativeGraphPolicy
{
    struct Result
    {
        bool ValidEdges = false;
        std::vector<bool> Reachable;
        std::vector<bool> CanExit;
    };

    // Bounded iterative traversal. A cycle is legal only when every reachable node can reach an exit.
    inline Result Analyze(const std::vector<std::vector<std::size_t>>& Edges, std::size_t Root)
    {
        Result Out;
        if (Edges.empty() || Edges.size() > 4096 || Root >= Edges.size()) { return Out; }
        Out.Reachable.resize(Edges.size(), false);
        Out.CanExit.resize(Edges.size(), false);
        std::vector<std::vector<std::size_t>> Parents(Edges.size());
        for (std::size_t Node = 0; Node < Edges.size(); ++Node)
        {
            if (Edges[Node].size() > 128) { return Out; }
            for (const std::size_t Next : Edges[Node])
            {
                if (Next >= Edges.size()) { return Out; }
                Parents[Next].push_back(Node);
            }
        }
        Out.ValidEdges = true;
        std::vector<std::size_t> Pending{Root};
        Out.Reachable[Root] = true;
        for (std::size_t Index = 0; Index < Pending.size(); ++Index)
        {
            for (const std::size_t Next : Edges[Pending[Index]])
            {
                if (!Out.Reachable[Next]) { Out.Reachable[Next] = true; Pending.push_back(Next); }
            }
        }
        Pending.clear();
        for (std::size_t Node = 0; Node < Edges.size(); ++Node)
        {
            if (Edges[Node].empty()) { Pending.push_back(Node); Out.CanExit[Node] = true; }
        }
        for (std::size_t Index = 0; Index < Pending.size(); ++Index)
        {
            for (const std::size_t Previous : Parents[Pending[Index]])
            {
                if (!Out.CanExit[Previous]) { Out.CanExit[Previous] = true; Pending.push_back(Previous); }
            }
        }
        return Out;
    }
}
