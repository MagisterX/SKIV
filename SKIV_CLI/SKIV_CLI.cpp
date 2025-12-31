// SKIV_CLI.cpp : Этот файл содержит функцию "main". Здесь начинается и заканчивается выполнение программы.
//

#include <iostream>
#include "SKIV_CLI.h"
#include <dxgi1_5.h>
#include <d3d11.h>
#include <filesystem>

#include "image.h"
#include <atlcomcli.h>
#include <Utility.h>
#include <corecrt_io.h>
#include <fcntl.h>
#include <Version.h>
#include "../version.h"

ID3D11Device* SKIF_pd3dDevice = nullptr;
ID3D11DeviceContext* SKIF_pd3dDeviceContext = nullptr;

// Prevent race conditions between asset loading and device init
//
void SKIF_WaitForDeviceInitD3D(void)
{
  while (SKIF_pd3dDevice == nullptr ||
    SKIF_pd3dDeviceContext == nullptr /* ||
    SKIF_g_pSwapChain        == nullptr  */)
  {
    Sleep(10UL);
  }
}

std::wstring join(const std::vector<std::wstring>& v, const std::wstring& sep = L", ")
{
  std::wstring result;
  for (size_t i = 0; i < v.size(); ++i)
  {
    result += v[i];
    if (i + 1 < v.size())
      result += sep;
  }
  return result;
}

CComPtr <ID3D11Device>
SKIF_D3D11_GetDevice(bool bWait)
{
  if (bWait)
    SKIF_WaitForDeviceInitD3D();

  return
    SKIF_pd3dDevice;
}

bool
IsValidPath(std::wstring& Path)
{
  std::filesystem::path p(Path);

  // If path is relative, make it absolute based on current working directory
  if (!p.is_absolute())
    p = std::filesystem::current_path() / p;

  // Handle ./ and ../
  p = std::filesystem::absolute(p);

  if (!std::filesystem::is_regular_file(p))
    return false;

  return true;
}

bool
IsValidOutputFilePath(const std::wstring& path)
{
  if (path.empty())
    return false;

  std::filesystem::path p(path);

  // 1. Check filename part exists
  auto filename = p.filename().wstring();
  if (filename.empty())
    return false;

  // 2. Check forbidden characters (Windows rules)
  if(CheckPathForInvalidChars(filename))
    return false;

  // 3. Windows also forbids filenames ending with dot or space
  if (!filename.empty() &&
    (filename.back() == L'.' || filename.back() == L' '))
    return false;

  return true;
}

bool
ExtensionCheckSDR(std::wstring ext)
{
  for (auto& allowed : allowedExtensions_sdr) {
    if (StrEq(ext, allowed))
      return true;
  }
  return false;
}

bool
ExtensionCheckHDR(std::wstring ext)
{
  for (auto& allowed : allowedExtensions_hdr) {
    if (StrEq(ext, allowed))
      return true;
  }
  return false;
}

int
ExtensionCheck()
{
  //some logic to abort on wrong input
  //won't handle double format like .png until we decode image though
  std::filesystem::path p(Config::FilePath);
  std::wstring extIn = p.extension().wstring();

  if (extIn.empty()) {
    LOG_E << L"Input File extension was not provided";
    return -7;
  }

  bool isInSDR = ExtensionCheckSDR(extIn);
  bool isInHDR = ExtensionCheckHDR(extIn);
  if (!isInSDR && !isInHDR)
  {
    LOG_E << L"Unsupported input file extension " << extIn;
    return -7;
  }
  p = Config::OutFilePath;
  std::wstring extOut = p.extension().wstring();
  bool isOutSDR = ExtensionCheckSDR(extOut);
  bool isOutHDR = ExtensionCheckHDR(extOut);
  if (!isOutSDR && !isOutHDR)
  {
    LOG_E << L"Unsupported output file extension " << extOut;
    return -7;
  }

  if (isInSDR && isInHDR)
  {
    //handle after decode
    return 0;
  }

  // Input is HDR-only (or considered HDR), it can be converted to HDR,
  // or to SDR if user requested SDR conversions via Config::SDR.
  if (isInHDR && !isInSDR) {
    if (Config::SDR) {
      if (!isOutSDR) {
        LOG_I << L"SDR conversion was requested";
        LOG_E << L"Output file extension " << extOut
          << L" not supported for SDR";
        return -7;
      }
    }
    else {
      if (!isOutHDR) {
        LOG_E << L"Output file extension " << extOut
          << L" not supported for HDR input extension " << extIn;
        return -7;
      }
    }
    return 0;
  }

  // isInSDR branch
  if (isInSDR) {
    // SDR inputs may only be converted to SDR outputs
    if (!ExtensionCheckSDR(extOut)) {
      LOG_E << L"Output file extension " << extOut
        << L" not supported for SDR input extension " << extIn;
      return -7;
    }
    return 0;
  }
  // Should never reach here
  LOG_E << L"Unexpected conversion state";
  return -7;
}

bool
CheckFile(std::wstring& Path)
{
  std::filesystem::path p(Path);

  if (std::filesystem::exists(p))
  {
    Path = p.wstring();
    return true;
  }
  else
    return false;
}

bool boundCheck(int value, int minValue, int maxValue, std::wstring valueName)
{
  if (value < minValue || value > maxValue) {
    LOG_E << valueName << L" must be between " << minValue << L" and " << maxValue
      << L", got: " << Config::Quality;
    return true;
  }
  else return false;
}

int
argCheck()
{
  if (!CheckFile(Config::FilePath))
  {
    LOG_E << L"Image file " << Config::FilePath << L" not found.";
    return -1;
  }

  if (Config::OutFilePath.empty()) {
    LOG_E << L"Output file was not specified.";
    return -2;
  }

  if (CheckFile(Config::OutFilePath))
  {
    LOG_E << L"Output file " << Config::OutFilePath << L" already exist.";
    return -2;
  }

  if (boundCheck(Config::Quality, 0, 100, L"Quality")
    || boundCheck(Config::Speed, 0, 10, L"Speed"))
  {
    return -5;
  }

  std::filesystem::path p(Config::OutFilePath);
  std::wstring ext = p.extension().wstring();
  if (StrEq(ext, L".png")) {
    if (Config::HDR_bitdepth == 0)
      Config::HDR_bitdepth = 16;//set default
    if (Config::HDR_bitdepth < 10 || Config::HDR_bitdepth>16)
    {
      LOG_E << L"HDR_bitdepth for png must be between 10 and 16"
        << L", got: " << Config::HDR_bitdepth;
      return -5;
    }
  }
  else if (StrEq(ext, L".avif")) {
    if (Config::HDR_bitdepth == 0)
      Config::HDR_bitdepth = 12;//set default
    if (Config::HDR_bitdepth != 8
      && Config::HDR_bitdepth != 10
      && Config::HDR_bitdepth != 12)
    {
      LOG_E << L"HDR_bitdepth for avif must be 8, 10 or 12"
        << L", got: " << Config::HDR_bitdepth;
      return -5;
    }
  }

  return ExtensionCheck();
}

void PrintOption(const std::wstring& flags, const std::wstring& desc) {
  std::wcout << L' ' << std::left << std::setw(30) << flags << desc << L"\n";
}

void PrintSubOption(const std::wstring& flags, const std::wstring& desc) {
  std::wcout << L"    " << std::left << std::setw(27) << flags << desc << L"\n";
}

int
printHelp()
{
  std::wcout << L"SKIV_CLI version " << SKIV_CLI_VERSION_STR_W<<L" based on SKIV " << SKIV_VERSION_STR_W << L"\n";
  std::wcout << L"Usage: SKIV_CLI [OPTIONS] <input_file> <output_file>\n\n";
  std::wcout << std::left; // left-align everything
  std::wcout << L"Options:\n";
  PrintOption(L"-c, --convert",            L"enable conversion");
  PrintOption(L"Available output formats:", L" ");
  PrintSubOption(L"hdr", join(allowedExtensions_hdr, L" "));
  PrintSubOption(L"sdr", join(allowedExtensions_sdr, L" "));
  PrintOption(L"--sdr",                    L"save as SDR output (default false)");
  PrintOption(L"-q, --quality <int>",      L"set quality for compression (from 1 to 100) (default 80) (avif, jxr, jxl, jpg, hdp)");
  PrintOption(L"-s, --speed <int>",        L"set speed for compression (from 1 to 10) (default 6) (avif, jxl)");
  PrintOption(L"-b, --hdr_bitdepth <int>", L"set bitdepth for compression");
  PrintSubOption(L"avif",                  L"8, 10, 12 (default 12)");
  PrintSubOption(L"png",                   L"(from 10 to 16) (default 16)");
  PrintOption(L"-p, --pix_fmt <string>",   L"set pixel format");
  PrintSubOption(L"avif",                  L"yuv444, yuv422, yuv420, yuv400 (default yuv444)");
  PrintOption(L"-v, --verbose",            L"enable verbose logging (default false)");
  PrintOption(L"-h, --help",               L"show this message");
  return 1;
}

int ParseIntArg(const wchar_t* arg, int& outValue)
{
  try {
    outValue = std::stoi(arg); // convert string → int
  }
  catch (const std::invalid_argument&) {
    LOG_E << L"Invalid number: " << arg;
    return -3;
  }
  catch (const std::out_of_range&) {
    LOG_E << L"Number out of range: " << arg;
    return -4;
  }

  return 0; // success
}

int CheckNextArg(size_t& i, int argc, LPWSTR* argv, int& value, wchar_t* name)
{
  if (i + 1 < argc) // check that next arg exists
  {     
    return ParseIntArg(argv[++i], value);
  }
  else {
    LOG_E << L"Missing value for " << name<< L" option.";
    return -6;
  }
}

int CheckNextArg(size_t& i, int argc, LPWSTR* argv, PixelFmt& value, wchar_t* name)
{
  if (++i < argc) // check that next arg exists
  {
    if (StrEq(argv[i], L"yuv444"))
      value = PixelFmt::YUV444;
    else if (StrEq(argv[i], L"yuv422"))
      value = PixelFmt::YUV422;
    else if (StrEq(argv[i], L"yuv420"))
      value = PixelFmt::YUV420;
    else if (StrEq(argv[i], L"yuv400"))
      value = PixelFmt::YUV400;
    else if (StrEq(argv[i], L"rgb"))
      value = PixelFmt::RGB;
    else if (StrEq(argv[i], L"rgba"))
      value = PixelFmt::RGBA;
    else {
      LOG_E << "Unknown pixel format: " << argv[i];
      return -3;
    }
    return 0;
  }
  else {
    LOG_E << L"Missing value for " << name << L" option.";
    return -6;
  }
}

int
SKIF_Startup_ProcessAllCmdLineArgs()
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

#ifdef _DEBUG
    // Debug-only command line
    LPCWSTR debugCmd =
      L"SKIV_CLI.exe "
      L"-c "
      L"\"test.jxr\" "
      L"\"test.avif\" "
      L"-p "
      L"yuv40 "
      L"--sdr";

    argv = CommandLineToArgvW(debugCmd, &argc);
#endif

    if (argc == 1 || StrEq(argv[1], L"--Help"))
      return printHelp();

    if (argv == nullptr)
        return -99; // Failed

    for (size_t i = 1; i < argc; i++) {
      if (StrEq(argv[i], L"--convert")
        || StrEq(argv[i], L"-c"))
      {
        Config::Convert = true;
        continue;
      }

      if (StrEq(argv[i], L"--verbose") 
        || StrEq(argv[i], L"-v"))
      {
        Config::Verbose = true;
        continue;
      }

      if (StrEq(argv[i], L"--sdr"))
      {
        Config::SDR = true;
        continue;
      }

      if (StrEq(argv[i], L"--quality")
        || StrEq(argv[i], L"-q"))
      {
        int rc = CheckNextArg(i, argc, argv, Config::Quality, L"quality");
        if (rc != 0) {
          return rc; // propagate error code
        }
        continue;
      }

      if (StrEq(argv[i], L"--speed")
        || StrEq(argv[i], L"-s"))
      {
        int rc = CheckNextArg(i, argc, argv, Config::Speed, L"speed");
        if (rc != 0) {
          return rc; // propagate error code
        }
        continue;
      }

      if (StrEq(argv[i], L"--hdr_bitdepth")
        || StrEq(argv[i], L"-b"))
      {
        int rc = CheckNextArg(i, argc, argv, Config::HDR_bitdepth, L"hdr bitdepth");
        if (rc != 0) {
          return rc; // propagate error code
        }
        continue;
      }

      if (StrEq(argv[i], L"--pix_fmt")
        || StrEq(argv[i], L"-p"))
      {
        int rc = CheckNextArg(i, argc, argv, Config::PixelFormat, L"pixel format");
        if (rc != 0) {
          return rc; // propagate error code
        }
        continue;
      }

      if (IsValidOutputFilePath(std::wstring(argv[i])))
      {
        if (Config::FilePath.empty()) {
          Config::FilePath = std::wstring(argv[i]); // first path
          continue;
        }
        else if (Config::OutFilePath.empty()) {
          Config::OutFilePath = std::wstring(argv[i]);  // second path
          continue;
        }
      }

      LOG_E << "Unexpected argument " << argv[i];
      return -7;

    }
    LocalFree(argv);
    return argCheck();
}

int
SKIF_ConvertImageCLI()
{
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL
        featureLevelArray[4] = {
          D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
          D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0
    };

    UINT createDeviceFlags = 0;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
      createDeviceFlags, featureLevelArray,
      sizeof(featureLevelArray) / sizeof featureLevel,
      D3D11_SDK_VERSION,
      &SKIF_pd3dDevice,
      &featureLevel,
      &SKIF_pd3dDeviceContext)))
    {
      LOG_E << "D3D11CreateDevice failed!";
      return -4;;
    }

    //processing image
    if (LoadLibraryTextureCLI(Config::FilePath, Config::OutFilePath))
    {
      LOG_I << L"Successfully converted image: " << Config::FilePath;
      return 0;
    }
    else
    {
      LOG_E << L"Failed to convert image.";
      return -5;
    }
}

int main()
{
  // Enable UTF-8 output
  SetConsoleOutputCP(CP_UTF8);

  // Make wcout work with UTF-8
  _setmode(_fileno(stdout), _O_U8TEXT);

#ifdef _DEBUG
  LOG_W << L"Debug Build .";
  Config::Debug = true;
#endif // _DEBUG

  std::wstring lmao = GetPathToSK();
  //processing arguments
    int init = SKIF_Startup_ProcessAllCmdLineArgs();
    if (init != 0)
      return init;

#ifdef _DEBUG
    /*LOG_I << L"Test complete .";
    return 101;*/
#endif

    // Start work
    if (Config::Convert)
        return SKIF_ConvertImageCLI();

    LOG_E << L"Unknown error.";
    return -666;
}

/*error codes for sanity check
-1  Input Image file not found
-2  Output image file already exist
-3  Invalid number (when parsing int arg)
-4  out of range (when parsing int arg)
-5  quality (or others) option out of range
-6  no arg after option like quality
-99 unknown error with argv
-666  Unknown error.
*/

// Запуск программы: CTRL+F5 или меню "Отладка" > "Запуск без отладки"
// Отладка программы: F5 или меню "Отладка" > "Запустить отладку"

// Советы по началу работы 
//   1. В окне обозревателя решений можно добавлять файлы и управлять ими.
//   2. В окне Team Explorer можно подключиться к системе управления версиями.
//   3. В окне "Выходные данные" можно просматривать выходные данные сборки и другие сообщения.
//   4. В окне "Список ошибок" можно просматривать ошибки.
//   5. Последовательно выберите пункты меню "Проект" > "Добавить новый элемент", чтобы создать файлы кода, или "Проект" > "Добавить существующий элемент", чтобы добавить в проект существующие файлы кода.
//   6. Чтобы снова открыть этот проект позже, выберите пункты меню "Файл" > "Открыть" > "Проект" и выберите SLN-файл.
