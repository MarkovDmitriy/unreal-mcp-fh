#include "UnrealMCPModule.h"
#include "UnrealMCPBridge.h"
#include "Modules/ModuleManager.h"
#include "EditorSubsystem.h"
#include "Editor.h"
#include "ILiveCodingModule.h"
#include "HAL/PlatformTime.h"

#define LOCTEXT_NAMESPACE "FUnrealMCPModule"

namespace
{
	// File-scope state mirror that the patch-complete delegate writes to.
	// `bCompiling` is queried live from ILiveCodingModule::IsCompiling() — we
	// only cache the "last finish" timestamp here.
	static bool        GLastResultKnown = false;
	static double      GLastFinishSeconds = 0.0;
}

void FUnrealMCPModule::StartupModule()
{
	UE_LOG(LogTemp, Display, TEXT("Unreal MCP Module has started"));

	if (ILiveCodingModule* LC = FModuleManager::LoadModulePtr<ILiveCodingModule>("LiveCoding"))
	{
		PatchCompleteHandle = LC->GetOnPatchCompleteDelegate().AddLambda([]()
		{
			GLastResultKnown = true;
			GLastFinishSeconds = FPlatformTime::Seconds();
			UE_LOG(LogTemp, Display, TEXT("UnrealMCP: Live Coding patch complete at %.2fs"), GLastFinishSeconds);
		});
	}
}

void FUnrealMCPModule::ShutdownModule()
{
	if (ILiveCodingModule* LC = FModuleManager::GetModulePtr<ILiveCodingModule>("LiveCoding"))
	{
		LC->GetOnPatchCompleteDelegate().Remove(PatchCompleteHandle);
	}
	PatchCompleteHandle.Reset();
	UE_LOG(LogTemp, Display, TEXT("Unreal MCP Module has shut down"));
}

FLiveCodingState FUnrealMCPModule::GetLiveCodingState()
{
	FLiveCodingState Out;
	if (ILiveCodingModule* LC = FModuleManager::GetModulePtr<ILiveCodingModule>("LiveCoding"))
	{
		Out.bCompiling = LC->IsCompiling();
	}
	Out.bHasResult = GLastResultKnown;
	Out.LastFinishTimeSeconds = GLastFinishSeconds;
	return Out;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealMCPModule, UnrealMCP)
