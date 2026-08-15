// Copyright Narrative Tools 2022. 


#include "Tales/QuestBlueprintGeneratedClass.h"
#include "Tales/Quest.h"
#include "Tales/QuestSM.h"

void UQuestBlueprintGeneratedClass::InitializeQuest(class UQuest* Quest)
{
	if (Quest)
	{
		//Do what UUserWidget uses to initialize quest from bgclass 
		Quest->DuplicateAndInitializeFromQuest(QuestTemplate);

		//TMap<FName, FObjectPropertyBase*> ObjectPropertiesMap;

		//for (TFieldIterator<FObjectPropertyBase>It(Quest->GetClass(), EFieldIterationFlags::Default); It; ++It)
		//{
		//	check(*It);
		//	ensureMsgf(!ObjectPropertiesMap.Contains(It->GetFName()), TEXT("There are properties with the same names: '%s'"), *It->GetName());
		//	ObjectPropertiesMap.Add(It->GetFName(), *It);
		//}

		//for(auto& State : QuestTemplate->States)
		//{
		//	if (State)
		//	{
		//		// Find state with the same name as the template and assign the new widget to it.
		//		if (FObjectPropertyBase** PropPtr = ObjectPropertiesMap.Find(State->GetFName()))
		//		{
		//			FObjectPropertyBase* Prop = *PropPtr;
		//			check(Prop);
		//			Prop->SetObjectPropertyValue_InContainer(UserWidget, State);
		//			UObject* Value = Prop->GetObjectPropertyValue_InContainer(UserWidget);
		//			check(Value == Widget);
		//		}
		//	}

		//}

	}
}

void UQuestBlueprintGeneratedClass::PostLoad()
{
	Super::PostLoad();

	if (QuestTemplate)
	{
		// We don't want any of these flags to carry over from the WidgetBlueprint
		QuestTemplate->ClearFlags(RF_Public | RF_ArchetypeObject | RF_DefaultSubObject);
	}
}

void UQuestBlueprintGeneratedClass::SetQuestTemplate(UQuest* InQuestTemplate)
{
	QuestTemplate = InQuestTemplate;

	//These flags will be on the blueprints quest template, need to clear them 
	if (QuestTemplate)
	{
		QuestTemplate->ClearFlags(RF_Public | RF_ArchetypeObject | RF_DefaultSubObject);
	}
}
