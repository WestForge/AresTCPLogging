// Copyright Epic Games, Inc. All Rights Reserved.

#include "TCPLoggingEditor.h"

#include "Analytics.h"
#include "Modules/ModuleManager.h"
#include "TCPLoggingSettings.h"

IMPLEMENT_MODULE(FTCPLoggingEditorModule, TCPLoggingEditor);

#define LOCTEXT_NAMESPACE "TCPLogging"

void FTCPLoggingEditorModule::StartupModule()
{
}

void FTCPLoggingEditorModule::ShutdownModule()
{
}

UTCPLoggingSettings::UTCPLoggingSettings(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SettingsDisplayName = LOCTEXT("SettingsDisplayName", "TCPLogging");
	SettingsTooltip = LOCTEXT("SettingsTooltip", "TCPLogging analytics configuration settings");
}

void UTCPLoggingSettings::ReadConfigSettings()
{
	FString ReadHostName =
		FAnalytics::Get().GetConfigValueFromIni(GetIniName(), GetReleaseIniSection(), TEXT("TCPLoggingHostName"), true);
	ReleaseHostName = ReadHostName;

	FString ReadPort = FAnalytics::Get().GetConfigValueFromIni(GetIniName(), GetReleaseIniSection(), TEXT("TCPLoggingPort"), true);
	ReleasePort = ReadPort;

	ReadHostName = FAnalytics::Get().GetConfigValueFromIni(GetIniName(), GetDebugIniSection(), TEXT("TCPLoggingHostName"), true);
	if (ReadHostName.Len() == 0)
	{
		ReadHostName = ReleaseHostName;
	}
	DebugHostName = ReadHostName;

	ReadPort = FAnalytics::Get().GetConfigValueFromIni(GetIniName(), GetDebugIniSection(), TEXT("TCPLoggingPort"), true);
	if (ReadPort.Len() == 0)
	{
		ReadPort = ReleasePort;
	}
	DebugPort = ReadPort;

	ReadHostName = FAnalytics::Get().GetConfigValueFromIni(GetIniName(), GetTestIniSection(), TEXT("TCPLoggingHostName"), true);
	if (ReadHostName.Len() == 0)
	{
		ReadHostName = ReleaseHostName;
	}
	TestHostName = ReadHostName;

	ReadPort = FAnalytics::Get().GetConfigValueFromIni(GetIniName(), GetTestIniSection(), TEXT("TCPLoggingPort"), true);
	if (ReadPort.Len() == 0)
	{
		ReadPort = ReleasePort;
	}
	TestPort = ReadPort;

	ReadHostName =
		FAnalytics::Get().GetConfigValueFromIni(GetIniName(), GetDevelopmentIniSection(), TEXT("TCPLoggingHostName"), true);
	if (ReadHostName.Len() == 0)
	{
		ReadHostName = ReleaseHostName;
	}
	DevelopmentHostName = ReadHostName;

	ReadPort = FAnalytics::Get().GetConfigValueFromIni(GetIniName(), GetDevelopmentIniSection(), TEXT("TCPLoggingPort"), true);
	if (ReadPort.Len() == 0)
	{
		ReadPort = ReleasePort;
	}
	DevelopmentPort = ReadPort;

	bReleaseGenerateSessionGuid =
		FAnalytics::Get()
			.GetConfigValueFromIni(GetIniName(), GetReleaseIniSection(), TEXT("TCPLoggingGenerateSessionGuid"), true)
			.ToBool();

	bTestGenerateSessionGuid =
		FAnalytics::Get()
			.GetConfigValueFromIni(GetIniName(), GetTestIniSection(), TEXT("TCPLoggingGenerateSessionGuid"), true)
			.ToBool();

	bDevelopmentGenerateSessionGuid =
		FAnalytics::Get()
			.GetConfigValueFromIni(GetIniName(), GetDevelopmentIniSection(), TEXT("TCPLoggingGenerateSessionGuid"), true)
			.ToBool();

	bDebugGenerateSessionGuid =
		FAnalytics::Get()
			.GetConfigValueFromIni(GetIniName(), GetDebugIniSection(), TEXT("TCPLoggingGenerateSessionGuid"), true)
			.ToBool();

	// timestamp

	bReleaseTimeStampEvents =
		FAnalytics::Get()
			.GetConfigValueFromIni(GetIniName(), GetReleaseIniSection(), TEXT("TCPLoggingTimeStampEvents"), true)
			.ToBool();

	bTestTimeStampEvents = FAnalytics::Get()
							   .GetConfigValueFromIni(GetIniName(), GetTestIniSection(), TEXT("TCPLoggingTimeStampEvents"), true)
							   .ToBool();

	bDevelopmentTimeStampEvents =
		FAnalytics::Get()
			.GetConfigValueFromIni(GetIniName(), GetDevelopmentIniSection(), TEXT("TCPLoggingTimeStampEvents"), true)
			.ToBool();

	bDebugTimeStampEvents = FAnalytics::Get()
								.GetConfigValueFromIni(GetIniName(), GetDebugIniSection(), TEXT("TCPLoggingTimeStampEvents"), true)
								.ToBool();
}

void UTCPLoggingSettings::WriteConfigSettings()
{
	FAnalytics::Get().WriteConfigValueToIni(GetIniName(), GetReleaseIniSection(), TEXT("TCPLoggingHostName"), ReleaseHostName);
	FAnalytics::Get().WriteConfigValueToIni(GetIniName(), GetTestIniSection(), TEXT("TCPLoggingHostName"), TestHostName);
	FAnalytics::Get().WriteConfigValueToIni(GetIniName(), GetDebugIniSection(), TEXT("TCPLoggingHostName"), DebugHostName);
	FAnalytics::Get().WriteConfigValueToIni(
		GetIniName(), GetDevelopmentIniSection(), TEXT("TCPLoggingHostName"), DevelopmentHostName);

	FAnalytics::Get().WriteConfigValueToIni(GetIniName(), GetReleaseIniSection(), TEXT("TCPLoggingPort"), ReleasePort);
	FAnalytics::Get().WriteConfigValueToIni(GetIniName(), GetTestIniSection(), TEXT("TCPLoggingPort"), TestPort);
	FAnalytics::Get().WriteConfigValueToIni(GetIniName(), GetDebugIniSection(), TEXT("TCPLoggingPort"), DebugPort);
	FAnalytics::Get().WriteConfigValueToIni(GetIniName(), GetDevelopmentIniSection(), TEXT("TCPLoggingPort"), DevelopmentPort);

	FString TrueValue = FString(TEXT("true"));
	FString FalseValue = FString(TEXT("false"));

	FAnalytics::Get().WriteConfigValueToIni(GetIniName(), GetReleaseIniSection(), TEXT("TCPLoggingGenerateSessionGuid"),
		bReleaseGenerateSessionGuid ? TrueValue : FalseValue);
	FAnalytics::Get().WriteConfigValueToIni(
		GetIniName(), GetTestIniSection(), TEXT("TCPLoggingGenerateSessionGuid"), bTestTimeStampEvents ? TrueValue : FalseValue);
	FAnalytics::Get().WriteConfigValueToIni(
		GetIniName(), GetDebugIniSection(), TEXT("TCPLoggingGenerateSessionGuid"), bDebugTimeStampEvents ? TrueValue : FalseValue);
	FAnalytics::Get().WriteConfigValueToIni(GetIniName(), GetDevelopmentIniSection(), TEXT("TCPLoggingGenerateSessionGuid"),
		bDevelopmentTimeStampEvents ? TrueValue : FalseValue);

	// timestamp

	FAnalytics::Get().WriteConfigValueToIni(GetIniName(), GetReleaseIniSection(), TEXT("TCPLoggingTimeStampEvents"),
		bReleaseTimeStampEvents ? TrueValue : FalseValue);
	FAnalytics::Get().WriteConfigValueToIni(GetIniName(), GetTestIniSection(), TEXT("TCPLoggingTimeStampEvents"),
		bTestGenerateSessionGuid ? TrueValue : FalseValue);
	FAnalytics::Get().WriteConfigValueToIni(GetIniName(), GetDebugIniSection(), TEXT("TCPLoggingTimeStampEvents"),
		bDebugGenerateSessionGuid ? TrueValue : FalseValue);
	FAnalytics::Get().WriteConfigValueToIni(GetIniName(), GetDevelopmentIniSection(), TEXT("TCPLoggingTimeStampEvents"),
		bDevelopmentGenerateSessionGuid ? TrueValue : FalseValue);
}

#undef LOCTEXT_NAMESPACE
