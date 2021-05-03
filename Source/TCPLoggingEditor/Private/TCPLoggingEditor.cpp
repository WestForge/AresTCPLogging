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
	FAnalytics::Get().WriteConfigValueToIni(
		GetIniName(), GetDevelopmentIniSection(), TEXT("TCPLoggingPort"), DevelopmentPort);
}

#undef LOCTEXT_NAMESPACE
