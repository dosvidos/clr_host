#include <windows.h>
#include <MSCorEE.h>
#include <MetaHost.h>
#include <assert.h>
#include <string>
#include <iostream>

#pragma comment(lib, "mscoree.lib")
using namespace std;

class ProfilingCLRHost : public IHostControl, public IHostGCManager
{
private:
    LONG m_refCount;
    LARGE_INTEGER m_lastGCStart;
    LARGE_INTEGER m_frequency;

public:
    ProfilingCLRHost() { QueryPerformanceFrequency(&m_frequency); }

    // IHostControl
    HRESULT __stdcall GetHostManager(REFIID riid, void** ppObject)
    {
        if(riid == IID_IHostGCManager)
        {
			*ppObject = static_cast<IHostGCManager*>(this);
            return S_OK;
        }

        *ppObject = NULL;
        return E_NOINTERFACE;
    }

    // IUnknown
    HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject)
    {
        if (riid == IID_IHostGCManager)
        {
			*ppvObject = static_cast<IHostGCManager*>(this);
            return S_OK;
        }

        *ppvObject = NULL;
        return E_NOINTERFACE;
    }

    HRESULT __stdcall SetAppDomainManager(DWORD appDomain, IUnknown* domainManager)
    {
        return S_OK;
    }

    ULONG __stdcall AddRef() { return InterlockedIncrement(&m_refCount); }
    ULONG __stdcall Release() { return InterlockedDecrement(&m_refCount); }

    // IHostGCManager
    HRESULT __stdcall ThreadIsBlockingForSuspension() { return S_OK; }

    HRESULT __stdcall SuspensionStarting()
    {
        m_lastGCStart;
        QueryPerformanceCounter(&m_lastGCStart);

        return S_OK;
    }

    HRESULT __stdcall SuspensionEnding(DWORD gen)
    {
        LARGE_INTEGER gcEnd;
        QueryPerformanceCounter(&gcEnd);
        double duration = ((gcEnd.QuadPart - m_lastGCStart.QuadPart))
            * 1000.0 / (double)m_frequency.QuadPart;

		if (gen != UINT_MAX)
			cout << "GC generation " << gen << " ended: " << duration << "ms" << endl;
		else
			cout << "CLR suspension ended: " << gen << " ms" << duration << endl;

        return S_OK;
    }
};

int wmain(int argc, wchar_t* argv[])
{
	if (argc < 5)
		cout << "Provide assembly_name entry_type_name enty_method_name." << endl;
	
	assert(argv[1] && argv[2] && argv[3]);
	wstring assembly(argv[1]), type(argv[2]), method(argv[3]);

    //DWORD startupFlags = STARTUP_CONCURRENT_GC;
	ICLRMetaHost *pMetaHost = NULL;
	HRESULT hr = CLRCreateInstance(CLSID_CLRMetaHost, IID_ICLRMetaHost, (LPVOID*)&pMetaHost);
    assert(SUCCEEDED(hr) && pMetaHost);

	wchar_t runtimeVersion[MAX_PATH];
	DWORD versionStringLength = _countof(runtimeVersion);
	hr = pMetaHost->GetVersionFromFile(assembly.c_str(), runtimeVersion, &versionStringLength);
	assert(SUCCEEDED(hr));

	ICLRRuntimeInfo *pRuntimeInfo = NULL;
	hr = pMetaHost->GetRuntime(runtimeVersion, IID_ICLRRuntimeInfo, (LPVOID*)&pRuntimeInfo);
	assert(SUCCEEDED(hr) && pRuntimeInfo);

	ICLRRuntimeHost* pClrRuntimeHost = NULL;
	pRuntimeInfo->GetInterface(CLSID_CLRRuntimeHost, IID_ICLRRuntimeHost, (LPVOID*)&pClrRuntimeHost);
	assert(SUCCEEDED(hr) && pClrRuntimeHost);

    ProfilingCLRHost customTimingHost;
	hr = pClrRuntimeHost->SetHostControl(&customTimingHost);
    assert(SUCCEEDED(hr));

	hr = pClrRuntimeHost->Start();
    assert(SUCCEEDED(hr));

    DWORD retcode;
	hr = pClrRuntimeHost->ExecuteInDefaultAppDomain(assembly.c_str(), type.c_str(), method.c_str(), L"" , &retcode);
    assert(SUCCEEDED(hr));

    return 0;
};

ICLRRuntimeHost* getRuntimeHostForSpecificRuntimeVersion(ICLRMetaHost* pMetaHost, wstring runtimeVersion)
{
	IEnumUnknown *pInstalledRuntimes = NULL;
	HRESULT hr = pMetaHost->EnumerateInstalledRuntimes(&pInstalledRuntimes);
	assert(SUCCEEDED(hr) && pInstalledRuntimes);

	ICLRRuntimeHost* pCLR = NULL;

	ICLRRuntimeInfo* pRuntimesArray[2] = { NULL };
	const ULONG maxFetch = _countof(pRuntimesArray);
	ULONG count = 0;

	for (hr = pInstalledRuntimes->Next(maxFetch, (IUnknown**)pRuntimesArray, &count); SUCCEEDED(hr) && count != 0;
	hr = pInstalledRuntimes->Next(maxFetch, (IUnknown**)pRuntimesArray, &count))
	{
		while (count)
		{
			--count;
			wchar_t version[MAX_PATH] = { 0 };
			DWORD numChars = _countof(version);
			hr = pRuntimesArray[count]->GetVersionString(version, &numChars);
			assert(SUCCEEDED(hr));
			wcout << version << endl;
			if (!pCLR && !runtimeVersion.compare(version))
			{
				hr = pRuntimesArray[count]->GetInterface(CLSID_CLRRuntimeHost, IID_ICLRRuntimeHost, (LPVOID*)&pCLR);
				assert(SUCCEEDED(hr) && pCLR);
			}
			else
				pRuntimesArray[count]->Release();
		}
	}

	return pCLR;
}