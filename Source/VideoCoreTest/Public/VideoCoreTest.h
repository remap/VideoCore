//
// VideoCoreTest.h
//
//  Generated on June 24 2020
//  Template created by Peter Gusev on 27 January 2020.
//  Copyright 2013-2019 Regents of the University of California
//

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "DDBase.h"

class FVideoCoreTestModule : public FDDBaseModule
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
