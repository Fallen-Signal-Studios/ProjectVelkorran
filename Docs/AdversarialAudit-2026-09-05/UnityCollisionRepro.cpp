// Audit evidence only. Two unchanged production test helper bodies with minimal declarations.
// This proves failure if the translation units are in one UBT unity file, not their actual UE grouping.
struct UNarrativeAttributeSetBase { static int GetShieldAttribute(); };
struct FStubASC { float GetNumericAttribute(int); };
struct ASovAxiomRuntimeTestCharacter { FStubASC* GetNarrativeAbilitySystemComponent(); };

namespace {
#line 88 "Source/ProjectVelkorran/Private/Tests/SovTarrikPayloadRuntimeTests.cpp"
float Shield(ASovAxiomRuntimeTestCharacter* Actor)
{
	return Actor->GetNarrativeAbilitySystemComponent()->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute());
}
}

namespace {
#line 86 "Source/ProjectVelkorran/Private/Tests/SovSelenePayloadRuntimeTests.cpp"
float Shield(ASovAxiomRuntimeTestCharacter* Target)
	{
		return Target->GetNarrativeAbilitySystemComponent()->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute());
	}
}
