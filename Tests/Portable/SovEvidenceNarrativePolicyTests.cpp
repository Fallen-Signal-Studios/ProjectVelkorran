// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovEvidencePolicy.h"
#include "Narrative/SovNarrativeGraphPolicy.h"
#include <cassert>
#include <iostream>
#include <limits>

int main()
{
    unsigned Checks = 0;
    for (unsigned Current = 0; Current <= 6; ++Current)
    {
        for (unsigned Requested = 0; Requested <= 6; ++Requested)
        {
            for (unsigned Flags = 0; Flags < 16; ++Flags)
            {
                const bool Independent = (Flags & 1) != 0, Authentic = (Flags & 2) != 0;
                const bool Destination = (Flags & 4) != 0, AlreadyCopied = (Flags & 8) != 0;
                bool Expected = false;
                if (Current <= 5 && Requested > 0 && Requested <= 5)
                {
                    if (Requested == 5) { Expected = Current >= 4 && Destination && !AlreadyCopied; }
                    else if (Requested == Current + 1)
                    { Expected = (Requested != 3 || Independent) && (Requested != 4 || Authentic); }
                }
                assert(SovEvidencePolicy::CanAdvance(Current, Requested, Independent, Authentic, Destination, AlreadyCopied) == Expected);
                ++Checks;
            }
        }
    }
    for (unsigned GraphBits = 0; GraphBits < 65536; ++GraphBits)
    {
        std::vector<std::vector<std::size_t>> Edges(4);
        bool Closure[4][4] = {};
        for (unsigned From = 0; From < 4; ++From)
        {
            Closure[From][From] = true;
            for (unsigned To = 0; To < 4; ++To)
            {
                if ((GraphBits & (1U << (From * 4 + To))) != 0)
                { Edges[From].push_back(To); Closure[From][To] = true; }
            }
        }
        // Independent all-pairs oracle proves the production traversal on every four-node directed graph.
        for (unsigned Via = 0; Via < 4; ++Via)
            for (unsigned From = 0; From < 4; ++From)
                for (unsigned To = 0; To < 4; ++To)
                    Closure[From][To] = Closure[From][To] || (Closure[From][Via] && Closure[Via][To]);
        const auto Result = SovNarrativeGraphPolicy::Analyze(Edges, 0);
        assert(Result.ValidEdges);
        for (unsigned Node = 0; Node < 4; ++Node)
        {
            bool Exit = false;
            for (unsigned Candidate = 0; Candidate < 4; ++Candidate)
            { Exit = Exit || (Edges[Candidate].empty() && Closure[Node][Candidate]); }
            assert(Result.Reachable[Node] == Closure[0][Node]);
            assert(Result.CanExit[Node] == Exit);
            Checks += 2;
        }
    }
    assert(!SovNarrativeGraphPolicy::Analyze({}, 0).ValidEdges);
    assert(!SovNarrativeGraphPolicy::Analyze({{1}}, 0).ValidEdges);
    assert(!SovNarrativeGraphPolicy::Analyze({{}}, 1).ValidEdges);
    assert(!SovNarrativeGraphPolicy::Analyze(std::vector<std::vector<std::size_t>>(4097), 0).ValidEdges);
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    const double Infinity = std::numeric_limits<double>::infinity();
    assert(SovEvidencePolicy::InScanCone(1000, 1000, .9, .866));
    assert(!SovEvidencePolicy::InScanCone(1000.01, 1000, 1, .866));
    assert(!SovEvidencePolicy::InScanCone(10, 1000, -.9, .866));
    assert(!SovEvidencePolicy::InScanCone(10, 1000, .865, .866));
    assert(!SovEvidencePolicy::InScanCone(10, 5001, 1, .866));
    assert(!SovEvidencePolicy::InScanCone(-1, 1000, 1, .866));
    assert(!SovEvidencePolicy::InScanCone(NaN, 1000, 1, .866));
    assert(!SovEvidencePolicy::InScanCone(1, Infinity, 1, .866));
    assert(!SovEvidencePolicy::InScanCone(1, 1000, NaN, .866));
    assert(!SovEvidencePolicy::InScanCone(1, 1000, 1, NaN));
    assert(!SovEvidencePolicy::InScanCone(1, 1000, 1.1, .866));
    std::cout << "Evidence and Narrative: " << Checks << " exhaustive progression/graph checks plus malformed graph and bounded scan cases passed.\n";
}
