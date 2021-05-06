// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnalyticsSettings.h"
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "TCPLoggingSettings.generated.h"

UCLASS()
class UTCPLoggingSettings : public UAnalyticsSettingsBase
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = "TCPLogging|Release", meta = (ConfigRestartRequired = true))
	FString ReleaseHostName;

	UPROPERTY(EditAnywhere, Category = "TCPLogging|Release", meta = (ConfigRestartRequired = true))
	FString ReleasePort;

	UPROPERTY(EditAnywhere, Category = "TCPLogging|Debug", meta = (ConfigRestartRequired = true))
	FString DebugHostName;

	UPROPERTY(EditAnywhere, Category = "TCPLogging|Debug", meta = (ConfigRestartRequired = true))
	FString DebugPort;

	UPROPERTY(EditAnywhere, Category = "TCPLogging|Test", meta = (ConfigRestartRequired = true))
	FString TestHostName;

	UPROPERTY(EditAnywhere, Category = "TCPLogging|Test", meta = (ConfigRestartRequired = true))
	FString TestPort;

	UPROPERTY(EditAnywhere, Category = "TCPLogging|Development", meta = (ConfigRestartRequired = true))
	FString DevelopmentHostName;

	UPROPERTY(EditAnywhere, Category = "TCPLogging|Development", meta = (ConfigRestartRequired = true))
	FString DevelopmentPort;

	UPROPERTY(EditAnywhere, Category = "TCPLogging|Release", meta = (ConfigRestartRequired = true))
	bool bReleaseGenerateSessionGuid;

	UPROPERTY(EditAnywhere, Category = "TCPLogging|Debug", meta = (ConfigRestartRequired = true))
	bool bDebugGenerateSessionGuid;

	UPROPERTY(EditAnywhere, Category = "TCPLogging|Test", meta = (ConfigRestartRequired = true))
	bool bTestGenerateSessionGuid;

	UPROPERTY(EditAnywhere, Category = "TCPLogging|Development", meta = (ConfigRestartRequired = true))
	bool bDevelopmentGenerateSessionGuid;

	UPROPERTY(EditAnywhere, Category = "TCPLogging|Release", meta = (ConfigRestartRequired = true))
	bool bReleaseTimeStampEvents;

	UPROPERTY(EditAnywhere, Category = "TCPLogging|Debug", meta = (ConfigRestartRequired = true))
	bool bDebugTimeStampEvents;

	UPROPERTY(EditAnywhere, Category = "TCPLogging|Development", meta = (ConfigRestartRequired = true))
	bool bDevelopmentTimeStampEvents;

	UPROPERTY(EditAnywhere, Category = "TCPLogging|Test", meta = (ConfigRestartRequired = true))
	bool bTestTimeStampEvents;

	// UAnalyticsSettingsBase interface
protected:
	/**
	 * Provides a mechanism to read the section based information into this UObject's properties
	 */
	virtual void ReadConfigSettings();
	/**
	 * Provides a mechanism to save this object's properties to the section based ini values
	 */
	virtual void WriteConfigSettings();
};
