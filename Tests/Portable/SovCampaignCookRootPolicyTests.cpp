// Compiles the same text rules the campaign cook-root validation uses.
#include "Validation/SovCampaignCookRootPolicy.h"
#include <cassert>
#include <iostream>
#include <string>

namespace
{
using namespace SovCampaignCookRootPolicy;

bool Cooked(const std::string& Line, const std::string& Expected)
{
	std::string Package;
	return CookedPackageFromCookListLine(Line, Package) && Package == Expected;
}

bool NotCooked(const std::string& Line)
{
	std::string Package = "stale";
	return !CookedPackageFromCookListLine(Line, Package) && Package.empty();
}

bool StructPath(const std::string& Value, const char* Field, const std::string& Expected)
{
	std::string Path;
	return PathFromConfigStruct(Value, Field, Path) && Path == Expected;
}

bool NoStructPath(const std::string& Value, const char* Field)
{
	std::string Path = "stale";
	return !PathFromConfigStruct(Value, Field, Path) && Path.empty();
}
}

int main()
{
	int Checks = 0;
	auto Check = [&Checks](bool Condition) { assert(Condition); ++Checks; };

	// Real line shapes from the UE 5.7 cook list, with and without the log prefix.
	Check(Cooked("[2026.09.13-03.58.38:798][  0]LogCookList: Display: /Game/Aurelion/Maps/L_Aurelion_M12, Instigator: CommandLinePackage",
		"/Game/Aurelion/Maps/L_Aurelion_M12"));
	Check(Cooked("/NarrativePro/Pro/Core/Tales/Events/NE_GiveXP, Instigator: HardDependency: /NarrativePro/Pro/Demo/Character/Definitions/Luca/Dialogue/DBP_Luca",
		"/NarrativePro/Pro/Core/Tales/Events/NE_GiveXP"));
	Check(Cooked("/NarrativePro/Pro/Core/BP/Framework/BP_NarrativeGameMode, Instigator: GameDefaultObject: GlobalDefaultGameMode",
		"/NarrativePro/Pro/Core/BP/Framework/BP_NarrativeGameMode"));
	Check(Cooked("   /Engine/Maps/Entry   ", "/Engine/Maps/Entry"));   // list without instigators
	Check(Cooked("LogCookList: Display: /ACLPlugin/ACLAnimBoneCompressionSettings, Instigator: StartupPackage",
		"/ACLPlugin/ACLAnimBoneCompressionSettings"));

	// Discovered but not cooked is never a root: that is the distinction T2 is about.
	Check(NotCooked("LogCookList: Display: Rejected: /NarrativePro/Pro/Editor/UI/Widgets/Tales/WBP_DefaultDialogueNode, Instigator: StartupPackage"));
	Check(NotCooked("Rejected: /Game/X, Instigator: HardDependency: /Game/Y"));

	// Other log lines, filename forms and malformed names are ignored rather than guessed at.
	Check(NotCooked("[2026.09.13-03.58.47:375][  0]LogCook: Display: INCREMENTAL COOK DEPENDENCIES: Disabled."));
	Check(NotCooked("../../../../Plugins/NNE/NNEDenoiser/Content/NNED_Oidn2-3_Fast.uasset, Instigator: CommandLineDirectory"));
	Check(NotCooked(""));
	Check(NotCooked("/"));
	Check(NotCooked("//Game/X"));
	Check(NotCooked("/Game/Folder/"));
	Check(NotCooked("/Game/Has Space, Instigator: StartupPackage"));
	Check(NotCooked("/Game/Quoted\", Instigator: StartupPackage"));
	Check(NotCooked("LogCookList: Display: , Instigator: StartupPackage"));
	Check(NotCooked("LogCookList: Display: Instigator: StartupPackage"));

	// Wide characters behave identically, as the engine passes TCHAR strings.
	{
		std::u16string Package;
		Check(CookedPackageFromCookListLine(std::u16string(u"/Game/A, Instigator: X"), Package) && Package == u"/Game/A");
		Check(!CookedPackageFromCookListLine(std::u16string(u"Rejected: /Game/A, Instigator: X"), Package));
	}

	// Packaging struct text exactly as DefaultGame.ini stores it.
	Check(StructPath("(Path=\"/Game/Aurelion/VFX\")", "Path", "/Game/Aurelion/VFX"));
	Check(StructPath("(FilePath=\"/Game/Aurelion/Maps/L_Aurelion_M12\")", "FilePath", "/Game/Aurelion/Maps/L_Aurelion_M12"));
	Check(StructPath("(Path=/Game/Unquoted)", "Path", "/Game/Unquoted"));
	Check(StructPath("( Path=\"/Game/Spaced\" )", "Path", "/Game/Spaced"));
	Check(StructPath("/Game/Bare", "Path", "/Game/Bare"));
	Check(StructPath("  /Game/BareTrimmed  ", "FilePath", "/Game/BareTrimmed"));
	Check(StructPath("(Other=1,Path=\"/Game/Second\")", "Path", "/Game/Second"));

	// "Path" must not read a "FilePath" value, and empty or unterminated values fail closed.
	Check(NoStructPath("(FilePath=\"/Game/Maps/M\")", "Path"));
	Check(NoStructPath("(Path=\"\")", "Path"));
	Check(NoStructPath("(Path=\"/Game/Unterminated)", "Path"));
	Check(NoStructPath("", "Path"));
	Check(NoStructPath("()", "Path"));
	Check(NoStructPath("None", "Path"));

	std::cout << Checks << " campaign cook-root text checks passed\n";
	return 0;
}
