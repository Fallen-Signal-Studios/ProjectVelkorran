// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cstddef>
#include <string>

/**
 * Text rules for the campaign cook-root validation, kept free of Unreal so Tests/Portable exercises them
 * without an editor build.
 *
 * Two inputs arrive as text. A UE 5.7 `-run=cook -CookList` log names every package the cooker would
 * cook, one per `LogCookList: Display:` line, prefixed `Rejected: ` when discovered but not cooked. And the
 * packaging settings store their paths as struct text such as `(Path="/Game/Aurelion/VFX")`.
 */
namespace SovCampaignCookRootPolicy
{
template<typename Char>
inline std::basic_string<Char> Widen(const char* Text)
{
	std::basic_string<Char> Result;
	for (; Text && *Text; ++Text) { Result.push_back(static_cast<Char>(*Text)); }
	return Result;
}

template<typename Char>
inline bool IsSpace(Char Character)
{
	return Character == Char(' ') || Character == Char('\t') || Character == Char('\r') || Character == Char('\n');
}

template<typename Char>
inline std::basic_string<Char> Trim(const std::basic_string<Char>& Text)
{
	std::size_t Begin = 0, End = Text.size();
	while (Begin < End && IsSpace(Text[Begin])) { ++Begin; }
	while (End > Begin && IsSpace(Text[End - 1])) { --End; }
	return Text.substr(Begin, End - Begin);
}

/** A long package name as the cooker prints it: rooted, at least one segment, no whitespace or quotes. */
template<typename Char>
inline bool IsLongPackageName(const std::basic_string<Char>& Text)
{
	if (Text.size() < 2 || Text[0] != Char('/') || Text[1] == Char('/')) { return false; }
	for (Char Character : Text)
	{
		if (IsSpace(Character) || Character == Char('"') || Character == Char('\'') || Character == Char(',')) { return false; }
	}
	return Text[Text.size() - 1] != Char('/');
}

/**
 * The package a cook-list line reports as cooked. False for rejected packages, instigator-only noise,
 * other log lines and filename forms, so a caller never treats "discovered" as "shipped".
 */
template<typename Char>
inline bool CookedPackageFromCookListLine(const std::basic_string<Char>& Line, std::basic_string<Char>& OutPackage)
{
	OutPackage.clear();
	std::basic_string<Char> Body = Line;
	const std::basic_string<Char> Marker = Widen<Char>("LogCookList: Display: ");
	const std::size_t MarkerAt = Body.find(Marker);
	if (MarkerAt != std::basic_string<Char>::npos) { Body = Body.substr(MarkerAt + Marker.size()); }
	Body = Trim(Body);
	if (Body.compare(0, 10, Widen<Char>("Rejected: ")) == 0) { return false; }
	const std::size_t InstigatorAt = Body.find(Widen<Char>(", Instigator: "));
	if (InstigatorAt != std::basic_string<Char>::npos) { Body = Trim(Body.substr(0, InstigatorAt)); }
	if (!IsLongPackageName(Body)) { return false; }
	OutPackage = Body;
	return true;
}

/**
 * The path inside packaging struct text, e.g. `(Path="/Game/X")` for Field "Path" or `(FilePath="/Game/M")`
 * for "FilePath". A bare rooted path is accepted as-is. The field must match as a whole name, so "Path"
 * never reads a "FilePath" value.
 */
template<typename Char>
inline bool PathFromConfigStruct(const std::basic_string<Char>& Value, const char* Field, std::basic_string<Char>& OutPath)
{
	OutPath.clear();
	const std::basic_string<Char> Text = Trim(Value);
	if (!Text.empty() && Text[0] == Char('/'))
	{
		OutPath = Text;
		return true;
	}
	const std::basic_string<Char> Key = Widen<Char>(Field) + Widen<Char>("=");
	std::size_t At = 0;
	while ((At = Text.find(Key, At)) != std::basic_string<Char>::npos)
	{
		const bool bWholeName = At == 0 || Text[At - 1] == Char('(') || Text[At - 1] == Char(',') || IsSpace(Text[At - 1]);
		if (!bWholeName) { ++At; continue; }
		std::size_t Begin = At + Key.size();
		std::size_t End;
		if (Begin < Text.size() && Text[Begin] == Char('"'))
		{
			++Begin;
			End = Text.find(Char('"'), Begin);
			if (End == std::basic_string<Char>::npos) { return false; }
		}
		else
		{
			End = Begin;
			while (End < Text.size() && Text[End] != Char(',') && Text[End] != Char(')') && !IsSpace(Text[End])) { ++End; }
		}
		OutPath = Text.substr(Begin, End - Begin);
		return !OutPath.empty();
	}
	return false;
}
}
