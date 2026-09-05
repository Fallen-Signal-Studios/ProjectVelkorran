// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cstddef>
namespace SovDiagnosticsPolicy
{
constexpr std::size_t MaximumRecords = 512;
constexpr std::size_t MaximumReceiptIds = 256;
template<typename Char> inline bool SafeId(const Char* Text, std::size_t Length)
{
	if (Length > 96 || (!Text && Length != 0)) { return false; }
	for (std::size_t I = 0; I < Length; ++I)
	{
		const Char C = Text[I];
		if (!((C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9') || C == '.' || C == '_' || C == '-')) { return false; }
	}
	return true;
}
inline bool DropOldestBeforeAppend(std::size_t Count) { return Count >= MaximumRecords; }
}
