// Copyright Epic Games, Inc. All Rights Reserved.

#include "TCPLogging.h"

#include "AnalyticsEventAttribute.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Misc/Paths.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "TCPLoggingProvider.h"

DEFINE_LOG_CATEGORY_STATIC(LogTCPLoggingAnalytics, Display, All);

IMPLEMENT_MODULE(FAnalyticsTCPLogging, TCPLogging)

void FAnalyticsTCPLogging::StartupModule()
{
	TCPLoggingProvider = MakeShareable(new FAnalyticsProviderTCPLogging());

	TSharedPtr<FAnalyticsProviderTCPLogging> Logging = StaticCastSharedPtr<FAnalyticsProviderTCPLogging>(TCPLoggingProvider);

}

void FAnalyticsTCPLogging::ShutdownModule()
{
	if (TCPLoggingProvider.IsValid())
	{
		TCPLoggingProvider->EndSession();
	}
}

TSharedPtr<IAnalyticsProvider> FAnalyticsTCPLogging::CreateAnalyticsProvider(
	const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const
{
	if (GetConfigValue.IsBound())
	{
		Config ConfigValues;
		ConfigValues.HostName = GetConfigValue.Execute(Config::GetKeyNameForHostName(), true);
		if (ConfigValues.HostName.IsEmpty())
		{
			UE_LOG(LogTCPLoggingAnalytics, Warning, TEXT("CreateAnalyticsProvider delegate did not contain required parameter %s"),
				*Config::GetKeyNameForHostName());
			return NULL;
		}
		ConfigValues.Port = GetConfigValue.Execute(Config::GetKeyNameForPort(), true);
		if (ConfigValues.Port == 0)
		{
			UE_LOG(LogTCPLoggingAnalytics, Warning, TEXT("CreateAnalyticsProvider delegate did not contain required parameter %s"),
				*Config::GetKeyNameForHostName());
			return NULL;
		}
		return CreateAnalyticsProvider(ConfigValues, GetConfigValue);
	}
	else
	{
		UE_LOG(LogTCPLoggingAnalytics, Warning, TEXT("CreateAnalyticsProvider called with an unbound delegate"));
	}
	return NULL;

	//return TCPLoggingProvider;
}


TSharedPtr<IAnalyticsProvider> FAnalyticsTCPLogging::CreateAnalyticsProvider(
	const Config& ConfigValues, const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const
{
}

// Provider

FAnalyticsProviderTCPLogging::FAnalyticsProviderTCPLogging()
	: bHasSessionStarted(false), bHasWrittenFirstEvent(false), Age(0), FileArchive(nullptr)
{
	UserId = FPlatformMisc::GetLoginId();
}

FAnalyticsProviderTCPLogging::~FAnalyticsProviderTCPLogging()
{
	if (bHasSessionStarted)
	{
		EndSession();
	}
}

bool FAnalyticsProviderTCPLogging::StartSession(const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (bHasSessionStarted)
	{
		EndSession();
	}

	Socket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("default"), false);

	ISocketSubsystem* SocketSubSystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	TSharedRef<FInternetAddr> Addr = SocketSubSystem->CreateInternetAddr(0, 0);

	char* Host = TCHAR_TO_ANSI(*HostName);
	ESocketErrors SocketError = SocketSubSystem->GetHostByName(Host, *Addr);
	Addr->SetPort(Port);

	connected = Socket->Connect(*Addr);
	SessionId = UserId + TEXT("-") + FDateTime::Now().ToString();

	if (Socket != nullptr)
	{
		FString payload = FString::Printf(TEXT("{"));
		FString::Printf(TEXT("\t\"sessionId\" : \"%s\","), *SessionId);
		FString::Printf(TEXT("\t\"events\" : ["));
		bHasSessionStarted = true;
		UE_LOG(LogTCPLoggingAnalytics, Display, TEXT("Session created file (%s) for user (%s)"), *FileName, *UserId);
	}
	else
	{
		UE_LOG(LogTCPLoggingAnalytics, Warning,
			TEXT("FAnalyticsProviderTCPLogging::StartSession failed to create file to log analytics events to"));
	}
	return bHasSessionStarted;
}

void FAnalyticsProviderTCPLogging::EndSession()
{
	if (Socket != nullptr)
	{
		Socket->Close();
		UE_LOG(LogTCPLoggingAnalytics, Display, TEXT("Session ended for user (%s) and session id (%s)"), *UserId, *SessionId);
	}

	bHasSessionStarted = false;
}

void FAnalyticsProviderTCPLogging::FlushEvents()
{
	if (Socket != nullptr)
	{
		UE_LOG(LogTCPLoggingAnalytics, Display, TEXT("Analytics socket flushed"));
	}
}

void FAnalyticsProviderTCPLogging::SetUserID(const FString& InUserID)
{
	if (!bHasSessionStarted)
	{
		UserId = InUserID;
		UE_LOG(LogTCPLoggingAnalytics, Display, TEXT("User is now (%s)"), *UserId);
	}
	else
	{
		// Log that we shouldn't switch users during a session
		UE_LOG(LogTCPLoggingAnalytics, Warning,
			TEXT("FAnalyticsProviderTCPLogging::SetUserID called while a session is in progress. Ignoring."));
	}
}

FString FAnalyticsProviderTCPLogging::GetUserID() const
{
	return UserId;
}

FString FAnalyticsProviderTCPLogging::GetSessionID() const
{
	return SessionId;
}

bool FAnalyticsProviderTCPLogging::SetSessionID(const FString& InSessionID)
{
6	if (!bHasSessionStarted)
	{
		SessionId = InSessionID;
		UE_LOG(LogTCPLoggingAnalytics, Display, TEXT("Session is now (%s)"), *SessionId);
	}
	else
	{
		// Log that we shouldn't switch session ids during a session
		UE_LOG(LogTCPLoggingAnalytics, Warning,
			TEXT("FAnalyticsProviderTCPLogging::SetSessionID called while a session is in progress. Ignoring."));
	}
	return !bHasSessionStarted;
}

void FAnalyticsProviderTCPLogging::RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (bHasSessionStarted)
	{
		check(Socket != nullptr);


		FString::Printf(TEXT("\t\t{"));
		FString::Printf(TEXT("\t\t\t\"eventName\" : \"%s\""), *EventName);
		if (Attributes.Num() > 0)
		{
			FString::Printf(TEXT(",\t\t\t\"attributes\" : ["));
			bool bHasWrittenFirstAttr = false;
			// Write out the list of attributes as an array of attribute objects
			for (auto Attr : Attributes)
			{
				if (bHasWrittenFirstAttr)
				{
					FString::Printf(TEXT("\t\t\t,"));
				}
				FString::Printf(TEXT("\t\t\t{"));
				FString::Printf(TEXT("\t\t\t\t\"name\" : \"%s\","), *Attr.GetName());
				FString::Printf(TEXT("\t\t\t\t\"value\" : \"%s\""), *Attr.GetValue());
				FString::Printf(TEXT("\t\t\t}"));
				bHasWrittenFirstAttr = true;
			}
			FString::Printf(TEXT("\t\t\t]"));
		}
		FString::Printf(TEXT("\t\t}"));

		UE_LOG(LogTCPLoggingAnalytics, Display, TEXT("Analytics event (%s) written with (%d) attributes"), *EventName,
			Attributes.Num());

		FString serialized = "Test|Test|Test|Test|Test";
		SendJSON(serialized);

	}
	else
	{
		UE_LOG(LogTCPLoggingAnalytics, Warning,
			TEXT("FAnalyticsProviderTCPLogging::RecordEvent called before StartSession. Ignoring."));
	}
}

void FAnalyticsProviderTCPLogging::RecordItemPurchase(
	const FString& ItemId, const FString& Currency, int PerItemCost, int ItemQuantity)
{
	if (bHasSessionStarted)
	{
		check(Socket != nullptr);

		FString::Printf(TEXT("\t\t{"));
		FString::Printf(TEXT("\t\t\t\"eventName\" : \"recordItemPurchase\","));

		FString::Printf(TEXT("\t\t\t\"attributes\" :"));
		FString::Printf(TEXT("\t\t\t["));

		FString::Printf(TEXT("\t\t\t\t{ \"name\" : \"itemId\", \t\"value\" : \"%s\" },"), *ItemId);
		FString::Printf(TEXT("\t\t\t\t{ \"name\" : \"currency\", \t\"value\" : \"%s\" },"), *Currency);
		FString::Printf(TEXT("\t\t\t\t{ \"name\" : \"perItemCost\", \t\"value\" : \"%d\" },"), PerItemCost);
		FString::Printf(TEXT("\t\t\t\t{ \"name\" : \"itemQuantity\", \t\"value\" : \"%d\" }"), ItemQuantity);

		FString::Printf(TEXT("\t\t\t]"));

		FString::Printf(TEXT("\t\t}"));

		UE_LOG(LogTCPLoggingAnalytics, Display, TEXT("(%d) number of item (%s) purchased with (%s) at a cost of (%d) each"),
			ItemQuantity, *ItemId, *Currency, PerItemCost);
	}
	else
	{
		UE_LOG(LogTCPLoggingAnalytics, Warning,
			TEXT("FAnalyticsProviderTCPLogging::RecordItemPurchase called before StartSession. Ignoring."));
	}
}

void FAnalyticsProviderTCPLogging::RecordCurrencyPurchase(const FString& GameCurrencyType, int GameCurrencyAmount,
	const FString& RealCurrencyType, float RealMoneyCost, const FString& PaymentProvider)
{
	if (bHasSessionStarted)
	{
		check(Socket != nullptr);

		FString::Printf(TEXT("\t\t{"));
		FString::Printf(TEXT("\t\t\t\"eventName\" : \"recordCurrencyPurchase\","));

		FString::Printf(TEXT("\t\t\t\"attributes\" :"));
		FString::Printf(TEXT("\t\t\t["));

		FString::Printf(TEXT("\t\t\t\t{ \"name\" : \"gameCurrencyType\", \t\"value\" : \"%s\" },"), *GameCurrencyType);
		FString::Printf(TEXT("\t\t\t\t{ \"name\" : \"gameCurrencyAmount\", \t\"value\" : \"%d\" },"), GameCurrencyAmount);
		FString::Printf(TEXT("\t\t\t\t{ \"name\" : \"realCurrencyType\", \t\"value\" : \"%s\" },"), *RealCurrencyType);
		FString::Printf(TEXT("\t\t\t\t{ \"name\" : \"realMoneyCost\", \t\"value\" : \"%f\" },"), RealMoneyCost);
		FString::Printf(TEXT("\t\t\t\t{ \"name\" : \"paymentProvider\", \t\"value\" : \"%s\" }"), *PaymentProvider);

		FString::Printf(TEXT("\t\t\t]"));

		FString::Printf(TEXT("\t\t}"));

		UE_LOG(LogTCPLoggingAnalytics, Display,
			TEXT("(%d) amount of in game currency (%s) purchased with (%s) at a cost of (%f) each"), GameCurrencyAmount,
			*GameCurrencyType, *RealCurrencyType, RealMoneyCost);
	}
	else
	{
		UE_LOG(LogTCPLoggingAnalytics, Warning,
			TEXT("FAnalyticsProviderTCPLogging::RecordCurrencyPurchase called before StartSession. Ignoring."));
	}
}

void FAnalyticsProviderTCPLogging::RecordCurrencyGiven(const FString& GameCurrencyType, int GameCurrencyAmount)
{
	if (bHasSessionStarted)
	{
		check(Socket != nullptr);


		FString::Printf(TEXT("\t\t{"));
		FString::Printf(TEXT("\t\t\t\"eventName\" : \"recordCurrencyGiven\","));

		FString::Printf(TEXT("\t\t\t\"attributes\" :"));
		FString::Printf(TEXT("\t\t\t["));

		FString::Printf(TEXT("\t\t\t\t{ \"name\" : \"gameCurrencyType\", \t\"value\" : \"%s\" },"), *GameCurrencyType);
		FString::Printf(TEXT("\t\t\t\t{ \"name\" : \"gameCurrencyAmount\", \t\"value\" : \"%d\" }"), GameCurrencyAmount);

		FString::Printf(TEXT("\t\t\t]"));

		FString::Printf(TEXT("\t\t}"));

		UE_LOG(LogTCPLoggingAnalytics, Display, TEXT("(%d) amount of in game currency (%s) given to user"), GameCurrencyAmount,
			*GameCurrencyType);
	}
	else
	{
		UE_LOG(LogTCPLoggingAnalytics, Warning,
			TEXT("FAnalyticsProviderTCPLogging::RecordCurrencyGiven called before StartSession. Ignoring."));
	}
}

void FAnalyticsProviderTCPLogging::RecordError(const FString& Error, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (bHasSessionStarted)
	{
		check(Socket != nullptr);

		FString::Printf(TEXT("\t\t{"));
		FString::Printf(TEXT("\t\t\t\"error\" : \"%s\","), *Error);

		FString::Printf(TEXT("\t\t\t\"attributes\" :"));
		FString::Printf(TEXT("\t\t\t["));
		bool bHasWrittenFirstAttr = false;
		// Write out the list of attributes as an array of attribute objects
		for (auto Attr : Attributes)
		{
			FString::Printf(TEXT("\t\t\t{"));
			FString::Printf(TEXT("\t\t\t\t\"name\" : \"%s\","), *Attr.GetName());
			FString::Printf(TEXT("\t\t\t\t\"value\" : \"%s\""), *Attr.GetValue());
			FString::Printf(TEXT("\t\t\t}"));
			bHasWrittenFirstAttr = true;
		}
		FString::Printf(TEXT("\t\t\t]"));

		FString::Printf(TEXT("\t\t}"));

		UE_LOG(LogTCPLoggingAnalytics, Display, TEXT("Error is (%s) number of attributes is (%d)"), *Error, Attributes.Num());
	}
	else
	{
		UE_LOG(LogTCPLoggingAnalytics, Warning,
			TEXT("FAnalyticsProviderTCPLogging::RecordError called before StartSession. Ignoring."));
	}
}

void FAnalyticsProviderTCPLogging::RecordProgress(
	const FString& ProgressType, const FString& ProgressName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (bHasSessionStarted)
	{
		check(Socket != nullptr);

		FString::Printf(TEXT("\t\t{"));
		FString::Printf(TEXT("\t\t\t\"eventType\" : \"Progress\","));
		FString::Printf(TEXT("\t\t\t\"progressType\" : \"%s\","), *ProgressType);
		FString::Printf(TEXT("\t\t\t\"progressName\" : \"%s\","), *ProgressName);

		FString::Printf(TEXT("\t\t\t\"attributes\" :"));
		FString::Printf(TEXT("\t\t\t["));
		bool bHasWrittenFirstAttr = false;
		// Write out the list of attributes as an array of attribute objects
		for (auto Attr : Attributes)
		{
			if (bHasWrittenFirstAttr)
			{
				FString::Printf(TEXT("\t\t\t,"));
			}
			FString::Printf(TEXT("\t\t\t{"));
			FString::Printf(TEXT("\t\t\t\t\"name\" : \"%s\","), *Attr.GetName());
			FString::Printf(TEXT("\t\t\t\t\"value\" : \"%s\""), *Attr.GetValue());
			FString::Printf(TEXT("\t\t\t}"));
			bHasWrittenFirstAttr = true;
		}
		FString::Printf(TEXT("\t\t\t]"));

		FString::Printf(TEXT("\t\t}"));

		UE_LOG(LogTCPLoggingAnalytics, Display, TEXT("Progress event is type (%s), named (%s), number of attributes is (%d)"),
			*ProgressType, *ProgressName, Attributes.Num());
	}
	else
	{
		UE_LOG(LogTCPLoggingAnalytics, Warning,
			TEXT("FAnalyticsProviderTCPLogging::RecordProgress called before StartSession. Ignoring."));
	}
}

void FAnalyticsProviderTCPLogging::RecordItemPurchase(
	const FString& ItemId, int ItemQuantity, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (bHasSessionStarted)
	{
		check(Socket != nullptr);

		FString::Printf(TEXT("\t\t{"));
		FString::Printf(TEXT("\t\t\t\"eventType\" : \"ItemPurchase\","));
		FString::Printf(TEXT("\t\t\t\"itemId\" : \"%s\","), *ItemId);
		FString::Printf(TEXT("\t\t\t\"itemQuantity\" : %d,"), ItemQuantity);

		FString::Printf(TEXT("\t\t\t\"attributes\" :"));
		FString::Printf(TEXT("\t\t\t["));
		bool bHasWrittenFirstAttr = false;
		// Write out the list of attributes as an array of attribute objects
		for (auto Attr : Attributes)
		{
			if (bHasWrittenFirstAttr)
			{
				FString::Printf(TEXT("\t\t\t,"));
			}
			FString::Printf(TEXT("\t\t\t{"));
			FString::Printf(TEXT("\t\t\t\t\"name\" : \"%s\","), *Attr.GetName());
			FString::Printf(TEXT("\t\t\t\t\"value\" : \"%s\""), *Attr.GetValue());
			FString::Printf(TEXT("\t\t\t}"));
			bHasWrittenFirstAttr = true;
		}
		FString::Printf(TEXT("\t\t\t]"));

		FString::Printf(TEXT("\t\t}"));

		UE_LOG(LogTCPLoggingAnalytics, Display, TEXT("Item purchase id (%s), quantity (%d), number of attributes is (%d)"), *ItemId,
			ItemQuantity, Attributes.Num());
	}
	else
	{
		UE_LOG(LogTCPLoggingAnalytics, Warning,
			TEXT("FAnalyticsProviderTCPLogging::RecordItemPurchase called before StartSession. Ignoring."));
	}
}

void FAnalyticsProviderTCPLogging::RecordCurrencyPurchase(
	const FString& GameCurrencyType, int GameCurrencyAmount, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (bHasSessionStarted)
	{
		check(Socket != nullptr);


		FString::Printf(TEXT("\t\t{"));
		FString::Printf(TEXT("\t\t\t\"eventType\" : \"CurrencyPurchase\","));
		FString::Printf(TEXT("\t\t\t\"gameCurrencyType\" : \"%s\","), *GameCurrencyType);
		FString::Printf(TEXT("\t\t\t\"gameCurrencyAmount\" : %d,"), GameCurrencyAmount);

		FString::Printf(TEXT("\t\t\t\"attributes\" :"));
		FString::Printf(TEXT("\t\t\t["));
		bool bHasWrittenFirstAttr = false;
		// Write out the list of attributes as an array of attribute objects
		for (auto Attr : Attributes)
		{
			if (bHasWrittenFirstAttr)
			{
				FString::Printf(TEXT("\t\t\t,"));
			}
			FString::Printf(TEXT("\t\t\t{"));
			FString::Printf(TEXT("\t\t\t\t\"name\" : \"%s\","), *Attr.GetName());
			FString::Printf(TEXT("\t\t\t\t\"value\" : \"%s\""), *Attr.GetValue());
			FString::Printf(TEXT("\t\t\t}"));
			bHasWrittenFirstAttr = true;
		}
		FString::Printf(TEXT("\t\t\t]"));

		FString::Printf(TEXT("\t\t}"));

		UE_LOG(LogTCPLoggingAnalytics, Display, TEXT("Currency purchase type (%s), quantity (%d), number of attributes is (%d)"),
			*GameCurrencyType, GameCurrencyAmount, Attributes.Num());
	}
	else
	{
		UE_LOG(LogTCPLoggingAnalytics, Warning,
			TEXT("FAnalyticsProviderTCPLogging::RecordCurrencyPurchase called before StartSession. Ignoring."));
	}
}

void FAnalyticsProviderTCPLogging::RecordCurrencyGiven(
	const FString& GameCurrencyType, int GameCurrencyAmount, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (bHasSessionStarted)
	{
		check(Socket != nullptr);

		FString::Printf(TEXT("\t\t{"));
		FString::Printf(TEXT("\t\t\t\"eventType\" : \"CurrencyGiven\","));
		FString::Printf(TEXT("\t\t\t\"gameCurrencyType\" : \"%s\","), *GameCurrencyType);
		FString::Printf(TEXT("\t\t\t\"gameCurrencyAmount\" : %d,"), GameCurrencyAmount);

		FString::Printf(TEXT("\t\t\t\"attributes\" :"));
		FString::Printf(TEXT("\t\t\t["));
		bool bHasWrittenFirstAttr = false;
		// Write out the list of attributes as an array of attribute objects
		for (auto Attr : Attributes)
		{
			if (bHasWrittenFirstAttr)
			{
				FString::Printf(TEXT("\t\t\t,"));
			}
			FString::Printf(TEXT("\t\t\t{"));
			FString::Printf(TEXT("\t\t\t\t\"name\" : \"%s\","), *Attr.GetName());
			FString::Printf(TEXT("\t\t\t\t\"value\" : \"%s\""), *Attr.GetValue());
			FString::Printf(TEXT("\t\t\t}"));
			bHasWrittenFirstAttr = true;
		}
		FString::Printf(TEXT("\t\t\t]"));

		FString::Printf(TEXT("\t\t}"));

		UE_LOG(LogTCPLoggingAnalytics, Display, TEXT("Currency given type (%s), quantity (%d), number of attributes is (%d)"),
			*GameCurrencyType, GameCurrencyAmount, Attributes.Num());
	}
	else
	{
		UE_LOG(LogTCPLoggingAnalytics, Warning,
			TEXT("FAnalyticsProviderTCPLogging::RecordCurrencyGiven called before StartSession. Ignoring."));
	}
}

void FAnalyticsProviderTCPLogging::SendJSON(FString& serialized)
{
	TCHAR* serializedChar = serialized.GetCharArray().GetData();
	int32 size = FCString::Strlen(serializedChar);
	int32 sent = 0;

	Socket->Send((uint8*) TCHAR_TO_UTF8(serializedChar), size, sent);
}