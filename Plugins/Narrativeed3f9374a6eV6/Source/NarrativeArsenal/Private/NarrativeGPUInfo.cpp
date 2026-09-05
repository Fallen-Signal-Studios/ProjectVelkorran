// Copyright Fallen Signal Studios. All Rights Reserved.
#include "ArsenalStatics.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "RHIGlobals.h"
#include "Templates/RefCounting.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include <dxgi1_4.h>
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"

namespace NarrativeGPUInfo
{
	bool Query(FGPUInfo& Out)
	{
		const auto& GPU = GRHIGlobals.GpuInfo;
		if (!GRHIGlobals.IsRHIInitialized || GRHIGlobals.UsingNullRHI || GPU.AdapterName.IsEmpty() || !GPU.VendorId) { return false; }
		TRefCountPtr<IDXGIFactory1> Factory;
		if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(Factory.GetInitReference())))
			|| !Factory.IsValid()) { return false; }
		TRefCountPtr<IDXGIAdapter1> Selected;
		DXGI_ADAPTER_DESC1 SelectedDescription = {};
		// Match the renderer's identity. Never silently report adapter zero on hybrid systems.
		for (UINT Index = 0; ; ++Index)
		{
			TRefCountPtr<IDXGIAdapter1> Candidate;
			const HRESULT Result = Factory->EnumAdapters1(Index, Candidate.GetInitReference());
			if (Result == DXGI_ERROR_NOT_FOUND) { break; }
			if (FAILED(Result) || !Candidate.IsValid()) { return false; }
			DXGI_ADAPTER_DESC1 Description = {};
			if (FAILED(Candidate->GetDesc1(&Description))) { return false; }
			if (Description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) { continue; }
			if (Description.VendorId != GPU.VendorId || Description.DeviceId != GPU.DeviceId
				|| !GPU.AdapterName.Equals(Description.Description, ESearchCase::IgnoreCase)) { continue; }
			// Identically named duplicate GPUs require an adapter LUID provider. Fail closed.
			if (Selected.IsValid()) { return false; }
			Selected = Candidate; SelectedDescription = Description;
		}
		if (!Selected.IsValid()) { return false; }
		TRefCountPtr<IDXGIAdapter3> Residency;
		if (FAILED(Selected->QueryInterface(__uuidof(IDXGIAdapter3), reinterpret_cast<void**>(Residency.GetInitReference())))
			|| !Residency.IsValid()) { return false; }
		DXGI_QUERY_VIDEO_MEMORY_INFO Memory = {};
		if (FAILED(Residency->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &Memory))) { return false; }
		const auto MiB = [](uint64 Bytes) { return static_cast<int32>(FMath::Min<uint64>(Bytes / (1024ull * 1024ull), MAX_int32)); };
		Out.TotalVRAM = MiB(SelectedDescription.DedicatedVideoMemory);
		Out.CurrentVRAM = MiB(Memory.CurrentUsage);
		Out.BudgetVRAM = MiB(Memory.Budget);
		Out.GPUBrand = GPU.AdapterName;
		return true;
	}
}
#endif

TArray<FString> UArsenalStatics::GetMonitorNames()
{
	TArray<FString> Names;
	if (!IsInGameThread() || !FSlateApplication::IsInitialized()) { return Names; }
	FDisplayMetrics Metrics;
	FSlateApplication::Get().GetDisplayMetrics(Metrics);
	for (const auto& Monitor : Metrics.MonitorInfo) { Names.Add(Monitor.Name); }
	return Names;
}

bool UArsenalStatics::GetGPUInfo(FGPUInfo& OutInfo)
{
	OutInfo = FGPUInfo();
#if PLATFORM_WINDOWS
	if (!IsInGameThread() || !GRHIGlobals.IsRHIInitialized || GRHIGlobals.UsingNullRHI) { return false; }
	// Blueprint property bindings may call more than once per frame. Cache failures too.
	static FGPUInfo Cached;
	static double LastQuery = -1.;
	static bool bSucceeded = false;
	const double Now = FPlatformTime::Seconds();
	if (LastQuery < 0. || Now - LastQuery >= 1.)
	{
		LastQuery = Now; Cached = FGPUInfo(); bSucceeded = NarrativeGPUInfo::Query(Cached);
	}
	if (bSucceeded) { OutInfo = Cached; }
	return bSucceeded;
#else
	return false;
#endif
}
