// Copyright Narrative Tools 2022. 


#include "QuestBlueprintCompiler.h"
#include "Tales/QuestBlueprintGeneratedClass.h"
#include "Tales/QuestSM.h"
#include "Tales/Quest.h"
#include "QuestGraphSchema.h"
#include "Kismet2/KismetReinstanceUtilities.h"
#include "QuestEditorSettings.h"

#define LOCTEXT_NAMESPACE "QuestBlueprintCompiler"

FQuestBlueprintCompilerContext::FQuestBlueprintCompilerContext(UQuestBlueprint* SourceQuestBP, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompilerOptions)
	: Super(SourceQuestBP, InMessageLog, InCompilerOptions)
	, NewQuestBlueprintClass(nullptr)
	, QuestSchema(nullptr)
{
}

FQuestBlueprintCompilerContext::~FQuestBlueprintCompilerContext()
{
}

void FQuestBlueprintCompilerContext::SpawnNewClass(const FString& NewClassName)
{

	NewQuestBlueprintClass = FindObject<UQuestBlueprintGeneratedClass>(Blueprint->GetOutermost(), *NewClassName);

	if (NewQuestBlueprintClass == nullptr)
	{
		NewQuestBlueprintClass = NewObject<UQuestBlueprintGeneratedClass>(Blueprint->GetOutermost(), FName(*NewClassName), RF_Public | RF_Transactional);
	}
	else
	{
		// Already existed, but wasn't linked in the Blueprint yet due to load ordering issues
		FBlueprintCompileReinstancer::Create(NewQuestBlueprintClass);
	}
	NewClass = NewQuestBlueprintClass;
}

void FQuestBlueprintCompilerContext::OnNewClassSet(UBlueprintGeneratedClass* ClassToUse)
{
	NewQuestBlueprintClass = Cast<UQuestBlueprintGeneratedClass>(ClassToUse);
}

void FQuestBlueprintCompilerContext::PrecompileFunction(FKismetFunctionContext& Context, EInternalCompilerFlags InternalFlags)
{
	Super::PrecompileFunction(Context, InternalFlags);
}

void FQuestBlueprintCompilerContext::CleanAndSanitizeClass(UBlueprintGeneratedClass* ClassToClean, UObject*& InOutOldCDO)
{
	UQuestBlueprint* QuestBP = GetQuestBlueprint();

	const bool bRecompilingOnLoad = Blueprint->bIsRegeneratingOnLoad;
	auto RenameObjectToTransientPackage = [bRecompilingOnLoad](UObject* ObjectToRename, const FName BaseName, bool bClearFlags)
	{
		const ERenameFlags RenFlags = REN_DontCreateRedirectors | (bRecompilingOnLoad ? REN_ForceNoResetLoaders : 0) | REN_NonTransactional | REN_DoNotDirty;

		if (BaseName.IsNone())
		{
			ObjectToRename->Rename(nullptr, GetTransientPackage(), RenFlags);
		}
		else
		{
			FName TransientArchetypeName = MakeUniqueObjectName(GetTransientPackage(), ObjectToRename->GetClass(), BaseName);
			ObjectToRename->Rename(*TransientArchetypeName.ToString(), GetTransientPackage(), RenFlags);
		}

		ObjectToRename->SetFlags(RF_Transient);

		if (bClearFlags)
		{
			ObjectToRename->ClearFlags(RF_Public | RF_Standalone | RF_ArchetypeObject);
		}
		FLinkerLoad::InvalidateExport(ObjectToRename);
	};

	if (!Blueprint->bIsRegeneratingOnLoad && bIsFullCompile)
	{
		if (UQuestBlueprintGeneratedClass* WBC_ToClean = Cast<UQuestBlueprintGeneratedClass>(ClassToClean))
		{
			if (UQuest* OldQuestTemplate = WBC_ToClean->GetQuestTemplate())
			{
				FString TransientArchetypeString = FString::Printf(TEXT("OLD_QUEST_TEMPLATE%s"), *OldQuestTemplate->GetName());
				RenameObjectToTransientPackage(OldQuestTemplate, *TransientArchetypeString, true);


				WBC_ToClean->SetQuestTemplate(nullptr);
			}
		}
	}

	//TODO maybe remove unused states and branches
	Super::CleanAndSanitizeClass(ClassToClean, InOutOldCDO);
}

void FQuestBlueprintCompilerContext::SaveSubObjectsFromCleanAndSanitizeClass(FSubobjectCollection& SubObjectsToSave, UBlueprintGeneratedClass* ClassToClean)
{
	Super::SaveSubObjectsFromCleanAndSanitizeClass(SubObjectsToSave, ClassToClean);

	check(ClassToClean == NewClass);
	NewQuestBlueprintClass = Cast<UQuestBlueprintGeneratedClass>((UObject*)NewClass);

	UQuestBlueprint* QuestBP = GetQuestBlueprint();
	SubObjectsToSave.AddObject(QuestBP->QuestTemplate);
}

void FQuestBlueprintCompilerContext::EnsureProperGeneratedClass(UClass*& TargetUClass)
{
	if (TargetUClass && !((UObject*)TargetUClass)->IsA(UQuestBlueprintGeneratedClass::StaticClass()))
	{
		FKismetCompilerUtilities::ConsignToOblivion(TargetUClass, Blueprint->bIsRegeneratingOnLoad);
		TargetUClass = nullptr;
	}
}


void FQuestBlueprintCompilerContext::CreateClassVariablesFromBlueprint()
{
	Super::CreateClassVariablesFromBlueprint();

	////Prototype attempt to trying to add vars to quest BP
	//if (UQuestBlueprint* QuestBPToScan = GetQuestBlueprint())
	//{
	//	UClass* ParentClass = QuestBPToScan->ParentClass;

	//	UQuest* QuestTemplate = QuestBPToScan->QuestTemplate;

	//	if (QuestTemplate)
	//	{
	//		UQuestState* State = QuestTemplate->GetStates().IsValidIndex(0) ? QuestTemplate->GetStates()[0] : nullptr;

	//		if (State)
	//		{
	//			FEdGraphPinType WidgetPinType(UEdGraphSchema_K2::PC_Object, NAME_None, State->GetClass(), EPinContainerType::None, false, FEdGraphTerminalType());
	//	
	//			// Always name the variable according to the underlying FName of the state object
	//			FProperty* StateProperty = CreateVariable(State->GetFName(), WidgetPinType);
	//			if ( StateProperty != nullptr )
	//			{
	//				//const FString DisplayName = Widget->IsGeneratedName() ? Widget->GetName() : Widget->GetLabelText().ToString();
	//				const FString DisplayName = State->GetName();

	//				StateProperty->SetMetaData(TEXT("DisplayName"), *DisplayName);
	//		
	//				// Only show variables if they're explicitly marked as variables.
	//				if (true)//Widget->bIsVariable )
	//				{
	//					StateProperty->SetPropertyFlags(CPF_BlueprintVisible | CPF_BlueprintReadOnly | CPF_DisableEditOnInstance);

	//					//const FString& CategoryName = Widget->GetCategoryName();
	//			
	//					// Only include Category metadata for variables (i.e. a visible/editable property); otherwise, UHT will raise a warning if this Blueprint is nativized.
	//					//WidgetProperty->SetMetaData(TEXT("Category"), *(CategoryName.IsEmpty() ? WidgetBP->GetName() : CategoryName));
	//				}

	//				StateProperty->SetPropertyFlags((CPF_PersistentInstance | CPF_ExportObject | CPF_InstancedReference));
	//				StateProperty->SetPropertyFlags(CPF_RepSkip);

	//				StateToMemberVariableMap.Add(State, StateProperty);
	//			}
	//		}
	//	}
	//}


}

void FQuestBlueprintCompilerContext::CopyTermDefaultsToDefaultObject(UObject* DefaultObject)
{
	FKismetCompilerContext::CopyTermDefaultsToDefaultObject(DefaultObject);


}

void FQuestBlueprintCompilerContext::FinishCompilingClass(UClass* Class)
{
	UQuestBlueprint* QuestBP = GetQuestBlueprint();
	UQuestBlueprintGeneratedClass* BPGClass = Cast<UQuestBlueprintGeneratedClass>(Class); 

	UClass* ParentClass = QuestBP->ParentClass;
	const bool bIsSkeletonOnly = CompileOptions.CompileType == EKismetCompileType::SkeletonOnly;

	if (!bIsSkeletonOnly)
	{
		if (!QuestBP->bHasBeenRegenerated)
		{
			UBlueprint::ForceLoadMembers(QuestBP->QuestTemplate);
		} 

		FixAbandonedQuestTemplate(QuestBP);

		if (BPGClass && QuestBP->QuestTemplate)
		{
			// Need to clear archetype flag before duplication as we check during dup to see if we should postload
			EObjectFlags PreviousFlags = QuestBP->QuestTemplate->GetFlags();
			QuestBP->QuestTemplate->ClearFlags(RF_ArchetypeObject);

			UQuest* NewQuestTemplate = Cast<UQuest>(StaticDuplicateObject(QuestBP->QuestTemplate, BPGClass, NAME_None, RF_AllFlags & ~RF_DefaultSubObject));
			BPGClass->SetQuestTemplate(NewQuestTemplate);

			QuestBP->QuestTemplate->SetFlags(PreviousFlags);
		}

		if (const UQuestEditorSettings* QuestSettings = GetDefault<UQuestEditorSettings>())
		{
			//Ensure we don't have duplicate IDs on any nodes. Map ID -> Node. If we find a key that already exists we have a duplicate
			TMap<FName, UQuestNode*> NodeMap;

			for (auto& Node : QuestBP->QuestTemplate->GetNodes())
			{
				if (Node)
				{
					//Always warn duplicate IDs even if warnings are disabled since this is a critical error 
					if (NodeMap.Find(Node->GetID()))
					{
						FText DuplicateIDError = FText::Format(LOCTEXT("DuplicateQuestIDsFound", "Found duplicate ID {0} on nodes {1} and {2}."), FText::FromString(Node->GetID().ToString()), FText::FromString(GetNameSafe(Node)), FText::FromString(GetNameSafe(NodeMap[Node->GetID()])));
						MessageLog.Error(*DuplicateIDError.ToString());
					}
					else
					{
						NodeMap.Add(Node->GetID(), Node);
					}
				}
			}
		}

	}

	Super::FinishCompilingClass(Class);
}

bool FQuestBlueprintCompilerContext::ValidateGeneratedClass(UBlueprintGeneratedClass* Class)
{
	bool SuperResult = Super::ValidateGeneratedClass(Class);
	bool Result = UQuestBlueprint::ValidateGeneratedClass(Class);

	return SuperResult && Result;
}


bool FQuestBlueprintCompilerContext::IsNodePure(const UEdGraphNode* Node) const
{
	if (const UK2Node* K2Node = Cast<const UK2Node>(Node))
	{
		return K2Node->IsNodePure();
	}
	// Uncomment this line from engine as it causes ensure on load which is annoying 
	//ensure(Node->IsA(UEdGraphNode_Comment::StaticClass()) || Node->IsA(UEdGraphNode_Documentation::StaticClass()));
	return true;
}

void FQuestBlueprintCompilerContext::FixAbandonedQuestTemplate(UQuestBlueprint* QuestBP)
{
	UQuest* QuestTemplate = QuestBP->QuestTemplate;

	if (ensure(QuestTemplate))
	{
		if (QuestTemplate->GetName() != TEXT("QuestTemplate"))
		{
			if (UQuest* AbandonedQuestTemplate = static_cast<UQuest*>(FindObjectWithOuter(QuestBP, UQuest::StaticClass(), TEXT("QuestTemplate"))))
			{
				AbandonedQuestTemplate->ClearFlags(RF_DefaultSubObject);
				AbandonedQuestTemplate->SetFlags(RF_Transient);
				AbandonedQuestTemplate->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional | REN_DoNotDirty);
			}

			QuestTemplate->Rename(TEXT("QuestTemplate"), nullptr, REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional | REN_DoNotDirty);
			QuestTemplate->SetFlags(RF_DefaultSubObject);
		}
	}
}

FQuestBlueprintCompiler::FQuestBlueprintCompiler()
{

}

bool FQuestBlueprintCompiler::CanCompile(const UBlueprint* Blueprint)
{
	return Cast<UQuestBlueprint>(Blueprint) != nullptr;
}

void FQuestBlueprintCompiler::PreCompile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions)
{

}

void FQuestBlueprintCompiler::Compile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results)
{
	if (UQuestBlueprint* QuestBlueprint = CastChecked<UQuestBlueprint>(Blueprint))
	{
		FQuestBlueprintCompilerContext Compiler(QuestBlueprint, Results, CompileOptions);
		Compiler.Compile();
		check(Compiler.NewClass);
	}
}

void FQuestBlueprintCompiler::PostCompile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions)
{

}

bool FQuestBlueprintCompiler::GetBlueprintTypesForClass(UClass* ParentClass, UClass*& OutBlueprintClass, UClass*& OutBlueprintGeneratedClass) const
{
	if (ParentClass == UQuest::StaticClass() || ParentClass->IsChildOf(UQuest::StaticClass()))
	{
		OutBlueprintClass = UQuestBlueprint::StaticClass();
		OutBlueprintGeneratedClass = UQuestBlueprintGeneratedClass::StaticClass();
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE