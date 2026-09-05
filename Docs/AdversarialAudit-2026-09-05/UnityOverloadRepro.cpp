// Audit evidence only. Unchanged Alive helper bodies with minimal type declarations.
// ValidSource passes Context.SourceASC.Get(), a mutable UAbilitySystemComponent*.
#include <cstdio>
constexpr float KINDA_SMALL_NUMBER = 0.0001f;
struct UNarrativeAttributeSetBase { static int GetHealthAttribute() { return 0; } };
struct UAbilitySystemComponent {
    bool bHasSet = false;
    template<class T> const T* GetSet() const { static T Value; return bHasSet ? &Value : nullptr; }
    bool HasMatchingGameplayTag(int) const { return false; }
    float GetNumericAttribute(int) const { return 100.f; }
};
template<class T> bool IsValid(const T* Value) { return Value != nullptr; }
struct FNarrativeGameplayTags { int State_IsDead = 0; static const FNarrativeGameplayTags& Get() { static FNarrativeGameplayTags T; return T; } };
struct FSovGameplayTags { int State_Fatal = 1; static const FSovGameplayTags& Get() { static FSovGameplayTags T; return T; } };

#if INCLUDE_PROTECTION
namespace {
#line 42 "Source/ProjectVelkorran/Private/Combat/SovProtectionInterceptReceipt.cpp"
bool Alive(UAbilitySystemComponent* ASC)
{
	return IsValid(ASC) && ASC->GetSet<UNarrativeAttributeSetBase>()
		&& !ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)
		&& !ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Fatal)
		&& ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > KINDA_SMALL_NUMBER;
}
}

#endif
namespace {
#line 21 "Source/ProjectVelkorran/Private/Combat/SovSelenePayload.cpp"
bool Alive(const UAbilitySystemComponent* ASC)
	{
		return IsValid(ASC) && !ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)
			&& !ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Fatal)
			&& (!ASC->GetSet<UNarrativeAttributeSetBase>()
				|| ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > KINDA_SMALL_NUMBER);
	}
}

int main() { UAbilitySystemComponent Source; const UAbilitySystemComponent* Target = &Source;
std::printf("mutable source lacking AttributeSet: %d; const target: %d\n", Alive(&Source), Alive(Target)); }
