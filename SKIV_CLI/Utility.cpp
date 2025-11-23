
#include <string>
#include <Windows.h>
#include <filesystem>
#include <cwctype>
#include "Utility.h"


std::string
SK_WideCharToUTF8(const std::wstring& in)
{
  // CC BY-SA 4.0: https://stackoverflow.com/a/59617138
  int count =
    WideCharToMultiByte(CP_UTF8, 0, in.c_str(), static_cast <int> (in.length()), NULL, 0, NULL, NULL);
  std::string out(count, 0);
  WideCharToMultiByte(CP_UTF8, 0, in.c_str(), -1, &out[0], count, NULL, NULL);

  return out;
}

bool
SKIF_Util_HasFileSignature(const std::vector<char>& header, const FileSignature& signature)
{
  if (header.size() >= signature.signature.size())
  {
    for (size_t i = 0; i < signature.signature.size(); ++i)
    {
      if (signature.mask[i] == 0xFF)
      {// Need to perform a reinterpret to prevent C's integer promotion from screwing with the comparison
        const unsigned char h = reinterpret_cast<const unsigned char&>(header[i]);

        if (signature.signature[i] != h)
          return false;
      }
    }
    return true;
  }
  return false;
}

// wstring vs wstring
bool StrEq(const std::wstring& a, const std::wstring& b)
{
  return _wcsicmp(a.c_str(), b.c_str()) == 0;
}

// wstring vs const wchar_t*
bool StrEq(const std::wstring& a, const wchar_t* b)
{
  return _wcsicmp(a.c_str(), b) == 0;
}

// const wchar_t* vs wstring
bool StrEq(const wchar_t* a, const std::wstring& b)
{
  return _wcsicmp(a, b.c_str()) == 0;
}

// const wchar_t* vs const wchar_t*
bool StrEq(const wchar_t* a, const wchar_t* b)
{
  return _wcsicmp(a, b) == 0;
}

std::wstring
SKIF_Util_ToLowerW(std::wstring_view input)
{
  std::wstring    copy = std::wstring(input);
  std::transform(copy.begin(), copy.end(), copy.begin(), [](wchar_t c) { return std::towlower(c); });
  return copy;
}

uint64_t
SK_File_GetSize(const wchar_t* wszFile)
{
  WIN32_FILE_ATTRIBUTE_DATA
    file_attrib_data = { };

  if (GetFileAttributesEx(wszFile,
    GetFileExInfoStandard,
    &file_attrib_data))
  {
    if (file_attrib_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
    {
      // If the target at wszFile is a symlink, the GetFileAttributesEx
      // function always returns a file size of zero. To get the size of the
      // file pointed to by the symlink, we can use _wstat64 instead.
      struct _stat64 buffer;
      if (_wstat64(wszFile, &buffer) != 0)
      {
        return 0ULL;
      }
      return buffer.st_size;
    }
    return ULARGE_INTEGER{ file_attrib_data.nFileSizeLow,
                            file_attrib_data.nFileSizeHigh }.QuadPart;
  }

  return 0ULL;
}

std::wstring
GetPathToSK()
{
  std::wstring path_to_sk =
    L"%USERPROFILE%\\My Documents";
  std::wstring temp(262, L'\0');
  ExpandEnvironmentStringsW(path_to_sk.c_str(), &temp[0], 260);
  return temp;
}

bool
CheckPathForInvalidChars(const std::wstring& filename)
{
  for (wchar_t ch : filename)
  {
    switch (ch)
    {
    case L'<':
    case L'>':
    case L':':
    case L'"':
    case L'/':
    case L'\\':
    case L'|':
    case L'?':
    case L'*':
      return true; // Found invalid symbol
    }
  }
  return false; // All characters ok
}

DWORD
WINAPI
SKIF_Util_GetWebUri(skif_get_web_uri_t* get)
{
  static SKIF_RegistrySettings& _registry = SKIF_RegistrySettings::GetInstance();

  ULONG     ulTimeout = 5000UL;
  PCWSTR rgpszAcceptTypes[] = { L"*/*", nullptr };
  HINTERNET hInetHTTPGetReq = nullptr,
    hInetHost = nullptr,
    hInetRoot = nullptr;

  // (Cleanup On Error)
  auto CLEANUP = [&](bool clean = false) ->
    DWORD
    {
      if (!clean)
      {

        // << L"WinInet Failure: " << SKIF_Util_GetErrorAsWStr(GetLastError(), GetModuleHandle(L"wininet.dll"));
      }

      if (hInetHTTPGetReq != nullptr) InternetCloseHandle(hInetHTTPGetReq);
      if (hInetHost != nullptr) InternetCloseHandle(hInetHost);
      if (hInetRoot != nullptr) InternetCloseHandle(hInetRoot);

      skif_get_web_uri_t* to_delete = nullptr;
      std::swap(get, to_delete);
      delete              to_delete;

      return 0;
    };

  hInetRoot =
    InternetOpen(
      L"Special K - Asset Crawler",
      INTERNET_OPEN_TYPE_DIRECT,
      nullptr, nullptr,
      0x00);

  if (hInetRoot == nullptr)
    return CLEANUP();

  DWORD_PTR dwInetCtx = 0;

  hInetHost =
    InternetConnect(hInetRoot,
      get->wszHostName,
      (get->https) ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT,
      nullptr, nullptr,
      INTERNET_SERVICE_HTTP,
      0x00,
      (DWORD_PTR)&dwInetCtx);

  if (hInetHost == nullptr)
    return CLEANUP();

  int flags = ((get->https) ? INTERNET_FLAG_SECURE : 0x0) |
    INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS |
    INTERNET_FLAG_IGNORE_CERT_DATE_INVALID | INTERNET_FLAG_IGNORE_CERT_CN_INVALID;

  flags |= INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE;

  std::wstring full_path = std::wstring(get->wszHostPath);
  if (get->wszExtraInfo[0] != L'\0')
    full_path += std::wstring(get->wszExtraInfo);

  hInetHTTPGetReq =
    HttpOpenRequest(hInetHost,
      get->method,
      full_path.c_str(),
      L"HTTP/1.1",
      nullptr,
      rgpszAcceptTypes,
      flags,
      (DWORD_PTR)&dwInetCtx);

  // Wait 5000 msecs for a dead connection, then give up
  //
  InternetSetOptionW(hInetHTTPGetReq, INTERNET_OPTION_RECEIVE_TIMEOUT,
    &ulTimeout, sizeof(ULONG));

  if (hInetHTTPGetReq == nullptr)
    return CLEANUP();

  if (HttpSendRequestW(hInetHTTPGetReq,
    get->header.c_str(),
    static_cast<DWORD>(get->header.length()),
    (LPVOID)get->body.c_str(),
    static_cast<DWORD>(get->body.size())))
  {
    DWORD dwStatusCode = 0;
    DWORD dwStatusCode_Len = sizeof(DWORD);

    DWORD dwContentLength = 0;
    DWORD dwContentLength_Len = sizeof(DWORD);
    DWORD dwSizeAvailable;

    HttpQueryInfo(hInetHTTPGetReq,
      HTTP_QUERY_STATUS_CODE |
      HTTP_QUERY_FLAG_NUMBER,
      &dwStatusCode,
      &dwStatusCode_Len,
      nullptr);

    if (dwStatusCode == 200)
    {
      HttpQueryInfo(hInetHTTPGetReq,
        HTTP_QUERY_CONTENT_LENGTH |
        HTTP_QUERY_FLAG_NUMBER,
        &dwContentLength,
        &dwContentLength_Len,
        nullptr);

      std::vector <char> http_chunk;
      std::vector <char> concat_buffer;

      while (InternetQueryDataAvailable(hInetHTTPGetReq,
        &dwSizeAvailable,
        0x00, NULL)
        )
      {
        if (dwSizeAvailable > 0)
        {
          DWORD dwSizeRead = 0;

          if (http_chunk.size() < dwSizeAvailable)
            http_chunk.resize(dwSizeAvailable);

          if (InternetReadFile(hInetHTTPGetReq,
            http_chunk.data(),
            dwSizeAvailable,
            &dwSizeRead)
            )
          {
            if (dwSizeRead == 0)
              break;

            concat_buffer.insert(concat_buffer.cend(),
              http_chunk.cbegin(),
              http_chunk.cbegin() + dwSizeRead);

            if (dwSizeRead < dwSizeAvailable)
              break;
          }
        }

        else
          break;
      }

      FILE* fOut = nullptr;

      _wfopen_s(&fOut, get->wszLocalPath, L"wb+");

      if (fOut != nullptr)
      {
        fwrite(concat_buffer.data(), concat_buffer.size(), 1, fOut);
        fflush(fOut);
        fclose(fOut);

        CLEANUP(true);
        return 1;
      }
    }

    else { // dwStatusCode != 200
      LOG << "HttpSendRequestW failed -> HTTP Status Code: " << dwStatusCode;
    }
  }

  return CLEANUP();
}

DWORD
SKIF_Util_GetWebResource(std::wstring url, std::wstring_view destination, std::wstring method, std::wstring header, std::string body)
{
  auto* get =
    new skif_get_web_uri_t{ };

  URL_COMPONENTSW urlcomps = { };

  urlcomps.dwStructSize = sizeof(URL_COMPONENTSW);

  urlcomps.lpszHostName = get->wszHostName;
  urlcomps.dwHostNameLength = INTERNET_MAX_HOST_NAME_LENGTH;

  urlcomps.lpszUrlPath = get->wszHostPath;
  urlcomps.dwUrlPathLength = INTERNET_MAX_PATH_LENGTH;

  urlcomps.lpszExtraInfo = get->wszExtraInfo;
  urlcomps.dwExtraInfoLength = INTERNET_MAX_PATH_LENGTH;

  if (!method.empty())
    get->method = method.c_str();

  if (!header.empty())
    get->header = header.c_str();

  if (!body.empty())
    get->body = body;

  if (InternetCrackUrl(url.c_str(), static_cast <DWORD> (url.length()), 0x00, &urlcomps))
  {
    wcsncpy(get->wszLocalPath,
      destination.data(),
      MAX_PATH);

    get->https = (urlcomps.nScheme == INTERNET_SCHEME_HTTPS);

    return SKIF_Util_GetWebUri(get);
  }

  else {
    LOG << "Failed to cracks a URL into its component parts!";
  }

  return 0;
}