#pragma once
#include <string>
#include <vector>
#include <typeindex>
#include <WinInet.h>
#include <atlbase.h>
#include <iostream>
#pragma comment(lib, "wininet.lib")

constexpr int
__stdcall
SK_GetBitness(void)
{
#ifdef _M_AMD64
  return 64;
#else
  return 32;
#endif
}

//#define SK_RunIf32Bit(x)         { SK_GetBitness () == 32  ? (x) :  0; }
//#define SK_RunIf64Bit(x)         { SK_GetBitness () == 64  ? (x) :  0; }
#define SK_RunLHIfBitness(b,l,r)   SK_GetBitness () == (b) ? (l) : (r)

struct FileSignature {
  std::wstring               mime_type = L"";
  std::vector <std::wstring> file_extensions = { };
  std::vector <uint8_t>      signature = { };
  std::vector <uint8_t>      mask = { };

  FileSignature(std::wstring m, std::vector <std::wstring> e, std::vector <uint8_t> s) : mime_type(m), file_extensions(e), signature(s)
  {
    // Fill the mask with 0xFF everywhere
    mask = std::vector <uint8_t>(signature.size(), 0xFF);
  };

  FileSignature(std::wstring m, std::vector <std::wstring> e, std::vector <uint8_t> s, std::vector <uint8_t> m2) : mime_type(m), file_extensions(e), signature(s), mask(m2)
  {
    if (mask.size() != signature.size())
      throw std::invalid_argument("different sizes for signature and mask");
  };
};
struct skif_get_web_uri_t {
  wchar_t wszHostName[INTERNET_MAX_HOST_NAME_LENGTH] = { };
  wchar_t wszHostPath[INTERNET_MAX_PATH_LENGTH] = { };
  wchar_t wszExtraInfo[INTERNET_MAX_PATH_LENGTH] = { };
  wchar_t wszLocalPath[MAX_PATH + 2] = { };
  LPCWSTR method = L"GET";
  bool         https = false;
  std::string  body;
  std::wstring header;
};


std::string    SK_WideCharToUTF8(const std::wstring& in);
std::wstring   GetPathToSK();
uint64_t       SK_File_GetSize(const wchar_t* wszFile);
std::wstring   SKIF_Util_ToLowerW(std::wstring_view input);
bool           SKIF_Util_HasFileSignature(const std::vector<char>& header, const FileSignature& signature);
bool           CheckPathForInvalidChars(const std::wstring& filename);
DWORD          SKIF_Util_GetWebResource(std::wstring url, std::wstring_view destination,
                                          std::wstring method = L"GET", std::wstring header = L"", std::string body = "");
bool StrEq(const std::wstring& a, const std::wstring& b);
bool StrEq(const std::wstring& a, const wchar_t* b);
bool StrEq(const wchar_t* a,      const std::wstring& b);
bool StrEq(const wchar_t* a,      const wchar_t* b);

const std::vector<std::wstring> allowedExtensions_hdr = {
        L".png", L".avif", L".jxl",
        L".jxr", L".hdr",  L".exr"
};

const std::vector<std::wstring> allowedExtensions_sdr = {
        L".png", L".jpg", L".jpeg", L".jxr",
        L".hdp", L".bmp", L".tiff", L".tif"
};


namespace Config {
  inline BOOL Verbose = false;
  inline BOOL Convert = false;
  inline BOOL SDR = false;

  inline std::wstring FilePath = L"";
  inline std::wstring OutFilePath = L"";

  inline int HDR_bitdepth = 0;
  inline int Quality = 100;
  inline int Speed = 6;
  inline int YUV_sampling = 444;

}

class LogStream
{
public:
  enum class Level { Error, Warning, Info, Normal };

  LogStream(Level level) : level(level)
  {
    if (level == Level::Normal)// && Config::Verbose != true)
      return;

    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    setColor();
    // Print prefix
    switch (level)
    {
    case Level::Error:   std::wcout << L"[ERROR] ";   break;
    case Level::Warning: std::wcout << L"[WARNING] "; break;
    case Level::Info:    std::wcout << L"[INFO] ";    break;
    default: break;
    }
  }

  ~LogStream()
  {
    std::wcout << std::endl;
    if (level != Level::Normal)
      resetColor();
  }

  template<typename T>
  LogStream& operator<<(const T& value)
  {
    std::wcout << value;
    return *this;
  }

private:
  HANDLE hConsole;
  Level level;

  void setColor()
  {
    switch (level)
    {
    case Level::Error:   SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY); break;
    case Level::Warning: SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY); break;
    case Level::Info:    SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY); break;
    default: break;
    }
  }

  void resetColor()
  {
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
  }
};
#define LOG_(severity)   LogStream(severity)
#define IF_VERBOSE() if (!Config::Verbose) ; else
// convenience “stream objects”
#define LOG_E LOG_(LogStream::Level::Error)
#define LOG_W LOG_(LogStream::Level::Warning)
#define LOG_I LOG_(LogStream::Level::Info)
#define LOG IF_VERBOSE() LOG_(LogStream::Level::Normal)

#define LOG_IF(severity, condition)  if (!(condition)) {;} else LOG_( severity)
//#define PLOG_IF(severity, condition)               PLOG_IF_(PLOG_DEFAULT_INSTANCE_ID, severity, condition)

//conditions, yay
#define LOG_VERBOSE_IF(condition) IF_VERBOSE()  LOG_IF(LogStream::Level::Normal, condition)
#define LOG_DEBUG_IF(condition)   IF_VERBOSE()  LOG_IF(LogStream::Level::Normal, condition)
#define LOG_INFO_IF(condition)                  LOG_IF(LogStream::Level::Info, condition)
#define LOG_WARNING_IF(condition)               LOG_IF(LogStream::Level::Warning, condition)
#define LOG_ERROR_IF(condition)                 LOG_IF(LogStream::Level::Error, condition)
//#define LOG_FATAL_IF(condition)                 LOG_IF(plog::fatal, condition)
//#define LOG_NONE_IF(condition)                  LOG_IF(plog::none, condition)

struct SKIF_RegistrySettings {

  // TODO: Rework this whole thing to not only hold a registry path but
  //       also hold the actual current value as well, allowing us to
  //       move away from ugly stuff like
  // 
  //  _registry.uiLastSelectedGame = newValue;
  //  _registry.regKVLastSelectedGame.putData  (_registry.uiLastSelectedGame);
  // 
  //       and instead do things like
  // 
  //  _registry.uiLastSelectedGame.putData (newValue);
  // 
  //       and have it automatically get stored in the registry as well.

  template <class _Tp>
  class KeyValue
  {
    struct KeyDesc {
      HKEY         hKey = HKEY_CURRENT_USER;
      wchar_t    wszSubKey[MAX_PATH] = { };
      wchar_t    wszKeyValue[MAX_PATH] = { };
      DWORD        dwType = REG_NONE;
      DWORD        dwFlags = RRF_RT_ANY;
    };

  public:
    bool         hasData(HKEY* hKey = nullptr);
    _Tp          getData(HKEY* hKey = nullptr);
    bool         putDataMultiSZ(std::vector<std::wstring> in);
    bool         putData(_Tp in)
    {
      if (ERROR_SUCCESS == _SetValue(&in))
        return true;

      return false;
    };

    static KeyValue <typename _Tp>
      MakeKeyValue(const wchar_t* wszSubKey,
        const wchar_t* wszKeyValue,
        HKEY           hKey = HKEY_CURRENT_USER,
        LPDWORD        pdwType = nullptr,
        DWORD          dwFlags = RRF_RT_ANY);

  protected:
  private:
    KeyDesc _desc;

    LSTATUS _SetValue(_Tp* pVal)
    {
      LSTATUS lStat = STATUS_INVALID_DISPOSITION;
      HKEY    hKeyToSet = 0;
      DWORD   dwDisposition = 0;
      DWORD   dwDataSize = 0;

      lStat =
        RegCreateKeyExW(
          _desc.hKey,
          _desc.wszSubKey,
          0x00, nullptr,
          REG_OPTION_NON_VOLATILE,
          KEY_ALL_ACCESS, nullptr,
          &hKeyToSet, &dwDisposition);

      auto type_idx =
        std::type_index(typeid (_Tp));

      if (type_idx == std::type_index(typeid (std::wstring)))
      {
        std::wstring _in = std::wstringstream(*pVal).str();

        _desc.dwType = REG_SZ;
        dwDataSize = (DWORD)_in.size() * sizeof(wchar_t);

        lStat =
          RegSetKeyValueW(hKeyToSet,
            nullptr,
            _desc.wszKeyValue,
            _desc.dwType,
            (LPBYTE)_in.data(), dwDataSize);

        RegCloseKey(hKeyToSet);

        return lStat;
      }

      if (type_idx == std::type_index(typeid (bool)))
      {
        _desc.dwType = REG_BINARY;
        dwDataSize = sizeof(bool);
      }

      if (type_idx == std::type_index(typeid (int)))
      {
        _desc.dwType = REG_DWORD;
        dwDataSize = sizeof(int);
      }

      if (type_idx == std::type_index(typeid (float)))
      {
        _desc.dwFlags = RRF_RT_DWORD;
        _desc.dwType = REG_BINARY;
        dwDataSize = sizeof(float);
      }

      lStat =
        RegSetKeyValueW(hKeyToSet,
          nullptr,
          _desc.wszKeyValue,
          _desc.dwType,
          pVal, dwDataSize);

      RegCloseKey(hKeyToSet);

      return lStat;
    };

    LSTATUS _GetValue(_Tp* pVal, DWORD* pLen = nullptr, HKEY* hKey = nullptr)
    {
      LSTATUS lStat =
        RegGetValueW((hKey != nullptr) ? *hKey : _desc.hKey,
          (hKey != nullptr) ? NULL : _desc.wszSubKey,
          _desc.wszKeyValue,
          _desc.dwFlags,
          &_desc.dwType,
          pVal, pLen);

      return lStat;
    };

    DWORD _SizeOfData(HKEY* hKey = nullptr)
    {
      DWORD len = 0;

      if (ERROR_SUCCESS ==
        _GetValue(nullptr, &len, hKey)
        ) return len;

      return 0;
    };
  };

#define SKIF_MakeRegKeyF   KeyValue <float>       ::MakeKeyValue
#define SKIF_MakeRegKeyB   KeyValue <bool>        ::MakeKeyValue
#define SKIF_MakeRegKeyI   KeyValue <int>         ::MakeKeyValue
#define SKIF_MakeRegKeyWS  KeyValue <std::wstring>::MakeKeyValue
#define SKIF_MakeRegKeyVEC KeyValue <std::vector <std::wstring>>::MakeKeyValue

  KeyValue <std::wstring> regKVPathSpecialK =
    SKIF_MakeRegKeyWS(LR"(SOFTWARE\Kaldaien\Special K\)",
      LR"(Path)");

  std::wstring wsPathSpecialK;

  // Encoder config
  struct {
    CRegKey key;
    int     qhdr_bitdepth = 12;
    int     qquality = 100;
    int     qspeed = 10;
    int     qyuv_sampling = 444;
  } avif;

  struct {
    CRegKey key;
    int     qquality = 100;
  } jxr;

  struct {
    CRegKey key;
    int     qquality = 100;
    int     qspeed = 10;
    int     qhdr_bitdepth = 16;
  } jxl;

  struct {
    CRegKey key;
    int     qhdr_bitdepth = 16;
  } png;

  static SKIF_RegistrySettings& GetInstance(void)
  {
    static SKIF_RegistrySettings instance;
    return instance;
  }

  SKIF_RegistrySettings(SKIF_RegistrySettings const&) = delete; // Delete copy constructor
  SKIF_RegistrySettings(SKIF_RegistrySettings&&) = delete; // Delete move constructor

private:
  SKIF_RegistrySettings(void);
};

