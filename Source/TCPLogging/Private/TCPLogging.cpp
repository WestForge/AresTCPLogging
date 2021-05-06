// Copyright Epic Games, Inc. All Rights Reserved.

#include "TCPLogging.h"

#include "Analytics.h"
#include "AnalyticsEventAttribute.h"
//#include "GenericPlatform/GenericPlatformMisc.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Misc/CString.h"
#include "Misc/DefaultValueHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/BufferArchive.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "TCPLoggingProvider.h"

DEFINE_LOG_CATEGORY_STATIC(LogTCPLoggingAnalytics, Display, All);

IMPLEMENT_MODULE(FAnalyticsTCPLogging, TCPLogging)

TSharedPtr<IAnalyticsProvider> FAnalyticsProviderTCPLogging::Provider;

FAnalyticsProviderTCPLogging::FAnalyticsProviderTCPLogging(
	const FString HostName, int32 PortNum, bool bGenerateSession, bool bTimeStamp)
{
	UE_LOG(LogTCPLoggingAnalytics, Verbose, TEXT("Initializing TCP Analytics provider"));

	Host = HostName;
	Port = PortNum;
	bGenerateSessionGuid = bGenerateSession;
	bTimeStampEvents = bTimeStamp;

	UserId = FPlatformMisc::GetLoginId();

	Socket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("default"), false);
}

void FAnalyticsTCPLogging::StartupModule()
{
	// TCPLoggingProvider = MakeShareable(new FAnalyticsProviderTCPLogging());
}

void FAnalyticsTCPLogging::ShutdownModule()
{
	FAnalyticsProviderTCPLogging::Destroy();
}

TSharedPtr<IAnalyticsProvider> FAnalyticsTCPLogging::CreateAnalyticsProvider(
	const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const
{
	if (GetConfigValue.IsBound())
	{
		const FString HostName = GetConfigValue.Execute(TEXT("TCPLoggingHostName"), true);
		const FString PortText = GetConfigValue.Execute(TEXT("TCPLoggingPort"), true);
		const bool bGenerateSessionGuid = GetConfigValue.Execute(TEXT("TCPLoggingGenerateSessionGuid"), true).ToBool();
		const bool bTimeStampEvents = GetConfigValue.Execute(TEXT("TCPLoggingTimeStampEvents"), true).ToBool();

		int32 Port;

		if (FDefaultValueHelper::ParseInt(PortText, Port))
		{
			return FAnalyticsProviderTCPLogging::Create(HostName, Port, bGenerateSessionGuid, bTimeStampEvents);
		}
		else
		{
			UE_LOG(LogTCPLoggingAnalytics, Warning, TEXT("FAnalyticsTCPLogging::CreateAnalyticsProvider invalid port number"));
		}
	}
	else
	{
		UE_LOG(
			LogTCPLoggingAnalytics, Warning, TEXT("FAnalyticsTCPLogging::CreateAnalyticsProvider called with an unbound delegate"));
	}
	return nullptr;
}

// Provider

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

	if (bGenerateSessionGuid)
	{
		FGuid SessionGUID;
		FGenericPlatformMisc::CreateGuid(SessionGUID);
		SessionId = SessionGUID.ToString(EGuidFormats::DigitsWithHyphensInBraces);
		DeviceId = FPlatformMisc::GetDeviceId();

		// UserId = FPlatformMisc::GetLoginId();
	}

	Socket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("default"), false);
	ISocketSubsystem* SocketSubSystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	const char* HostChar = TCHAR_TO_ANSI(*Host);
	auto ResolveInfo = SocketSubSystem->GetHostByName(HostChar);
	while (!ResolveInfo->IsComplete())
		;

	if (ResolveInfo->GetErrorCode() == 0)
	{
		const FInternetAddr* Addr = &ResolveInfo->GetResolvedAddress();
		uint32 OutIP = 0;
		Addr->GetIp(OutIP);

		UE_LOG(LogTCPLoggingAnalytics, Warning, TEXT("IP is %d.%d.%d.%d: "), 0xff & (OutIP >> 24), 0xff & (OutIP >> 16),
			0xff & (OutIP >> 8), 0xff & OutIP);

		TSharedRef<FInternetAddr> Address = SocketSubSystem->CreateInternetAddr();
		Address->SetIp(OutIP);
		Address->SetPort(Port);
		bHasSessionStarted = Socket->Connect(*Address);

		if (bHasSessionStarted)
		{
			FString message = FString::Printf(TEXT("{"));
			message.Append(FString::Printf(TEXT("\"eventName\" : \"Session.Start\",")));
			if (bGenerateSessionGuid)
			{
				message.Append(FString::Printf(TEXT("\"sessionId\" : \"%s\","), *SessionId));
				message.Append(FString::Printf(TEXT("\"deviceId\" : \"%s\","), *DeviceId));
			}
			if (bTimeStampEvents)
			{
				message.Append(FString::Printf(TEXT("\"timestamp\" : \"%s\","), *FDateTime::Now().ToString()));
			}
			message.Append(FString::Printf(TEXT("\"userId\" : \"%s\""), *UserId));

			if (Attributes.Num() > 0)
			{
				message.Append(FString::Printf(TEXT(",\"attributes\" : [")));
				bool bHasWrittenFirstAttr = false;
				// Write out the list of attributes as an array of attribute objects
				for (auto Attr : Attributes)
				{
					if (bHasWrittenFirstAttr)
					{
						message + FString::Printf(TEXT(","));
					}
					message.Append(FString::Printf(TEXT("{")));
					message.Append(FString::Printf(TEXT("\"name\" : \"%s\","), *Attr.GetName()));
					if (Attr.GetValue().IsNumeric())
					{
						message.Append(FString::Printf(TEXT("\"value\" : %s"), *Attr.GetValue()));
					}
					else
					{
						message.Append(FString::Printf(TEXT("\"value\" : \"%s\""), *Attr.GetValue()));
					}
					message.Append(FString::Printf(TEXT("}")));
					bHasWrittenFirstAttr = true;
				}
				message.Append(FString::Printf(TEXT("]")));
			}
			message.Append(FString::Printf(TEXT("}\n")));

			SendJSON(message);
		}
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
		check(Socket != nullptr);

		FString message = FString::Printf(TEXT("{"));
		message.Append(FString::Printf(TEXT("\"eventName\" : \"%s\""), *EventName));
		if (Attributes.Num() > 0)
		{
			message.Append(FString::Printf(TEXT(",\"attributes\" : [")));
			bool bHasWrittenFirstAttr = false;
			// Write out the list of attributes as an array of attribute objects
			for (auto Attr : Attributes)
			{
				if (bHasWrittenFirstAttr)
				{
					message.Append(FString::Printf(TEXT("\t\t\t,")));
				}
				message.Append(FString::Printf(TEXT("{")));
				message.Append(FString::Printf(TEXT("\"name\" : \"%s\","), *Attr.GetName()));
				if (Attr.GetValue().IsNumeric())
				{
					message.Append(FString::Printf(TEXT("\"value\" : %s"), *Attr.GetValue()));
				}
				else
				{
					message.Append(FString::Printf(TEXT("\"value\" : \"%s\""), *Attr.GetValue()));
				}
				message.Append(FString::Printf(TEXT("}")));
				bHasWrittenFirstAttr = true;
			}
			message.Append(FString::Printf(TEXT("]")));
		}
		message.Append(FString::Printf(TEXT("}\n")));

		UE_LOG(LogTCPLoggingAnalytics, Display, TEXT("Analytics event (%s) written with (%d) attributes"), *EventName,
			Attributes.Num());

		SendJSON(message);
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

		FString message = FString::Printf(TEXT("{"));
		message.Append(FString::Printf(TEXT("\"eventName\" : \"recordItemPurchase\",")));

		message.Append(FString::Printf(TEXT("\"attributes\" :")));
		message.Append(FString::Printf(TEXT("[")));

		message.Append(FString::Printf(TEXT("{ \"name\" : \"itemId\", \t\"value\" : \"%s\" },"), *ItemId));
		message.Append(FString::Printf(TEXT("{ \"name\" : \"currency\", \t\"value\" : \"%s\" },"), *Currency));
		message.Append(FString::Printf(TEXT("{ \"name\" : \"perItemCost\", \t\"value\" : \"%d\" },"), PerItemCost));
		message.Append(FString::Printf(TEXT("{ \"name\" : \"itemQuantity\", \t\"value\" : \"%d\" }"), ItemQuantity));

		message.Append(FString::Printf(TEXT("]")));

		message.Append(FString::Printf(TEXT("}\n")));

		SendJSON(message);

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

		FString message = FString::Printf(TEXT("{"));
		message.Append(FString::Printf(TEXT("\"eventName\" : \"recordCurrencyPurchase\",")));

		message.Append(FString::Printf(TEXT("\"attributes\" :")));
		message.Append(FString::Printf(TEXT("[")));

		message.Append(FString::Printf(TEXT("{ \"name\" : \"gameCurrencyType\", \t\"value\" : \"%s\" },"), *GameCurrencyType));
		message.Append(FString::Printf(TEXT("{ \"name\" : \"gameCurrencyAmount\", \t\"value\" : \"%d\" },"), GameCurrencyAmount));
		message.Append(FString::Printf(TEXT("{ \"name\" : \"realCurrencyType\", \t\"value\" : \"%s\" },"), *RealCurrencyType));
		message.Append(FString::Printf(TEXT("{ \"name\" : \"realMoneyCost\", \t\"value\" : \"%f\" },"), RealMoneyCost));
		message.Append(FString::Printf(TEXT("{ \"name\" : \"paymentProvider\", \t\"value\" : \"%s\" }"), *PaymentProvider));

		message.Append(FString::Printf(TEXT("]")));

		message.Append(FString::Printf(TEXT("}")));
		SendJSON(message);

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

		FString message = FString::Printf(TEXT("\t\t{"));
		message.Append(FString::Printf(TEXT("\t\t\t\"eventName\" : \"recordCurrencyGiven\",")));

		message.Append(FString::Printf(TEXT("\t\t\t\"attributes\" :")));
		message.Append(FString::Printf(TEXT("\t\t\t[")));

		message.Append(
			FString::Printf(TEXT("\t\t\t\t{ \"name\" : \"gameCurrencyType\", \t\"value\" : \"%s\" },"), *GameCurrencyType));
		message.Append(
			FString::Printf(TEXT("\t\t\t\t{ \"name\" : \"gameCurrencyAmount\", \t\"value\" : \"%d\" }"), GameCurrencyAmount));

		message.Append(FString::Printf(TEXT("\t\t\t]")));

		message.Append(FString::Printf(TEXT("\t\t}")));

		SendJSON(message);

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

		FString message = FString::Printf(TEXT("\t\t{"));
		message.Append(FString::Printf(TEXT("\t\t\t\"error\" : \"%s\","), *Error));

		message.Append(FString::Printf(TEXT("\t\t\t\"attributes\" :")));
		message.Append(FString::Printf(TEXT("\t\t\t[")));
		bool bHasWrittenFirstAttr = false;
		// Write out the list of attributes as an array of attribute objects
		for (auto Attr : Attributes)
		{
			message.Append(FString::Printf(TEXT("\t\t\t{")));
			message.Append(FString::Printf(TEXT("\t\t\t\t\"name\" : \"%s\","), *Attr.GetName()));
			if (Attr.GetValue().IsNumeric())
			{
				message.Append(FString::Printf(TEXT("\"value\" : %s"), *Attr.GetValue()));
			}
			else
			{
				message.Append(FString::Printf(TEXT("\"value\" : \"%s\""), *Attr.GetValue()));
			}
			message.Append(FString::Printf(TEXT("\t\t\t}")));
			bHasWrittenFirstAttr = true;
		}
		message.Append(FString::Printf(TEXT("\t\t\t]")));

		message.Append(FString::Printf(TEXT("\t\t}")));

		SendJSON(message);

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

		FString message = FString::Printf(TEXT("\t\t{"));
		message.Append(FString::Printf(TEXT("\t\t\t\"eventType\" : \"Progress\",")));
		message.Append(FString::Printf(TEXT("\t\t\t\"progressType\" : \"%s\","), *ProgressType));
		message.Append(FString::Printf(TEXT("\t\t\t\"progressName\" : \"%s\","), *ProgressName));

		message.Append(FString::Printf(TEXT("\t\t\t\"attributes\" :")));
		message.Append(FString::Printf(TEXT("\t\t\t[")));
		bool bHasWrittenFirstAttr = false;
		// Write out the list of attributes as an array of attribute objects
		for (auto Attr : Attributes)
		{
			if (bHasWrittenFirstAttr)
			{
				message.Append(FString::Printf(TEXT("\t\t\t,")));
			}
			message.Append(FString::Printf(TEXT("\t\t\t{")));
			message.Append(FString::Printf(TEXT("\t\t\t\t\"name\" : \"%s\","), *Attr.GetName()));
			if (Attr.GetValue().IsNumeric())
			{
				message.Append(FString::Printf(TEXT("\"value\" : %s"), *Attr.GetValue()));
			}
			else
			{
				message.Append(FString::Printf(TEXT("\"value\" : \"%s\""), *Attr.GetValue()));
			}
			message.Append(FString::Printf(TEXT("\t\t\t}")));
			bHasWrittenFirstAttr = true;
		}
		message.Append(FString::Printf(TEXT("\t\t\t]")));

		message.Append(FString::Printf(TEXT("\t\t}")));

		SendJSON(message);

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

		FString message = FString::Printf(TEXT("{"));
		message.Append(FString::Printf(TEXT("\"eventType\" : \"ItemPurchase\",")));
		message.Append(FString::Printf(TEXT("\"itemId\" : \"%s\","), *ItemId));
		message.Append(FString::Printf(TEXT("\"itemQuantity\" : %d,"), ItemQuantity));

		message.Append(FString::Printf(TEXT("\"attributes\" :")));
		message.Append(FString::Printf(TEXT("[")));
		bool bHasWrittenFirstAttr = false;
		// Write out the list of attributes as an array of attribute objects
		for (auto Attr : Attributes)
		{
			if (bHasWrittenFirstAttr)
			{
				message.Append(FString::Printf(TEXT(",")));
			}
			message.Append(FString::Printf(TEXT("{")));
			message.Append(FString::Printf(TEXT("\"name\" : \"%s\","), *Attr.GetName()));
			if (Attr.GetValue().IsNumeric())
			{
				message.Append(FString::Printf(TEXT("\"value\" : %s"), *Attr.GetValue()));
			}
			else
			{
				message.Append(FString::Printf(TEXT("\"value\" : \"%s\""), *Attr.GetValue()));
			}
			message.Append(FString::Printf(TEXT("}")));
			bHasWrittenFirstAttr = true;
		}
		message.Append(FString::Printf(TEXT("]")));

		message.Append(FString::Printf(TEXT("}\n")));

		SendJSON(message);

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

		FString message = FString::Printf(TEXT("{"));
		message.Append(FString::Printf(TEXT("\"eventType\" : \"CurrencyPurchase\",")));
		message.Append(FString::Printf(TEXT("\"gameCurrencyType\" : \"%s\","), *GameCurrencyType));
		message.Append(FString::Printf(TEXT("\"gameCurrencyAmount\" : %d,"), GameCurrencyAmount));

		message.Append(FString::Printf(TEXT("\"attributes\" :")));
		message.Append(FString::Printf(TEXT("[")));
		bool bHasWrittenFirstAttr = false;
		// Write out the list of attributes as an array of attribute objects
		for (auto Attr : Attributes)
		{
			if (bHasWrittenFirstAttr)
			{
				FString::Printf(TEXT(","));
			}
			message.Append(FString::Printf(TEXT("{")));
			message.Append(FString::Printf(TEXT("\"name\" : \"%s\","), *Attr.GetName()));
			if (Attr.GetValue().IsNumeric())
			{
				message.Append(FString::Printf(TEXT("\"value\" : %s"), *Attr.GetValue()));
			}
			else
			{
				message.Append(FString::Printf(TEXT("\"value\" : \"%s\""), *Attr.GetValue()));
			}
			message.Append(FString::Printf(TEXT("}")));
			bHasWrittenFirstAttr = true;
		}
		message.Append(FString::Printf(TEXT("]")));

		message.Append(FString::Printf(TEXT("}\n")));

		SendJSON(message);

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

		FString message = FString::Printf(TEXT("{"));
		message.Append(FString::Printf(TEXT("\"eventType\" : \"CurrencyGiven\",")));
		message.Append(FString::Printf(TEXT("\"gameCurrencyType\" : \"%s\","), *GameCurrencyType));
		message.Append(FString::Printf(TEXT("\"gameCurrencyAmount\" : %d,"), GameCurrencyAmount));

		message.Append(FString::Printf(TEXT("\"attributes\" :")));
		message.Append(FString::Printf(TEXT("[")));
		bool bHasWrittenFirstAttr = false;
		// Write out the list of attributes as an array of attribute objects
		for (auto Attr : Attributes)
		{
			if (bHasWrittenFirstAttr)
			{
				message.Append(FString::Printf(TEXT(",")));
			}
			message.Append(FString::Printf(TEXT("{")));
			message.Append(FString::Printf(TEXT("\"name\" : \"%s\","), *Attr.GetName()));
			if (Attr.GetValue().IsNumeric())
			{
				message.Append(FString::Printf(TEXT("\"value\" : %s"), *Attr.GetValue()));
			}
			else
			{
				message.Append(FString::Printf(TEXT("\"value\" : \"%s\""), *Attr.GetValue()));
			}
			message.Append(FString::Printf(TEXT("}")));
			bHasWrittenFirstAttr = true;
		}
		message.Append(FString::Printf(TEXT("]")));

		message.Append(FString::Printf(TEXT("}\n")));

		SendJSON(message);

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
	TCHAR* payloadChar = serialized.GetCharArray().GetData();
	int32 JSONSize = FCString::Strlen(payloadChar);
	int32 AmountSent = 0;

	Socket->Send((uint8*) TCHAR_TO_UTF8(payloadChar), JSONSize, AmountSent);
}