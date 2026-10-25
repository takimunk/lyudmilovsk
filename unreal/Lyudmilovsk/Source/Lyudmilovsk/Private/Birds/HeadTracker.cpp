#include "HeadTracker.h"

#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"

#if PLATFORM_MAC
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace
{
	TAutoConsoleVariable<int32> CVarPort(TEXT("lyud.Bird.HeadPort"), 47821, TEXT("Localhost UDP port the AirPods helper sends the head pose to."));
	TAutoConsoleVariable<FString> CVarSigns(TEXT("lyud.Bird.HeadSigns"), TEXT("1 1 1"), TEXT("Sign of the AirPods yaw, pitch and roll (flip one with -1 if your head turns the other way)."));
}

void FHeadTracker::Start()
{
#if PLATFORM_MAC
	if (Socket < 0)
	{
		Socket = socket(AF_INET, SOCK_DGRAM, 0);
		if (Socket >= 0)
		{
			sockaddr_in Addr = {};
			Addr.sin_family = AF_INET;
			Addr.sin_port = htons((uint16)CVarPort.GetValueOnGameThread());
			Addr.sin_addr.s_addr = inet_addr("127.0.0.1");
			fcntl(Socket, F_SETFL, fcntl(Socket, F_GETFL, 0) | O_NONBLOCK);
			if (bind(Socket, (sockaddr*)&Addr, sizeof Addr) != 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("Bird head tracking: UDP port %d is taken, no AirPods tracking"), CVarPort.GetValueOnGameThread());
				close(Socket);
				Socket = -1;
			}
		}
	}
	if (Socket >= 0 && !bLaunched)
	{
		Launch();
	}
#endif
}

void FHeadTracker::Launch()
{
#if PLATFORM_MAC
	const FString App = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("HeadTracker/LyudHeadTracker.app"));
	if (FPaths::DirectoryExists(App))
	{
		// through LaunchServices, so macOS asks for motion access on the helper's behalf, not the editor's
		const FString Args = FString::Printf(TEXT("-g -j -n -a \"%s\" --args --port %d --pid %d"), *App, CVarPort.GetValueOnGameThread(), FPlatformProcess::GetCurrentProcessId());
		FProcHandle H = FPlatformProcess::CreateProc(TEXT("/usr/bin/open"), *Args, true, true, true, nullptr, 0, nullptr, nullptr);
		FPlatformProcess::CloseProc(H);
		LastState = TEXT("starting");
		bLaunched = true;
		return;
	}
	if (!bBuilding)
	{
		// first use on this Mac: build the helper from tools/headtracker (a few seconds), then start it
		const FString Script = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../../tools/headtracker/build.sh"));
		if (FPaths::FileExists(Script))
		{
			FProcHandle H = FPlatformProcess::CreateProc(TEXT("/bin/sh"), *FString::Printf(TEXT("\"%s\""), *Script), true, true, true, nullptr, 0, nullptr, nullptr);
			FPlatformProcess::CloseProc(H);
			LastState = TEXT("building the helper");
		}
		else
		{
			LastState = TEXT("no helper");
		}
		bBuilding = true;
	}
#endif
}

void FHeadTracker::Stop()
{
#if PLATFORM_MAC
	if (Socket >= 0)
	{
		close(Socket);
		Socket = -1;
	}
#endif
	// the helper exits on its own with the game; while the game runs it stays for the next flight
}

void FHeadTracker::Poll()
{
#if PLATFORM_MAC
	if (Socket < 0)
	{
		return;
	}
	if (!bLaunched && bBuilding && FPlatformTime::Seconds() - LastLaunchTry > 1.0)
	{
		LastLaunchTry = FPlatformTime::Seconds();
		Launch();
	}
	TArray<FString> Signs;
	CVarSigns.GetValueOnGameThread().ParseIntoArrayWS(Signs);
	auto Sign = [&Signs](int32 I) { return Signs.IsValidIndex(I) ? FCString::Atof(*Signs[I]) : 1.f; };
	ANSICHAR Buf[256];
	for (int32 Guard = 0; Guard < 256; Guard++)
	{
		const ssize_t N = recv(Socket, Buf, sizeof Buf - 1, 0);
		if (N <= 0)
		{
			break;
		}
		Buf[N] = 0;
		const FString Line = UTF8_TO_TCHAR(Buf);
		if (Line.StartsWith(TEXT("a ")))
		{
			TArray<FString> P;
			Line.ParseIntoArrayWS(P);
			if (P.Num() >= 4)
			{
				// CoreMotion: yaw + to the left about up, pitch + nose up, roll + right ear down
				Pose = FRotator(FMath::RadiansToDegrees(FCString::Atof(*P[2])) * Sign(1),
				                -FMath::RadiansToDegrees(FCString::Atof(*P[1])) * Sign(0),
				                FMath::RadiansToDegrees(FCString::Atof(*P[3])) * Sign(2));
				LastSample = FPlatformTime::Seconds();
			}
		}
		else if (Line.StartsWith(TEXT("s ")) && !Line.StartsWith(TEXT("s alive")))
		{
			LastState = Line.Mid(2);
		}
	}
#endif
}

bool FHeadTracker::IsTracking() const
{
	return bFake || FPlatformTime::Seconds() - LastSample < 0.5;
}
