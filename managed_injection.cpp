// Managed Injection.cpp : Defines the exported functions for the DLL application.
//

#include <Windows.h>
#include <MetaHost.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <Psapi.h>
#include <tlhelp32.h>
#include <sstream>
#include <process.h>

using namespace std;

#pragma comment(lib, "mscoree.lib")
#pragma comment(lib, "PSAPI.lib")

VOID StartAssembly(const vector<wstring>& params);
vector<wstring> GetSettings(HMODULE hMod);

string injDir;

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD fwdReason, LPVOID lpvReserved)
{
	BOOL success = TRUE;
	if (fwdReason == DLL_PROCESS_ATTACH)
	{
		HANDLE hFileMapT = OpenFileMapping(FILE_MAP_READ | FILE_MAP_WRITE,
			FALSE, TEXT("Global\\MMFJex"));

		if (hFileMapT != NULL) 
		{
			PVOID pView = MapViewOfFile(hFileMapT,
				FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
			if (pView != NULL) 
			{
				injDir.assign((LPCSTR) pView);
                UnmapViewOfFile(pView);
            } 
			else
                success = FALSE;

             CloseHandle(hFileMapT);

		}
		else
			success = FALSE;
	}

	if (success == TRUE)
		StartAssembly(GetSettings(hInstance));
	return 0;
}


VOID StartAssembly(const vector<wstring>& params) 
{
	ICLRMetaHost *pMetaHost = NULL;
    HRESULT hr;
    hr = CLRCreateInstance(CLSID_CLRMetaHost, IID_ICLRMetaHost,
        (LPVOID*)&pMetaHost);

    if (SUCCEEDED(hr))
    {
        IEnumUnknown *peunkRuntimes;
        hr = pMetaHost->EnumerateInstalledRuntimes(&peunkRuntimes);
        if (SUCCEEDED(hr))
        {
            // *** FINDING LATEST RUNTIME ***
            IUnknown *punkRuntime;
            ICLRRuntimeInfo *prtiLatest = NULL;
            WCHAR szLatestRuntimeVersion[MAX_PATH];
            while (peunkRuntimes->Next(1, &punkRuntime, NULL) == S_OK)
            {
                ICLRRuntimeInfo *prtiCurrent;
                hr = punkRuntime->QueryInterface(IID_PPV_ARGS(&prtiCurrent));
                if (SUCCEEDED(hr))
                {
                    if (!prtiLatest)
                    {
                        hr = prtiCurrent->QueryInterface(IID_PPV_ARGS(&prtiLatest));
                        if (SUCCEEDED(hr))
                        {
                            DWORD cch = ARRAYSIZE(szLatestRuntimeVersion);
                            hr = prtiLatest->GetVersionString(szLatestRuntimeVersion, &cch);
                        }
                    }
                    else
                    {
                        WCHAR szCurrentRuntimeVersion[MAX_PATH];
                        DWORD cch = ARRAYSIZE(szCurrentRuntimeVersion);
                        hr = prtiCurrent->GetVersionString(szCurrentRuntimeVersion, &cch);
                        if (SUCCEEDED(hr))
                        {
                            if (wcsncmp(szLatestRuntimeVersion, szCurrentRuntimeVersion, cch) < 0)
                            {
                                hr = prtiCurrent->GetVersionString(szLatestRuntimeVersion, &cch);
                                if (SUCCEEDED(hr))
                                {
                                    prtiLatest->Release();
                                    hr = prtiCurrent->QueryInterface(IID_PPV_ARGS(&prtiLatest));
                                }
                            }
                        }
                    }
                    prtiCurrent->Release();
                }
                punkRuntime->Release();
            }
            peunkRuntimes->Release();

            // *** STARTING CLR ***
            if (SUCCEEDED(hr))
            {
                ICLRRuntimeHost *prth;
                hr = prtiLatest->GetInterface(CLSID_CLRRuntimeHost, IID_ICLRRuntimeHost, (LPVOID*)&prth);
                if (SUCCEEDED(hr))
                {
					hr = prth->Start();
					if (SUCCEEDED(hr))
					{
						DWORD dwRet = 0;
						hr = prth->ExecuteInDefaultAppDomain(params.at(0).c_str(), params.at(1).c_str(), 
							params.at(2).c_str(), params.at(3).c_str(), &dwRet); // hr = 0x80131513 (System.MissingMethodException) [Crashes here]
						if (SUCCEEDED(hr))
						{
							// *** Success ***
							MessageBox(GetDesktopWindow(), L"Successfully injected assembly.", L"Success", MB_OK);
						}
						hr = prth->Stop();
					}
					prth->Release();
                }
            } 
        }
        pMetaHost->Release();
    }
}

vector<wstring> GetSettings(HMODULE hMod)
{
	ifstream infile;
	string path;
	path.assign(injDir);
	path.append("\\managed settings.txt");

	infile.open(path, std::ios_base::in);

	if (!infile.is_open())
	{
		MessageBox(GetDesktopWindow(), L"Could not read 'managed settings.txt'!", L"Error", MB_OK);
		FreeLibrary(hMod);
	}

	vector<wstring> *file = new vector<wstring>();
	string line;
	file->clear();

	while (getline(infile, line, infile.widen('\n')))
	{
		wstring wline(line.length(), L'');
		copy(line.begin(), line.end(), wline.begin());
		file->push_back (wline);
	}
	infile.close();

	return *file;
}

string ExtractDirectory( const string& path )
{
	return path.substr( 0, path.find_last_of( L'\\' ) +1 );
}