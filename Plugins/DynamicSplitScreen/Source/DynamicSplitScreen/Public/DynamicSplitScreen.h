// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

DYNAMICSPLITSCREEN_API DECLARE_LOG_CATEGORY_EXTERN(LogDynamicSplitScreen, Log, All);

class FDynamicSplitScreenModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
