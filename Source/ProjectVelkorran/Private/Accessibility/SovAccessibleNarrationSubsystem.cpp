// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Accessibility/SovAccessibleNarrationSubsystem.h"
#include "Modules/ModuleManager.h"
#include "Async/Async.h"
#if SOV_WITH_TEXT_TO_SPEECH
#include "TextToSpeechModule.h"
#include "GenericPlatform/ITextToSpeechFactory.h"
#include "GenericPlatform/TextToSpeechBase.h"
#endif

#define LOCTEXT_NAMESPACE "SovAccessibleNarration"
namespace
{
#if SOV_WITH_TEXT_TO_SPEECH
	class FSovPlatformSpeech final : public ISovAccessibleSpeech
	{
	public:
		explicit FSovPlatformSpeech(TSharedRef<FTextToSpeechBase> InSpeech) : Speech(InSpeech) {}
		virtual ~FSovPlatformSpeech() override { Stop(); }
		virtual bool Speak(const FString& Text, FSimpleDelegate Finished) override
		{
			Speech->Activate();
			if (!Speech->IsActive() || Speech->IsMuted()) { return false; }
			Speech->SetTextToSpeechFinishedSpeakingDelegate(
				FTextToSpeechBase::FOnTextToSpeechFinishSpeaking::CreateLambda([Finished]() { Finished.ExecuteIfBound(); }));
			Speech->Speak(Text);
			return true;
		}
		virtual void Stop() override
		{
			// Unbind BEFORE stopping: stopping is never a successful completion.
			Speech->SetTextToSpeechFinishedSpeakingDelegate(FTextToSpeechBase::FOnTextToSpeechFinishSpeaking());
			Speech->StopSpeaking();
			Speech->Deactivate();
		}
	private:
		TSharedRef<FTextToSpeechBase> Speech;
	};
	TSharedPtr<ITextToSpeechFactory> PlatformFactory()
	{
		ITextToSpeechModule* Module = FModuleManager::LoadModulePtr<ITextToSpeechModule>(TEXT("TextToSpeech"));
		return Module ? Module->GetPlatformFactory() : nullptr;
	}
#endif
}

bool USovAccessibleNarrationSubsystem::IsSupported() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (TestBackendFactory) { return true; }
#endif
#if SOV_WITH_TEXT_TO_SPEECH
	return PlatformFactory().IsValid();
#else
	return false;
#endif
}

FText USovAccessibleNarrationSubsystem::GetUnavailableReason() const
{
	return IsSupported() ? FText::GetEmpty() : LOCTEXT("Unavailable",
		"Completion-aware narration is unavailable on this platform. Dialogue pressure will wait; choices remain selectable.");
}

TSharedPtr<ISovAccessibleSpeech> USovAccessibleNarrationSubsystem::CreateBackend() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (TestBackendFactory) { return TestBackendFactory(); }
#endif
#if SOV_WITH_TEXT_TO_SPEECH
	if (TSharedPtr<ITextToSpeechFactory> Factory = PlatformFactory())
	{
		return MakeShared<FSovPlatformSpeech>(Factory->Create());
	}
#endif
	return nullptr;
}

bool USovAccessibleNarrationSubsystem::Announce(UObject* Owner, const FText& Text,
	FGuid& OutRequest, FSovNarrationCompletion Completion)
{
	check(IsInGameThread());
	OutRequest.Invalidate();
	if (bShuttingDown || !IsValid(Owner) || Text.IsEmpty()) { return false; }
	const uint64 Expected = ++Generation;
	Retire(true);
	// A cancelled owner's callback may have submitted a newer request.
	if (Generation != Expected || bShuttingDown) { return false; }
	TSharedPtr<ISovAccessibleSpeech> NewBackend = CreateBackend();
	if (!NewBackend || Generation != Expected || bShuttingDown || !IsValid(Owner)) { return false; }
	Backend = NewBackend;
	RequestOwner = Owner;
	ActiveRequest = FGuid::NewGuid();
	OutRequest = ActiveRequest;
	ActiveCompletion = MoveTemp(Completion);
	const FGuid Request = ActiveRequest;
	const TWeakObjectPtr<USovAccessibleNarrationSubsystem> WeakThis(this);
	const bool bAccepted = NewBackend->Speak(Text.ToString(), FSimpleDelegate::CreateLambda([WeakThis, Request]()
	{
		const auto Complete = [WeakThis, Request]()
		{ if (USovAccessibleNarrationSubsystem* Self = WeakThis.Get()) { Self->Finish(Request, true); } };
		if (IsInGameThread()) { Complete(); }
		else { AsyncTask(ENamedThreads::GameThread, Complete); }
	}));
	if (!bAccepted)
	{
		Finish(Request, false);
		// A synchronous backend/cancellation callback may have replaced this reference's contents.
		if (OutRequest == Request) { OutRequest.Invalidate(); }
	}
	return bAccepted && Generation == Expected && !bShuttingDown;
}

void USovAccessibleNarrationSubsystem::Retire(bool bNotify)
{
	TSharedPtr<ISovAccessibleSpeech> Retired = MoveTemp(Backend);
	const FGuid Request = ActiveRequest;
	const TWeakObjectPtr<UObject> Owner = RequestOwner;
	FSovNarrationCompletion Callback = MoveTemp(ActiveCompletion);
	ActiveRequest.Invalidate();
	RequestOwner.Reset();
	ActiveCompletion.Unbind();
	if (Retired) { Retired->Stop(); }
	if (bNotify && Owner.IsValid() && Request.IsValid()) { Callback.ExecuteIfBound(Request, false); }
}

void USovAccessibleNarrationSubsystem::Finish(FGuid Request, bool bCompleted)
{
	check(IsInGameThread());
	if (!Request.IsValid() || Request != ActiveRequest) { return; }
	const TWeakObjectPtr<UObject> Owner = RequestOwner;
	FSovNarrationCompletion Callback = MoveTemp(ActiveCompletion);
	// Keep the backend alive until this stack (which may be its callback) has unwound.
	TSharedPtr<ISovAccessibleSpeech> FinishedBackend = Backend;
	Retire(false);
	// Some platform dispatch stacks do not hold their object across delegate invocation.
	// Release it after this completion callback has returned to the backend.
	AsyncTask(ENamedThreads::GameThread, [FinishedBackend]() { (void)FinishedBackend; });
	if (Owner.IsValid()) { Callback.ExecuteIfBound(Request, bCompleted); }
}

void USovAccessibleNarrationSubsystem::Cancel(UObject* Owner)
{
	if (RequestOwner.Get() != Owner) { return; }
	++Generation;
	Retire(true);
}

void USovAccessibleNarrationSubsystem::Deinitialize()
{
	bShuttingDown = true;
	++Generation;
	Retire(true);
	Super::Deinitialize();
}
#undef LOCTEXT_NAMESPACE
