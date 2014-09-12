VOID StartAssembly(vector<wstring> const& params) 
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
							params.at(2).c_str(), params.at(3).c_str(), &dwRet);
						// hr = 0x80131513 (System.MissingMethodException)

						if (SUCCEEDED(hr))
						{
							// *** Success ***
							MessageBox(GetDesktopWindow(), L"Successfully called managed function.", L"Success", MB_OK);
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

