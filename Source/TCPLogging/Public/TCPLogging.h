// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IAnalyticsProviderModule.h"
#include "Modules/ModuleManager.h"

class IAnalyticsProvider;

/**
 * The public interface to this module
 */
class FAnalyticsTCPLogging : public IAnalyticsProviderModule
{
	/** Singleton for analytics */
	TSharedPtr<IAnalyticsProvider> TCPLoggingProvider;

	//--------------------------------------------------------------------------
	// Module functionality
	//--------------------------------------------------------------------------
public:
	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline FAnalyticsTCPLogging& Get()
	{
		return FModuleManager::LoadModuleChecked<FAnalyticsTCPLogging>("TCPLogging");
	}

public:
	/**
	 * Defines required configuration values for multicast analytics provider.
	 * Basically, you provide a list of provider modules that define the providers you want
	 * to multicast events to. Beyond that, each provider module created will use the
	 * provided configuration delegate to configure itself, so that configuration delegate
	 * must be able to configure each specific provider as well (see CreateAnalyticsProvider function below).
	 */
	struct Config
	{
		/** Host name of remote TCP service */
		FString HostName;

		static FString GetKeyNameForHostName()
		{
			return TEXT("HostName");
		}

		/** Port number for */
		int32 Port;

		int32 GetKeyNameForPort()
		{
			return Port;
		}

		Config()
		{
			Port = 0;
		}
	};

	//--------------------------------------------------------------------------
	// provider factory functions
	//--------------------------------------------------------------------------
public:
	/**
	 * IAnalyticsProviderModule interface.
	 * Creates the analytics provider given a configuration delegate.
	 * The keys required exactly match the field names in the Config object.
	 *
	 * When a particular provider module is loaded, it will create an instance and use the
	 * provided Configuration delegate to configure each provider.
	 */
	virtual TSharedPtr<IAnalyticsProvider> CreateAnalyticsProvider(
		const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const;

	/**
	 * Construct an analytics provider directly from a config object (and a delegate to provide configuration to each configured
	 * provider).
	 */
	virtual TSharedPtr<IAnalyticsProvider> CreateAnalyticsProvider(
		const Config& ConfigValues, const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const;

private:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
