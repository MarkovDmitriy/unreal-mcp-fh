#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

struct FLiveCodingState
{
	bool bCompiling = false;
	bool bHasResult = false;
	/** Seconds since the last patch-complete event fired (FPlatformTime::Seconds() basis). */
	double LastFinishTimeSeconds = 0.0;
};

class FUnrealMCPModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static inline FUnrealMCPModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FUnrealMCPModule>("UnrealMCP");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("UnrealMCP");
	}

	/** Read current Live Coding status. Safe to call from any thread. */
	static FLiveCodingState GetLiveCodingState();

private:
	FDelegateHandle PatchCompleteHandle;
};
