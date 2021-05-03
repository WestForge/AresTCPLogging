// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Analytics.h"
#include "AnalyticsEventAttribute.h"
#include "CoreMinimal.h"
#include "Interfaces/IAnalyticsProvider.h"

class Error;

class FAnalyticsProviderTCPLogging : public IAnalyticsProvider
{
public:
	/** Tracks whether we need to start the session or restart it */
	bool bHasSessionStarted;
	/** Id representing the user the analytics are recording for */
	FString UserId;
	/** Unique Id representing the session the analytics are recording for */
	FString SessionId;

	FString Host;
	int32 Port;

	static TSharedPtr<IAnalyticsProvider> Provider;

protected:

	// TSharedRef<FInternetAddr> Addr;
	FSocket* Socket;

public:
	FAnalyticsProviderTCPLogging(const FString HostName, int32 Port);
	virtual ~FAnalyticsProviderTCPLogging();

	static TSharedPtr<IAnalyticsProvider> Create(const FString HostName, int32 Port)
	{
		if (!Provider.IsValid())
		{
			Provider = TSharedPtr<IAnalyticsProvider>(new FAnalyticsProviderTCPLogging(HostName, Port));
		}
		return Provider;
	}

	static void Destroy()
	{
		Provider.Reset();
	}

	virtual bool StartSession(const TArray<FAnalyticsEventAttribute>& Attributes) override;
	virtual void EndSession() override;
	virtual void FlushEvents() override;

	virtual void SetUserID(const FString& InUserID) override;
	virtual FString GetUserID() const override;

	virtual FString GetSessionID() const override;
	virtual bool SetSessionID(const FString& InSessionID) override;

	virtual void RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes) override;

	virtual void RecordItemPurchase(const FString& ItemId, const FString& Currency, int PerItemCost, int ItemQuantity) override;

	virtual void RecordCurrencyPurchase(const FString& GameCurrencyType, int GameCurrencyAmount, const FString& RealCurrencyType,
		float RealMoneyCost, const FString& PaymentProvider) override;

	virtual void RecordCurrencyGiven(const FString& GameCurrencyType, int GameCurrencyAmount) override;

	virtual void RecordItemPurchase(
		const FString& ItemId, int ItemQuantity, const TArray<FAnalyticsEventAttribute>& EventAttrs) override;
	virtual void RecordCurrencyPurchase(
		const FString& GameCurrencyType, int GameCurrencyAmount, const TArray<FAnalyticsEventAttribute>& EventAttrs) override;
	virtual void RecordCurrencyGiven(
		const FString& GameCurrencyType, int GameCurrencyAmount, const TArray<FAnalyticsEventAttribute>& EventAttrs) override;
	virtual void RecordError(const FString& Error, const TArray<FAnalyticsEventAttribute>& EventAttrs) override;
	virtual void RecordProgress(
		const FString& ProgressType, const FString& ProgressHierarchy, const TArray<FAnalyticsEventAttribute>& EventAttrs) override;

protected:
	void SendJSON(FString& serialized);
};
