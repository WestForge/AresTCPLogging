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

	Logging->HostName = this->Config.HostName;
	Logging->Port = this->Config.Port;
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
	return TCPLoggingProvider;
}

// Provider

FAnalyticsProviderTCPLogging::FAnalyticsProviderTCPLogging()
	: bHasSessionStarted(false), bHasWrittenFirstEvent(false), Age(0), FileArchive(nullptr)
{
	FileArchive = nullptr;
	AnalyticsFilePath = FPaths::ProjectSavedDir() + TEXT("Analytics/");
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
	ESocketErrors SocketError = SocketSubSystem->GetHostByName("irc.twitch.tv", *Addr);

	FIPv4Address ip(127, 0, 0, 1);
	TSharedRef<FInternetAddr> addr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	addr->SetIp(ip.Value);
	addr->SetPort(Port);

	connected = Socket->Connect(*addr);

	SessionId = UserId + TEXT("-") + FDateTime::Now().ToString();
	const FString FileName = AnalyticsFilePath + SessionId + TEXT(".analytics");
	// Close the old file and open a new one
	FileArchive = IFileManager::Get().CreateFileWriter(*FileName);
	if (FileArchive != nullptr)
	{
		FileArchive->Logf(TEXT("{"));
		FileArchive->Logf(TEXT("\t\"sessionId\" : \"%s\","), *SessionId);
		FileArchive->Logf(TEXT("\t\"userId\" : \"%s\","), *UserId);
		if (BuildInfo.Len() > 0)
		{
			FileArchive->Logf(TEXT("\t\"buildInfo\" : \"%s\","), *BuildInfo);
		}
		if (Age != 0)
		{
			FileArchive->Logf(TEXT("\t\"age\" : %d,"), Age);
		}
		if (Gender.Len() > 0)
		{
			FileArchive->Logf(TEXT("\t\"gender\" : \"%s\","), *Gender);
		}
		if (Location.Len() > 0)
		{
			FileArchive->Logf(TEXT("\t\"location\" : \"%s\","), *Location);
		}
		FileArchive->Logf(TEXT("\t\"events\" : ["));
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
	}
	if (FileArchive != nullptr)
	{
		FileArchive->Logf(TEXT("\t]"));
		FileArchive->Logf(TEXT("}"));
		FileArchive->Flush();
		FileArchive->Close();
		delete FileArchive;
		FileArchive = nullptr;
		UE_LOG(LogTCPLoggingAnalytics, Display, TEXT("Session ended for user (%s) and session id (%s)"), *UserId, *SessionId);
	}
	bHasWrittenFirstEvent = false;
	bHasSessionStarted = false;
}

void FAnalyticsProviderTCPLogging::FlushEvents()
{
	if (FileArchive != nullptr)
	{
		FileArchive->Flush();
		UE_LOG(LogTCPLoggingAnalytics, Display, TEXT("Analytics file flushed"));
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
	if (!bHasSessionStarted)
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
		check(FileArchive != nullptr);

		if (bHasWrittenFirstEvent)
		{
			FileArchive->Logf(TEXT(","));
		}
		bHasWrittenFirstEvent = true;

		FileArchive->Logf(TEXT("\t\t{"));
		FileArchive->Logf(TEXT("\t\t\t\"eventName\" : \"%s\""), *EventName);
		if (Attributes.Num() > 0)
		{
			FileArchive->Logf(TEXT(",\t\t\t\"attributes\" : ["));
			bool bHasWrittenFirstAttr = false;
			// Write out the list of attributes as an array of attribute objects
			for (auto Attr : Attributes)
			{
				if (bHasWrittenFirstAttr)
				{
					FileArchive->Logf(TEXT("\t\t\t,"));
				}
				FileArchive->Logf(TEXT("\t\t\t{"));
				FileArchive->Logf(TEXT("\t\t\t\t\"name\" : \"%s\","), *Attr.GetName());
				FileArchive->Logf(TEXT("\t\t\t\t\"value\" : \"%s\""), *Attr.GetValue());
				FileArchive->Logf(TEXT("\t\t\t}"));
				bHasWrittenFirstAttr = true;
			}
			FileArchive->Logf(TEXT("\t\t\t]"));
		}
		FileArchive->Logf(TEXT("\t\t}"));

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
		check(FileArchive != nullptr);

		if (bHasWrittenFirstEvent)
		{
			FileArchive->Logf(TEXT("\t\t,"));
		}
		bHasWrittenFirstEvent = true;

		FileArchive->Logf(TEXT("\t\t{"));
		FileArchive->Logf(TEXT("\t\t\t\"eventName\" : \"recordItemPurchase\","));

		FileArchive->Logf(TEXT("\t\t\t\"attributes\" :"));
		FileArchive->Logf(TEXT("\t\t\t["));

		FileArchive->Logf(TEXT("\t\t\t\t{ \"name\" : \"itemId\", \t\"value\" : \"%s\" },"), *ItemId);
		FileArchive->Logf(TEXT("\t\t\t\t{ \"name\" : \"currency\", \t\"value\" : \"%s\" },"), *Currency);
		FileArchive->Logf(TEXT("\t\t\t\t{ \"name\" : \"perItemCost\", \t\"value\" : \"%d\" },"), PerItemCost);
		FileArchive->Logf(TEXT("\t\t\t\t{ \"name\" : \"itemQuantity\", \t\"value\" : \"%d\" }"), ItemQuantity);

		FileArchive->Logf(TEXT("\t\t\t]"));

		FileArchive->Logf(TEXT("\t\t}"));

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
		check(FileArchive != nullptr);

		if (bHasWrittenFirstEvent)
		{
			FileArchive->Logf(TEXT("\t\t,"));
		}
		bHasWrittenFirstEvent = true;

		FileArchive->Logf(TEXT("\t\t{"));
		FileArchive->Logf(TEXT("\t\t\t\"eventName\" : \"recordCurrencyPurchase\","));

		FileArchive->Logf(TEXT("\t\t\t\"attributes\" :"));
		FileArchive->Logf(TEXT("\t\t\t["));

		FileArchive->Logf(TEXT("\t\t\t\t{ \"name\" : \"gameCurrencyType\", \t\"value\" : \"%s\" },"), *GameCurrencyType);
		FileArchive->Logf(TEXT("\t\t\t\t{ \"name\" : \"gameCurrencyAmount\", \t\"value\" : \"%d\" },"), GameCurrencyAmount);
		FileArchive->Logf(TEXT("\t\t\t\t{ \"name\" : \"realCurrencyType\", \t\"value\" : \"%s\" },"), *RealCurrencyType);
		FileArchive->Logf(TEXT("\t\t\t\t{ \"name\" : \"realMoneyCost\", \t\"value\" : \"%f\" },"), RealMoneyCost);
		FileArchive->Logf(TEXT("\t\t\t\t{ \"name\" : \"paymentProvider\", \t\"value\" : \"%s\" }"), *PaymentProvider);

		FileArchive->Logf(TEXT("\t\t\t]"));

		FileArchive->Logf(TEXT("\t\t}"));

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
		check(FileArchive != nullptr);

		if (bHasWrittenFirstEvent)
		{
			FileArchive->Logf(TEXT("\t\t,"));
		}
		bHasWrittenFirstEvent = true;

		FileArchive->Logf(TEXT("\t\t{"));
		FileArchive->Logf(TEXT("\t\t\t\"eventName\" : \"recordCurrencyGiven\","));

		FileArchive->Logf(TEXT("\t\t\t\"attributes\" :"));
		FileArchive->Logf(TEXT("\t\t\t["));

		FileArchive->Logf(TEXT("\t\t\t\t{ \"name\" : \"gameCurrencyType\", \t\"value\" : \"%s\" },"), *GameCurrencyType);
		FileArchive->Logf(TEXT("\t\t\t\t{ \"name\" : \"gameCurrencyAmount\", \t\"value\" : \"%d\" }"), GameCurrencyAmount);

		FileArchive->Logf(TEXT("\t\t\t]"));

		FileArchive->Logf(TEXT("\t\t}"));

		UE_LOG(LogTCPLoggingAnalytics, Display, TEXT("(%d) amount of in game currency (%s) given to user"), GameCurrencyAmount,
			*GameCurrencyType);
	}
	else
	{
		UE_LOG(LogTCPLoggingAnalytics, Warning,
			TEXT("FAnalyticsProviderTCPLogging::RecordCurrencyGiven called before StartSession. Ignoring."));
	}
}

void FAnalyticsProviderTCPLogging::SetAge(int InAge)
{
	Age = InAge;
}

void FAnalyticsProviderTCPLogging::SetLocation(const FString& InLocation)
{
	Location = InLocation;
}

void FAnalyticsProviderTCPLogging::SetGender(const FString& InGender)
{
	Gender = InGender;
}

void FAnalyticsProviderTCPLogging::SetBuildInfo(const FString& InBuildInfo)
{
	BuildInfo = InBuildInfo;
}

void FAnalyticsProviderTCPLogging::RecordError(const FString& Error, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (bHasSessionStarted)
	{
		check(FileArchive != nullptr);

		if (bHasWrittenFirstEvent)
		{
			FileArchive->Logf(TEXT("\t\t,"));
		}
		bHasWrittenFirstEvent = true;

		FileArchive->Logf(TEXT("\t\t{"));
		FileArchive->Logf(TEXT("\t\t\t\"error\" : \"%s\","), *Error);

		FileArchive->Logf(TEXT("\t\t\t\"attributes\" :"));
		FileArchive->Logf(TEXT("\t\t\t["));
		bool bHasWrittenFirstAttr = false;
		// Write out the list of attributes as an array of attribute objects
		for (auto Attr : Attributes)
		{
			if (bHasWrittenFirstAttr)
			{
				FileArchive->Logf(TEXT("\t\t\t,"));
			}
			FileArchive->Logf(TEXT("\t\t\t{"));
			FileArchive->Logf(TEXT("\t\t\t\t\"name\" : \"%s\","), *Attr.GetName());
			FileArchive->Logf(TEXT("\t\t\t\t\"value\" : \"%s\""), *Attr.GetValue());
			FileArchive->Logf(TEXT("\t\t\t}"));
			bHasWrittenFirstAttr = true;
		}
		FileArchive->Logf(TEXT("\t\t\t]"));

		FileArchive->Logf(TEXT("\t\t}"));

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
		check(FileArchive != nullptr);

		if (bHasWrittenFirstEvent)
		{
			FileArchive->Logf(TEXT("\t\t,"));
		}
		bHasWrittenFirstEvent = true;

		FileArchive->Logf(TEXT("\t\t{"));
		FileArchive->Logf(TEXT("\t\t\t\"eventType\" : \"Progress\","));
		FileArchive->Logf(TEXT("\t\t\t\"progressType\" : \"%s\","), *ProgressType);
		FileArchive->Logf(TEXT("\t\t\t\"progressName\" : \"%s\","), *ProgressName);

		FileArchive->Logf(TEXT("\t\t\t\"attributes\" :"));
		FileArchive->Logf(TEXT("\t\t\t["));
		bool bHasWrittenFirstAttr = false;
		// Write out the list of attributes as an array of attribute objects
		for (auto Attr : Attributes)
		{
			if (bHasWrittenFirstAttr)
			{
				FileArchive->Logf(TEXT("\t\t\t,"));
			}
			FileArchive->Logf(TEXT("\t\t\t{"));
			FileArchive->Logf(TEXT("\t\t\t\t\"name\" : \"%s\","), *Attr.GetName());
			FileArchive->Logf(TEXT("\t\t\t\t\"value\" : \"%s\""), *Attr.GetValue());
			FileArchive->Logf(TEXT("\t\t\t}"));
			bHasWrittenFirstAttr = true;
		}
		FileArchive->Logf(TEXT("\t\t\t]"));

		FileArchive->Logf(TEXT("\t\t}"));

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
		check(FileArchive != nullptr);

		if (bHasWrittenFirstEvent)
		{
			FileArchive->Logf(TEXT("\t\t,"));
		}
		bHasWrittenFirstEvent = true;

		FileArchive->Logf(TEXT("\t\t{"));
		FileArchive->Logf(TEXT("\t\t\t\"eventType\" : \"ItemPurchase\","));
		FileArchive->Logf(TEXT("\t\t\t\"itemId\" : \"%s\","), *ItemId);
		FileArchive->Logf(TEXT("\t\t\t\"itemQuantity\" : %d,"), ItemQuantity);

		FileArchive->Logf(TEXT("\t\t\t\"attributes\" :"));
		FileArchive->Logf(TEXT("\t\t\t["));
		bool bHasWrittenFirstAttr = false;
		// Write out the list of attributes as an array of attribute objects
		for (auto Attr : Attributes)
		{
			if (bHasWrittenFirstAttr)
			{
				FileArchive->Logf(TEXT("\t\t\t,"));
			}
			FileArchive->Logf(TEXT("\t\t\t{"));
			FileArchive->Logf(TEXT("\t\t\t\t\"name\" : \"%s\","), *Attr.GetName());
			FileArchive->Logf(TEXT("\t\t\t\t\"value\" : \"%s\""), *Attr.GetValue());
			FileArchive->Logf(TEXT("\t\t\t}"));
			bHasWrittenFirstAttr = true;
		}
		FileArchive->Logf(TEXT("\t\t\t]"));

		FileArchive->Logf(TEXT("\t\t}"));

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
		check(FileArchive != nullptr);

		if (bHasWrittenFirstEvent)
		{
			FileArchive->Logf(TEXT("\t\t,"));
		}
		bHasWrittenFirstEvent = true;

		FileArchive->Logf(TEXT("\t\t{"));
		FileArchive->Logf(TEXT("\t\t\t\"eventType\" : \"CurrencyPurchase\","));
		FileArchive->Logf(TEXT("\t\t\t\"gameCurrencyType\" : \"%s\","), *GameCurrencyType);
		FileArchive->Logf(TEXT("\t\t\t\"gameCurrencyAmount\" : %d,"), GameCurrencyAmount);

		FileArchive->Logf(TEXT("\t\t\t\"attributes\" :"));
		FileArchive->Logf(TEXT("\t\t\t["));
		bool bHasWrittenFirstAttr = false;
		// Write out the list of attributes as an array of attribute objects
		for (auto Attr : Attributes)
		{
			if (bHasWrittenFirstAttr)
			{
				FileArchive->Logf(TEXT("\t\t\t,"));
			}
			FileArchive->Logf(TEXT("\t\t\t{"));
			FileArchive->Logf(TEXT("\t\t\t\t\"name\" : \"%s\","), *Attr.GetName());
			FileArchive->Logf(TEXT("\t\t\t\t\"value\" : \"%s\""), *Attr.GetValue());
			FileArchive->Logf(TEXT("\t\t\t}"));
			bHasWrittenFirstAttr = true;
		}
		FileArchive->Logf(TEXT("\t\t\t]"));

		FileArchive->Logf(TEXT("\t\t}"));

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
		check(FileArchive != nullptr);

		if (bHasWrittenFirstEvent)
		{
			FileArchive->Logf(TEXT("\t\t,"));
		}
		bHasWrittenFirstEvent = true;

		FileArchive->Logf(TEXT("\t\t{"));
		FileArchive->Logf(TEXT("\t\t\t\"eventType\" : \"CurrencyGiven\","));
		FileArchive->Logf(TEXT("\t\t\t\"gameCurrencyType\" : \"%s\","), *GameCurrencyType);
		FileArchive->Logf(TEXT("\t\t\t\"gameCurrencyAmount\" : %d,"), GameCurrencyAmount);

		FileArchive->Logf(TEXT("\t\t\t\"attributes\" :"));
		FileArchive->Logf(TEXT("\t\t\t["));
		bool bHasWrittenFirstAttr = false;
		// Write out the list of attributes as an array of attribute objects
		for (auto Attr : Attributes)
		{
			if (bHasWrittenFirstAttr)
			{
				FileArchive->Logf(TEXT("\t\t\t,"));
			}
			FileArchive->Logf(TEXT("\t\t\t{"));
			FileArchive->Logf(TEXT("\t\t\t\t\"name\" : \"%s\","), *Attr.GetName());
			FileArchive->Logf(TEXT("\t\t\t\t\"value\" : \"%s\""), *Attr.GetValue());
			FileArchive->Logf(TEXT("\t\t\t}"));
			bHasWrittenFirstAttr = true;
		}
		FileArchive->Logf(TEXT("\t\t\t]"));

		FileArchive->Logf(TEXT("\t\t}"));

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