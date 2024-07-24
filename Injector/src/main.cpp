#include "pch.h"
#include <Windows.h>
#include <iostream>
#include <sstream>
#include <filesystem>

#include <SimpleIni.h>

#include "injector.h"
#include "util.h"
#include <extens/logger.h>

const std::string ProcName = "Client-Win64-Shipping.exe";

static CSimpleIni ini;

bool OpenGameProcess(HANDLE* phProcess, HANDLE* phThread);

int main(int argc, char* argv[])
{
	Logger::SetLevel(Logger::Level::Debug, Logger::LoggerType::ConsoleLogger);

	auto path = std::filesystem::path(argv[0]).parent_path();
	current_path(path);

	WaitForCloseProcess(ProcName);

	Sleep(1000);

	ini.SetUnicode();
	ini.LoadFile("Settings.ini");

	std::string dllPath = ini.GetValue("Settings", "DLL", "");
	if (dllPath.empty())
	{
		auto result = SelectFile("DLL Files\0*.DLL\0All Files\0*.*\0", "Select the DLL file");
		if (result.has_value())
		{
			dllPath = result.value();
			ini.SetValue("Settings", "DLL", dllPath.c_str());
			ini.SaveFile("Settings.ini");
		}
		else
		{
			std::cout << "No DLL selected. Exiting.\n";
			return 1;
		}
	}

	HANDLE hProcess, hThread;
	bool success = OpenGameProcess(&hProcess, &hThread);
	if (!success)
	{
		LOG_ERROR("Failed to open Client-Win64-Shipping process.");
		system("pause");
		return 1;
	}

	LOG_INFO("Process opened successfully.");

	current_path(path);
	ini.SaveFile("Settings.ini");

	std::filesystem::path currentDllPath = std::filesystem::current_path() / dllPath;

#ifdef _DEBUG
	std::filesystem::path tempDllPath = std::filesystem::temp_directory_path() / dllPath;

	std::error_code ec;
	std::filesystem::copy_file(currentDllPath, tempDllPath, std::filesystem::copy_options::update_existing, ec);
	if (ec)
	{
		LOG_ERROR("Copy dll failed: %s", ec.message().c_str());
		std::system("pause");
	}

	InjectDLL(hProcess, tempDllPath.string());
#else
	InjectDLL(hProcess, currentDllPath.string());
#endif

	Sleep(2000);
	ResumeThread(hThread);

	CloseHandle(hProcess);
}

bool OpenGameProcess(HANDLE* phProcess, HANDLE* phThread)
{
	HANDLE hToken;
	BOOL TokenRet = OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &hToken);

	if (!TokenRet)
	{
		LOG_LAST_ERROR("Privilege escalation failed!");
		return false;
	}

	auto filePath = GetOrSelectPath(ini, "Settings", "Executable", "wuwa path", "Executable\0Client-Win64-Shipping.exe;\0");
	auto commandline = ini.GetValue("Settings", "WuwaCommandLine");

	std::string newCommandLine;
	if (commandline != nullptr) {
		newCommandLine = commandline;
	}

	LPSTR lpstr = newCommandLine.empty() ? nullptr : const_cast<LPSTR>(newCommandLine.c_str());

	if (!filePath)
		return false;

	DWORD pid = FindProcessId("explorer.exe");
	if (pid == 0)
	{
		LOG_ERROR("Can't find 'explorer' pid!");
		return false;
	}

	std::string CurrentDirectory = filePath.value();
	int pos = CurrentDirectory.rfind("\\", CurrentDirectory.length());
	CurrentDirectory = CurrentDirectory.substr(0, pos);

	HANDLE handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);

	SIZE_T lpsize = 0;
	InitializeProcThreadAttributeList(NULL, 1, 0, &lpsize);

	char* temp = new char[lpsize];
	LPPROC_THREAD_ATTRIBUTE_LIST AttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)temp;
	InitializeProcThreadAttributeList(AttributeList, 1, 0, &lpsize);
	if (!UpdateProcThreadAttribute(AttributeList, 0, PROC_THREAD_ATTRIBUTE_PARENT_PROCESS,
		&handle, sizeof(HANDLE), NULL, NULL))
	{
		LOG_WARNING("UpdateProcThreadAttribute failed ! (%d).\n", GetLastError());
	}

	STARTUPINFOEXA si{};
	si.StartupInfo.cb = sizeof(si);
	si.lpAttributeList = AttributeList;

	PROCESS_INFORMATION pi{};
	BOOL result = CreateProcessAsUserA(hToken, const_cast<LPSTR>(filePath->data()), lpstr,
		0, 0, 0, EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED, 0,
		(LPSTR)CurrentDirectory.data(), (LPSTARTUPINFOA)&si, &pi);

	bool isOpened = result;
	if (isOpened)
	{
		ini.SaveFile("Settings.ini");
		*phThread = pi.hThread;
		*phProcess = pi.hProcess;
	}
	else
	{
		LOG_LAST_ERROR("Failed to create game process.");
		LOG_ERROR("If you have problem with Client-Win64-Shipping.exe path. You can change it manually in Settings.ini.");
	}

	DeleteProcThreadAttributeList(AttributeList);
	delete[] temp;
	return isOpened;
}