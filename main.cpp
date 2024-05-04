#include "main_plugin.h"

#include <spdlog/sinks/basic_file_sink.h>

void MessageListener(SKSE::MessagingInterface::Message* message);
void OnPapyrusVRMessage(SKSE::MessagingInterface::Message* message);
void SetupLog();

// Interfaces for communicating with other SKSE plugins.
static SKSE::detail::SKSEMessagingInterface* g_messaging;
static SKSE::PluginHandle                    g_pluginHandle = 0xffff;

void InitializeHooking()
{
	auto& trampoline = SKSE::GetTrampoline();
	trampoline.create(128);
}

// Main plugin entry point.
SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
	SKSE::Init(skse);
	SetupLog();

	SKSE::GetMessagingInterface()->RegisterListener(MessageListener);

	g_pluginHandle = skse->GetPluginHandle();
	g_messaging = (SKSE::detail::SKSEMessagingInterface*)skse->QueryInterface(
		SKSE::LoadInterface::kMessaging);

	return true;
}

// Receives messages about the game's state that SKSE broadcasts to all plugins.
void MessageListener(SKSE::MessagingInterface::Message* message)
{
	using namespace SKSE::log;

	switch (message->type)
	{
	case SKSE::MessagingInterface::kPostLoad:
		info("Registering for SkyrimVRTools messages");
		g_messaging->RegisterListener(g_pluginHandle, "SkyrimVRTools", OnPapyrusVRMessage);
		break;

	case SKSE::MessagingInterface::kPostPostLoad:
		info("kPostPostLoad: querying higgs interface");
		g_higgsInterface = HiggsPluginAPI::GetHiggsInterface001(g_pluginHandle, g_messaging);
		if (g_higgsInterface) { info("Got higgs interface"); }
		else
		{
			critical("Plugin disabled: HIGGS interface not found");
			//g_pluginError = true;
		}
		break;

	case SKSE::MessagingInterface::kDataLoaded:
		backpackvr::Init();

	case SKSE::MessagingInterface::kPostLoadGame:
		backpackvr::OnGameLoad();
	default:
		break;
	}
}

// Listener for papyrusvr Messages
void OnPapyrusVRMessage(SKSE::MessagingInterface::Message* message)
{
	SKSE::log::info("SkyrimVRTools message received");
	if (message)
	{
		if (message->type == kPapyrusVR_Message_Init && message->data)
		{
			backpackvr::g_papyrusvr = (PapyrusVRAPI*)message->data;
		}
	}
}

// Initialize logging system.
void SetupLog()
{
	auto logsFolder = SKSE::log::log_directory();
	if (!logsFolder)
	{
		SKSE::stl::report_and_fail("SKSE log_directory not provided, logs disabled.");
		return;
	}
	auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
	auto logFilePath = *logsFolder / std::format("{}.log", pluginName);
	auto fileLoggerPtr =
		std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true);
	auto loggerPtr = std::make_shared<spdlog::logger>("log", std::move(fileLoggerPtr));
	spdlog::set_default_logger(std::move(loggerPtr));
	spdlog::set_level(spdlog::level::trace);
	spdlog::flush_on(spdlog::level::trace);
}
