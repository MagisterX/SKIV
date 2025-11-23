//#ifndef NOMINMAX
//#define NOMINMAX
//#endif
#include <string>
#include <iostream>
#include <atlbase.h>
#include "Utility.h"
#include "Image.h"
#include <d3d11.h>
#include "SKIV_CLI.h"
#include <utility>
#include <avif/avif.h>
#include <corecrt_io.h>
#include <wincodec.h>

#define SKIV_DEFAULT_GENERAL_PURPOSE_DECODER ImageDecoder_WIC
//#define SKIV_DEFAULT_GENERAL_PURPOSE_DECODER ImageDecoder_stbi
#define STB_IMAGE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_TGA
#define STBI_ONLY_BMP
#define STBI_ONLY_PSD
#define STBI_ONLY_GIF
#define STBI_ONLY_HDR
//#define STBI_ONLY_PIC
//#define STBI_ONLY_PNM

#ifdef _M_X64
#include <jxl/codestream_header.h>
#include <jxl/decode.h>
#include <jxl/decode_cxx.h>
#include <jxl/resizable_parallel_runner.h>
#include <jxl/resizable_parallel_runner_cxx.h>
#include <jxl/types.h>
#endif
#include <jxl/encode.h>
#include <jxl/encode_cxx.h>
#include <jxl/thread_parallel_runner.h>
#include <jxl/thread_parallel_runner_cxx.h>

#include <stb_image.h>

#include "DirectXTex.h"
#include <utility/DirectXTexEXR.h>
#include <filesystem>



#pragma region Variables
#define SKIV_DEFAULT_GENERAL_PURPOSE_DECODER ImageDecoder_WIC

extern CComPtr <ID3D11Device> SKIF_D3D11_GetDevice(bool bWait = true);

enum ImageDecoder {
  ImageDecoder_None,
  ImageDecoder_WIC,
  ImageDecoder_DDS,
  ImageDecoder_stbi,
  ImageDecoder_JXL,
#ifdef _M_X64
  ImageDecoder_EXR,
#endif
  ImageDecoder_HDR,
  ImageDecoder_UHDR,
  ImageDecoder_AVIF
};

std::wstring           defaultHDRFileExt = L".png";
std::wstring           defaultSDRFileExt = L".png";

thread_local stbi__context::iccp_s SKIV_STBI_ICCP;
thread_local bool                  SKIV_STBI_srgb;
thread_local stbi__context::cicp_s SKIV_STBI_CICP;
thread_local stbi__context::sbit_s SKIV_STBI_SBIT;
thread_local stbi__result_info     SKIV_STBI_ResultInfo;

struct image_s {
  struct file_s {
    std::wstring filename = { }; // Image filename
    std::string filename_utf8 = { };
    std::wstring folder_path = { }; // Parent folder path
    std::string folder_path_utf8 = { };
    std::wstring path = { }; // Image path (full)
    std::string path_utf8 = { };
    uint64_t     size = 0;
    file_s() {};
  } file_info;

  int          bpc = 0;
  int          channels = 0;
  float        width = 0.0f;
  float        height = 0.0f;
  static float zoom;//     = 1.0f; // 1.0f = 100%; max: 5.0f; min: 0.05f
  static
    ImageScaling scaling;//= ImageScaling_Auto;
  ImVec2       uv0 = ImVec2(0, 0);
  ImVec2       uv1 = ImVec2(1, 1);
  ImVec2       avail_size;        // Holds the frame size used (affected by the scaling method)
  ImVec2       avail_size_cache;  // Holds a cached value used to determine if avail_size needs recalculating

  bool         is_hdr = false;
  bool         is_dds = false;

  CComPtr <ID3D11ShaderResourceView>  pRawTexSRV;
  CComPtr <ID3D11UnorderedAccessView> pGamutCoverageUAV;
  CComPtr <ID3D11ShaderResourceView>  pGamutCoverageSRV;

  struct light_info_s {
    float max_cll = 0.0f;
    char  max_cll_name = '?';
    float max_nits = 0.0f;
    float min_nits = 0.0f;
    float avg_nits = 0.0f;
    float p99_nits = 0.0f;
    bool  isHDR = false;
  } light_info;

  struct gamut_info_s {
    struct pixel_samples_s {
      uint32_t rec_709;
      uint32_t rec_2020;
      uint32_t dci_p3;
      uint32_t ap1;
      uint32_t ap0;
      uint32_t undefined;
      uint32_t total;

      float getPercentRec709(void) const;
      float getPercentRec2020(void) const;
      float getPercentDCIP3(void) const;
      float getPercentAP1(void) const;
      float getPercentAP0(void) const;
      float getPercentUndefined(void) const;
    } pixel_counts;
  } colorimetry;

  // copy assignment
  image_s& operator= (const image_s other) noexcept
  {
    file_info = other.file_info;
    bpc = other.bpc;
    channels = other.channels;
    width = other.width;
    height = other.height;
    zoom = other.zoom;
    scaling = other.scaling;
    uv0 = other.uv0;
    uv1 = other.uv1;
    avail_size = other.avail_size;
    avail_size_cache = other.avail_size_cache;
    pRawTexSRV.p = other.pRawTexSRV.p;
    pGamutCoverageSRV.p = other.pGamutCoverageSRV.p;
    pGamutCoverageUAV.p = other.pGamutCoverageUAV.p;
    is_hdr = other.is_hdr;
    is_dds = other.is_dds;
    light_info = other.light_info;
    colorimetry = other.colorimetry;
    return *this;
  }

  void reset(void)
  {
    file_info = { };
    bpc = 0;
    channels = 0;
    width = 0.0f;
    height = 0.0f;
    //zoom                = 1.0f;
    //scaling             = ImageScaling_Auto;
    uv0 = ImVec2(0, 0);
    uv1 = ImVec2(1, 1);
    avail_size = { };
    avail_size_cache = { };
    pRawTexSRV.p = nullptr;
    pGamutCoverageSRV.p = nullptr;
    pGamutCoverageUAV.p = nullptr;
    is_hdr = false;
    is_dds = false;
    light_info = { };
    colorimetry = { };
  }
};

float        image_s::zoom = 1.0f;
ImageScaling image_s::scaling = ImageScaling_Auto;

float
image_s::gamut_info_s::pixel_samples_s::getPercentRec709(void) const
{
  return (total == 0 || rec_709 == 0) ? 0.0f : 100.0f *
    static_cast <float> (static_cast <double> (rec_709) /
      static_cast <double> (total));
}

float
image_s::gamut_info_s::pixel_samples_s::getPercentRec2020(void) const
{
  return (total == 0 || rec_2020 == 0) ? 0.0f : 100.0f *
    static_cast <float> (static_cast <double> (rec_2020) /
      static_cast <double> (total));
};

float
image_s::gamut_info_s::pixel_samples_s::getPercentDCIP3(void) const
{
  return (total == 0 || dci_p3 == 0) ? 0.0f : 100.0f *
    static_cast <float> (static_cast <double> (dci_p3) /
      static_cast <double> (total));
};

float
image_s::gamut_info_s::pixel_samples_s::getPercentAP1(void) const
{
  return (total == 0 || ap1 == 0) ? 0.0f : 100.0f *
    static_cast <float> (static_cast <double> (ap1) /
      static_cast <double> (total));
};

float
image_s::gamut_info_s::pixel_samples_s::getPercentAP0(void) const
{
  return (total == 0 || ap0 == 0) ? 0.0f : 100.0f *
    static_cast <float> (static_cast <double> (ap0) /
      static_cast <double> (total));
};

float
image_s::gamut_info_s::pixel_samples_s::getPercentUndefined(void) const
{
  return (total == 0 || undefined == 0) ? 0.0f : 100.0f *
    static_cast <float> (static_cast <double> (undefined) /
      static_cast <double> (total));
};

#if (defined _M_IX86) || (defined _M_X64)
# define SK_PNG_GetUint32(x)                    _byteswap_ulong (x)
# define SK_PNG_SetUint32(x,y)              x = _byteswap_ulong (y)
# define SK_PNG_DeclareUint32(x,y) uint32_t x = SK_PNG_SetUint32((x),(y))
#else
# define SK_PNG_GetUint32(x)                    (x)
# define SK_PNG_SetUint32(x,y)              x = (y)
# define SK_PNG_DeclareUint32(x,y) uint32_t x = SK_PNG_SetUint32((x),(y))
#endif

struct SK_PNG_HDR_cHRM_Payload
{
  SK_PNG_DeclareUint32(white_x, 31270);
  SK_PNG_DeclareUint32(white_y, 32900);
  SK_PNG_DeclareUint32(red_x, 70800);
  SK_PNG_DeclareUint32(red_y, 29200);
  SK_PNG_DeclareUint32(green_x, 17000);
  SK_PNG_DeclareUint32(green_y, 79700);
  SK_PNG_DeclareUint32(blue_x, 13100);
  SK_PNG_DeclareUint32(blue_y, 04600);
};

struct SK_PNG_HDR_sBIT_Payload
{
  uint8_t red_bits = 10; // 12 if source was scRGB (compression optimization)
  uint8_t green_bits = 10; // 12 if source was scRGB (compression optimization)
  uint8_t blue_bits = 10; // 12 if source was scRGB (compression optimization)
};

struct SK_PNG_HDR_mDCv_Payload
{
  struct {
    SK_PNG_DeclareUint32(red_x, 35400); // 0.708 / 0.00002
    SK_PNG_DeclareUint32(red_y, 14600); // 0.292 / 0.00002
    SK_PNG_DeclareUint32(green_x, 8500); // 0.17  / 0.00002
    SK_PNG_DeclareUint32(green_y, 39850); // 0.797 / 0.00002
    SK_PNG_DeclareUint32(blue_x, 6550); // 0.131 / 0.00002
    SK_PNG_DeclareUint32(blue_y, 2300); // 0.046 / 0.00002
  } primaries;

  struct {
    SK_PNG_DeclareUint32(x, 15635); // 0.3127 / 0.00002
    SK_PNG_DeclareUint32(y, 16450); // 0.3290 / 0.00002
  } white_point;

  // The only real data we need to fill-in
  struct {
    SK_PNG_DeclareUint32(maximum, 10000000); // 1000.0 cd/m^2
    SK_PNG_DeclareUint32(minimum, 1);        // 0.0001 cd/m^2
  } luminance;
};

struct SK_PNG_HDR_cLLi_Payload
{
  SK_PNG_DeclareUint32(max_cll, 10000000); // 1000 / 0.0001
  SK_PNG_DeclareUint32(max_fall, 2500000); //  250 / 0.0001
};

/* This is for compression type. PNG 1.0-1.2 only define the single type. */
constexpr uint8_t PNG_COMPRESSION_TYPE_BASE = 0; /* Deflate method 8, 32K window */
#define PNG_COMPRESSION_TYPE_DEFAULT PNG_COMPRESSION_TYPE_BASE

//
// ICC Profile for tonemapping comes courtesy of ledoge
//
//   https://github.com/ledoge/jxr_to_png
//
struct SK_PNG_HDR_iCCP_Payload
{
  char          profile_name[20] = "RGB_D65_202_Rel_PeQ";
  uint8_t       compression_type = PNG_COMPRESSION_TYPE_DEFAULT;

  unsigned char profile_data[2178] = {
  0x78, 0x9C, 0xED, 0x97, 0x79, 0x58, 0x13, 0x67, 0x1E, 0xC7, 0x47, 0x50,
  0x59, 0x95, 0x2A, 0xAC, 0xED, 0xB6, 0x8B, 0xA8, 0x54, 0x20, 0x20, 0x42,
  0xE5, 0xF4, 0x00, 0x51, 0x40, 0x05, 0xAF, 0x6A, 0x04, 0x51, 0x6E, 0x84,
  0x70, 0xAF, 0x20, 0x24, 0xDC, 0x87, 0x0C, 0xA8, 0x88, 0x20, 0x09, 0x90,
  0x04, 0x12, 0x24, 0x24, 0x90, 0x03, 0x82, 0xA0, 0x41, 0x08, 0x24, 0x41,
  0x2E, 0x21, 0x01, 0x12, 0x83, 0x4A, 0x10, 0xA9, 0x56, 0xB7, 0x8A, 0xE0,
  0xAD, 0x21, 0xE0, 0xB1, 0x6B, 0x31, 0x3B, 0x49, 0x74, 0x09, 0x6D, 0xD7,
  0x3E, 0xCF, 0x3E, 0xFD, 0xAF, 0x4E, 0x3E, 0xF3, 0xBC, 0xBF, 0x79, 0xBF,
  0xEF, 0xBC, 0x33, 0x9F, 0xC9, 0xFC, 0x31, 0x2F, 0x00, 0xE8, 0xBC, 0x8D,
  0x4A, 0x3E, 0x62, 0x30, 0xD7, 0x09, 0x00, 0xA2, 0x63, 0xE2, 0x91, 0xEE,
  0x6E, 0x2E, 0x06, 0x7B, 0x82, 0x82, 0x0D, 0xB4, 0x46, 0x01, 0x6D, 0x60,
  0x0E, 0xA0, 0xDC, 0x82, 0x10, 0xA8, 0x58, 0x67, 0x38, 0x7C, 0x8F, 0xEA,
  0xE8, 0x57, 0x1B, 0x34, 0xEA, 0xF5, 0xB0, 0x6A, 0xAC, 0xC4, 0x42, 0x31,
  0xD7, 0xF2, 0x84, 0x9D, 0x68, 0xBD, 0xA9, 0xD6, 0x43, 0xEB, 0x16, 0xE5,
  0xBD, 0xFC, 0xC6, 0xD2, 0xFC, 0xF8, 0xFF, 0x38, 0xEF, 0xE3, 0xB6, 0x30,
  0x24, 0x14, 0x85, 0x80, 0xDA, 0x9F, 0xA1, 0x7D, 0x1B, 0x22, 0x16, 0x19,
  0x0F, 0x4D, 0xE9, 0x04, 0xD5, 0x46, 0x49, 0xF1, 0xB1, 0x8A, 0x3A, 0x04,
  0xAA, 0xBF, 0x44, 0x44, 0x04, 0x41, 0xED, 0x9C, 0x64, 0xA8, 0x36, 0x47,
  0x44, 0x22, 0x62, 0xA1, 0x9A, 0x06, 0xD5, 0xDA, 0x48, 0x2F, 0x6F, 0x1F,
  0xA8, 0x66, 0x29, 0xC6, 0x84, 0xAB, 0xEA, 0x1E, 0x45, 0x1D, 0xAC, 0xAA,
  0x47, 0x14, 0xB5, 0xB3, 0xB5, 0x8B, 0x25, 0x54, 0x3F, 0x03, 0x80, 0xC5,
  0x97, 0x5C, 0xAC, 0x9D, 0xA1, 0x5A, 0xA7, 0x06, 0xEA, 0x87, 0x47, 0x1F,
  0x49, 0x50, 0x5C, 0xF7, 0x83, 0x03, 0xA0, 0x1D, 0x1A, 0xE3, 0xE9, 0x01,
  0xB5, 0x30, 0x68, 0xD7, 0x07, 0xDC, 0x01, 0x37, 0xC0, 0x05, 0x08, 0x04,
  0xB6, 0x01, 0xEB, 0x00, 0x3B, 0xA8, 0xB5, 0x06, 0x2C, 0xA1, 0x3D, 0x10,
  0xEA, 0x0F, 0x05, 0x8E, 0x40, 0x2D, 0x1C, 0x6A, 0xF7, 0x43, 0xCF, 0xEC,
  0xB7, 0xE7, 0x98, 0xAF, 0x9C, 0x63, 0x2B, 0xF4, 0x83, 0xAE, 0x06, 0xDD,
  0x8A, 0x81, 0x6A, 0xC8, 0xCC, 0x73, 0x42, 0x85, 0xD9, 0x58, 0xAB, 0xCE,
  0xD2, 0x86, 0x5C, 0xE7, 0xDD, 0x91, 0xCB, 0x27, 0xCD, 0x00, 0x40, 0xAB,
  0x18, 0x00, 0xA6, 0x0B, 0xE5, 0xF2, 0x77, 0x54, 0xB9, 0x7C, 0x9A, 0x0A,
  0x00, 0x9A, 0xB7, 0x01, 0xA0, 0x33, 0x4B, 0xE5, 0x0B, 0x00, 0x0B, 0x74,
  0x80, 0x39, 0x33, 0x73, 0xD5, 0x45, 0x00, 0x80, 0xDB, 0x51, 0xB9, 0x5C,
  0x9E, 0x3D, 0xD3, 0x67, 0x16, 0x09, 0xF5, 0x8F, 0x42, 0xF3, 0xD4, 0xCF,
  0xF4, 0x19, 0x68, 0x01, 0xC0, 0xA2, 0xF3, 0x00, 0x70, 0x65, 0x69, 0x74,
  0x58, 0xBC, 0x95, 0xA2, 0x47, 0x53, 0x73, 0x81, 0xEA, 0x6E, 0x7F, 0xF1,
  0x2F, 0xFE, 0xEA, 0x78, 0x8E, 0x86, 0xE6, 0xDC, 0x79, 0xF3, 0xB5, 0xFE,
  0xB2, 0x60, 0xE1, 0x22, 0xED, 0x2F, 0x16, 0x2F, 0xD1, 0xD1, 0xFD, 0xEB,
  0xD2, 0x2F, 0xBF, 0xFA, 0xDB, 0xD7, 0xDF, 0xFC, 0x5D, 0x6F, 0x99, 0xFE,
  0xF2, 0x15, 0x2B, 0x0D, 0xBE, 0x5D, 0x65, 0x68, 0x64, 0x0C, 0x33, 0x31,
  0x5D, 0x6D, 0xB6, 0xC6, 0xDC, 0xE2, 0xBB, 0xB5, 0x96, 0x56, 0xD6, 0x36,
  0xB6, 0x76, 0xEB, 0xD6, 0x6F, 0xD8, 0x68, 0xEF, 0xB0, 0xC9, 0x71, 0xF3,
  0x16, 0x27, 0x67, 0x97, 0xAD, 0xDB, 0xB6, 0xBB, 0xBA, 0xED, 0xD8, 0xB9,
  0x6B, 0xF7, 0x9E, 0xEF, 0xF7, 0xEE, 0x83, 0xEF, 0x77, 0xF7, 0x38, 0xE0,
  0x79, 0xF0, 0x10, 0x74, 0x6F, 0xBE, 0x7E, 0xFE, 0x01, 0x81, 0x87, 0x83,
  0x82, 0x11, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xFF, 0x38, 0x12,
  0x1D, 0x73, 0x34, 0x36, 0x0E, 0x89, 0x8A, 0x4F, 0x48, 0x4C, 0x4A, 0x4E,
  0x49, 0x4D, 0x4B, 0xCF, 0x38, 0x96, 0x09, 0x66, 0x65, 0x1F, 0x3F, 0x71,
  0x32, 0xE7, 0x54, 0xEE, 0xE9, 0xBC, 0xFC, 0x33, 0x05, 0x68, 0x4C, 0x61,
  0x51, 0x31, 0x16, 0x87, 0x2F, 0x29, 0x25, 0x10, 0xCB, 0xCE, 0x96, 0x93,
  0x2A, 0xC8, 0x94, 0xCA, 0x2A, 0x2A, 0x8D, 0xCE, 0xA8, 0xAE, 0x61, 0xD6,
  0x9E, 0xAB, 0xAB, 0x3F, 0x7F, 0x81, 0xD5, 0x70, 0xB1, 0xB1, 0x89, 0xDD,
  0xDC, 0xC2, 0xE1, 0xF2, 0x5A, 0x2F, 0xB5, 0xB5, 0x77, 0x74, 0x76, 0x5D,
  0xEE, 0xEE, 0xE1, 0x0B, 0x7A, 0xFB, 0xFA, 0x85, 0xA2, 0x2B, 0xE2, 0x81,
  0xAB, 0xD7, 0xAE, 0x0F, 0x4A, 0x86, 0x6E, 0x0C, 0xDF, 0x1C, 0xF9, 0xE1,
  0xD6, 0xED, 0x1F, 0xEF, 0xDC, 0xFD, 0xE7, 0x4F, 0xF7, 0xEE, 0x8F, 0x3E,
  0x18, 0x1B, 0x7F, 0xF8, 0xE8, 0xF1, 0x93, 0xA7, 0xCF, 0x9E, 0xBF, 0x78,
  0x29, 0x9D, 0x90, 0x4D, 0x4E, 0xBD, 0x7A, 0xFD, 0xE6, 0xED, 0xBF, 0xFE,
  0xFD, 0xEE, 0xE7, 0xE9, 0xF7, 0xF2, 0xCF, 0xFE, 0x7F, 0x72, 0x7F, 0x10,
  0x04, 0xB2, 0x32, 0x34, 0x4E, 0x85, 0x2F, 0xAC, 0x70, 0x33, 0x64, 0x1B,
  0xEF, 0xE8, 0x99, 0x1F, 0x7A, 0x41, 0x2F, 0xAE, 0x66, 0x55, 0x22, 0x1D,
  0x36, 0x37, 0x2D, 0x7B, 0x6E, 0x7A, 0xE6, 0xFC, 0xEC, 0xC8, 0xC5, 0xC4,
  0x5D, 0xC6, 0x17, 0x4D, 0x76, 0x76, 0x6B, 0x41, 0x11, 0x52, 0x19, 0xAD,
  0xF0, 0xC1, 0xAE, 0xF4, 0x2D, 0x34, 0x08, 0x4E, 0x35, 0x4E, 0xF3, 0xB6,
  0x25, 0x59, 0xC1, 0xB9, 0xDA, 0x11, 0x75, 0xFA, 0xC8, 0x6A, 0xC3, 0x24,
  0x3A, 0x6C, 0xDF, 0x3A, 0xD6, 0xBE, 0xF5, 0x75, 0x70, 0x07, 0x82, 0xFB,
  0x9E, 0x44, 0xAF, 0xA3, 0x3B, 0x23, 0xCE, 0xEA, 0xC7, 0x51, 0x57, 0x25,
  0xD2, 0x8C, 0x93, 0x69, 0x26, 0xE8, 0x25, 0x62, 0xF4, 0x12, 0x21, 0x5A,
  0xF7, 0x12, 0x46, 0x8F, 0x5C, 0x64, 0x15, 0x87, 0x0F, 0xB1, 0xCF, 0x43,
  0xDB, 0x80, 0xE5, 0xE6, 0x69, 0x95, 0xAB, 0xF9, 0xC0, 0x03, 0x3E, 0x30,
  0xCA, 0x07, 0x7E, 0xE4, 0x03, 0x7D, 0x02, 0x80, 0xD6, 0xAB, 0x17, 0xCD,
  0x8E, 0xD9, 0x57, 0x96, 0xE7, 0x98, 0x43, 0xB4, 0x1C, 0x33, 0x6C, 0x52,
  0xD2, 0x38, 0x66, 0xD8, 0x30, 0x66, 0x54, 0x3B, 0x6E, 0x4A, 0x78, 0x68,
  0x1D, 0xDB, 0x83, 0xF2, 0x26, 0xE7, 0x39, 0x49, 0xF7, 0x13, 0x3F, 0x42,
  0x90, 0xEE, 0x2F, 0x95, 0xBA, 0xE3, 0xA5, 0x07, 0xCE, 0xC8, 0x7C, 0x93,
  0xFA, 0xE2, 0xFD, 0x64, 0xDE, 0xB8, 0x59, 0xF8, 0x60, 0x65, 0x3E, 0x45,
  0x93, 0x7E, 0x79, 0xAF, 0x10, 0x29, 0x1A, 0x27, 0xB2, 0x34, 0x4E, 0x1E,
  0x9B, 0x9B, 0x1F, 0xA1, 0x5D, 0xB9, 0xC3, 0xA8, 0x19, 0xB6, 0x83, 0xAF,
  0xF0, 0x52, 0x29, 0xCF, 0xCB, 0x3C, 0x3E, 0x0F, 0x04, 0xB5, 0x72, 0xA2,
  0x74, 0xCA, 0x77, 0xC3, 0x2E, 0x9A, 0xAA, 0x2B, 0xAF, 0x0C, 0xC4, 0x19,
  0x1C, 0x2E, 0xFA, 0x36, 0x3C, 0x0D, 0x96, 0xE1, 0x63, 0x57, 0x61, 0x05,
  0xE7, 0xA9, 0x29, 0x6F, 0x64, 0xED, 0xB3, 0xAF, 0x87, 0x6F, 0x26, 0xB8,
  0xEF, 0x4D, 0xF2, 0x8E, 0x9D, 0xAD, 0xAC, 0x23, 0x46, 0xEB, 0x0A, 0xD1,
  0x4B, 0xDB, 0x30, 0xCB, 0xC8, 0x45, 0xD6, 0xC8, 0x12, 0xA5, 0x72, 0x56,
  0xB9, 0x79, 0xFA, 0x27, 0x94, 0xCB, 0xFE, 0x78, 0xE5, 0x2F, 0x2A, 0x4E,
  0x2F, 0x26, 0xE7, 0x2C, 0xA9, 0x8A, 0xFD, 0xBA, 0x16, 0xBE, 0x86, 0xBB,
  0x66, 0xB7, 0x60, 0x41, 0x18, 0x6B, 0x19, 0xB2, 0xC6, 0x10, 0xF2, 0xD2,
  0x25, 0xE6, 0xEB, 0x96, 0xE5, 0x2E, 0x2D, 0x47, 0x2E, 0xA3, 0xBB, 0x5B,
  0x34, 0x9B, 0xEF, 0xE1, 0x2F, 0x08, 0xBB, 0xF0, 0x21, 0x32, 0x49, 0x27,
  0x9A, 0xA4, 0x97, 0x98, 0x82, 0xA0, 0xC5, 0x99, 0x80, 0x8D, 0x34, 0x5B,
  0x8F, 0xD6, 0xC5, 0x91, 0xF5, 0xFA, 0x28, 0xA5, 0xB2, 0xFB, 0xF7, 0x17,
  0xDD, 0xF7, 0x5E, 0xF0, 0xD8, 0x5F, 0xE6, 0xE9, 0x97, 0xE2, 0x9B, 0xB4,
  0x3B, 0xAA, 0x62, 0x39, 0x92, 0x66, 0x98, 0x48, 0x83, 0x41, 0xCA, 0x18,
  0xBD, 0x01, 0xCC, 0x32, 0x11, 0x46, 0xBF, 0xAD, 0xD0, 0x88, 0x52, 0xBC,
  0x01, 0x59, 0x12, 0xEE, 0x90, 0x8F, 0x81, 0x94, 0x2D, 0xD4, 0x94, 0xEF,
  0xF0, 0x81, 0x7E, 0xC1, 0x1C, 0x5A, 0xEF, 0xF2, 0x98, 0xE6, 0xA3, 0xF0,
  0xDF, 0x50, 0x36, 0x3E, 0xF7, 0x69, 0xE5, 0x09, 0xCF, 0xDF, 0x57, 0xB6,
  0x6C, 0xAE, 0xB4, 0x6A, 0xAE, 0xB0, 0x6A, 0x39, 0x65, 0xC7, 0x0B, 0x71,
  0xEA, 0xDE, 0x78, 0x48, 0xA4, 0x1B, 0xD5, 0xB0, 0x1C, 0x55, 0x63, 0x94,
  0xC4, 0x80, 0x59, 0x37, 0x56, 0x59, 0x37, 0x91, 0x6D, 0x9A, 0x72, 0xD7,
  0x73, 0x42, 0x9D, 0x2F, 0xDB, 0x7B, 0x09, 0xA1, 0x68, 0x85, 0x2A, 0x72,
  0xA4, 0x32, 0x37, 0x53, 0xE9, 0x9B, 0xE9, 0x18, 0x67, 0xE6, 0x91, 0x5D,
  0x6C, 0x27, 0xFF, 0xCB, 0x5F, 0x45, 0x9F, 0x5F, 0x19, 0x0F, 0x45, 0x74,
  0x58, 0x40, 0x0A, 0x2F, 0x20, 0xA5, 0x25, 0x20, 0xAD, 0xEA, 0x30, 0x08,
  0x86, 0x62, 0xDC, 0x15, 0x2F, 0x0C, 0xC3, 0x18, 0xEA, 0x87, 0x94, 0xB1,
  0x0E, 0xD7, 0xB1, 0x0E, 0x03, 0xD8, 0x4D, 0x5D, 0x38, 0x67, 0x6A, 0x09,
  0x3C, 0xB1, 0x0C, 0xE5, 0x58, 0x50, 0x64, 0x97, 0x4D, 0xB2, 0x48, 0xAF,
  0x5A, 0x2D, 0xD0, 0x18, 0x13, 0x68, 0x3C, 0x10, 0x68, 0xDE, 0xED, 0x9D,
  0x2F, 0xEC, 0x5D, 0x42, 0xEF, 0x33, 0x3D, 0xDA, 0x12, 0x07, 0x2F, 0xCB,
  0xDF, 0x7C, 0x0A, 0x52, 0x36, 0x66, 0x8F, 0x19, 0x37, 0x29, 0xB9, 0x38,
  0x0E, 0x3B, 0x37, 0x6E, 0x46, 0x7C, 0x64, 0xAB, 0x52, 0x76, 0x9E, 0xA5,
  0xEC, 0xFE, 0x51, 0xD9, 0x2F, 0xA9, 0x2F, 0x61, 0xB6, 0xB2, 0xCF, 0x2C,
  0xE5, 0xE0, 0xA1, 0xEE, 0xE0, 0xA1, 0xCE, 0xE0, 0x21, 0x66, 0xC8, 0xF0,
  0x89, 0xC8, 0x5B, 0x9E, 0xF1, 0x23, 0x46, 0x49, 0x6C, 0x58, 0x72, 0xAD,
  0x49, 0x32, 0xC3, 0x04, 0x21, 0xE9, 0x41, 0x48, 0x3A, 0x11, 0x12, 0x66,
  0xE8, 0x8D, 0x93, 0x51, 0x3F, 0x78, 0x26, 0x8C, 0x18, 0xFF, 0x37, 0x8A,
  0x10, 0x09, 0x22, 0x44, 0xDD, 0x11, 0x57, 0xEA, 0xA2, 0xC4, 0xB9, 0x31,
  0x83, 0x5E, 0xC9, 0x83, 0x26, 0x29, 0x8D, 0x26, 0x29, 0x4C, 0x93, 0x14,
  0x86, 0x49, 0x1A, 0xEB, 0x6A, 0x1A, 0x4B, 0x94, 0xD6, 0xD0, 0x9C, 0xDE,
  0x88, 0xCD, 0xE4, 0x86, 0xE4, 0x74, 0x5A, 0x82, 0x75, 0xE6, 0xE9, 0x8C,
  0xD5, 0xA9, 0x74, 0x53, 0x72, 0xE2, 0x6D, 0x72, 0xE2, 0x08, 0x39, 0x51,
  0x48, 0x49, 0xA9, 0xAF, 0x04, 0x41, 0x3A, 0x76, 0x3B, 0x8E, 0x68, 0x9F,
  0x53, 0x61, 0x99, 0x51, 0x65, 0x26, 0x34, 0x7F, 0x2C, 0x34, 0x7F, 0x24,
  0x34, 0xBF, 0x27, 0xFC, 0x4E, 0x2C, 0xB4, 0x65, 0x8A, 0x5C, 0x51, 0xBC,
  0x64, 0x0F, 0x52, 0xC1, 0x96, 0xDC, 0x32, 0xAB, 0x87, 0x6B, 0x5A, 0x3E,
  0xC2, 0x7E, 0x68, 0x7E, 0xFE, 0xE1, 0xDA, 0xB3, 0x8F, 0x37, 0xC4, 0xF1,
  0x13, 0x7C, 0x28, 0xF9, 0xCE, 0x52, 0x0F, 0xA2, 0x1A, 0x04, 0xE9, 0x01,
  0xFC, 0xC4, 0xC1, 0x82, 0x49, 0xFF, 0x64, 0x85, 0xB2, 0x42, 0x53, 0x1D,
  0xAC, 0xCC, 0xB7, 0x68, 0xD2, 0x5F, 0xA1, 0x4C, 0x92, 0x8A, 0x49, 0x52,
  0x11, 0x49, 0x7A, 0x99, 0x24, 0xAD, 0x25, 0x4F, 0x66, 0xD1, 0xDE, 0x6D,
  0xC7, 0x76, 0x6C, 0x3C, 0x59, 0xBF, 0x36, 0xA3, 0xDA, 0x8C, 0xF4, 0x52,
  0x4C, 0x7A, 0x79, 0x85, 0xF4, 0xF2, 0x72, 0x85, 0x32, 0xA2, 0xAB, 0x45,
  0xE4, 0x67, 0x57, 0xC9, 0xCF, 0xC4, 0xE4, 0xE7, 0x3D, 0xE4, 0xE7, 0x75,
  0x95, 0xD2, 0x6C, 0xC6, 0x5B, 0x57, 0x5C, 0xBB, 0xFD, 0xC9, 0x7A, 0x4B,
  0x28, 0x62, 0xDC, 0xBB, 0xC5, 0xB8, 0x37, 0xC2, 0xB8, 0x2F, 0xAE, 0x1E,
  0x6D, 0xAC, 0x19, 0xCF, 0xAD, 0x7B, 0xB1, 0x9B, 0xC8, 0x71, 0xCC, 0xAD,
  0xB5, 0x3A, 0xC6, 0x58, 0xC3, 0x69, 0x93, 0x72, 0xDA, 0x5E, 0x70, 0xDA,
  0x7E, 0xE2, 0xB4, 0x77, 0x73, 0x3B, 0x09, 0xAD, 0x7D, 0xFE, 0xD5, 0x4C,
  0xD7, 0x42, 0xEA, 0xFA, 0x6C, 0xAA, 0x85, 0x04, 0x25, 0x93, 0xA0, 0x26,
  0x24, 0xA8, 0x27, 0x92, 0xF8, 0x9B, 0x92, 0xA4, 0xA6, 0x21, 0x10, 0xEC,
  0xC9, 0xF7, 0xA6, 0x61, 0xB7, 0xE5, 0x97, 0xDB, 0x3E, 0x71, 0xE8, 0x51,
  0xD2, 0xFD, 0x64, 0x53, 0xD7, 0x53, 0x47, 0xCE, 0xD3, 0x2D, 0xD4, 0x67,
  0xAE, 0xA8, 0xFE, 0x34, 0xBF, 0xAA, 0x82, 0xAD, 0x13, 0x07, 0x89, 0x33,
  0x1C, 0x22, 0x4C, 0x1C, 0xC2, 0xCB, 0x7C, 0x0A, 0xA6, 0x0E, 0x27, 0xF7,
  0x27, 0xF9, 0xCB, 0x7C, 0x71, 0xEA, 0x4C, 0xFA, 0x62, 0x27, 0xFD, 0x8A,
  0x26, 0x03, 0xF2, 0x5E, 0x85, 0xA4, 0x70, 0x06, 0x28, 0x9C, 0x01, 0xB2,
  0x92, 0x72, 0xEE, 0x00, 0xAE, 0xF5, 0x6A, 0x66, 0x97, 0xC4, 0xAB, 0x92,
  0xED, 0x92, 0x57, 0x6B, 0xF3, 0xA9, 0x48, 0x4C, 0x51, 0x42, 0xE6, 0x8A,
  0xCB, 0xB9, 0x62, 0x28, 0x02, 0xBB, 0x06, 0xBD, 0x2A, 0x9B, 0xB6, 0x42,
  0x51, 0x6B, 0x3F, 0x45, 0x09, 0xB9, 0x55, 0x48, 0xBA, 0x24, 0xC4, 0xB7,
  0x8B, 0xB2, 0xBA, 0xAF, 0x7A, 0x53, 0x1B, 0xB7, 0xE5, 0x33, 0x6D, 0xBA,
  0x3B, 0xAA, 0xBA, 0x3B, 0x2A, 0x95, 0x90, 0x7B, 0x3A, 0x4B, 0xF9, 0x5D,
  0x27, 0x84, 0x82, 0x80, 0x9A, 0xF3, 0x6E, 0x05, 0xD5, 0x76, 0xC3, 0x8C,
  0xEA, 0x0F, 0x54, 0xD3, 0x87, 0xAB, 0x2B, 0x6E, 0x32, 0x0B, 0x6E, 0xB1,
  0x22, 0x9A, 0x29, 0x70, 0x3C, 0xC5, 0x5E, 0x86, 0x94, 0x2B, 0x99, 0x96,
  0x21, 0xA7, 0x64, 0xA8, 0xBB, 0xB2, 0x44, 0xAE, 0x0C, 0x04, 0x07, 0x73,
  0x83, 0x99, 0xC5, 0x6E, 0x53, 0x41, 0x65, 0x6A, 0x10, 0x5F, 0x05, 0x97,
  0xBE, 0x42, 0x60, 0xDE, 0x44, 0xA6, 0x8A, 0xD3, 0x03, 0x27, 0x03, 0x70,
  0xEA, 0x4C, 0x05, 0x60, 0xA7, 0x02, 0x8B, 0xA6, 0x82, 0xF2, 0x5F, 0x87,
  0xA5, 0xF2, 0x3B, 0x4A, 0x95, 0x94, 0x28, 0xC1, 0x09, 0x3A, 0xD0, 0x7D,
  0x1D, 0x99, 0x03, 0x5D, 0x87, 0xE9, 0x0D, 0xDB, 0xF9, 0xED, 0xA5, 0x0A,
  0x3E, 0x11, 0xB5, 0x97, 0x28, 0x99, 0x89, 0x18, 0x0D, 0xDB, 0x05, 0x6D,
  0xA5, 0x4A, 0x4A, 0x94, 0xE0, 0x7A, 0xDB, 0xD0, 0xFD, 0xED, 0xE0, 0x40,
  0x67, 0x10, 0x83, 0xE5, 0xDA, 0xC7, 0x2B, 0xFD, 0x48, 0x49, 0x3F, 0x0F,
  0xD7, 0xCF, 0xC3, 0x88, 0x5A, 0xB3, 0xAE, 0xB7, 0x05, 0x43, 0xD6, 0xD7,
  0x58, 0xA5, 0x6A, 0xE0, 0xAF, 0xB3, 0x0A, 0x07, 0x1B, 0x8E, 0xDF, 0x6C,
  0x0A, 0xAB, 0xAF, 0xDD, 0x75, 0xFF, 0x2C, 0x41, 0x8D, 0xD2, 0xFB, 0x67,
  0xB1, 0xA3, 0xA4, 0xD3, 0xE3, 0x94, 0x58, 0x1E, 0xC9, 0x63, 0x3A, 0xB9,
  0x56, 0x0D, 0xE6, 0x74, 0x4A, 0xF5, 0x74, 0x2A, 0x65, 0x1A, 0x04, 0x6F,
  0x9C, 0x0A, 0x7D, 0x1D, 0x8E, 0x57, 0x03, 0xA7, 0x20, 0xA2, 0xF8, 0x4D,
  0xE4, 0x99, 0xB7, 0x31, 0x69, 0xFD, 0x5C, 0x9C, 0x1A, 0x58, 0x21, 0xB7,
  0x58, 0xC8, 0x2D, 0xB8, 0xC2, 0x03, 0x07, 0x5B, 0x11, 0xFF, 0x5F, 0x24,
  0xE4, 0xE2, 0xD4, 0x50, 0x44, 0x22, 0x28, 0xE2, 0x82, 0x83, 0x3C, 0x84,
  0x90, 0x83, 0x53, 0x03, 0x2B, 0xE2, 0x14, 0x8B, 0x38, 0x05, 0x62, 0x0E,
  0x28, 0xE1, 0x85, 0x88, 0x9B, 0x70, 0x6A, 0x60, 0xC5, 0xEC, 0xE2, 0x01,
  0x76, 0xC1, 0x35, 0x76, 0xD6, 0x8D, 0x96, 0xD0, 0xA1, 0x3A, 0xDC, 0x6C,
  0x8A, 0x6F, 0xD4, 0xA1, 0x87, 0xEB, 0x8F, 0xDF, 0xBE, 0x10, 0x39, 0x46,
  0xC0, 0xAB, 0x81, 0x1B, 0x23, 0x60, 0xC7, 0x88, 0x85, 0xE3, 0x65, 0xB9,
  0x8F, 0x49, 0xC8, 0xF7, 0xE9, 0x25, 0x6A, 0xE0, 0x95, 0x60, 0xDF, 0x67,
  0xA0, 0xE5, 0xD0, 0x77, 0xC8, 0xE7, 0x4F, 0xD1, 0xCF, 0xFE, 0x7F, 0x66,
  0xFF, 0x68, 0x17, 0x67, 0xE5, 0x7A, 0x56, 0x53, 0x53, 0xB5, 0xA8, 0xFD,
  0xC5, 0x6A, 0x15, 0x88, 0x0D, 0x42, 0x06, 0xA9, 0xAF, 0x5D, 0x7F, 0xEF,
  0xF8, 0x3F, 0x0B, 0x10, 0x3B, 0xD9
  };
};



avifImageCreate_pfn            SK_avifImageCreate = nullptr;
avifRGBImageSetDefaults_pfn    SK_avifRGBImageSetDefaults = nullptr;
avifRGBImageAllocatePixels_pfn SK_avifRGBImageAllocatePixels = nullptr;
avifImageRGBToYUV_pfn          SK_avifImageRGBToYUV = nullptr;
avifImageYUVToRGB_pfn          SK_avifImageYUVToRGB = nullptr;
avifImageSetProfileICC_pfn     SK_avifImageSetProfileICC = nullptr;
avifEncoderCreate_pfn          SK_avifEncoderCreate = nullptr;
avifEncoderAddImage_pfn        SK_avifEncoderAddImage = nullptr;
avifEncoderFinish_pfn          SK_avifEncoderFinish = nullptr;
avifImageDestroy_pfn           SK_avifImageDestroy = nullptr;
avifEncoderDestroy_pfn         SK_avifEncoderDestroy = nullptr;
avifRGBImageFreePixels_pfn     SK_avifRGBImageFreePixels = nullptr;

avifDecoderCreate_pfn          SK_avifDecoderCreate = nullptr;
avifDecoderDestroy_pfn         SK_avifDecoderDestroy = nullptr;

avifDecoderRead_pfn            SK_avifDecoderRead = nullptr;
avifDecoderReadMemory_pfn      SK_avifDecoderReadMemory = nullptr;
avifDecoderSetIOMemory_pfn     SK_avifDecoderSetIOMemory = nullptr;
avifDecoderParse_pfn           SK_avifDecoderParse = nullptr;
avifDecoderNextImage_pfn       SK_avifDecoderNextImage = nullptr;

bool
isAVIFEncoderAvailable(void)
{
  static bool init = false;

  static const wchar_t* wszPluginArch =
    SK_RunLHIfBitness(64, LR"(x64\)",
      LR"(x86\)");

  static const wchar_t* wszDownloadURL =
    SK_RunLHIfBitness(64, LR"(https://sk-data.special-k.info/addon/ImageCodecs/libavif/libavif_x64.dll)",
      LR"(https://sk-data.special-k.info/addon/ImageCodecs/libavif/libavif_x86.dll)");

  SKIF_RegistrySettings& _registry =
    SKIF_RegistrySettings::GetInstance();

  std::filesystem::path avif_dll =
    _registry.regKVPathSpecialK.getData();

  std::error_code                          ec;
  if (!std::filesystem::exists(avif_dll, ec))
  {
    avif_dll = std::filesystem::path(GetPathToSK());

    avif_dll /= LR"(My Mods\SpecialK\)";
  }

  avif_dll /=
    LR"(PlugIns\ThirdParty\Image Codecs\libavif\)";

  std::filesystem::create_directories(avif_dll, ec);

  avif_dll /= SK_RunLHIfBitness(64, LR"(libavif_x64.dll)",
    LR"(libavif_x86.dll)");

  if (!std::filesystem::exists(avif_dll, ec))
  {
    SKIF_Util_GetWebResource(wszDownloadURL, avif_dll.wstring());
  }

  HMODULE hModAVIF =
    LoadLibraryW(avif_dll.c_str());

  if (hModAVIF != 0)
  {
    SK_avifImageCreate = (avifImageCreate_pfn)GetProcAddress(hModAVIF, "avifImageCreate");
    SK_avifRGBImageSetDefaults = (avifRGBImageSetDefaults_pfn)GetProcAddress(hModAVIF, "avifRGBImageSetDefaults");
    SK_avifRGBImageAllocatePixels = (avifRGBImageAllocatePixels_pfn)GetProcAddress(hModAVIF, "avifRGBImageAllocatePixels");
    SK_avifImageRGBToYUV = (avifImageRGBToYUV_pfn)GetProcAddress(hModAVIF, "avifImageRGBToYUV");
    SK_avifImageYUVToRGB = (avifImageYUVToRGB_pfn)GetProcAddress(hModAVIF, "avifImageYUVToRGB");
    SK_avifImageSetProfileICC = (avifImageSetProfileICC_pfn)GetProcAddress(hModAVIF, "avifImageSetProfileICC");
    SK_avifEncoderCreate = (avifEncoderCreate_pfn)GetProcAddress(hModAVIF, "avifEncoderCreate");
    SK_avifEncoderAddImage = (avifEncoderAddImage_pfn)GetProcAddress(hModAVIF, "avifEncoderAddImage");
    SK_avifEncoderFinish = (avifEncoderFinish_pfn)GetProcAddress(hModAVIF, "avifEncoderFinish");
    SK_avifImageDestroy = (avifImageDestroy_pfn)GetProcAddress(hModAVIF, "avifImageDestroy");
    SK_avifEncoderDestroy = (avifEncoderDestroy_pfn)GetProcAddress(hModAVIF, "avifEncoderDestroy");
    SK_avifRGBImageFreePixels = (avifRGBImageFreePixels_pfn)GetProcAddress(hModAVIF, "avifRGBImageFreePixels");

    SK_avifDecoderCreate = (avifDecoderCreate_pfn)GetProcAddress(hModAVIF, "avifDecoderCreate");
    SK_avifDecoderDestroy = (avifDecoderDestroy_pfn)GetProcAddress(hModAVIF, "avifDecoderDestroy");

    SK_avifDecoderRead = (avifDecoderRead_pfn)GetProcAddress(hModAVIF, "avifDecoderRead");
    SK_avifDecoderReadMemory = (avifDecoderReadMemory_pfn)GetProcAddress(hModAVIF, "avifDecoderReadMemory");
    SK_avifDecoderSetIOMemory = (avifDecoderSetIOMemory_pfn)GetProcAddress(hModAVIF, "avifDecoderSetIOMemory");
    SK_avifDecoderParse = (avifDecoderParse_pfn)GetProcAddress(hModAVIF, "avifDecoderParse");
    SK_avifDecoderNextImage = (avifDecoderNextImage_pfn)GetProcAddress(hModAVIF, "avifDecoderNextImage");

    init =
      (SK_avifImageCreate != nullptr &&
        SK_avifRGBImageSetDefaults != nullptr &&
        SK_avifRGBImageAllocatePixels != nullptr &&
        SK_avifImageRGBToYUV != nullptr &&
        SK_avifImageSetProfileICC != nullptr &&
        SK_avifEncoderCreate != nullptr &&
        SK_avifEncoderAddImage != nullptr &&
        SK_avifEncoderFinish != nullptr &&
        SK_avifImageDestroy != nullptr &&
        SK_avifEncoderDestroy != nullptr &&
        SK_avifRGBImageFreePixels != nullptr &&
        SK_avifDecoderCreate != nullptr &&
        SK_avifDecoderDestroy != nullptr &&
        SK_avifDecoderRead != nullptr &&
        SK_avifDecoderReadMemory != nullptr &&
        SK_avifImageYUVToRGB != nullptr &&
        SK_avifDecoderSetIOMemory != nullptr &&
        SK_avifDecoderParse != nullptr &&
        SK_avifDecoderNextImage != nullptr);

    // The AVIF DLL is out-of-date, delete it and attempt to reacquire.
    if (!init)
    {
      FreeLibrary(hModAVIF);
      DeleteFileW(avif_dll.c_str());
    }
  }
  return init;
}

void sk_avif_add_icc_to_image(avifImage* img)
{
  if (!SK_avifImageSetProfileICC)
    return;

  // Array size is 4344
  static const uint8_t RGB_D65_202_Rel_PeQ[] = {
    0x00, 0x00, 0x10, 0xf8, 0x6a, 0x78, 0x6c, 0x20, 0x04, 0x40, 0x00, 0x00, 0x6d, 0x6e, 0x74, 0x72,
    0x52, 0x47, 0x42, 0x20, 0x4c, 0x61, 0x62, 0x20, 0x07, 0xe3, 0x00, 0x0c, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x61, 0x63, 0x73, 0x70, 0x41, 0x50, 0x50, 0x4c, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0xf6, 0xd6, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xd3, 0x2d,
    0x6a, 0x78, 0x6c, 0x20, 0x1d, 0x75, 0x49, 0x8e, 0x1a, 0xf4, 0xbb, 0x57, 0x36, 0x0b, 0x8a, 0xef,
    0x18, 0x30, 0x2c, 0x83, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0a, 0x64, 0x65, 0x73, 0x63, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x44,
    0x63, 0x70, 0x72, 0x74, 0x00, 0x00, 0x01, 0x40, 0x00, 0x00, 0x00, 0x24, 0x77, 0x74, 0x70, 0x74,
    0x00, 0x00, 0x01, 0x64, 0x00, 0x00, 0x00, 0x14, 0x63, 0x68, 0x61, 0x64, 0x00, 0x00, 0x01, 0x78,
    0x00, 0x00, 0x00, 0x2c, 0x63, 0x69, 0x63, 0x70, 0x00, 0x00, 0x01, 0xa4, 0x00, 0x00, 0x00, 0x0c,
    0x72, 0x58, 0x59, 0x5a, 0x00, 0x00, 0x01, 0xb0, 0x00, 0x00, 0x00, 0x14, 0x67, 0x58, 0x59, 0x5a,
    0x00, 0x00, 0x01, 0xc4, 0x00, 0x00, 0x00, 0x14, 0x62, 0x58, 0x59, 0x5a, 0x00, 0x00, 0x01, 0xd8,
    0x00, 0x00, 0x00, 0x14, 0x41, 0x32, 0x42, 0x30, 0x00, 0x00, 0x01, 0xec, 0x00, 0x00, 0x0e, 0xbc,
    0x42, 0x32, 0x41, 0x30, 0x00, 0x00, 0x10, 0xa8, 0x00, 0x00, 0x00, 0x50, 0x6d, 0x6c, 0x75, 0x63,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0c, 0x65, 0x6e, 0x55, 0x53,
    0x00, 0x00, 0x00, 0x26, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x52, 0x00, 0x47, 0x00, 0x42, 0x00, 0x5f,
    0x00, 0x44, 0x00, 0x36, 0x00, 0x35, 0x00, 0x5f, 0x00, 0x32, 0x00, 0x30, 0x00, 0x32, 0x00, 0x5f,
    0x00, 0x52, 0x00, 0x65, 0x00, 0x6c, 0x00, 0x5f, 0x00, 0x50, 0x00, 0x65, 0x00, 0x51, 0x00, 0x00,
    0x6d, 0x6c, 0x75, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0c,
    0x65, 0x6e, 0x55, 0x53, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x43, 0x00, 0x43,
    0x00, 0x30, 0x00, 0x00, 0x58, 0x59, 0x5a, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf6, 0xd6,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xd3, 0x2d, 0x73, 0x66, 0x33, 0x32, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x0c, 0x40, 0x00, 0x00, 0x05, 0xdd, 0xff, 0xff, 0xf3, 0x2a, 0x00, 0x00, 0x07, 0x92,
    0x00, 0x00, 0xfd, 0x90, 0xff, 0xff, 0xfb, 0xa3, 0xff, 0xff, 0xfd, 0xa3, 0x00, 0x00, 0x03, 0xdb,
    0x00, 0x00, 0xc0, 0x81, 0x63, 0x69, 0x63, 0x70, 0x00, 0x00, 0x00, 0x00, 0x09, 0x10, 0x00, 0x01,
    0x58, 0x59, 0x5a, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xac, 0x68, 0x00, 0x00, 0x47, 0x6f,
    0xff, 0xff, 0xff, 0x82, 0x58, 0x59, 0x5a, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2a, 0x69,
    0x00, 0x00, 0xac, 0xe3, 0x00, 0x00, 0x07, 0xad, 0x58, 0x59, 0x5a, 0x20, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x20, 0x07, 0x00, 0x00, 0x0b, 0xae, 0x00, 0x00, 0xcc, 0x13, 0x6d, 0x66, 0x74, 0x31,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x09, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03,
    0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13,
    0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
    0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43,
    0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53,
    0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63,
    0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73,
    0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80, 0x81, 0x82, 0x83,
    0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92, 0x93,
    0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3,
    0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3,
    0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3,
    0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3,
    0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3,
    0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3,
    0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, 0x00, 0x01, 0x02, 0x03,
    0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13,
    0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
    0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43,
    0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53,
    0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63,
    0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73,
    0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80, 0x81, 0x82, 0x83,
    0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92, 0x93,
    0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3,
    0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3,
    0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3,
    0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3,
    0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3,
    0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3,
    0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, 0x00, 0x01, 0x02, 0x03,
    0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13,
    0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
    0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43,
    0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53,
    0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63,
    0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73,
    0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80, 0x81, 0x82, 0x83,
    0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92, 0x93,
    0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3,
    0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3,
    0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3,
    0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3,
    0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3,
    0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3,
    0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, 0x00, 0x80, 0x80, 0x00,
    0x81, 0x7d, 0x02, 0x87, 0x67, 0x0a, 0x9e, 0x47, 0x23, 0xb5, 0x25, 0x48, 0xc4, 0x06, 0x65, 0xaf,
    0x1a, 0x71, 0xa8, 0x22, 0x76, 0xa5, 0x26, 0x04, 0x7b, 0x82, 0x04, 0x7c, 0x7f, 0x06, 0x82, 0x69,
    0x0e, 0x99, 0x4a, 0x25, 0xb2, 0x27, 0x49, 0xc3, 0x07, 0x65, 0xaf, 0x1a, 0x72, 0xa8, 0x22, 0x76,
    0xa5, 0x26, 0x1e, 0x5a, 0x93, 0x1f, 0x5b, 0x90, 0x20, 0x62, 0x7a, 0x25, 0x7b, 0x59, 0x34, 0x9d,
    0x31, 0x50, 0xb9, 0x0c, 0x68, 0xac, 0x1c, 0x72, 0xa7, 0x23, 0x77, 0xa5, 0x26, 0x4f, 0x36, 0xb0,
    0x4f, 0x37, 0xac, 0x50, 0x3b, 0x98, 0x52, 0x4c, 0x76, 0x58, 0x6f, 0x49, 0x68, 0x9b, 0x1c, 0x71,
    0xa3, 0x22, 0x76, 0xa4, 0x25, 0x78, 0xa4, 0x27, 0x8e, 0x0f, 0xcd, 0x8e, 0x0f, 0xca, 0x8e, 0x11,
    0xbc, 0x8f, 0x1a, 0x9f, 0x91, 0x31, 0x71, 0x95, 0x64, 0x3a, 0x8a, 0x8e, 0x33, 0x80, 0x9c, 0x2c,
    0x7b, 0xa1, 0x29, 0xc5, 0x00, 0xe4, 0xc5, 0x00, 0xe3, 0xc5, 0x00, 0xdc, 0xc5, 0x00, 0xc8, 0xc6,
    0x00, 0xa4, 0xc7, 0x1a, 0x6d, 0xb5, 0x6e, 0x4f, 0x9a, 0x8a, 0x3d, 0x86, 0x99, 0x30, 0xe5, 0x23,
    0xb4, 0xe5, 0x23, 0xb4, 0xe5, 0x23, 0xb3, 0xe5, 0x23, 0xb1, 0xe5, 0x24, 0xaa, 0xe6, 0x28, 0x98,
    0xe7, 0x32, 0x70, 0xc4, 0x73, 0x59, 0x9f, 0x8a, 0x40, 0xf0, 0x51, 0x99, 0xf0, 0x51, 0x99, 0xf0,
    0x51, 0x99, 0xf0, 0x51, 0x98, 0xf0, 0x51, 0x97, 0xf0, 0x52, 0x95, 0xf0, 0x54, 0x8c, 0xf2, 0x5b,
    0x77, 0xc8, 0x74, 0x5c, 0xf2, 0x59, 0x94, 0xf2, 0x59, 0x94, 0xf2, 0x59, 0x94, 0xf2, 0x59, 0x94,
    0xf2, 0x59, 0x94, 0xf2, 0x5a, 0x93, 0xf2, 0x5a, 0x91, 0xf3, 0x5c, 0x8a, 0xf5, 0x63, 0x79, 0x02,
    0x84, 0x81, 0x02, 0x85, 0x7e, 0x04, 0x8b, 0x68, 0x0c, 0xa1, 0x48, 0x24, 0xb6, 0x26, 0x48, 0xc5,
    0x07, 0x65, 0xaf, 0x1a, 0x71, 0xa8, 0x22, 0x76, 0xa5, 0x26, 0x05, 0x7f, 0x83, 0x05, 0x80, 0x80,
    0x07, 0x86, 0x6a, 0x10, 0x9c, 0x4b, 0x26, 0xb2, 0x28, 0x49, 0xc3, 0x07, 0x65, 0xaf, 0x1a, 0x72,
    0xa8, 0x22, 0x76, 0xa5, 0x26, 0x1f, 0x5f, 0x94, 0x20, 0x60, 0x91, 0x21, 0x67, 0x7b, 0x26, 0x7d,
    0x5a, 0x35, 0x9e, 0x31, 0x50, 0xba, 0x0c, 0x68, 0xac, 0x1c, 0x72, 0xa7, 0x23, 0x77, 0xa5, 0x26,
    0x4f, 0x39, 0xb0, 0x4f, 0x3a, 0xad, 0x50, 0x3e, 0x98, 0x52, 0x4e, 0x77, 0x59, 0x70, 0x49, 0x68,
    0x9b, 0x1c, 0x71, 0xa3, 0x22, 0x76, 0xa4, 0x25, 0x78, 0xa4, 0x27, 0x8e, 0x10, 0xcd, 0x8e, 0x11,
    0xca, 0x8e, 0x13, 0xbd, 0x8f, 0x1b, 0x9f, 0x91, 0x32, 0x72, 0x96, 0x64, 0x3a, 0x8a, 0x8e, 0x33,
    0x81, 0x9c, 0x2c, 0x7c, 0xa1, 0x29, 0xc5, 0x00, 0xe4, 0xc5, 0x00, 0xe3, 0xc5, 0x00, 0xdc, 0xc5,
    0x00, 0xc8, 0xc6, 0x00, 0xa4, 0xc7, 0x1a, 0x6d, 0xb5, 0x6e, 0x4f, 0x9a, 0x8a, 0x3d, 0x86, 0x9a,
    0x30, 0xe5, 0x23, 0xb4, 0xe5, 0x23, 0xb4, 0xe5, 0x23, 0xb3, 0xe5, 0x23, 0xb1, 0xe5, 0x24, 0xaa,
    0xe6, 0x28, 0x98, 0xe7, 0x32, 0x70, 0xc4, 0x73, 0x59, 0x9f, 0x8a, 0x40, 0xf0, 0x51, 0x99, 0xf0,
    0x51, 0x99, 0xf0, 0x51, 0x99, 0xf0, 0x51, 0x98, 0xf0, 0x51, 0x97, 0xf0, 0x52, 0x95, 0xf0, 0x54,
    0x8c, 0xf2, 0x5b, 0x77, 0xc8, 0x74, 0x5c, 0xf2, 0x59, 0x94, 0xf2, 0x59, 0x94, 0xf2, 0x59, 0x94,
    0xf2, 0x59, 0x94, 0xf2, 0x59, 0x94, 0xf2, 0x5a, 0x93, 0xf2, 0x5a, 0x91, 0xf3, 0x5c, 0x8a, 0xf5,
    0x63, 0x79, 0x0d, 0x9e, 0x89, 0x0e, 0x9f, 0x86, 0x0f, 0xa2, 0x70, 0x17, 0xaa, 0x50, 0x2b, 0xb9,
    0x2b, 0x4b, 0xc6, 0x09, 0x66, 0xb0, 0x1b, 0x72, 0xa8, 0x23, 0x76, 0xa5, 0x26, 0x11, 0x99, 0x8b,
    0x11, 0x9a, 0x88, 0x13, 0x9c, 0x72, 0x1b, 0xa5, 0x52, 0x2d, 0xb6, 0x2c, 0x4c, 0xc5, 0x09, 0x66,
    0xaf, 0x1b, 0x72, 0xa8, 0x23, 0x76, 0xa5, 0x26, 0x27, 0x7c, 0x99, 0x27, 0x7c, 0x96, 0x28, 0x80,
    0x80, 0x2d, 0x8c, 0x5e, 0x39, 0xa4, 0x34, 0x53, 0xbb, 0x0e, 0x69, 0xad, 0x1c, 0x73, 0xa7, 0x23,
    0x77, 0xa5, 0x26, 0x52, 0x4d, 0xb2, 0x52, 0x4e, 0xaf, 0x53, 0x51, 0x9a, 0x55, 0x5c, 0x79, 0x5b,
    0x77, 0x4b, 0x6a, 0x9e, 0x1d, 0x72, 0xa4, 0x23, 0x76, 0xa4, 0x26, 0x78, 0xa4, 0x27, 0x8f, 0x1a,
    0xce, 0x8f, 0x1b, 0xcb, 0x8f, 0x1c, 0xbd, 0x90, 0x24, 0xa0, 0x92, 0x38, 0x72, 0x96, 0x67, 0x3b,
    0x8b, 0x8f, 0x33, 0x81, 0x9c, 0x2d, 0x7c, 0xa1, 0x29, 0xc5, 0x00, 0xe4, 0xc5, 0x00, 0xe3, 0xc5,
    0x00, 0xdd, 0xc5, 0x00, 0xc9, 0xc6, 0x01, 0xa4, 0xc7, 0x1d, 0x6e, 0xb6, 0x6f, 0x50, 0x9a, 0x8a,
    0x3d, 0x86, 0x9a, 0x30, 0xe5, 0x23, 0xb4, 0xe5, 0x23, 0xb4, 0xe5, 0x23, 0xb3, 0xe5, 0x23, 0xb1,
    0xe5, 0x25, 0xab, 0xe6, 0x28, 0x98, 0xe7, 0x32, 0x70, 0xc4, 0x73, 0x59, 0x9f, 0x8a, 0x40, 0xf0,
    0x51, 0x99, 0xf0, 0x51, 0x99, 0xf0, 0x51, 0x99, 0xf0, 0x51, 0x98, 0xf0, 0x51, 0x97, 0xf0, 0x52,
    0x95, 0xf1, 0x55, 0x8c, 0xf2, 0x5b, 0x77, 0xc8, 0x74, 0x5c, 0xf2, 0x59, 0x94, 0xf2, 0x59, 0x94,
    0xf2, 0x59, 0x94, 0xf2, 0x59, 0x94, 0xf2, 0x59, 0x94, 0xf2, 0x5a, 0x93, 0xf2, 0x5a, 0x91, 0xf3,
    0x5c, 0x8a, 0xf5, 0x63, 0x79, 0x30, 0xb6, 0xa1, 0x31, 0xb6, 0x9e, 0x31, 0xb7, 0x87, 0x35, 0xba,
    0x64, 0x40, 0xc3, 0x39, 0x57, 0xcb, 0x11, 0x6a, 0xb1, 0x1d, 0x73, 0xa8, 0x24, 0x77, 0xa6, 0x26,
    0x32, 0xb3, 0xa2, 0x32, 0xb4, 0x9f, 0x33, 0xb4, 0x88, 0x37, 0xb8, 0x65, 0x41, 0xc2, 0x3a, 0x58,
    0xca, 0x11, 0x6a, 0xb1, 0x1e, 0x73, 0xa8, 0x24, 0x77, 0xa6, 0x26, 0x3d, 0xa3, 0xa9, 0x3e, 0xa3,
    0xa5, 0x3e, 0xa5, 0x8f, 0x41, 0xa9, 0x6c, 0x4a, 0xb5, 0x40, 0x5d, 0xc2, 0x15, 0x6d, 0xae, 0x1f,
    0x74, 0xa8, 0x24, 0x77, 0xa5, 0x26, 0x5e, 0x79, 0xba, 0x5e, 0x79, 0xb7, 0x5e, 0x7b, 0xa2, 0x60,
    0x80, 0x80, 0x65, 0x8f, 0x52, 0x71, 0xa8, 0x22, 0x76, 0xa6, 0x25, 0x77, 0xa5, 0x26, 0x78, 0xa4,
    0x27, 0x93, 0x3b, 0xd1, 0x93, 0x3b, 0xce, 0x93, 0x3c, 0xc1, 0x94, 0x41, 0xa3, 0x96, 0x50, 0x76,
    0x9a, 0x73, 0x3d, 0x8d, 0x91, 0x35, 0x82, 0x9d, 0x2d, 0x7c, 0xa2, 0x29, 0xc6, 0x02, 0xe5, 0xc6,
    0x02, 0xe4, 0xc6, 0x03, 0xde, 0xc7, 0x06, 0xca, 0xc7, 0x0f, 0xa5, 0xc8, 0x28, 0x6f, 0xb7, 0x71,
    0x50, 0x9a, 0x8b, 0x3e, 0x87, 0x9a, 0x30, 0xe5, 0x25, 0xb5, 0xe5, 0x25, 0xb4, 0xe5, 0x25, 0xb4,
    0xe5, 0x25, 0xb2, 0xe6, 0x26, 0xab, 0xe6, 0x2a, 0x99, 0xe8, 0x34, 0x70, 0xc4, 0x73, 0x59, 0x9f,
    0x8a, 0x41, 0xf0, 0x51, 0x99, 0xf0, 0x51, 0x99, 0xf0, 0x51, 0x99, 0xf0, 0x51, 0x98, 0xf0, 0x52,
    0x97, 0xf0, 0x52, 0x95, 0xf1, 0x55, 0x8c, 0xf2, 0x5c, 0x77, 0xc8, 0x75, 0x5c, 0xf2, 0x59, 0x94,
    0xf2, 0x59, 0x94, 0xf2, 0x59, 0x94, 0xf2, 0x59, 0x94, 0xf2, 0x5a, 0x94, 0xf2, 0x5a, 0x93, 0xf2,
    0x5a, 0x91, 0xf3, 0x5c, 0x8a, 0xf5, 0x63, 0x79, 0x62, 0xd4, 0xc3, 0x62, 0xd4, 0xc0, 0x62, 0xd4,
    0xa9, 0x64, 0xd6, 0x84, 0x69, 0xda, 0x55, 0x74, 0xd8, 0x24, 0x77, 0xb5, 0x26, 0x78, 0xaa, 0x27,
    0x78, 0xa6, 0x27, 0x63, 0xd3, 0xc4, 0x63, 0xd3, 0xc0, 0x63, 0xd3, 0xa9, 0x65, 0xd5, 0x85, 0x6a,
    0xd9, 0x55, 0x75, 0xd8, 0x25, 0x77, 0xb5, 0x26, 0x78, 0xaa, 0x27, 0x78, 0xa6, 0x27, 0x68, 0xcb,
    0xc6, 0x68, 0xcb, 0xc3, 0x68, 0xcc, 0xac, 0x6a, 0xcd, 0x88, 0x6e, 0xd2, 0x58, 0x78, 0xd2, 0x27,
    0x79, 0xb3, 0x27, 0x79, 0xa9, 0x27, 0x79, 0xa6, 0x27, 0x7b, 0xb0, 0xcf, 0x7b, 0xb0, 0xcb, 0x7b,
    0xb1, 0xb6, 0x7c, 0xb3, 0x93, 0x7f, 0xb9, 0x64, 0x86, 0xc0, 0x30, 0x80, 0xac, 0x2c, 0x7c, 0xa6,
    0x29, 0x7a, 0xa5, 0x28, 0x9f, 0x76, 0xdb, 0x9f, 0x76, 0xd8, 0x9f, 0x76, 0xca, 0xa0, 0x79, 0xad,
    0xa1, 0x80, 0x80, 0xa5, 0x93, 0x45, 0x94, 0x99, 0x3a, 0x86, 0x9e, 0x30, 0x7d, 0xa2, 0x2a, 0xca,
    0x2c, 0xe9, 0xca, 0x2c, 0xe8, 0xca, 0x2c, 0xe1, 0xca, 0x2e, 0xcd, 0xca, 0x34, 0xa9, 0xcb, 0x46,
    0x73, 0xba, 0x78, 0x53, 0x9d, 0x8d, 0x3f, 0x88, 0x9a, 0x31, 0xe7, 0x2b, 0xb7, 0xe7, 0x2b, 0xb7,
    0xe7, 0x2b, 0xb7, 0xe7, 0x2b, 0xb5, 0xe7, 0x2c, 0xae, 0xe7, 0x2f, 0x9b, 0xe9, 0x38, 0x71, 0xc5,
    0x75, 0x5a, 0xa0, 0x8b, 0x41, 0xf0, 0x53, 0x99, 0xf0, 0x53, 0x99, 0xf0, 0x53, 0x99, 0xf0, 0x53,
    0x99, 0xf0, 0x53, 0x98, 0xf0, 0x54, 0x95, 0xf1, 0x56, 0x8d, 0xf3, 0x5d, 0x78, 0xc8, 0x75, 0x5c,
    0xf2, 0x5a, 0x94, 0xf2, 0x5a, 0x94, 0xf2, 0x5a, 0x94, 0xf2, 0x5a, 0x94, 0xf2, 0x5a, 0x94, 0xf2,
    0x5a, 0x93, 0xf2, 0x5b, 0x91, 0xf3, 0x5d, 0x8a, 0xf5, 0x63, 0x79, 0x9d, 0xf0, 0xcd, 0x9d, 0xf0,
    0xcb, 0x9d, 0xf0, 0xc2, 0x9d, 0xf0, 0xaa, 0x9f, 0xf3, 0x81, 0xa4, 0xfb, 0x45, 0x93, 0xbf, 0x39,
    0x85, 0xad, 0x2f, 0x7d, 0xa7, 0x2a, 0x9d, 0xef, 0xcd, 0x9d, 0xef, 0xcc, 0x9d, 0xef, 0xc2, 0x9e,
    0xf0, 0xaa, 0x9f, 0xf3, 0x81, 0xa5, 0xfb, 0x45, 0x93, 0xbf, 0x39, 0x85, 0xad, 0x2f, 0x7d, 0xa7,
    0x2a, 0x9f, 0xec, 0xcf, 0x9f, 0xec, 0xcd, 0x9f, 0xed, 0xc4, 0x9f, 0xed, 0xac, 0xa1, 0xf0, 0x82,
    0xa6, 0xf8, 0x46, 0x94, 0xbe, 0x3a, 0x85, 0xad, 0x30, 0x7d, 0xa7, 0x2a, 0xa6, 0xe1, 0xda, 0xa6,
    0xe1, 0xd8, 0xa6, 0xe2, 0xcd, 0xa7, 0xe3, 0xb3, 0xa8, 0xe6, 0x88, 0xac, 0xee, 0x4b, 0x99, 0xb8,
    0x3d, 0x88, 0xaa, 0x31, 0x7e, 0xa6, 0x2b, 0xb8, 0xbd, 0xf0, 0xb8, 0xbd, 0xee, 0xb8, 0xbd, 0xe0,
    0xb8, 0xbe, 0xc3, 0xb9, 0xc0, 0x98, 0xbb, 0xc8, 0x5d, 0xa7, 0xa9, 0x46, 0x90, 0xa3, 0x37, 0x82,
    0xa3, 0x2d, 0xd3, 0x73, 0xf2, 0xd3, 0x73, 0xf1, 0xd3, 0x73, 0xea, 0xd3, 0x74, 0xd7, 0xd3, 0x77,
    0xb4, 0xd4, 0x80, 0x80, 0xc4, 0x8b, 0x59, 0xa4, 0x93, 0x44, 0x8b, 0x9c, 0x34, 0xea, 0x3b, 0xc4,
    0xea, 0x3b, 0xc4, 0xea, 0x3b, 0xc3, 0xea, 0x3c, 0xc1, 0xeb, 0x3d, 0xb8, 0xeb, 0x3f, 0xa3, 0xec,
    0x46, 0x73, 0xc9, 0x7b, 0x5c, 0xa2, 0x8d, 0x43, 0xf1, 0x56, 0x99, 0xf1, 0x56, 0x99, 0xf1, 0x56,
    0x99, 0xf1, 0x56, 0x99, 0xf1, 0x57, 0x98, 0xf1, 0x57, 0x95, 0xf2, 0x5a, 0x8d, 0xf4, 0x60, 0x78,
    0xc9, 0x77, 0x5d, 0xf2, 0x5b, 0x94, 0xf2, 0x5b, 0x94, 0xf2, 0x5b, 0x94, 0xf2, 0x5b, 0x94, 0xf2,
    0x5b, 0x94, 0xf3, 0x5b, 0x93, 0xf3, 0x5c, 0x91, 0xf3, 0x5e, 0x8a, 0xf5, 0x64, 0x79, 0xb8, 0xce,
    0xa0, 0xb8, 0xce, 0x9f, 0xb8, 0xce, 0x9f, 0xb8, 0xce, 0x9c, 0xb9, 0xce, 0x94, 0xbb, 0xcf, 0x7f,
    0xc1, 0xd3, 0x58, 0xa1, 0xb5, 0x42, 0x8a, 0xaa, 0x33, 0xb8, 0xce, 0xa0, 0xb8, 0xce, 0x9f, 0xb8,
    0xce, 0x9f, 0xb8, 0xce, 0x9c, 0xb9, 0xce, 0x94, 0xbb, 0xcf, 0x7f, 0xc1, 0xd3, 0x58, 0xa1, 0xb5,
    0x42, 0x8a, 0xaa, 0x33, 0xb8, 0xcd, 0xa0, 0xb8, 0xcd, 0xa0, 0xb8, 0xcd, 0x9f, 0xb9, 0xcd, 0x9c,
    0xb9, 0xcd, 0x94, 0xbb, 0xcf, 0x80, 0xc1, 0xd2, 0x58, 0xa1, 0xb4, 0x43, 0x8a, 0xaa, 0x33, 0xbb,
    0xc9, 0xa0, 0xbb, 0xc9, 0xa0, 0xbb, 0xc9, 0x9f, 0xbb, 0xca, 0x9d, 0xbc, 0xca, 0x95, 0xbe, 0xcb,
    0x81, 0xc3, 0xcf, 0x59, 0xa3, 0xb3, 0x44, 0x8b, 0xa9, 0x33, 0xc3, 0xbf, 0xa2, 0xc3, 0xbf, 0xa1,
    0xc3, 0xbf, 0xa1, 0xc3, 0xbf, 0x9f, 0xc4, 0xc0, 0x97, 0xc5, 0xc1, 0x84, 0xca, 0xc6, 0x5e, 0xa8,
    0xae, 0x47, 0x8d, 0xa7, 0x35, 0xd6, 0xa6, 0xa7, 0xd6, 0xa6, 0xa7, 0xd6, 0xa6, 0xa7, 0xd6, 0xa7,
    0xa5, 0xd6, 0xa7, 0x9e, 0xd7, 0xa9, 0x8d, 0xda, 0xb0, 0x68, 0xb6, 0xa0, 0x50, 0x95, 0xa0, 0x3a,
    0xf2, 0x72, 0xff, 0xf2, 0x72, 0xff, 0xf2, 0x72, 0xfd, 0xf2, 0x72, 0xf4, 0xf2, 0x73, 0xde, 0xf2,
    0x76, 0xb9, 0xf2, 0x80, 0x80, 0xd2, 0x88, 0x62, 0xa9, 0x92, 0x47, 0xf4, 0x61, 0x9a, 0xf4, 0x61,
    0x9a, 0xf4, 0x61, 0x9a, 0xf4, 0x61, 0x9a, 0xf4, 0x61, 0x99, 0xf5, 0x62, 0x97, 0xf5, 0x63, 0x8f,
    0xf7, 0x69, 0x7a, 0xcd, 0x7c, 0x5f, 0xf3, 0x5e, 0x94, 0xf3, 0x5e, 0x94, 0xf3, 0x5e, 0x94, 0xf3,
    0x5e, 0x94, 0xf3, 0x5e, 0x94, 0xf4, 0x5e, 0x93, 0xf4, 0x5f, 0x91, 0xf4, 0x61, 0x8b, 0xf6, 0x66,
    0x7a, 0xc5, 0xbf, 0x97, 0xc5, 0xbf, 0x97, 0xc5, 0xbf, 0x96, 0xc5, 0xbf, 0x96, 0xc5, 0xbf, 0x94,
    0xc6, 0xbf, 0x8e, 0xc8, 0xbf, 0x7f, 0xce, 0xc1, 0x60, 0xa5, 0xb1, 0x45, 0xc5, 0xbe, 0x97, 0xc5,
    0xbe, 0x97, 0xc5, 0xbf, 0x96, 0xc5, 0xbf, 0x96, 0xc5, 0xbf, 0x94, 0xc6, 0xbf, 0x8e, 0xc8, 0xbf,
    0x7f, 0xce, 0xc1, 0x60, 0xa5, 0xb1, 0x45, 0xc5, 0xbe, 0x97, 0xc5, 0xbe, 0x97, 0xc5, 0xbe, 0x96,
    0xc5, 0xbe, 0x96, 0xc5, 0xbe, 0x94, 0xc6, 0xbf, 0x8e, 0xc8, 0xbf, 0x7f, 0xce, 0xc1, 0x60, 0xa6,
    0xb1, 0x45, 0xc6, 0xbd, 0x97, 0xc6, 0xbd, 0x97, 0xc6, 0xbd, 0x96, 0xc6, 0xbd, 0x96, 0xc6, 0xbd,
    0x94, 0xc7, 0xbd, 0x8e, 0xc9, 0xbe, 0x80, 0xce, 0xc0, 0x61, 0xa6, 0xb0, 0x46, 0xc8, 0xba, 0x97,
    0xc8, 0xba, 0x97, 0xc8, 0xba, 0x97, 0xc8, 0xba, 0x96, 0xc9, 0xba, 0x94, 0xc9, 0xba, 0x8f, 0xcb,
    0xbb, 0x81, 0xd1, 0xbd, 0x62, 0xa8, 0xae, 0x47, 0xd0, 0xb0, 0x97, 0xd0, 0xb0, 0x97, 0xd0, 0xb0,
    0x97, 0xd0, 0xb0, 0x97, 0xd0, 0xb0, 0x95, 0xd1, 0xb0, 0x90, 0xd2, 0xb1, 0x83, 0xd7, 0xb4, 0x66,
    0xad, 0xaa, 0x4a, 0xe2, 0x9b, 0x98, 0xe2, 0x9b, 0x98, 0xe2, 0x9b, 0x98, 0xe2, 0x9b, 0x98, 0xe2,
    0x9b, 0x97, 0xe2, 0x9b, 0x93, 0xe3, 0x9d, 0x89, 0xe6, 0xa0, 0x70, 0xba, 0x9d, 0x53, 0xfd, 0x78,
    0xaa, 0xfd, 0x78, 0xaa, 0xfd, 0x78, 0xaa, 0xfd, 0x78, 0xaa, 0xfd, 0x78, 0xa9, 0xfd, 0x79, 0xa7,
    0xfd, 0x7a, 0xa0, 0xfd, 0x80, 0x80, 0xd5, 0x87, 0x65, 0xf6, 0x67, 0x95, 0xf6, 0x67, 0x95, 0xf6,
    0x67, 0x95, 0xf6, 0x67, 0x95, 0xf6, 0x67, 0x94, 0xf6, 0x67, 0x94, 0xf6, 0x68, 0x92, 0xf7, 0x69,
    0x8c, 0xf8, 0x6e, 0x7b, 0xc9, 0xb9, 0x94, 0xc9, 0xb9, 0x94, 0xc9, 0xb9, 0x94, 0xc9, 0xb9, 0x94,
    0xc9, 0xb9, 0x93, 0xca, 0xb9, 0x92, 0xca, 0xb9, 0x8d, 0xcc, 0xba, 0x80, 0xd2, 0xbb, 0x63, 0xc9,
    0xb9, 0x94, 0xc9, 0xb9, 0x94, 0xc9, 0xb9, 0x94, 0xc9, 0xb9, 0x94, 0xc9, 0xb9, 0x93, 0xca, 0xb9,
    0x92, 0xca, 0xb9, 0x8d, 0xcc, 0xba, 0x80, 0xd2, 0xbb, 0x63, 0xc9, 0xb9, 0x94, 0xc9, 0xb9, 0x94,
    0xc9, 0xb9, 0x94, 0xc9, 0xb9, 0x94, 0xc9, 0xb9, 0x93, 0xca, 0xb9, 0x92, 0xca, 0xb9, 0x8d, 0xcc,
    0xba, 0x80, 0xd2, 0xbb, 0x63, 0xca, 0xb9, 0x94, 0xca, 0xb9, 0x94, 0xca, 0xb9, 0x94, 0xca, 0xb9,
    0x94, 0xca, 0xb9, 0x93, 0xca, 0xb9, 0x92, 0xcb, 0xb9, 0x8d, 0xcc, 0xb9, 0x80, 0xd2, 0xba, 0x63,
    0xca, 0xb8, 0x94, 0xca, 0xb8, 0x94, 0xca, 0xb8, 0x94, 0xca, 0xb8, 0x94, 0xca, 0xb8, 0x93, 0xcb,
    0xb8, 0x92, 0xcb, 0xb8, 0x8d, 0xcd, 0xb8, 0x80, 0xd3, 0xba, 0x64, 0xcd, 0xb4, 0x94, 0xcd, 0xb4,
    0x94, 0xcd, 0xb4, 0x94, 0xcd, 0xb4, 0x94, 0xcd, 0xb4, 0x93, 0xcd, 0xb5, 0x92, 0xce, 0xb5, 0x8d,
    0xd0, 0xb5, 0x81, 0xd5, 0xb7, 0x65, 0xd4, 0xac, 0x94, 0xd4, 0xac, 0x94, 0xd4, 0xac, 0x94, 0xd4,
    0xac, 0x94, 0xd4, 0xac, 0x94, 0xd4, 0xac, 0x92, 0xd5, 0xac, 0x8e, 0xd6, 0xad, 0x83, 0xdb, 0xaf,
    0x69, 0xe5, 0x98, 0x95, 0xe5, 0x98, 0x95, 0xe5, 0x98, 0x95, 0xe5, 0x98, 0x95, 0xe5, 0x98, 0x94,
    0xe5, 0x98, 0x93, 0xe5, 0x99, 0x90, 0xe6, 0x9a, 0x88, 0xe9, 0x9d, 0x72, 0xfe, 0x7c, 0x96, 0xfe,
    0x7c, 0x96, 0xfe, 0x7c, 0x96, 0xfe, 0x7c, 0x96, 0xfe, 0x7c, 0x95, 0xfe, 0x7c, 0x95, 0xfe, 0x7c,
    0x93, 0xfe, 0x7d, 0x8e, 0xff, 0x80, 0x80, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
    0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
    0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
    0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
    0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
    0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8,
    0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8,
    0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8,
    0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8,
    0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8,
    0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
    0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
    0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
    0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
    0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
    0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8,
    0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8,
    0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8,
    0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8,
    0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8,
    0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
    0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
    0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
    0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
    0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
    0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8,
    0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8,
    0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8,
    0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8,
    0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8,
    0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, 0x00, 0x6d, 0x42, 0x41, 0x20, 0x00, 0x00, 0x00, 0x00,
    0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x61, 0x72, 0x61, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x70, 0x61, 0x72, 0x61, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x70, 0x61, 0x72, 0x61, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00
  };

  SK_avifImageSetProfileICC(img,
    RGB_D65_202_Rel_PeQ,
    sizeof(RGB_D65_202_Rel_PeQ));
}

#include <ultrahdr/ultrahdr_api.h>

using uhdr_create_encoder_pfn = uhdr_codec_private_t * (*)(void);
using uhdr_enc_set_quality_pfn = uhdr_error_info_t(*)(uhdr_codec_private_t* enc, int quality, uhdr_img_label_t intent);
using uhdr_enc_set_raw_image_pfn = uhdr_error_info_t(*)(uhdr_codec_private_t* enc, uhdr_raw_image_t* img, uhdr_img_label_t intent);
using uhdr_enc_set_output_format_pfn = uhdr_error_info_t(*)(uhdr_codec_private_t* enc, uhdr_codec_t media_type);
using uhdr_encode_pfn = uhdr_error_info_t(*)(uhdr_codec_private_t* enc);
using uhdr_get_encoded_stream_pfn = uhdr_compressed_image_t * (*)(uhdr_codec_private_t* enc);
using uhdr_release_encoder_pfn = void                     (*)(uhdr_codec_private_t* enc);
using uhdr_enc_set_min_max_content_boost_pfn = uhdr_error_info_t(*)(uhdr_codec_private_t* enc, float min_boost, float max_boost);
using uhdr_enc_set_preset_pfn = uhdr_error_info_t(*)(uhdr_codec_private_t* enc, uhdr_enc_preset_t preset);

using is_uhdr_image_pfn = int                      (*)(void* data, int size);

using uhdr_create_decoder_pfn = uhdr_codec_private_t * (*)(void);
using uhdr_release_decoder_pfn = void                     (*)(uhdr_codec_private_t* dec);
using uhdr_dec_set_image_pfn = uhdr_error_info_t(*)(uhdr_codec_private_t* dec, uhdr_compressed_image_t* img);
using uhdr_dec_set_out_color_transfer_pfn = uhdr_error_info_t(*)(uhdr_codec_private_t* dec, uhdr_color_transfer_t ct);
using uhdr_dec_set_out_img_format_pfn = uhdr_error_info_t(*)(uhdr_codec_private_t* dec, uhdr_img_fmt_t fmt);
using uhdr_dec_set_out_max_display_boost_pfn = uhdr_error_info_t(*)(uhdr_codec_private_t* dec, float display_boost);
using uhdr_dec_probe_pfn = uhdr_error_info_t(*)(uhdr_codec_private_t* dec);
using uhdr_decode_pfn = uhdr_error_info_t(*)(uhdr_codec_private_t* dec);
using uhdr_get_decoded_image_pfn = uhdr_raw_image_t * (*)(uhdr_codec_private_t* dec);
using uhdr_get_gain_map_image_pfn = uhdr_raw_image_t * (*)(uhdr_codec_private_t* dec);
using uhdr_dec_get_gain_map_metadata_pfn = uhdr_gainmap_metadata_t * (*)(uhdr_codec_private_t* dec);

uhdr_create_encoder_pfn                sk_uhdr_create_encoder = nullptr;
uhdr_enc_set_quality_pfn               sk_uhdr_enc_set_quality = nullptr;
uhdr_enc_set_raw_image_pfn             sk_uhdr_enc_set_raw_image = nullptr;
uhdr_enc_set_output_format_pfn         sk_uhdr_enc_set_output_format = nullptr;
uhdr_encode_pfn                        sk_uhdr_encode = nullptr;
uhdr_get_encoded_stream_pfn            sk_uhdr_get_encoded_stream = nullptr;
uhdr_release_encoder_pfn               sk_uhdr_release_encoder = nullptr;
uhdr_enc_set_min_max_content_boost_pfn sk_uhdr_enc_set_min_max_content_boost = nullptr;
uhdr_enc_set_preset_pfn                sk_uhdr_enc_set_preset = nullptr;

is_uhdr_image_pfn                      sk_is_uhdr_image = nullptr;

uhdr_create_decoder_pfn                sk_uhdr_create_decoder = nullptr;
uhdr_release_decoder_pfn               sk_uhdr_release_decoder = nullptr;
uhdr_dec_set_image_pfn                 sk_uhdr_dec_set_image = nullptr;
uhdr_dec_set_out_color_transfer_pfn    sk_uhdr_dec_set_out_color_transfer = nullptr;
uhdr_dec_set_out_img_format_pfn        sk_uhdr_dec_set_out_img_format = nullptr;
uhdr_dec_set_out_max_display_boost_pfn sk_uhdr_dec_set_out_max_display_boost = nullptr;
uhdr_dec_probe_pfn                     sk_uhdr_dec_probe = nullptr;
uhdr_decode_pfn                        sk_uhdr_decode = nullptr;
uhdr_get_decoded_image_pfn             sk_uhdr_get_decoded_image = nullptr;
uhdr_get_gain_map_image_pfn            sk_uhdr_get_gain_map_image = nullptr;
uhdr_dec_get_gain_map_metadata_pfn     sk_uhdr_dec_get_gain_map_metadata = nullptr;

const std::initializer_list<FileSignature> supported_formats =
{
  FileSignature { L"image/jpeg",                { L".jpg", L".jpeg" },  { 0xFF, 0xD8, 0x00, 0x00 },   // JPEG (SOI; Start of Image)
                                                                        { 0xFF, 0xFF, 0x00, 0x00 } }, // JPEG App Markers are masked as they can be all over the place (e.g. 0xFF 0xD8 0xFF 0xED)
  FileSignature { L"image/png",                 { L".png"  },           { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A } },
  FileSignature { L"image/webp",                { L".webp" },           { 0x52, 0x49, 0x46, 0x46, 0x00, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50 },   // 52 49 46 46 ?? ?? ?? ?? 57 45 42 50
                                                                        { 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF } }, // mask
  FileSignature { L"image/bmp",                 { L".bmp"  },           { 0x42, 0x4D } },
  FileSignature { L"image/vnd.ms-photo",        { L".jxr", L".hdp"  },  { 0x49, 0x49, 0xBC } },
  FileSignature { L"image/vnd.adobe.photoshop", { L".psd"  },           { 0x38, 0x42, 0x50, 0x53 } },
  FileSignature { L"image/tiff",                { L".tiff", L".tif" },  { 0x49, 0x49, 0x2A, 0x00 } }, // TIFF: little-endian
  FileSignature { L"image/tiff",                { L".tiff", L".tif" },  { 0x4D, 0x4D, 0x00, 0x2A } }, // TIFF: big-endian
  FileSignature { L"image/gif",                 { L".gif"  },           { 0x47, 0x49, 0x46, 0x38, 0x37, 0x61 } }, // GIF87a
  FileSignature { L"image/gif",                 { L".gif"  },           { 0x47, 0x49, 0x46, 0x38, 0x39, 0x61 } }, // GIF89a
  FileSignature { L"image/vnd.radiance",        { L".hdr"  },           { 0x23, 0x3F, 0x52, 0x41, 0x44, 0x49, 0x41, 0x4E, 0x43, 0x45, 0x0A } }, // Radiance High Dynamic Range image file
#ifdef _M_X64
  FileSignature { L"image/x-exr",               { L".exr"  },           { 0x76, 0x2F, 0x31, 0x01 } },
#endif
  FileSignature { L"image/heic",                { L".heic" },           { 0x00, 0x00, 0x00, 0x20, 0x66, 0x74, 0x79, 0x70, 0x68, 0x65, 0x69, 0x63 },   // ftypheic
                                                                        { 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF } }, // ?? ?? ?? ?? 66 74 79 70 68 65 69 63
  FileSignature { L"image/avif",                { L".avif" },           { 0x00, 0x00, 0x00, 0x20, 0x66, 0x74, 0x79, 0x70, 0x61, 0x76, 0x69, 0x66 },   // ftypavif
                                                                        { 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF } }, // ?? ?? ?? ?? 66 74 79 70 61 76 69 66
  FileSignature { L"image/jxl",                 { L".jxl"  },           { 0xFF, 0x0A } },                                                             // Naked
  FileSignature { L"image/jxl",                 { L".jxl"  },           { 0x00, 0x00, 0x00, 0x0C, 0x4A, 0x58, 0x4C, 0x20, 0x0D, 0x0A, 0x87, 0x0A } }, // ISOBMFF-based container
  FileSignature { L"image/vnd-ms.dds",          { L".dds"  },           { 0x44, 0x44, 0x53, 0x20 } },
  //FileSignature { L"image/x-targa",             { L".tga"  },           { 0x00, } }, // TGA has no real unique header identifier, so just use the file extension on those
};

class SKIV_ScopedThreadPriority_Viewer
{
public:
  SKIV_ScopedThreadPriority_Viewer(int prio = THREAD_PRIORITY_TIME_CRITICAL) {
    orig_prio_ =
      GetThreadPriority(GetCurrentThread());

    orig_process_class_ =
      GetPriorityClass(GetCurrentProcess());

    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    SetThreadPriority(GetCurrentThread(), prio);
  };

  ~SKIV_ScopedThreadPriority_Viewer(void) {
    SetPriorityClass(GetCurrentProcess(), orig_process_class_);
    SetThreadPriority(GetCurrentThread(), orig_prio_);
  }

private:
  int orig_prio_;
  int orig_process_class_;
};

class SK_AutoFile {
public:
  SK_AutoFile(FILE* pFile) : file_(pFile)
  {
    if (file_ != nullptr)
    {
      auto orig_pos = ftell(pFile);
      fseek(pFile, 0, SEEK_END);
      size_ = ftell(pFile);
      fseek(pFile, orig_pos, SEEK_SET);
    }
  }

  ~SK_AutoFile(void)
  {
    if (file_ != nullptr)
    {
      fclose(std::exchange(file_, nullptr));
    }
  }

  size_t getInitialSize(void) const {
    return size_;
  }

private:
  size_t size_;
  FILE* file_;
};
#pragma endregion

template<class _Tp>
bool
SKIF_RegistrySettings::KeyValue<_Tp>::hasData(HKEY* hKey)
{
  _Tp   out = _Tp();
  DWORD dwOutLen;

  auto type_idx =
    std::type_index(typeid (_Tp));;

  if (type_idx == std::type_index(typeid (std::wstring)))
  {
    _desc.dwFlags = RRF_RT_REG_SZ;
    _desc.dwType = REG_SZ;

    // Two null terminators are stored at the end of REG_SZ, so account for those
    return (_SizeOfData(hKey) > 4);
  }

  if (type_idx == std::type_index(typeid (bool)))
  {
    _desc.dwType = REG_BINARY;
    dwOutLen = sizeof(bool);
  }

  if (type_idx == std::type_index(typeid (int)))
  {
    _desc.dwType = REG_DWORD;
    dwOutLen = sizeof(int);
  }

  if (type_idx == std::type_index(typeid (float)))
  {
    _desc.dwFlags = RRF_RT_REG_BINARY;
    _desc.dwType = REG_BINARY;
    dwOutLen = sizeof(float);
  }

  if (ERROR_SUCCESS == _GetValue(&out, &dwOutLen, hKey))
    return true;

  return false;
}

std::vector <std::wstring>
SKIF_RegistrySettings::KeyValue<std::vector <std::wstring>>::getData(HKEY* hKey)
{
  _desc.dwFlags = RRF_RT_REG_MULTI_SZ;
  _desc.dwType = REG_MULTI_SZ;
  DWORD dwOutLen = _SizeOfData(hKey);

  std::wstring out(dwOutLen, '\0');

  if (ERROR_SUCCESS !=
    RegGetValueW((hKey != nullptr) ? *hKey : _desc.hKey,
      (hKey != nullptr) ? NULL : _desc.wszSubKey,
      _desc.wszKeyValue,
      _desc.dwFlags,
      &_desc.dwType,
      out.data(), &dwOutLen)) return std::vector <std::wstring>();

  std::vector <std::wstring> vector;

  const wchar_t* currentItem = (const wchar_t*)out.data();

  // Parse the given wstring into a vector
  while (*currentItem)
  {
    vector.push_back(currentItem);
    currentItem = currentItem + _tcslen(currentItem) + 1;
  }

  return vector;
}

template<>
std::wstring
SKIF_RegistrySettings::KeyValue<std::wstring>::getData(HKEY* hKey)
{
  _desc.dwFlags = RRF_RT_REG_SZ;
  _desc.dwType = REG_SZ;
  DWORD dwOutLen = _SizeOfData(hKey);

  std::wstring out(dwOutLen, '\0');

  if (ERROR_SUCCESS !=
    RegGetValueW((hKey != nullptr) ? *hKey : _desc.hKey,
      (hKey != nullptr) ? NULL : _desc.wszSubKey,
      _desc.wszKeyValue,
      _desc.dwFlags,
      &_desc.dwType,
      out.data(), &dwOutLen)) return std::wstring();

  // Strip null terminators
  out.erase(std::find(out.begin(), out.end(), '\0'), out.end());

  return out;
}

template<class _Tp>
_Tp
SKIF_RegistrySettings::KeyValue<_Tp>::getData(HKEY* hKey)
{
  _Tp   out = _Tp();
  DWORD dwOutLen;

  auto type_idx =
    std::type_index(typeid (_Tp));

  if (type_idx == std::type_index(typeid (bool)))
  {
    _desc.dwType = REG_BINARY;
    dwOutLen = sizeof(bool);
  }

  if (type_idx == std::type_index(typeid (int)))
  {
    _desc.dwType = REG_DWORD;
    dwOutLen = sizeof(int);
  }

  if (type_idx == std::type_index(typeid (float)))
  {
    _desc.dwFlags = RRF_RT_REG_BINARY;
    _desc.dwType = REG_BINARY;
    dwOutLen = sizeof(float);
  }

  if (ERROR_SUCCESS !=
    _GetValue(&out, &dwOutLen, hKey)) out = _Tp();

  return out;
}

template<class _Tp>
SKIF_RegistrySettings::KeyValue<_Tp>
SKIF_RegistrySettings::KeyValue<_Tp>::MakeKeyValue(const wchar_t* wszSubKey, const wchar_t* wszKeyValue, HKEY hKey, LPDWORD pdwType, DWORD dwFlags)
{
  KeyValue <_Tp> kv;

  wcsncpy_s(kv._desc.wszSubKey, MAX_PATH,
    wszSubKey, _TRUNCATE);

  wcsncpy_s(kv._desc.wszKeyValue, MAX_PATH,
    wszKeyValue, _TRUNCATE);

  kv._desc.hKey = hKey;
  kv._desc.dwType = (pdwType != nullptr) ?
    *pdwType : REG_NONE;
  kv._desc.dwFlags = dwFlags;

  return kv;
}

SKIF_RegistrySettings::SKIF_RegistrySettings(void)
{
  HKEY hKey = nullptr;

  LSTATUS lsKey = RegCreateKeyW(HKEY_CURRENT_USER, LR"(SOFTWARE\Kaldaien\Special K\Viewer\)", &hKey);

  if (lsKey != ERROR_SUCCESS)
    hKey = nullptr;

  lsKey =
    RegCreateKeyW(HKEY_CURRENT_USER,
      LR"(SOFTWARE\Kaldaien\Special K\Viewer\AVIF\)",
      &avif.key.m_hKey);
  lsKey =
    RegCreateKeyW(HKEY_CURRENT_USER,
      LR"(SOFTWARE\Kaldaien\Special K\Viewer\JPEG XL\)",
      &jxl.key.m_hKey);
  lsKey =
    RegCreateKeyW(HKEY_CURRENT_USER,
      LR"(SOFTWARE\Kaldaien\Special K\Viewer\JPEG XR\)",
      &jxr.key.m_hKey);
  lsKey =
    RegCreateKeyW(HKEY_CURRENT_USER,
      LR"(SOFTWARE\Kaldaien\Special K\Viewer\PNG\)",
      &png.key.m_hKey);

  if (hKey != nullptr)
    RegCloseKey(hKey);

  // Special K stuff

  if (regKVPathSpecialK.hasData())
    wsPathSpecialK = regKVPathSpecialK.getData();
}

static void
SK_WIC_SetMaximumQuality(IPropertyBag2* props)
{
  if (props == nullptr)
    return;

  PROPBAG2 opt = { .pstrName = L"ImageQuality" };
  VARIANT  var = { VT_R4,0,0,0, {.fltVal = 1.0f } };

  PROPBAG2 opt2 = { .pstrName = L"FilterOption" };
  VARIANT  var2 = { VT_UI1,0,0,0, {.bVal = WICPngFilterAdaptive } };

  props->Write(1, &opt, &var);
  props->Write(1, &opt2, &var2);
}

bool isJXLDecoderAvailable(void)
{
  static HMODULE hModBrotliCommon = nullptr;
  static HMODULE hModBrotliDec = nullptr;
  static HMODULE hModBrotliEnc = nullptr;
  static HMODULE hModJXL = nullptr;
  static HMODULE hModJXLCMS = nullptr;
  static HMODULE hModJXLThreads = nullptr;

  static const wchar_t* wszPluginArch =
    SK_RunLHIfBitness(64, LR"(x64\)",
      LR"(x86\)");
    
      SKIF_RegistrySettings & _registry =
        SKIF_RegistrySettings::GetInstance();

      std::wstring path_to_sk =
        _registry.regKVPathSpecialK.getData();

      std::error_code                            ec;
      if (!std::filesystem::exists(path_to_sk, ec))
      {
        path_to_sk =
          GetPathToSK();

        path_to_sk += LR"(\My Mods\SpecialK\)";
      }

      if (std::filesystem::exists(path_to_sk, ec))
      {
        path_to_sk += LR"(\PlugIns\ThirdParty\Image Codecs\libjxl\)";
        path_to_sk += wszPluginArch;

        std::filesystem::create_directories
                                    (path_to_sk, ec);
        if (std::filesystem::exists(path_to_sk, ec))
        {
          std::wstring path_to_brotlicommon = path_to_sk + L"brotlicommon.dll";
          std::wstring path_to_brotlienc = path_to_sk + L"brotlienc.dll";
          std::wstring path_to_brotlidec = path_to_sk + L"brotlidec.dll";
          std::wstring path_to_jxl_threads = path_to_sk + L"jxl_threads.dll";
          std::wstring path_to_jxl_cms = path_to_sk + L"jxl_cms.dll";
          std::wstring path_to_jxl = path_to_sk + L"jxl.dll";

          if (!std::filesystem::exists(path_to_brotlicommon, ec))
            SKIF_Util_GetWebResource(L"https://sk-data.special-k.info/addon/ImageCodecs/libjxl/x64/brotlicommon.dll", path_to_brotlicommon);

          if (!std::filesystem::exists(path_to_brotlienc, ec))
            SKIF_Util_GetWebResource(L"https://sk-data.special-k.info/addon/ImageCodecs/libjxl/x64/brotlienc.dll",    path_to_brotlienc);

          if (!std::filesystem::exists(path_to_brotlidec, ec))
            SKIF_Util_GetWebResource(L"https://sk-data.special-k.info/addon/ImageCodecs/libjxl/x64/brotlidec.dll",    path_to_brotlidec);

          if (!std::filesystem::exists(path_to_jxl, ec))
            SKIF_Util_GetWebResource(L"https://sk-data.special-k.info/addon/ImageCodecs/libjxl/x64/jxl.dll",          path_to_jxl);

          if (!std::filesystem::exists(path_to_jxl_cms, ec))
            SKIF_Util_GetWebResource(L"https://sk-data.special-k.info/addon/ImageCodecs/libjxl/x64/jxl_cms.dll",      path_to_jxl_cms);

          if (!std::filesystem::exists(path_to_jxl_threads, ec))
            SKIF_Util_GetWebResource(L"https://sk-data.special-k.info/addon/ImageCodecs/libjxl/x64/jxl_threads.dll",  path_to_jxl_threads);

          // JXL depends on CMS to be loaded first

          hModBrotliCommon = LoadLibraryW(path_to_brotlicommon.c_str());
          hModBrotliDec = LoadLibraryW(path_to_brotlidec.c_str());
          hModBrotliEnc = LoadLibraryW(path_to_brotlienc.c_str());
          hModJXLThreads = LoadLibraryW(path_to_jxl_threads.c_str());
          hModJXLCMS = LoadLibraryW(path_to_jxl_cms.c_str());
          hModJXL = LoadLibraryW(path_to_jxl.c_str());

          if (hModBrotliCommon != nullptr &&
               hModBrotliDec != nullptr &&
               hModBrotliEnc != nullptr &&
               hModJXL != nullptr &&
               hModJXLCMS != nullptr &&
               hModJXLThreads != nullptr)
          {
            LOG << "Loaded JPEG XL DLLs from: " << path_to_sk;
            return true;
          }
        }
      }

      if (hModBrotliCommon == nullptr) hModBrotliCommon = LoadLibraryW(L"brotlicommon.dll");
      if (hModBrotliDec == nullptr) hModBrotliDec = LoadLibraryW(L"brotlidec.dll");
      if (hModBrotliEnc == nullptr) hModBrotliEnc = LoadLibraryW(L"brotlienc.dll");
      if (hModJXLThreads == nullptr) hModJXLThreads = LoadLibraryW(L"jxl_threads.dll");
      if (hModJXLCMS == nullptr) hModJXLCMS = LoadLibraryW(L"jxl_cms.dll");
      if (hModJXL == nullptr) hModJXL = LoadLibraryW(L"jxl.dll");

      if (hModBrotliCommon != nullptr &&
        hModBrotliDec != nullptr &&
        hModBrotliEnc != nullptr &&
        hModJXL != nullptr &&
        hModJXLCMS != nullptr &&
        hModJXLThreads != nullptr)
      {
        LOG_I << "Loaded JPEG XL DLLs from default DLL search path";
        return true;
      }

  const bool supported =
    (hModBrotliCommon != nullptr &&
      hModBrotliDec != nullptr &&
      hModBrotliEnc != nullptr &&
      hModJXL != nullptr &&
      hModJXLThreads != nullptr &&
      hModJXLCMS != nullptr);

  if (!supported)
  {
  }

  return supported;
}

DirectX::XMVECTOR
SKIV_Image_Rec709toICtCp(DirectX::XMVECTOR N)
{
  using namespace DirectX;

  XMVECTOR ret = N;

  ret = XMVector3Transform(ret, c_from709toXYZ);
  ret = XMVector3Transform(ret, c_fromXYZtoLMS);

  ret =
    SKIV_Image_LinearToPQ(XMVectorMax(ret, g_XMZero), PQ.MaxPQ);

  static const DirectX::XMMATRIX ConvMat = // Transposed
  {
    { 0.5000f,  1.6137f,  4.3780f, 0.0f },
    { 0.5000f, -3.3234f, -4.2455f, 0.0f },
    { 0.0000f,  1.7097f, -0.1325f, 0.0f },
    { 0.0f,     0.0f,     0.0f,    1.0f }
  };

  return
    XMVector3Transform(ret, ConvMat);
};

DirectX::XMVECTOR
SKIV_Image_ICtCptoRec709(DirectX::XMVECTOR N)
{
  using namespace DirectX;

  XMVECTOR ret = N;

#pragma warning( push )
#pragma warning( disable : 4305 )

  static const DirectX::XMMATRIX ConvMat = // Transposed
  {
    { 1.0,                  1.0,                  1.0,                 0.0f },
    { 0.00860514569398152, -0.00860514569398152,  0.56004885956263900, 0.0f },
    { 0.11103560447547328, -0.11103560447547328, -0.32063747023212210, 0.0f },
    { 0.0f,                 0.0f,                 0.0f,                1.0f }
  };

#pragma warning( pop ) 

  ret =
    XMVector3Transform(ret, ConvMat);

  ret = SKIV_Image_PQToLinear(ret, PQ.MaxPQ);
  ret = XMVector3Transform(ret, c_fromLMStoXYZ);

  return
    XMVector3Transform(ret, c_fromXYZto709);
};

static SK_PNG_HDR_cLLi_Payload
SKIV_HDR_CalculateContentLightInfo(const DirectX::Image& img)
{
  using namespace DirectX;

  SK_PNG_HDR_cLLi_Payload clli;

  float N = 0.0f;
  float fLumAccum = 0.0f;
  float fMaxLum = 0.0f;
  float fMinLum = 5240320.0f;

  EvaluateImage(img,
    [&](const XMVECTOR* pixels, size_t width, size_t y)
    {
      UNREFERENCED_PARAMETER(y);

      float fScanlineLum = 0.0f;

      switch (img.format)
      {
      case DXGI_FORMAT_R10G10B10A2_UNORM:
      {
        for (size_t j = 0; j < width; ++j)
        {
          XMVECTOR v =
            *pixels++;

          v =
            XMVector3Transform(
              SKIV_Image_PQToLinear(XMVectorSaturate(v)), c_from2020toXYZ
            );

          const float fLum =
            XMVectorGetY(v);

          fMaxLum =
            std::max(fMaxLum, fLum);

          fMinLum =
            std::min(fMinLum, fLum);

          fScanlineLum += fLum;
        }
      } break;

      case DXGI_FORMAT_R16G16B16A16_FLOAT:
      case DXGI_FORMAT_R32G32B32A32_FLOAT:
      {
        for (size_t j = 0; j < width; ++j)
        {
          XMVECTOR v =
            *pixels++;

          v =
            XMVector3Transform(v, c_from709toXYZ);

          const float fLum =
            XMVectorGetY(v);

          fMaxLum =
            std::max(fMaxLum, fLum);

          fMinLum =
            std::min(fMinLum, fLum);

          fScanlineLum += fLum;
        }
      } break;

      default:
        break;
      }

      fLumAccum +=
        (fScanlineLum / static_cast <float> (width));
      ++N;
    }
  );

  if (N > 0.0)
  {
    // 0 nits - 10k nits (limit imposed by PQ)
    fMinLum = std::clamp(fMinLum, 0.0f, 125.0f);
    fMaxLum = std::clamp(fMaxLum, fMinLum, 125.0f);

    const float fLumRange =
      (fMaxLum - fMinLum);

    auto        luminance_freq = std::make_unique <uint32_t[]>(65536);
    ZeroMemory(luminance_freq.get(), sizeof(uint32_t) * 65536);

    EvaluateImage(img,
      [&](const XMVECTOR* pixels, size_t width, size_t y)
      {
        UNREFERENCED_PARAMETER(y);

        for (size_t j = 0; j < width; ++j)
        {
          XMVECTOR v = *pixels++;

          v =
            XMVectorMax(g_XMZero, XMVector3Transform(v, c_from709toXYZ));

          luminance_freq[
            std::clamp((int)
              std::roundf(
                (XMVectorGetY(v) - fMinLum) /
                (fLumRange / 65536.0f)),
              0, 65535)]++;
        }
      });

    double percent = 100.0;
    const double img_size = (double)img.width *
      (double)img.height;

    for (auto i = 65535; i >= 0; --i)
    {
      percent -=
        100.0 * ((double)luminance_freq[i] / img_size);

      if (percent <= 99.5)
      {
        fMaxLum =
          fMinLum + (fLumRange * ((float)i / 65536.0f));

        break;
      }
    }

    SK_PNG_SetUint32(clli.max_cll,
      static_cast <uint32_t> ((80.0f * fMaxLum) / 0.0001f));
    SK_PNG_SetUint32(clli.max_fall,
      static_cast <uint32_t> ((80.0f * (fLumAccum / N)) / 0.0001f));
  }

  return clli;
}

static bool
sk_png_remove_chunk(const char* szName, void* data, size_t& size)
{
  if (szName == nullptr || data == nullptr || size < 12 || strlen(szName) < 4)
  {
    return false;
  }

  size_t   erase_pos = 0;
  uint8_t* erase_ptr = nullptr;

  // Effectively a string search, but ignoring nul-bytes in both
  //   the character array being searched and the pattern...
  std::string_view data_view((const char*)data, size);
  if (erase_pos = data_view.find(szName, 0, 4);
    erase_pos == data_view.npos)
  {
    return false;
  }

  erase_pos -= 4; // Rollback to the chunk's length field
  erase_ptr =
    ((uint8_t*)data + erase_pos);

  uint32_t chunk_size = *(uint32_t*)erase_ptr;

  // Length is Big Endian, Intel/AMD CPUs are Little Endian
#if (defined _M_IX86) || (defined _M_X64)
  chunk_size = _byteswap_ulong(chunk_size);
#endif

  size_t size_to_erase = (size_t)12 + chunk_size;

  memmove(erase_ptr,
    erase_ptr + size_to_erase,
    size - erase_pos - size_to_erase);

  size -= size_to_erase;

  return true;
}

static uint32_t
png_crc32(const void* typeless_data, size_t offset, size_t len, uint32_t crc)
{
  auto data =
    (const BYTE*)typeless_data;

  uint32_t c;

  static uint32_t
    png_crc_table[256] = { };
  if (png_crc_table[0] == 0)
  {
    for (auto i = 0; i < 256; ++i)
    {
      c = i;

      for (auto j = 0; j < 8; ++j)
      {
        if ((c & 1) == 1)
          c = (0xEDB88320 ^ ((c >> 1) & 0x7FFFFFFF));
        else
          c = ((c >> 1) & 0x7FFFFFFF);
      }

      png_crc_table[i] = c;
    }
  }

  c =
    (crc ^ 0xffffffff);

  for (auto k = offset; k < (offset + len); ++k)
  {
    c =
      png_crc_table[(c ^ data[k]) & 255] ^
      ((c >> 8) & 0xFFFFFF);
  }

  return
    (c ^ 0xffffffff);
}

DirectX::XMVECTOR
SKIV_Image_LinearToPQ(DirectX::XMVECTOR N, DirectX::XMVECTOR maxPQValue)
{
  using namespace DirectX;

  XMVECTOR ret;

  ret =
    XMVectorPow(XMVectorDivide(XMVectorMax(N, g_XMZero), maxPQValue), PQ.N);

  XMVECTOR nd =
    XMVectorDivide(
      XMVectorAdd(PQ.C1, XMVectorMultiply(PQ.C2, ret)),
      XMVectorAdd(g_XMOne, XMVectorMultiply(PQ.C3, ret))
    );

  return
    XMVectorPow(nd, PQ.M);
};

float
SKIV_Image_LinearToPQY(float N)
{
  const float fScaledN =
    fabs(N * 0.008f); // 0.008 = 1/125.0

  float ret =
    pow(fScaledN, 0.1593017578125f);

  float nd =
    fabs((0.8359375f + (18.8515625f * ret)) /
      (1.0f + (18.6875f * ret)));

  return
    pow(nd, 78.84375f);
};

int
SKIV_DXGI_NumberOfChannels(DXGI_FORMAT format)
{
  switch (format)
  {
  case DXGI_FORMAT_R32G32B32A32_TYPELESS:
  case DXGI_FORMAT_R32G32B32A32_FLOAT:
  case DXGI_FORMAT_R32G32B32A32_UINT:
  case DXGI_FORMAT_R32G32B32A32_SINT:
  case DXGI_FORMAT_R16G16B16A16_TYPELESS:
  case DXGI_FORMAT_R16G16B16A16_FLOAT:
  case DXGI_FORMAT_R16G16B16A16_UNORM:
  case DXGI_FORMAT_R16G16B16A16_UINT:
  case DXGI_FORMAT_R16G16B16A16_SNORM:
  case DXGI_FORMAT_R16G16B16A16_SINT:
  case DXGI_FORMAT_R10G10B10A2_TYPELESS:
  case DXGI_FORMAT_R10G10B10A2_UNORM:
  case DXGI_FORMAT_R10G10B10A2_UINT:
  case DXGI_FORMAT_R8G8B8A8_TYPELESS:
  case DXGI_FORMAT_R8G8B8A8_UNORM:
  case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
  case DXGI_FORMAT_R8G8B8A8_UINT:
  case DXGI_FORMAT_R8G8B8A8_SNORM:
  case DXGI_FORMAT_R8G8B8A8_SINT:
  case DXGI_FORMAT_B8G8R8A8_TYPELESS:
  case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
  case DXGI_FORMAT_B8G8R8A8_UNORM:
  case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
  case DXGI_FORMAT_B5G5R5A1_UNORM:
  case DXGI_FORMAT_B4G4R4A4_UNORM:
    return 4;

  case DXGI_FORMAT_R32G32B32_TYPELESS:
  case DXGI_FORMAT_R32G32B32_FLOAT:
  case DXGI_FORMAT_R32G32B32_UINT:
  case DXGI_FORMAT_R32G32B32_SINT:
  case DXGI_FORMAT_R11G11B10_FLOAT:
  case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
  case DXGI_FORMAT_B5G6R5_UNORM:
  case DXGI_FORMAT_B8G8R8X8_UNORM:
  case DXGI_FORMAT_B8G8R8X8_TYPELESS:
  case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
    return 3;

  case DXGI_FORMAT_R32G32_TYPELESS:
  case DXGI_FORMAT_R32G32_FLOAT:
  case DXGI_FORMAT_R32G32_UINT:
  case DXGI_FORMAT_R32G32_SINT:
  case DXGI_FORMAT_R32G8X24_TYPELESS:
  case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
  case DXGI_FORMAT_R16G16_TYPELESS:
  case DXGI_FORMAT_R16G16_FLOAT:
  case DXGI_FORMAT_R16G16_UNORM:
  case DXGI_FORMAT_R16G16_UINT:
  case DXGI_FORMAT_R16G16_SNORM:
  case DXGI_FORMAT_R16G16_SINT:
  case DXGI_FORMAT_R24G8_TYPELESS:
  case DXGI_FORMAT_D24_UNORM_S8_UINT:
  case DXGI_FORMAT_R8G8_TYPELESS:
  case DXGI_FORMAT_R8G8_UNORM:
  case DXGI_FORMAT_R8G8_UINT:
  case DXGI_FORMAT_R8G8_SNORM:
  case DXGI_FORMAT_R8G8_SINT:
    return 2;

  case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
  case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
  case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
  case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
  case DXGI_FORMAT_R32_TYPELESS:
  case DXGI_FORMAT_D32_FLOAT:
  case DXGI_FORMAT_R32_FLOAT:
  case DXGI_FORMAT_R32_UINT:
  case DXGI_FORMAT_R32_SINT:
  case DXGI_FORMAT_R16_TYPELESS:
  case DXGI_FORMAT_R16_FLOAT:
  case DXGI_FORMAT_D16_UNORM:
  case DXGI_FORMAT_R16_UNORM:
  case DXGI_FORMAT_R16_UINT:
  case DXGI_FORMAT_R16_SNORM:
  case DXGI_FORMAT_R16_SINT:
  case DXGI_FORMAT_R8_TYPELESS:
  case DXGI_FORMAT_R8_UNORM:
  case DXGI_FORMAT_R8_UINT:
  case DXGI_FORMAT_R8_SNORM:
  case DXGI_FORMAT_R8_SINT:
  case DXGI_FORMAT_A8_UNORM:
  case DXGI_FORMAT_R1_UNORM:
    return 1;

  case DXGI_FORMAT_R8G8_B8G8_UNORM:
  case DXGI_FORMAT_G8R8_G8B8_UNORM:
    return -1; // ?

  case DXGI_FORMAT_BC1_TYPELESS:
  case DXGI_FORMAT_BC1_UNORM:
  case DXGI_FORMAT_BC1_UNORM_SRGB:
  case DXGI_FORMAT_BC2_TYPELESS:
  case DXGI_FORMAT_BC2_UNORM:
  case DXGI_FORMAT_BC2_UNORM_SRGB:
  case DXGI_FORMAT_BC3_TYPELESS:
  case DXGI_FORMAT_BC3_UNORM:
  case DXGI_FORMAT_BC3_UNORM_SRGB:
    return 4;

  case DXGI_FORMAT_BC4_TYPELESS:
  case DXGI_FORMAT_BC4_UNORM:
  case DXGI_FORMAT_BC4_SNORM:
    return 1;

  case DXGI_FORMAT_BC5_TYPELESS:
  case DXGI_FORMAT_BC5_UNORM:
  case DXGI_FORMAT_BC5_SNORM:
    return 2;

  case DXGI_FORMAT_BC6H_TYPELESS:
  case DXGI_FORMAT_BC6H_UF16:
  case DXGI_FORMAT_BC6H_SF16:
    return 3;

  case DXGI_FORMAT_BC7_TYPELESS:
  case DXGI_FORMAT_BC7_UNORM:
  case DXGI_FORMAT_BC7_UNORM_SRGB:
    return 4;

  case DXGI_FORMAT_AYUV:
  case DXGI_FORMAT_Y410:
  case DXGI_FORMAT_Y416:
  case DXGI_FORMAT_NV12:
  case DXGI_FORMAT_P010:
  case DXGI_FORMAT_P016:
  case DXGI_FORMAT_420_OPAQUE:
  case DXGI_FORMAT_YUY2:
  case DXGI_FORMAT_Y210:
  case DXGI_FORMAT_Y216:
  case DXGI_FORMAT_NV11:
  case DXGI_FORMAT_AI44:
  case DXGI_FORMAT_IA44:
  case DXGI_FORMAT_P8:
  case DXGI_FORMAT_A8P8:
    return -1; // ?

  case DXGI_FORMAT_P208:
  case DXGI_FORMAT_V208:
  case DXGI_FORMAT_V408:
    return -1; // ?

  default:
    return 0;
  }
}

DirectX::XMVECTOR
SKIV_Image_PQToLinear(DirectX::XMVECTOR N, DirectX::XMVECTOR maxPQValue)
{
  using namespace DirectX;

  XMVECTOR ret;

  ret =
    XMVectorPow(XMVectorMax(N, g_XMZero), PQ.RcpM);

  XMVECTOR nd;

  nd =
    XMVectorDivide(
      XMVectorMax(XMVectorSubtract(ret, PQ.C1), g_XMZero),
      XMVectorSubtract(PQ.C2,
        XMVectorMultiply(PQ.C3, ret)));

  ret =
    XMVectorMultiply(XMVectorPow(nd, PQ.RcpN), maxPQValue);

  return ret;
};

HRESULT
SKIV_Image_LoadUltraHDR(DirectX::ScratchImage& image, void* data, int size)
{
  if (!sk_uhdr_create_decoder)
    return E_NOTIMPL;

  auto decoder =
    sk_uhdr_create_decoder();

  if (!decoder)
    return E_UNEXPECTED;

  uhdr_compressed_image_t uhdr_image;

  uhdr_image.data = data;
  uhdr_image.data_sz = size;
  uhdr_image.capacity = size;
  uhdr_image.cg = UHDR_CG_BT_709;//UHDR_CG_UNSPECIFIED;
  uhdr_image.ct = UHDR_CT_LINEAR;//UHDR_CT_UNSPECIFIED;
  uhdr_image.range = UHDR_CR_FULL_RANGE;//UHDR_CR_UNSPECIFIED;

  sk_uhdr_dec_set_image(decoder, &uhdr_image);
  sk_uhdr_dec_probe(decoder);
  sk_uhdr_dec_set_out_color_transfer(decoder, UHDR_CT_LINEAR);
  sk_uhdr_dec_set_out_img_format(decoder, UHDR_IMG_FMT_64bppRGBAHalfFloat);
  sk_uhdr_decode(decoder);

  auto decoded_img =
    sk_uhdr_get_decoded_image(decoder);

  if (!decoded_img)
    return E_UNEXPECTED;

  DirectX::Image img;

  img.pixels = (uint8_t*)decoded_img->planes[UHDR_PLANE_PACKED];
  img.rowPitch = decoded_img->stride[UHDR_PLANE_PACKED] * sizeof(uint16_t) * 4;
  img.width = decoded_img->w;
  img.height = decoded_img->h;
  img.format = DXGI_FORMAT_R16G16B16A16_FLOAT;

  DirectX::ScratchImage
    unscaled_image;
  unscaled_image.InitializeFromImage(img);

  auto metadata =
    sk_uhdr_dec_get_gain_map_metadata(decoder);

  DirectX::TransformImage(*unscaled_image.GetImage(0, 0, 0),
    [&](DirectX::XMVECTOR* outPixels, const DirectX::XMVECTOR* inPixels, size_t width, size_t y)
    {
      using namespace DirectX;

      for (size_t j = 0; j < width; ++j)
      {
        XMVECTOR value = inPixels[j];

        value =
          XMVectorMultiply(value, XMVectorReplicate(metadata->hdr_capacity_max));

        outPixels[j] = value;
      }

      UNREFERENCED_PARAMETER(y);
    }, image);

  sk_uhdr_release_decoder(decoder);

  return S_OK;
}

bool isUHDRCodecAvailable(void)
{
  static HMODULE hModUHDR = nullptr;

  static const wchar_t* wszPluginArch =
    SK_RunLHIfBitness(64, LR"(x64\)",
      LR"(x86\)");

  static const wchar_t* wszDownloadURL =
    SK_RunLHIfBitness(64, LR"(https://sk-data.special-k.info/addon/ImageCodecs/libuhdr/x64/uhdr.dll)",
      LR"(https://sk-data.special-k.info/addon/ImageCodecs/libuhdr/x86/uhdr.dll)");

      SKIF_RegistrySettings & _registry =
        SKIF_RegistrySettings::GetInstance();

      std::wstring path_to_sk =
        _registry.regKVPathSpecialK.getData();

      std::error_code                            ec;
      if (!std::filesystem::exists(path_to_sk, ec))
      {
        path_to_sk = GetPathToSK();
        path_to_sk += LR"(\My Mods\SpecialK\)";
      }

      if (std::filesystem::exists(path_to_sk, ec))
      {
        path_to_sk += LR"(\PlugIns\ThirdParty\Image Codecs\libultrahdr\)";
        path_to_sk += wszPluginArch;

        std::filesystem::create_directories
                                    (path_to_sk, ec);
        if (std::filesystem::exists(path_to_sk, ec))
        {
          std::wstring path_to_uhdr = path_to_sk + L"uhdr.dll";

          if (!std::filesystem::exists(path_to_uhdr, ec))
            SKIF_Util_GetWebResource(wszDownloadURL, path_to_uhdr);

          hModUHDR = LoadLibraryW(path_to_uhdr.c_str());

          if (hModUHDR != nullptr)
          {
            LOG_I << "Loaded Ultra HDR from: " << path_to_sk;
          }
        }
      }

      if (hModUHDR == nullptr)
      {
        hModUHDR = LoadLibraryW(L"uhdr.dll");
        if (hModUHDR != nullptr)
        {
          LOG_I << "Loaded Ultra HDR from default DLL search path";
        }
      }

      if (hModUHDR != nullptr)
      {
        sk_uhdr_create_encoder = (uhdr_create_encoder_pfn)GetProcAddress(hModUHDR, "uhdr_create_encoder");
        sk_uhdr_enc_set_quality = (uhdr_enc_set_quality_pfn)GetProcAddress(hModUHDR, "uhdr_enc_set_quality");
        sk_uhdr_enc_set_raw_image = (uhdr_enc_set_raw_image_pfn)GetProcAddress(hModUHDR, "uhdr_enc_set_raw_image");
        sk_uhdr_enc_set_output_format = (uhdr_enc_set_output_format_pfn)GetProcAddress(hModUHDR, "uhdr_enc_set_output_format");
        sk_uhdr_encode = (uhdr_encode_pfn)GetProcAddress(hModUHDR, "uhdr_encode");
        sk_uhdr_get_encoded_stream = (uhdr_get_encoded_stream_pfn)GetProcAddress(hModUHDR, "uhdr_get_encoded_stream");
        sk_uhdr_release_encoder = (uhdr_release_encoder_pfn)GetProcAddress(hModUHDR, "uhdr_release_encoder");
        sk_uhdr_enc_set_min_max_content_boost = (uhdr_enc_set_min_max_content_boost_pfn)GetProcAddress(hModUHDR, "uhdr_enc_set_min_max_content_boost");
        sk_uhdr_enc_set_preset = (uhdr_enc_set_preset_pfn)GetProcAddress(hModUHDR, "uhdr_enc_set_preset");

        sk_is_uhdr_image = (is_uhdr_image_pfn)GetProcAddress(hModUHDR, "is_uhdr_image");

        sk_uhdr_create_decoder = (uhdr_create_decoder_pfn)GetProcAddress(hModUHDR, "uhdr_create_decoder");
        sk_uhdr_release_decoder = (uhdr_release_decoder_pfn)GetProcAddress(hModUHDR, "uhdr_release_decoder");
        sk_uhdr_dec_set_image = (uhdr_dec_set_image_pfn)GetProcAddress(hModUHDR, "uhdr_dec_set_image");
        sk_uhdr_dec_set_out_color_transfer = (uhdr_dec_set_out_color_transfer_pfn)GetProcAddress(hModUHDR, "uhdr_dec_set_out_color_transfer");
        sk_uhdr_dec_set_out_img_format = (uhdr_dec_set_out_img_format_pfn)GetProcAddress(hModUHDR, "uhdr_dec_set_out_img_format");
        sk_uhdr_dec_set_out_max_display_boost = (uhdr_dec_set_out_max_display_boost_pfn)GetProcAddress(hModUHDR, "uhdr_dec_set_out_max_display_boost");
        sk_uhdr_dec_probe = (uhdr_dec_probe_pfn)GetProcAddress(hModUHDR, "uhdr_dec_probe");
        sk_uhdr_decode = (uhdr_decode_pfn)GetProcAddress(hModUHDR, "uhdr_decode");
        sk_uhdr_get_decoded_image = (uhdr_get_decoded_image_pfn)GetProcAddress(hModUHDR, "uhdr_get_decoded_image");
        sk_uhdr_get_gain_map_image = (uhdr_get_gain_map_image_pfn)GetProcAddress(hModUHDR, "uhdr_get_gain_map_image");
        sk_uhdr_dec_get_gain_map_metadata = (uhdr_dec_get_gain_map_metadata_pfn)GetProcAddress(hModUHDR, "uhdr_dec_get_gain_map_metadata");

        return true;
      }
      return false;

  const bool supported =
    (hModUHDR != nullptr);

  if (!supported)
  {
    LOG_E << "UltraHDR Unsupported because Special K is not Installed";
    LOG << "Please install Special K and run SKIV again to view UltraHDR images.\r\n\t";
    LOG << "> You may also manually place (a 64-bit version of) uhdr.dll in SKIV's directory.";
  }
  return supported;
}

bool
SKIV_Image_IsUltraHDR(const wchar_t* wszFileName)
{
  if (!isUHDRCodecAvailable())
    return false;

  FILE* fImageFile =
    _wfopen(wszFileName, L"rb");

  if (fImageFile != nullptr)
  {
    fseek(fImageFile, 0, SEEK_END);
    auto size = ftell(fImageFile);
    auto data =
      std::make_unique <uint8_t[]>(size);

    rewind(fImageFile);
    fread(data.get(), 1, size, fImageFile);
    fclose(fImageFile);

    return
      sk_is_uhdr_image(data.get(), size) != 0;
  }

  return false;
}

bool
LoadLibraryTexture(image_s& image)
{
  SKIV_ScopedThreadPriority_Viewer _scoped_thread_prio;

  CComPtr <ID3D11Texture2D> pRawTex2D;
  CComPtr <ID3D11Texture2D> pGamutCoverageTex2D;
  DirectX::TexMetadata        meta = { };
  DirectX::ScratchImage        img = { };
  DirectX::ScratchImage        img_srgb = { };

  bool succeeded = false;
  bool converted = false;
  bool need_srgb = false;

  if (image.file_info.path.empty())
    return false;

  const std::filesystem::path
    imagePath(image.file_info.path.data());

  std::wstring ext = SKIF_Util_ToLowerW(imagePath.extension().wstring());
  std::string szPath = SK_WideCharToUTF8(image.file_info.path);

  ImageDecoder decoder = ImageDecoder_None;

  if (ext == L".tga")
    decoder = SKIV_DEFAULT_GENERAL_PURPOSE_DECODER;

  FILE* pImageFile = nullptr;
  const FileSignature* image_sig = nullptr;

  if (decoder == ImageDecoder_None)
  {
    static size_t
      maxLength = 0;
    if (maxLength == 0)
    {
      for (auto& type : supported_formats)
        if (type.signature.size() > maxLength)
          maxLength = type.signature.size();
    }

    pImageFile =
      _wfopen(imagePath.c_str(), L"rb");

    if (!pImageFile)
    {
      LOG_E << "Failed to open file!";
      return false;
    }

    std::vector <char>
      buffer(maxLength);
    fread(buffer.data(), maxLength, 1, pImageFile);
    rewind(pImageFile);

    for (auto& type : supported_formats)
    {
      if (SKIF_Util_HasFileSignature(buffer, type))
      {
        image_sig = &type;

        LOG << "Detected an " << type.mime_type << " image";

        decoder =
          (type.mime_type == L"image/jpeg") ?
          (SKIV_Image_IsUltraHDR(imagePath.c_str()) ? ImageDecoder_UHDR :
            SKIV_DEFAULT_GENERAL_PURPOSE_DECODER) :
          (type.mime_type == L"image/png") ? ImageDecoder_stbi :
          //(type.mime_type == L"image/png"                 ) ? ImageDecoder_WIC  : // Use WIC for proper color correction and for decoding many PNG images that stbi cannot
          (type.mime_type == L"image/bmp") ? SKIV_DEFAULT_GENERAL_PURPOSE_DECODER :
          (type.mime_type == L"image/vnd.adobe.photoshop") ? ImageDecoder_stbi : // Consider gamma broken, since stbi doesn't handle it correctly
          (type.mime_type == L"image/gif") ? SKIV_DEFAULT_GENERAL_PURPOSE_DECODER :
          (type.mime_type == L"image/vnd.radiance") ? ImageDecoder_HDR :
          //(type.mime_type == L"image/x-targa"             ) ? ImageDecoder_stbi : // TGA has no real unique header identifier, so just use the file extension on those
          (type.mime_type == L"image/vnd.ms-photo") ? ImageDecoder_WIC :
          (type.mime_type == L"image/webp") ? ImageDecoder_WIC :
          (type.mime_type == L"image/tiff") ? ImageDecoder_WIC :
          (type.mime_type == L"image/heif") ? ImageDecoder_WIC :
          (type.mime_type == L"image/heic") ? ImageDecoder_WIC :
          (type.mime_type == L"image/avif") ? ImageDecoder_AVIF :
          (type.mime_type == L"image/jxl") ? ImageDecoder_JXL :
          (type.mime_type == L"image/vnd-ms.dds") ? ImageDecoder_DDS :
#ifdef _M_X64
          (type.mime_type == L"image/x-exr") ? ImageDecoder_EXR :
#endif
          ImageDecoder_WIC;   // Not actually being used

        // None of this is technically correct other than the .hdr case,
        //   they can all be SDR or HDR.
        if (type.mime_type == L"image/vnd.radiance" || // .hdr
          type.mime_type == L"image/vnd.ms-photo" || // .jxr
          type.mime_type == L"image/avif" || // .avif
          type.mime_type == L"image/x-exr")          // .exr
        {
          image.is_hdr = true;
        }

        if (type.mime_type == L"image/png")
        {
          // XXX: Check for the appropriate chunk
          need_srgb = true;
        }

        break;
      }
    }
  }

  SK_AutoFile _(pImageFile);

  auto _scratchMemory =
    std::make_unique <unsigned char[]>(_.getInitialSize());

  LOG_ERROR_IF(decoder == ImageDecoder_None) << "Failed to detect file type!";
  LOG_DEBUG_IF(decoder == ImageDecoder_stbi) << "Using stbi decoder...";
  LOG_DEBUG_IF(decoder == ImageDecoder_WIC)  << "Using WIC decoder...";
  LOG_DEBUG_IF(decoder == ImageDecoder_DDS)  << "Using DDS decoder...";
#ifdef _M_X64
  LOG_DEBUG_IF(decoder == ImageDecoder_JXL)  << "Using JPEG XL decoder...";
  LOG_DEBUG_IF(decoder == ImageDecoder_EXR)  << "Using OpenEXR decoder...";
#endif
  LOG_DEBUG_IF(decoder == ImageDecoder_HDR)  << "Using Radiance HDR decoder...";
  LOG_DEBUG_IF(decoder == ImageDecoder_UHDR) << "Using Ultra HDR decoder...";

  if (decoder == ImageDecoder_None)
    return false;

  if (decoder == ImageDecoder_UHDR)
  {
    fseek(pImageFile, 0, SEEK_SET);
    fread(_scratchMemory.get(), _.getInitialSize(), 1, pImageFile);
    rewind(pImageFile);

    image.light_info.isHDR = true;
    image.is_hdr = true;

    SKIV_Image_LoadUltraHDR(img, _scratchMemory.get(), static_cast <int> (_.getInitialSize()));

    meta = img.GetMetadata();
    meta.dimension = DirectX::TEX_DIMENSION_TEXTURE2D;

    converted = true;
    succeeded = true;
  }

  if (decoder == ImageDecoder_stbi)
  {
    // If desired_channels is non-zero, *channels_in_file has the number of components that _would_ have been
    // output otherwise. E.g. if you set desired_channels to 4, you will always get RGBA output, but you can
    // check *channels_in_file to see if it's trivially opaque because e.g. there were only 3 channels in the source image.

    int width = 0,
      height = 0,
      channels_in_file = 0,
      desired_channels = STBI_rgb_alpha;

    SKIV_STBI_CICP = { };
    SKIV_STBI_SBIT = { };
    SKIV_STBI_ResultInfo = { };
    SKIV_STBI_srgb = false;

    // Stop using STB, it was convenient to parse a few of the chunks,
    //   but it cannot be used for anything else due to gamma issues.
    //#define HAS_WORKING_STB_GAMMA
#define STBI_FLOAT
#if defined (STBI_FLOAT) || !defined(HAS_WORKING_STB_GAMMA)
    // Check whether the image is a HDR image or not
    image.light_info.isHDR = stbi_is_hdr_from_file(pImageFile);

    LOG << "STBI thinks the image is... " << ((image.light_info.isHDR) ? "HDR" : "SDR");

    fseek(pImageFile, 0, SEEK_SET);
    fread(_scratchMemory.get(), _.getInitialSize(), 1, pImageFile);
    rewind(pImageFile);

    bool cicp = false;

    if (image_sig->mime_type == L"image/png")
    {
      SKIV_STBI_ICCP.iCCP = false;

      std::string_view     data_view((const char*)_scratchMemory.get(), _.getInitialSize());
      if (auto cicp_pos = data_view.find("cICP", 0, 4);
        cicp_pos != data_view.npos)
      {
        memcpy(&SKIV_STBI_CICP, &_scratchMemory.get()[cicp_pos + 4], 4);

        cicp = true;
      }

      if (auto sbit_pos = data_view.find("sBIT", 0, 4);
        sbit_pos != data_view.npos)
      {
        unsigned long size =
          *((unsigned long*)&_scratchMemory.get()[sbit_pos - 4]);

#if (defined _M_IX86) || (defined _M_X64)
        size = _byteswap_ulong(size);
#endif

        memcpy(&SKIV_STBI_SBIT, &_scratchMemory.get()[sbit_pos + 4], std::min(4ul, size));
      }

      if (auto iccp_pos = data_view.find("iCCP", 0, 4);
        iccp_pos != data_view.npos)
      {
        unsigned long size =
          *((unsigned long*)&_scratchMemory.get()[iccp_pos - 4]);

#if (defined _M_IX86) || (defined _M_X64)
        size = _byteswap_ulong(size);
#endif

        SKIV_STBI_ICCP.iCCP = true;
        //memcpy (&SKIV_STBI_SBIT, &_scratchMemory.get ()[iccp_pos+4], std::min (4ul, size));
      }
    }

    float* pixels = SKIV_STBI_CICP.primaries != 0 ?
      nullptr :
      stbi_loadf_from_memory(_scratchMemory.get(), static_cast <int> (_.getInitialSize()), &width, &height, &channels_in_file, desired_channels);
    typedef float         pixel_size;
    DXGI_FORMAT           dxgi_format = DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT;
#ifndef STBI_FLOAT
    unsigned char* pixels = stbi_load(szPath.c_str(), &width, &height, &channels_in_file, desired_channels);
    typedef unsigned char pixel_size;
    constexpr DXGI_FORMAT dxgi_format = DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;
#endif

    // Fall back to using WIC if STB fails to parse the file
    if (pixels == NULL && SKIV_STBI_CICP.primaries == 0)
    {
      decoder = ImageDecoder_WIC;
      LOG_E << "Using WIC decoder due to STB failing with: " << stbi_failure_reason();
    }

    else if (pixels != nullptr && SKIV_STBI_srgb && !cicp && !SKIV_STBI_ICCP.iCCP)
    {
      decoder = ImageDecoder_WIC;
      LOG_E << "Using WIC decoder due to STB incorrectly handling sRGB";
    }
#endif

    else
    {
#ifdef _DEBUG

      LOG << "SKIV_STBI_cICP:";
      LOG << ".primaries    : " << (int)SKIV_STBI_CICP.primaries;
      LOG << ".transfer_func: " << (int)SKIV_STBI_CICP.transfer_func;
      LOG << ".matrix_coeffs: " << (int)SKIV_STBI_CICP.matrix_coeffs;
      LOG << ".full_range   : " << (int)SKIV_STBI_CICP.full_range;

#endif // _DEBUG

      if (SKIV_STBI_ICCP.iCCP && SKIV_STBI_SBIT.red_bits <= 8)
      {
        SKIV_STBI_CICP.primaries = 9;
        SKIV_STBI_CICP.transfer_func = 16;
        SKIV_STBI_CICP.matrix_coeffs = 0;
      }

      if (SKIV_STBI_CICP.primaries != 0)
      {
        if (SKIV_STBI_SBIT.red_bits > 8)
          dxgi_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        else
          dxgi_format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

        assert(SKIV_STBI_CICP.primaries == 9); // BT 2020
        assert(SKIV_STBI_CICP.transfer_func == 16); // ST 2084
        assert(SKIV_STBI_CICP.matrix_coeffs == 0); // Identity
        // RGB is currently the only supported color model in PNG,
        //   and as such Matrix Coefficients shall be set to 0.
        // 
        // But presumably it may eventually also be:
        //    0 (RGB)
        //    9 (BT.2020 Non-Constant Luminance)
        //   10 (BT.2020 Constant Luminance)
        //   14 (BT.2100 ICtCp)

        image.light_info.isHDR = true;
        image.is_hdr = true;
      }

      meta.width = width;
      meta.height = height;
      meta.depth = 1;
      meta.arraySize = 1;
      meta.mipLevels = 1;
      meta.format = dxgi_format; // STBI_rgb_alpha
      meta.dimension = DirectX::TEX_DIMENSION_TEXTURE2D;

      if (dxgi_format == DXGI_FORMAT_R32G32B32A32_FLOAT ||
        ((dxgi_format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
          dxgi_format == DXGI_FORMAT_R16G16B16A16_FLOAT) && image.is_hdr))
      {
#ifndef HAS_WORKING_STB_GAMMA
        if (!((dxgi_format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
          dxgi_format == DXGI_FORMAT_R16G16B16A16_FLOAT) && image.is_hdr))
        {
          decoder = ImageDecoder_WIC;
          LOG << "Using WIC decoder due to STB incorrectly handling sRGB";
        }
#endif
        // Good grief this is inefficient, let's convert it to something reasonable...
        DirectX::ScratchImage raw_fp32_img;

        // Check for BT.2020 using ST.2084 (HDR10)
        if (SKIV_STBI_CICP.primaries == 9 &&
          SKIV_STBI_CICP.transfer_func == 16)
        {
          DirectX::ScratchImage temp_img = { };
          DirectX::ScratchImage temp_img2 = { };

          if (SUCCEEDED(
            DirectX::LoadFromWICMemory(
              _scratchMemory.get(), _.getInitialSize(),
              DirectX::WIC_FLAGS_FILTER_POINT | DirectX::WIC_FLAGS_FORCE_LINEAR,
              &meta, temp_img)))
          {
            image.bpc =
              (int)DirectX::BitsPerColor(meta.format);
            image.channels =
              DirectX::HasAlpha(meta.format) ? 4 : 3; // Expect 3... 4 would be weird for an HDR image

            LOG << "HDR10 PNG detected, transforming to scRGB...";

            // PNG will be loaded as UNORM, we need to convert to float...
            if (SUCCEEDED(DirectX::Convert(*temp_img.GetImages(), DXGI_FORMAT_R16G16B16A16_FLOAT, DirectX::TEX_FILTER_DEFAULT, 0.0f, temp_img2)))
              if (SUCCEEDED(img.InitializeFromImage(*temp_img2.GetImage(0, 0, 0))))
              {
                using namespace DirectX;

                TransformImage(temp_img2.GetImages(),
                  temp_img2.GetImageCount(),
                  temp_img2.GetMetadata(),
                  [&](XMVECTOR* outPixels, const XMVECTOR* inPixels, size_t width, size_t y)
                  {
                    UNREFERENCED_PARAMETER(y);

                    for (size_t j = 0; j < width; ++j)
                    {
                      XMVECTOR v = inPixels[j];

                      outPixels[j] =
                        XMVector3Transform(SKIV_Image_PQToLinear(v), c_Bt2100toscRGB);
                    }
                  }, img);

                meta.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
                converted = true;
                succeeded = true;

                if (SKIV_STBI_SBIT.red_bits != 0)
                {
                  image.bpc = SKIV_STBI_SBIT.red_bits;

                  image.channels = 0;

                  if (SKIV_STBI_SBIT.red_bits > 0) image.channels++;
                  if (SKIV_STBI_SBIT.green_bits > 0) image.channels++;
                  if (SKIV_STBI_SBIT.blue_bits > 0) image.channels++;
                  if (SKIV_STBI_SBIT.alpha_bits > 0) image.channels++;
                }
              }
          }
        }

        if ((!converted) && SUCCEEDED(raw_fp32_img.Initialize2D(meta.format, width, height, 1, 1)))
        {
          size_t   imageSize = width * height * desired_channels * sizeof(pixel_size);
          uint8_t* pDest = raw_fp32_img.GetImage(0, 0, 0)->pixels;
          memcpy(pDest, pixels, imageSize);

          image.bpc = SKIV_STBI_ResultInfo.bits_per_channel;
          image.channels = channels_in_file;

          if (image.bpc > 32)
            image.bpc = 32;

          // Still overkill for SDR, but we're saving some VRAM...
          const DXGI_FORMAT final_format =
            image.bpc <= 8 ? //image.channels == 1 ? DXGI_FORMAT_R16_UNORM          :
            //image.channels == 2 ? DXGI_FORMAT_R16G16_UNORM       :
            //image.channels == 3 ? DXGI_FORMAT_R16G16B16A16_UNORM :
            //image.channels == 4 ? DXGI_FORMAT_R16G16B16A16_UNORM
            //                      : DXGI_FORMAT_UNKNOWN
            image.channels == 1 ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB :
            image.channels == 2 ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB :
            image.channels == 3 ? DXGI_FORMAT_B8G8R8X8_UNORM_SRGB :
            image.channels == 4 ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            : DXGI_FORMAT_UNKNOWN
            :
            image.bpc <= 10 ? //image.channels == 1 ? DXGI_FORMAT_R16_UNORM          :
            //image.channels == 2 ? DXGI_FORMAT_R16G16_UNORM       :
            //image.channels == 3 ? DXGI_FORMAT_R16G16B16A16_UNORM :
            //image.channels == 4 ? DXGI_FORMAT_R16G16B16A16_UNORM
            //                      : DXGI_FORMAT_UNKNOWN
            image.channels == 1 ? DXGI_FORMAT_R10G10B10A2_UNORM :
            image.channels == 2 ? DXGI_FORMAT_R10G10B10A2_UNORM :
            image.channels == 3 ? DXGI_FORMAT_R10G10B10A2_UNORM :
            image.channels == 4 ? DXGI_FORMAT_R10G10B10A2_UNORM
            : DXGI_FORMAT_UNKNOWN
            :
            image.bpc == 16 ? image.channels == 1 ? DXGI_FORMAT_R16G16B16A16_UNORM :
            image.channels == 2 ? DXGI_FORMAT_R16G16B16A16_UNORM :
            image.channels == 3 ? DXGI_FORMAT_R16G16B16A16_UNORM :
            image.channels == 4 ? DXGI_FORMAT_R16G16B16A16_UNORM
            : DXGI_FORMAT_UNKNOWN
            :
            image.bpc >= 32 ? image.channels == 1 ? DXGI_FORMAT_R32G32B32A32_FLOAT :
            image.channels == 2 ? DXGI_FORMAT_R32G32B32A32_FLOAT :
            image.channels == 3 ? DXGI_FORMAT_R32G32B32A32_FLOAT :
            image.channels == 4 ? DXGI_FORMAT_R32G32B32A32_FLOAT
            : DXGI_FORMAT_UNKNOWN
            : DXGI_FORMAT_UNKNOWN;

          if (SUCCEEDED(DirectX::Convert(*raw_fp32_img.GetImages(), final_format, DirectX::TEX_FILTER_DEFAULT, 0.0f, img)))
          {
            meta.format = final_format;
            converted = true;
            succeeded = true;
          }
        }
      }

      if (converted == false && SUCCEEDED(img.Initialize2D(meta.format, width, height, 1, 1)))
      {
        image.bpc = SKIV_STBI_ResultInfo.bits_per_channel;
        image.channels = channels_in_file;

        size_t   imageSize = width * height * desired_channels * sizeof(pixel_size);
        uint8_t* pDest = img.GetImage(0, 0, 0)->pixels;
        memcpy(pDest, pixels, imageSize);

        succeeded = true;
      }

      stbi_image_free(pixels);

      //decoder = ImageDecoder_WIC;
      //LOG_I << "Using WIC decoder";
    }
  }

  if (decoder == ImageDecoder_WIC)
  {
    fseek(pImageFile, 0, SEEK_SET);
    fread(_scratchMemory.get(), _.getInitialSize(), 1, pImageFile);
    rewind(pImageFile);

    if (SUCCEEDED(
      DirectX::LoadFromWICMemory(
        _scratchMemory.get(), _.getInitialSize(),
        DirectX::WIC_FLAGS_FILTER_POINT | DirectX::WIC_FLAGS_DEFAULT_SRGB,
        &meta, img)))
    {
      image.bpc =
        (int)DirectX::BitsPerColor(meta.format);
      image.channels =
        DirectX::HasAlpha(meta.format) ? 4 : 3; // 2 and 1 channel images are unsupported for now


      if (image.is_hdr && (image_sig->mime_type == L"image/vnd.ms-photo" ||
        image_sig->mime_type == L"image/avif"))
      {
        if (DirectX::BitsPerColor(meta.format) < 16)
        {
          LOG_W << "HDR capable image format does not contain HDR pixels... demoting to SDR!";
          image.is_hdr = false;

          if (!DirectX::IsSRGB(meta.format))
            need_srgb = true;
        }
        else
        {
          image.light_info.isHDR = true;
        }
      }

      // DirectXTex does not handle gamma on 10-bpc PNGs correctly
      if (need_srgb && DirectX::BitsPerColor(meta.format) != 8)
      {
        using namespace DirectX;

        TransformImage(*img.GetImages(),
          [&](_Out_writes_(width)       XMVECTOR* outPixels,
            _In_reads_(width) const XMVECTOR* inPixels,
            size_t    width,
            size_t)
          {
            for (size_t j = 0; j < width; ++j)
            {
              XMVECTOR value =
                inPixels[j];
              outPixels[j] =
                XMColorSRGBToRGB(
                  XMVectorSaturate(value)
                );
            }
          }, img_srgb
        );

        std::swap(img, img_srgb);
      }

      succeeded = true;
    }
  }

  if (decoder == ImageDecoder_DDS)
  {
    fseek(pImageFile, 0, SEEK_SET);
    fread(_scratchMemory.get(), _.getInitialSize(), 1, pImageFile);
    rewind(pImageFile);

    if (SUCCEEDED(
      DirectX::LoadFromDDSMemory(
        _scratchMemory.get(), _.getInitialSize(),
        DirectX::DDS_FLAGS_PERMISSIVE,
        &meta, img)))
    {
      succeeded = true;
      meta = img.GetMetadata();

      image.bpc = static_cast <int> (DirectX::BitsPerColor(meta.format));
      image.channels = SKIV_DXGI_NumberOfChannels(meta.format);
      image.is_dds = true;

      if (DirectX::MakeTypeless(meta.format) == DXGI_FORMAT_BC6H_TYPELESS)
      {
        image.is_hdr = true;
      }
    }
  }

#ifdef _M_X64
  if (decoder == ImageDecoder_EXR)
  {
    using namespace DirectX;

    TexMetadata exr_meta;

    if (SUCCEEDED(LoadFromEXRFile(imagePath.c_str(), &exr_meta, img)))
    {
      image.bpc = static_cast <int> (DirectX::BitsPerColor(exr_meta.format));
      image.channels = 3 + (DirectX::HasAlpha(exr_meta.format) ?
        1 : 0);

      succeeded = true;

      image.light_info.isHDR = true;
      image.is_hdr = true;

      image.width = static_cast <float> (exr_meta.width);
      image.height = static_cast <float> (exr_meta.height);

      meta.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
      meta.width = static_cast <size_t> (image.width);
      meta.height = static_cast <size_t> (image.height);
      meta.depth = 1;
      meta.arraySize = 1;
      meta.mipLevels = 1;
      meta.dimension = DirectX::TEX_DIMENSION_TEXTURE2D;
    }
  }
#endif

  if (decoder == ImageDecoder_HDR)
  {
    fseek(pImageFile, 0, SEEK_SET);
    fread(_scratchMemory.get(), _.getInitialSize(), 1, pImageFile);
    rewind(pImageFile);

    using namespace DirectX;

    TexMetadata hdr_meta;

    if (SUCCEEDED(LoadFromHDRMemory(_scratchMemory.get(), _.getInitialSize(), &hdr_meta, img)))
    {
      image.bpc = static_cast <int> (DirectX::BitsPerColor(hdr_meta.format));
      image.channels = 3 + (DirectX::HasAlpha(hdr_meta.format) ?
        1 : 0);

      succeeded = true;

      image.light_info.isHDR = true;
      image.is_hdr = true;

      image.width = static_cast <float> (hdr_meta.width);
      image.height = static_cast <float> (hdr_meta.height);

      meta.format = hdr_meta.format;
      meta.width = static_cast <size_t> (image.width);
      meta.height = static_cast <size_t> (image.height);
      meta.depth = 1;
      meta.arraySize = 1;
      meta.mipLevels = 1;
      meta.dimension = DirectX::TEX_DIMENSION_TEXTURE2D;
    }
  }

  if (decoder == ImageDecoder_AVIF)
  {
    if (!isAVIFEncoderAvailable())
    {
      LOG_E << L"Unworkable libavif DLL present, will attempt to re-download the next time an AVIF image is decoded.";
    }

    else
    {
      auto avif_decoder =
        SK_avifDecoderCreate();

      SYSTEM_INFO     si = { };
      GetSystemInfo(&si);

      avif_decoder->maxThreads =
        std::min(64U, std::min((UINT)si.dwNumberOfProcessors, (UINT)__popcnt64(si.dwActiveProcessorMask)));

      fseek(pImageFile, 0, SEEK_SET);
      fread(_scratchMemory.get(), _.getInitialSize(), 1, pImageFile);
      rewind(pImageFile);

      SK_avifDecoderSetIOMemory(avif_decoder, _scratchMemory.get(), _.getInitialSize());
      SK_avifDecoderParse(avif_decoder);

      // We only want 1 image, if there are more... too bad.
      if (SK_avifDecoderNextImage(avif_decoder) == AVIF_RESULT_OK)
      {
        avifRGBImage                 rgb;
        SK_avifRGBImageSetDefaults(&rgb, avif_decoder->image);

        int bpc = rgb.depth;


        rgb.depth = 16;
        rgb.format = AVIF_RGB_FORMAT_RGBA;
        rgb.maxThreads = std::min(64U, std::min((UINT)si.dwNumberOfProcessors, (UINT)__popcnt64(si.dwActiveProcessorMask)));
        rgb.ignoreAlpha = true;
        rgb.isFloat = true;

        SK_avifRGBImageAllocatePixels(&rgb);
        SK_avifImageYUVToRGB(avif_decoder->image, &rgb);

        image.width = static_cast <float> (rgb.width);
        image.height = static_cast <float> (rgb.height);

        DirectX::ScratchImage temp_img;

        if (SUCCEEDED(temp_img.Initialize2D(DXGI_FORMAT_R16G16B16A16_FLOAT, static_cast <size_t> (image.width),
          static_cast <size_t> (image.height), 1, 1)))
        {
          using namespace DirectX;

          image.channels = 3;
          image.bpc = bpc;

          // XXX
          image.light_info.isHDR = true;
          image.is_hdr = true;

          succeeded = true;

          meta.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
          meta.width = static_cast <size_t> (image.width);
          meta.height = static_cast <size_t> (image.height);
          meta.depth = 1;
          meta.arraySize = 1;
          meta.mipLevels = 1;
          meta.dimension = DirectX::TEX_DIMENSION_TEXTURE2D;

          void* pixels_buffer = static_cast <void*>(temp_img.GetPixels());

#ifdef _DEBUG
          size_t pixels_buffer_size = temp_img.GetPixelsSize();
          assert(pixels_buffer_size >= rgb.rowBytes * rgb.height);
#endif

          memcpy(pixels_buffer, rgb.pixels, rgb.rowBytes * rgb.height);

          if (SUCCEEDED(TransformImage(*temp_img.GetImages(),
            [&](XMVECTOR* outPixels,
              const XMVECTOR* inPixels,
              size_t    width,
              size_t    y)
            {
              UNREFERENCED_PARAMETER(y);

              for (size_t j = 0; j < width; ++j)
              {
                XMVECTOR v = inPixels[j];

                v =
                  XMVector3Transform(SKIV_Image_PQToLinear(v), c_Bt2100toscRGB);

                outPixels[j] = v;
              }
            }, img)
          )
            )
          {
            temp_img.Release();
          }
        }

        SK_avifRGBImageFreePixels(&rgb);
      }

      SK_avifDecoderDestroy(avif_decoder);
    }
  }

  if (decoder == ImageDecoder_JXL)
  {
    static HMODULE hModJXL;
    hModJXL = LoadLibraryW(L"jxl.dll");

    static HMODULE hModJXLThreads;
    hModJXLThreads = LoadLibraryW(L"jxl_threads.dll");

    using JxlDecoderCreate_pfn = JxlDecoder * (*)(const JxlMemoryManager* memory_manager);
    using JxlDecoderDestroy_pfn = void             (*)(JxlDecoder* dec);
    using JxlDecoderSubscribeEvents_pfn = JxlDecoderStatus(*)(JxlDecoder* dec, int events_wanted);
    using JxlDecoderSetInput_pfn = JxlDecoderStatus(*)(JxlDecoder* dec, const uint8_t* data, size_t  size);
    using JxlDecoderImageOutBufferSize_pfn = JxlDecoderStatus(*)(const JxlDecoder* dec, const JxlPixelFormat* format, size_t* size);
    using JxlDecoderSetImageOutBuffer_pfn = JxlDecoderStatus(*)(JxlDecoder* dec, const JxlPixelFormat* format, void* buffer, size_t  size);
    using JxlDecoderSetImageOutBitDepth_pfn = JxlDecoderStatus(*)(JxlDecoder* dec, const JxlBitDepth* bit_depth);
    using JxlDecoderGetBasicInfo_pfn = JxlDecoderStatus(*)(const JxlDecoder* dec, JxlBasicInfo* info);
    using JxlDecoderProcessInput_pfn = JxlDecoderStatus(*)(JxlDecoder* dec);
    using JxlDecoderCloseInput_pfn = void             (*)(JxlDecoder* dec);
    using JxlDecoderSetPreferredColorProfile_pfn = JxlDecoderStatus(*)(JxlDecoder* dec, const JxlColorEncoding* color_encoding);
    using JxlDecoderGetColorAsEncodedProfile_pfn = JxlDecoderStatus(*)(const JxlDecoder* dec, JxlColorProfileTarget target, JxlColorEncoding* color_encoding);
    using JxlDecoderSetParallelRunner_pfn = JxlDecoderStatus(*)(JxlDecoder* dec,
      JxlParallelRunner parallel_runner,
      void* parallel_runner_opaque);

    using JxlResizableParallelRunnerCreate_pfn = void* (*)(const JxlMemoryManager* memory_manager);
    using JxlResizableParallelRunnerSuggestThreads_pfn = uint32_t(*)(uint64_t xsize, uint64_t ysize);
    using JxlResizableParallelRunnerSetThreads_pfn = void     (*)(void* runner_opaque, size_t num_threads);
    using JxlResizableParallelRunnerDestroy_pfn = void     (*)(void* runner_opaque);
    using JxlResizableParallelRunner_pfn = JxlParallelRetCode(*)(void* runner_opaque,
      void* jpegxl_opaque,
      JxlParallelRunInit     init,
      JxlParallelRunFunction func,
      uint32_t               start_range,
      uint32_t               end_range);

    JxlDecoderSubscribeEvents_pfn          jxlDecoderSubscribeEvents = (JxlDecoderSubscribeEvents_pfn)GetProcAddress(hModJXL, "JxlDecoderSubscribeEvents");
    JxlDecoderSetInput_pfn                 jxlDecoderSetInput = (JxlDecoderSetInput_pfn)GetProcAddress(hModJXL, "JxlDecoderSetInput");
    JxlDecoderImageOutBufferSize_pfn       jxlDecoderImageOutBufferSize = (JxlDecoderImageOutBufferSize_pfn)GetProcAddress(hModJXL, "JxlDecoderImageOutBufferSize");
    JxlDecoderCreate_pfn                   jxlDecoderCreate = (JxlDecoderCreate_pfn)GetProcAddress(hModJXL, "JxlDecoderCreate");
    JxlDecoderDestroy_pfn                  jxlDecoderDestroy = (JxlDecoderDestroy_pfn)GetProcAddress(hModJXL, "JxlDecoderDestroy");
    JxlDecoderGetBasicInfo_pfn             jxlDecoderGetBasicInfo = (JxlDecoderGetBasicInfo_pfn)GetProcAddress(hModJXL, "JxlDecoderGetBasicInfo");
    JxlDecoderProcessInput_pfn             jxlDecoderProcessInput = (JxlDecoderProcessInput_pfn)GetProcAddress(hModJXL, "JxlDecoderProcessInput");
    JxlDecoderCloseInput_pfn               jxlDecoderCloseInput = (JxlDecoderCloseInput_pfn)GetProcAddress(hModJXL, "JxlDecoderCloseInput");
    JxlDecoderSetImageOutBuffer_pfn        jxlDecoderSetImageOutBuffer = (JxlDecoderSetImageOutBuffer_pfn)GetProcAddress(hModJXL, "JxlDecoderSetImageOutBuffer");
    JxlDecoderSetImageOutBitDepth_pfn      jxlDecoderSetImageOutBitDepth = (JxlDecoderSetImageOutBitDepth_pfn)GetProcAddress(hModJXL, "JxlDecoderSetImageOutBitDepth");
    JxlDecoderSetParallelRunner_pfn        jxlDecoderSetParallelRunner = (JxlDecoderSetParallelRunner_pfn)GetProcAddress(hModJXL, "JxlDecoderSetParallelRunner");
    JxlDecoderSetPreferredColorProfile_pfn jxlDecoderSetPreferredColorProfile = (JxlDecoderSetPreferredColorProfile_pfn)GetProcAddress(hModJXL, "JxlDecoderSetPreferredColorProfile");
    JxlDecoderGetColorAsEncodedProfile_pfn jxlDecoderGetColorAsEncodedProfile = (JxlDecoderGetColorAsEncodedProfile_pfn)GetProcAddress(hModJXL, "JxlDecoderGetColorAsEncodedProfile");

    JxlResizableParallelRunnerCreate_pfn         jxlResizableParallelRunnerCreate = (JxlResizableParallelRunnerCreate_pfn)GetProcAddress(hModJXLThreads, "JxlResizableParallelRunnerCreate");
    JxlResizableParallelRunnerSuggestThreads_pfn jxlResizableParallelRunnerSuggestThreads = (JxlResizableParallelRunnerSuggestThreads_pfn)GetProcAddress(hModJXLThreads, "JxlResizableParallelRunnerSuggestThreads");
    JxlResizableParallelRunnerSetThreads_pfn     jxlResizableParallelRunnerSetThreads = (JxlResizableParallelRunnerSetThreads_pfn)GetProcAddress(hModJXLThreads, "JxlResizableParallelRunnerSetThreads");
    JxlResizableParallelRunnerDestroy_pfn        jxlResizableParallelRunnerDestroy = (JxlResizableParallelRunnerDestroy_pfn)GetProcAddress(hModJXLThreads, "JxlResizableParallelRunnerDestroy");
    JxlResizableParallelRunner_pfn               jxlResizableParallelRunner = (JxlResizableParallelRunner_pfn)GetProcAddress(hModJXLThreads, "JxlResizableParallelRunner");

    JxlDecoder* jxl_decoder = jxlDecoderCreate != nullptr &&
      jxlResizableParallelRunnerCreate != nullptr ?
      jxlDecoderCreate(nullptr) : nullptr;
    void* jxl_runner = jxlResizableParallelRunnerCreate != nullptr ?
      jxlResizableParallelRunnerCreate(nullptr) : nullptr;

    if (jxl_decoder != nullptr &&
      jxl_runner != nullptr &&
      JXL_DEC_SUCCESS ==
      jxlDecoderSubscribeEvents(jxl_decoder, JXL_DEC_BASIC_INFO |
        JXL_DEC_COLOR_ENCODING |
        JXL_DEC_FULL_IMAGE) &&
      JXL_DEC_SUCCESS ==
      jxlDecoderSetParallelRunner(jxl_decoder, jxlResizableParallelRunner,
        jxl_runner))
    {
      JxlColorEncoding actual_encoding = { };

      JxlBasicInfo   info = { };
      JxlPixelFormat format =
      { 4, JXL_TYPE_FLOAT, JXL_NATIVE_ENDIAN, 0 };

      auto bpp =
        (format.data_type == JXL_TYPE_FLOAT) ? (sizeof(float) * format.num_channels) :
        (format.data_type == JXL_TYPE_UINT8) ? (sizeof(uint8_t) * format.num_channels) :
        (format.data_type == JXL_TYPE_UINT16) ? (sizeof(uint16_t) * format.num_channels) :
        (format.data_type == JXL_TYPE_FLOAT16) ? (sizeof(float) / 2) * format.num_channels :
        sizeof(uint8_t) * format.num_channels;

      fseek(pImageFile, 0, SEEK_SET);
      fread(_scratchMemory.get(), _.getInitialSize(), 1, pImageFile);
      rewind(pImageFile);

      jxlDecoderSetInput(jxl_decoder, _scratchMemory.get(), _.getInitialSize());
      jxlDecoderCloseInput(jxl_decoder);

      for (;;)
      {
        JxlDecoderStatus status =
          jxlDecoderProcessInput(jxl_decoder);

        if (status == JXL_DEC_ERROR)
        {
          LOG_E << "Decoder error";
          break;
        }

        else if (status == JXL_DEC_NEED_MORE_INPUT)
        {
          LOG_E << "Error, already provided all input";
          break;
        }

        else if (status == JXL_DEC_BASIC_INFO)
        {
          if (JXL_DEC_SUCCESS != jxlDecoderGetBasicInfo(jxl_decoder, &info))
          {
            LOG_E << "JxlDecoderGetBasicInfo failed";
            break;
          }

          image.width = static_cast <float> (info.xsize);
          image.height = static_cast <float> (info.ysize);

          jxlResizableParallelRunnerSetThreads(jxl_runner,
            std::max(8U, jxlResizableParallelRunnerSuggestThreads(info.xsize, info.ysize)));
        }

        else if (status == JXL_DEC_COLOR_ENCODING)
        {
          static constexpr JxlColorEncoding
            scrgb_encoding = { .color_space = JXL_COLOR_SPACE_RGB,
                               .white_point = JXL_WHITE_POINT_D65,
                               .primaries = JXL_PRIMARIES_SRGB,
                               .transfer_function = JXL_TRANSFER_FUNCTION_LINEAR,
                               .rendering_intent = JXL_RENDERING_INTENT_PERCEPTUAL };

          if (JXL_DEC_SUCCESS !=
            jxlDecoderSetPreferredColorProfile(jxl_decoder, &scrgb_encoding))
          {
            LOG_E << "JxlDecoderSetPreferredColorProfile failed";
          }

          if (JXL_DEC_SUCCESS !=
            jxlDecoderGetColorAsEncodedProfile(jxl_decoder, JXL_COLOR_PROFILE_TARGET_DATA, &actual_encoding))
          {
            LOG_E << "jxlDecoderGetColorAsEncodedProfile failed";
          }
        }

        else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER)
        {
          size_t buffer_size;
          if (JXL_DEC_SUCCESS !=
            jxlDecoderImageOutBufferSize(jxl_decoder, &format, &buffer_size))
          {
            LOG_E << "JxlDecoderImageOutBufferSize failed";
            break;
          }

          if (buffer_size != image.width * image.height * bpp)
          {
            LOG_E << "Invalid out buffer size " << buffer_size << " " << (int)image.width * (int)image.height * bpp;
            break;
          }

          if (SUCCEEDED(img.Initialize2D(DXGI_FORMAT_R32G32B32A32_FLOAT, static_cast <size_t> (image.width),
            static_cast <size_t> (image.height), 1, 1)))
          {
            image.bpc = info.bits_per_sample;
            image.channels = info.num_color_channels;

            void* pixels_buffer = static_cast <void*>(img.GetPixels());
            size_t pixels_buffer_size = img.GetPixelsSize();

            std::ignore = jxlDecoderSetImageOutBitDepth;

            if (JXL_DEC_SUCCESS != jxlDecoderSetImageOutBuffer(jxl_decoder, &format,
              pixels_buffer,
              pixels_buffer_size))
            {
              LOG_E << "JxlDecoderSetImageOutBuffer failed";
              break;
            }
          }
        }

        else if (status == JXL_DEC_FULL_IMAGE)
        {
          // Nothing to do. Do not yet return. If the image is an animation, more
          // full frames may be decoded. This example only keeps the last one.
        }

        else if (status == JXL_DEC_SUCCESS)
        {
          succeeded = true;

          meta.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
          meta.width = static_cast <size_t> (image.width);
          meta.height = static_cast <size_t> (image.height);
          meta.depth = 1;
          meta.arraySize = 1;
          meta.mipLevels = 1;
          meta.dimension = DirectX::TEX_DIMENSION_TEXTURE2D;

          bool wcg = false;
          bool hdr = false;

          using namespace DirectX;

          ScratchImage converted_img;

          const XMVECTOR vRelativeToAbsoluteNits =
            XMVectorReplicate(info.intensity_target != 255.0f ? info.intensity_target / 80.0f
              : 1.0f);

          const bool bIsHDR10 =
            actual_encoding.primaries == JXL_PRIMARIES_2100 &&
            actual_encoding.transfer_function == JXL_TRANSFER_FUNCTION_PQ;

          const bool bIsRec709Linear =
            actual_encoding.primaries == JXL_PRIMARIES_SRGB &&
            actual_encoding.transfer_function == JXL_TRANSFER_FUNCTION_LINEAR;

          if (actual_encoding.white_point != JXL_WHITE_POINT_D65)
          {
            LOG_W << "Unexpected non-D65 white point";
          }

          if (!(bIsHDR10 || bIsRec709Linear))
          {
            LOG << "Encoded image is neither HDR10 nor scRGB...";
          }

          if (SUCCEEDED(TransformImage(*img.GetImages(),
            [&](    XMVECTOR* outPixels,
              const XMVECTOR* inPixels,
              size_t    width,
              size_t    y)
            {
              UNREFERENCED_PARAMETER(y);

              for (size_t j = 0; j < width; ++j)
              {
                XMVECTOR v = inPixels[j];

                if (bIsRec709Linear)
                {
                  v =
                    XMVectorMultiply(v, vRelativeToAbsoluteNits);
                }

                else if (bIsHDR10)
                {
                  v =
                    XMVector3Transform(SKIV_Image_PQToLinear(v), c_Bt2100toscRGB);
                }

                uint32_t xm_test_rec709 = 0x0,
                  xm_test_hdr = 0x0;

                if (XMVectorGreaterOrEqualR(&xm_test_rec709, v, g_XMZero);
                  XMComparisonAnyFalse(xm_test_rec709))
                {
                  wcg = true;
                }

                if (XMVectorGreaterR(&xm_test_hdr, v, g_XMOne);
                  XMComparisonAnyTrue(xm_test_hdr))
                {
                  hdr = true;
                }

                outPixels[j] = v;
              }
            }, converted_img)
          )
            )
          {
            DirectX::Convert(*converted_img.GetImages(), DXGI_FORMAT_R16G16B16A16_FLOAT, DirectX::TEX_FILTER_DEFAULT, 0.0f, img);
          }

          image.light_info.isHDR = wcg || hdr;
          image.is_hdr = wcg || hdr;
          break;
        }
        else
        {
          LOG_E << "Unknown decoder status";
          break;
        }
      }
    }

    if (jxl_decoder != nullptr)
      jxlDecoderDestroy(jxl_decoder);

    if (jxl_runner != nullptr)
      jxlResizableParallelRunnerDestroy(nullptr);
  }

  // Push the existing texture to a stack to be released after the frame
  //   Do this regardless of whether we could actually load the new cover or not
  //
  // Do we need this in CLI?
  //if (image.pRawTexSRV.p != nullptr)
  //{
  //  extern concurrency::concurrent_queue <IUnknown*> SKIF_ResourcesToFree;
  //  LOG << "SKIF_ResourcesToFree: Pushing " << image.pRawTexSRV.p << " to be released";;
  //  SKIF_ResourcesToFree.push(image.pRawTexSRV.p);
  //  image.pRawTexSRV.p = nullptr;
  //}

  if (!succeeded)
    return false;

  DirectX::ScratchImage* pImg = &img;
  DirectX::ScratchImage   converted_img;

  // We don't want single-channel icons, so convert to RGBA
  if (meta.format == DXGI_FORMAT_R8_UNORM)
  {
    if (SUCCEEDED(DirectX::Convert(pImg->GetImages(), pImg->GetImageCount(), pImg->GetMetadata(), DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, 0.0f, converted_img)))
    {
      meta = converted_img.GetMetadata();
      pImg = &converted_img;
    }
  }

#if 0
  // Downscale covers to 220x330, which will then be shown in horizon mode
  if (false)
  {
    size_t width = 220;
    size_t height = 330;

    if (
      SUCCEEDED(
        DirectX::Resize(
          pImg->GetImages(), pImg->GetImageCount(),
          pImg->GetMetadata(), width, height,
          DirectX::TEX_FILTER_FANT,
          converted_img
        )
      )
      )
    {
      meta = converted_img.GetMetadata();
      pImg = &converted_img;
    }
  }
#endif

  auto pDevice =
    SKIF_D3D11_GetDevice();

  if (!pDevice)
    return false;

  pRawTex2D = nullptr;

  succeeded = false;

  if (image.is_hdr)
  {
    using namespace DirectX;

    assert(meta.format == DXGI_FORMAT_R16G16B16A16_FLOAT ||
      meta.format == DXGI_FORMAT_R32G32B32A32_FLOAT);

    XMVECTOR vMaxCLL = g_XMZero;
    float    fMaxLum = 0.0f;
    float    fMinLum = 5240320.0f;
    float    fMaxLum99 = 0.0f;

    auto        luminance_freq = std::make_unique <uint32_t[]>(65536);
    ZeroMemory(luminance_freq.get(), sizeof(uint32_t) * 65536);

    double dLumAccum = 0.0;

    static constexpr float FLT16_MIN = 0.0000000894069671630859375f;

    EvaluateImage(pImg->GetImages(),
      pImg->GetImageCount(),
      pImg->GetMetadata(),
      [&](const XMVECTOR* pixels, size_t width, size_t y)
      {
        UNREFERENCED_PARAMETER(y);

        XMVECTOR vColorXYZ;
        XMVECTOR vColorDCIP3;
        XMVECTOR vColor2020;
        XMVECTOR vColorAP1;
        XMVECTOR vColorAP0;
        XMVECTOR v;

        uint32_t xm_test_all = 0x0;

        double dScanlineLum = 0.0;

        for (size_t j = 0; j < width; ++j)
        {
          v = *pixels;

          vMaxCLL =
            XMVectorMax(v, vMaxCLL);

          vColorXYZ =
            XMVector3Transform(v, c_from709toXYZ);

          xm_test_all = 0x0;

#define FP16_MIN 0.0005f

          if (XMVectorGreaterOrEqualR(&xm_test_all, v, g_XMZero);
            XMComparisonAllTrue(xm_test_all) || XMVectorGetY(vColorXYZ) < FP16_MIN)
          {
            image.colorimetry.pixel_counts.rec_709++;
          }

          else
          {
            vColorDCIP3 =
              XMVector3Transform(v, c_from709toDCIP3);

            if (XMVectorGreaterOrEqualR(&xm_test_all, vColorDCIP3, g_XMZero);
              XMComparisonAnyFalse(xm_test_all))
            {
              vColor2020 =
                XMVector3Transform(v, c_from709to2020);

              if (XMVectorGreaterOrEqualR(&xm_test_all, vColor2020, g_XMZero);
                XMComparisonAnyFalse(xm_test_all))
              {
                vColorAP1 =
                  XMVector3Transform(v, c_from709toAP1);

                if (XMVectorGreaterOrEqualR(&xm_test_all, vColorAP1, g_XMZero);
                  XMComparisonAnyFalse(xm_test_all))
                {
                  vColorAP0 =
                    XMVector3Transform(v, c_from709toAP0);

                  if (XMVectorGreaterOrEqualR(&xm_test_all, vColorAP0, g_XMZero);
                    XMComparisonAnyFalse(xm_test_all))
                  {
                    image.colorimetry.pixel_counts.undefined++;
                  }

                  else
                  {
                    image.colorimetry.pixel_counts.ap0++;
                  }
                }

                else
                {
                  image.colorimetry.pixel_counts.ap1++;
                }
              }

              else
              {
                image.colorimetry.pixel_counts.rec_2020++;
              }
            }

            else
            {
              image.colorimetry.pixel_counts.dci_p3++;
            }
          }

          image.colorimetry.pixel_counts.total++;

          const float fLum =
            XMVectorGetY(vColorXYZ);

          fMaxLum =
            std::max(fMaxLum, fLum);
          fMinLum =
            std::min(fMinLum, fLum);

          dScanlineLum +=
            std::max(0.0, static_cast <double> (fLum));

          pixels++;
        }

        dLumAccum +=
          (dScanlineLum / static_cast <float> (width));
      });

    float fMaxLumActual = fMaxLum;
    float fMinLumActual = std::max(fMinLum, 0.0f);

    // 0 nits - 10k nits (appropriate for screencap, but not HDR photography)
    fMinLum = std::clamp(fMinLum, 0.0f, 125.0f);
    fMaxLum = std::clamp(fMaxLum, fMinLum, 125.0f);

    const float fLumRange =
      (fMaxLum - fMinLum);

    EvaluateImage(pImg->GetImages(),
      pImg->GetImageCount(),
      pImg->GetMetadata(),
      [&](const XMVECTOR* pixels, size_t width, size_t y)
      {
        UNREFERENCED_PARAMETER(y);

        for (size_t j = 0; j < width; ++j)
        {
          XMVECTOR v = *pixels++;

          v =
            XMVectorMax(g_XMZero, XMVector3Transform(v, c_from709toXYZ));

          luminance_freq[
            std::clamp((int)
              std::roundf(
                (XMVectorGetY(v) - fMinLum) /
                (fLumRange / 65536.0f)),
              0, 65535)]++;
        }
      });

    double percent = 100.0;
    const double img_size = (double)pImg->GetMetadata().width *
      (double)pImg->GetMetadata().height;

    for (auto i = 65535; i >= 0; --i)
    {
      percent -=
        100.0 * ((double)luminance_freq[i] / img_size);

      if (percent <= 99.94)
      {
        LOG << "99.94th percentile luminance: " <<
          80.0f * (fMinLum + (fLumRange * ((float)i / 65536.0f)))
          << " nits";

        fMaxLum99 =
          fMinLum + (fLumRange * ((float)i / 65536.0f));

        break;
      }
    }

    const float fMaxCLL =
      std::max({
        XMVectorGetX(vMaxCLL),
        XMVectorGetY(vMaxCLL),
        XMVectorGetZ(vMaxCLL)
        });

    XMVECTOR vMaxCLLReplicated =
      XMVectorReplicate(fMaxCLL);

    char cMaxChannel =
      fMaxCLL == XMVectorGetX(vMaxCLL) ? 'R' :
      fMaxCLL == XMVectorGetY(vMaxCLL) ? 'G' :
      fMaxCLL == XMVectorGetZ(vMaxCLL) ? 'B' :
      'X';

    // Use the maximum luminance if for some reason the percentile calc failed
    if (fMaxLum99 <= 0.01f)
      fMaxLum99 = fMaxLum;

    image.light_info.max_cll = fMaxCLL;
    image.light_info.max_cll_name = cMaxChannel;
    image.light_info.max_nits = std::max(0.0f, fMaxLumActual * 80.0f); // scRGB
    image.light_info.min_nits = std::max(0.0f, fMinLumActual * 80.0f); // scRGB
    image.light_info.p99_nits = std::max(0.0f, fMaxLum99 * 80.0f); // scRGB

    // We use the sum of averages per-scanline to help avoid overflow
    image.light_info.avg_nits = static_cast <float> (80.0 *
      (dLumAccum / static_cast <double> (meta.height)));
  }

  HRESULT hr =
    DirectX::CreateTexture(pDevice, pImg->GetImages(), pImg->GetImageCount(), meta, (ID3D11Resource**)&pRawTex2D.p);

  if (SUCCEEDED(hr))
  {
    if (image.is_hdr)
    {
      D3D11_TEXTURE2D_DESC
        texDesc = { };
      texDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
      texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
      texDesc.SampleDesc = { .Count = 1,
                             .Quality = 0 };
      texDesc.Usage = D3D11_USAGE_DEFAULT;
      texDesc.ArraySize = 1;
      texDesc.Width = 1024;
      texDesc.Height = 1024;

      if (SUCCEEDED(pDevice->CreateTexture2D(&texDesc, nullptr, &pGamutCoverageTex2D.p)))
      {
        pDevice->CreateUnorderedAccessView(pGamutCoverageTex2D.p, nullptr, &image.pGamutCoverageUAV.p);
        pDevice->CreateShaderResourceView(pGamutCoverageTex2D.p, nullptr, &image.pGamutCoverageSRV.p);

        CComPtr <ID3D11DeviceContext>  pDevCtx;
        pDevice->GetImmediateContext(&pDevCtx);

        if (pDevCtx.p != nullptr)
        {
          FLOAT fClearColor[] = { 0.f, 0.f, 0.f, 0.f };
          pDevCtx->ClearUnorderedAccessViewFloat(image.pGamutCoverageUAV, fClearColor);
        }
      }
    }

    // Remember HDR images read using the WIC encoder
    if ((meta.format == DXGI_FORMAT_R16G16B16A16_FLOAT ||
      meta.format == DXGI_FORMAT_R32G32B32A32_FLOAT) && decoder == ImageDecoder_WIC)
      image.light_info.isHDR = true;

    D3D11_SHADER_RESOURCE_VIEW_DESC
      srv_desc = { };
    srv_desc.Format = DXGI_FORMAT_UNKNOWN;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = UINT_MAX;
    srv_desc.Texture2D.MostDetailedMip = 0;

    if (pRawTex2D.p != nullptr && SUCCEEDED(pDevice->CreateShaderResourceView(pRawTex2D.p, &srv_desc, &image.pRawTexSRV.p)))
    {
      // Update the image width/height
      image.width = static_cast<float>(meta.width);
      image.height = static_cast<float>(meta.height);

      succeeded = true;
    }

    // SRV is holding a reference, this is not needed anymore.
    pRawTex2D = nullptr;
    pGamutCoverageTex2D = nullptr;
  }

  return succeeded;
};


static bool
SKIV_PNG_MakeHDR(const wchar_t* wszFilePath,
  const DirectX::Image& encoded_img,
  const DirectX::Image& raw_img)
{
  static SKIF_RegistrySettings& _registry =
    SKIF_RegistrySettings::GetInstance();

  std::ignore = encoded_img;

  static const BYTE _test[] = { 0x49, 0x45, 0x4E, 0x44 };

  if (png_crc32((const BYTE*)_test, 0, 4, 0) == 0xae426082)
  {
    LOG << "png_crc32 == TRUE";

    FILE*
      fPNG = _wfopen(wszFilePath, L"r+b");
    if (fPNG != nullptr)
    {
      fseek(fPNG, 0, SEEK_END);
      size_t size = ftell(fPNG);
      rewind(fPNG);

      auto data =
        std::make_unique <uint8_t[]>(size);

      if (!data)
      {
        fclose(fPNG);
        return false;
      }

      fread(data.get(), size, 1, fPNG);
      rewind(fPNG);

      sk_png_remove_chunk("sRGB", data.get(), size);
      sk_png_remove_chunk("gAMA", data.get(), size);

      fwrite(data.get(), size, 1, fPNG);

      // Truncate the file
      _chsize(_fileno(fPNG), static_cast <long> (size));

      size_t         insert_pos = 0;
      const uint8_t* insert_ptr = nullptr;

      // Effectively a string search, but ignoring nul-bytes in both
      //   the character array being searched and the pattern...
      std::string_view  data_view((const char*)data.get(), size);
      if (insert_pos = data_view.find("IDAT", 0, 4);
        insert_pos == data_view.npos)
      {
        fclose(fPNG);
        return false;
      }

      insert_pos -= 4; // Rollback to the chunk's length field
      insert_ptr =
        (data.get() + insert_pos);

      fseek(fPNG, static_cast <long> (insert_pos), SEEK_SET);

      struct SK_PNG_Chunk {
        uint32_t      len;
        unsigned char name[4];
        void* data;
        uint32_t      crc;
        uint32_t      _native_len;

        void write(FILE* fStream)
        {
          // Length is Big Endian, Intel/AMD CPUs are Little Endian
          if (_native_len == 0)
          {
            _native_len = len;
#if (defined _M_IX86) || (defined _M_X64)
            len = _byteswap_ulong(_native_len);
#endif
          }

          crc =
            png_crc32(data, 0, _native_len, png_crc32(name, 0, 4, 0x0));
#if (defined _M_IX86) || (defined _M_X64)
          crc = _byteswap_ulong(crc);
#endif

          fwrite(&len, 8, 1, fStream);
          fwrite(data, _native_len, 1, fStream);
          fwrite(&crc, 4, 1, fStream);
        };
      };

      uint8_t cicp_data[] = {
        9,  // BT.2020 Color Primaries
        16, // ST.2084 EOTF (PQ)
        0,  // Identity Coefficients
        1,  // Full Range
      };

      // Embedded ICC Profile so that Discord will render in HDR
      SK_PNG_HDR_iCCP_Payload iccp_data;

      SK_PNG_HDR_cHRM_Payload chrm_data; // Rec 2020 chromaticity
      SK_PNG_HDR_sBIT_Payload sbit_data; // Bits in original source (max=12)
      SK_PNG_HDR_mDCv_Payload mdcv_data; // Display capabilities
      SK_PNG_HDR_cLLi_Payload clli_data; // Content light info

      clli_data =
        SKIV_HDR_CalculateContentLightInfo(raw_img);

      sbit_data = {
        static_cast <unsigned char> (DirectX::BitsPerColor(raw_img.format)),
        static_cast <unsigned char> (DirectX::BitsPerColor(raw_img.format)),
        static_cast <unsigned char> (DirectX::BitsPerColor(raw_img.format))
      };

      if (raw_img.format != DXGI_FORMAT_R10G10B10A2_UNORM)
      {
        // If using compression optimization, max bits = 12
        sbit_data.red_bits = static_cast <uint8_t> (Config::HDR_bitdepth);
        sbit_data.green_bits = static_cast <uint8_t> (Config::HDR_bitdepth);
        sbit_data.blue_bits = static_cast <uint8_t> (Config::HDR_bitdepth);
      }

      // We don't actually know the mastering display, but some effort should be made
      //   to read this metadata and preserve it if it exists when SKIV originally
      //     loads HDR images.
# if 0
      auto& rb =
        SK_GetCurrentRenderBackend();

      auto& active_display =
        rb.displays[rb.active_display];

      SK_PNG_SetUint32(mdcv_data.luminance.minimum,
        static_cast <uint32_t> (round(active_display.gamut.minY / 0.0001f)));
      SK_PNG_SetUint32(mdcv_data.luminance.maximum,
        static_cast <uint32_t> (round(active_display.gamut.maxY / 0.0001f)));

      SK_PNG_SetUint32(mdcv_data.primaries.red_x,
        static_cast <uint32_t> (round(active_display.gamut.xr / 0.00002)));
      SK_PNG_SetUint32(mdcv_data.primaries.red_y,
        static_cast <uint32_t> (round(active_display.gamut.yr / 0.00002)));

      SK_PNG_SetUint32(mdcv_data.primaries.green_x,
        static_cast <uint32_t> (round(active_display.gamut.xg / 0.00002)));
      SK_PNG_SetUint32(mdcv_data.primaries.green_y,
        static_cast <uint32_t> (round(active_display.gamut.yg / 0.00002)));

      SK_PNG_SetUint32(mdcv_data.primaries.blue_x,
        static_cast <uint32_t> (round(active_display.gamut.xb / 0.00002)));
      SK_PNG_SetUint32(mdcv_data.primaries.blue_y,
        static_cast <uint32_t> (round(active_display.gamut.yb / 0.00002)));

      SK_PNG_SetUint32(mdcv_data.white_point.x,
        static_cast <uint32_t> (round(active_display.gamut.Xw / 0.00002)));
      SK_PNG_SetUint32(mdcv_data.white_point.y,
        static_cast <uint32_t> (round(active_display.gamut.Yw / 0.00002)));
#endif

      SK_PNG_Chunk clli_chunk = { sizeof(clli_data),               { 'c','L','L','i' }, &clli_data };
      SK_PNG_Chunk iccp_chunk = { sizeof(SK_PNG_HDR_iCCP_Payload), { 'i','C','C','P' }, &iccp_data };
      SK_PNG_Chunk cicp_chunk = { sizeof(cicp_data),               { 'c','I','C','P' }, &cicp_data };
      SK_PNG_Chunk sbit_chunk = { sizeof(sbit_data),               { 's','B','I','T' }, &sbit_data };
      SK_PNG_Chunk chrm_chunk = { sizeof(chrm_data),               { 'c','H','R','M' }, &chrm_data };
#if 0
      SK_PNG_Chunk mdcv_chunk = { sizeof(mdcv_data),               { 'm','D','C','v' }, &mdcv_data };
#endif

      iccp_chunk.write(fPNG);
      clli_chunk.write(fPNG);
      cicp_chunk.write(fPNG);
      sbit_chunk.write(fPNG);
      chrm_chunk.write(fPNG);
#if 0
      mdcv_chunk.write(fPNG);
#endif

      // Write the remainder of the original file
      fwrite(insert_ptr, size - insert_pos, 1, fPNG);

      auto final_size =
        ftell(fPNG);

      auto full_png =
        std::make_unique <unsigned char[]>(final_size);

      rewind(fPNG);
      fread(full_png.get(), final_size, 1, fPNG);
      fclose(fPNG);

      return true;
    }
  }

  return false;
}

static bool
SKIV_HDR_SavePNGToDisk(const wchar_t* wszPNGPath, const DirectX::Image* png_image,
  const DirectX::Image* raw_image,
  const char* szUtf8MetadataTitle, bool isHDR)
{
  if (wszPNGPath == nullptr ||
    png_image == nullptr ||
    raw_image == nullptr)
  {
    LOG_DEBUG_IF(wszPNGPath == nullptr) << "wszPNGPath == nullptr";
    LOG_DEBUG_IF(png_image == nullptr) << "png_image  == nullptr";
    LOG_DEBUG_IF(raw_image == nullptr) << "raw_image  == nullptr";
    return false;
  }

  std::string metadata_title(
    szUtf8MetadataTitle != nullptr ?
    szUtf8MetadataTitle :
    "HDR10 PNG");

  if (SUCCEEDED(
    DirectX::SaveToWICFile(*png_image, DirectX::WIC_FLAGS_NONE,
      GetWICCodec(DirectX::WIC_CODEC_PNG),
      wszPNGPath, &GUID_WICPixelFormat48bppRGB,
      SK_WIC_SetMaximumQuality/*,
    [&](IWICMetadataQueryWriter *pMQW)
    {
      SK_WIC_SetMetadataTitle (pMQW, metadata_title);
    }*/)))
  {
    LOG << "DirectX::SaveToWICFile ( ): SUCCEEDED";

    return (isHDR) ? SKIV_PNG_MakeHDR(wszPNGPath, *png_image, *raw_image)
      : true;
  }
  else
    LOG_E << "DirectX::SaveToWICFile ( ): FAILED";

  return false;
}


static bool
SKIV_HDR_ConvertImageToPNG(const DirectX::Image& raw_hdr_img, DirectX::ScratchImage& png_img)
{
  static SKIF_RegistrySettings& _registry =
    SKIF_RegistrySettings::GetInstance();

  using namespace DirectX;

  if (auto typeless_fmt = DirectX::MakeTypeless(raw_hdr_img.format);
    typeless_fmt == DXGI_FORMAT_R8G8B8A8_TYPELESS ||
    typeless_fmt == DXGI_FORMAT_R10G10B10A2_TYPELESS ||
    typeless_fmt == DXGI_FORMAT_R16G16B16A16_TYPELESS ||
    typeless_fmt == DXGI_FORMAT_R32G32B32A32_TYPELESS)
  {
    if (png_img.GetImageCount() == 0)
    {
      // Early SDR exit
      if (typeless_fmt == DXGI_FORMAT_R8G8B8A8_TYPELESS)
        return (SUCCEEDED(png_img.InitializeFromImage(raw_hdr_img)));

      if (FAILED(png_img.Initialize2D(DXGI_FORMAT_R16G16B16A16_UNORM,
        raw_hdr_img.width,
        raw_hdr_img.height, 1, 1)))
      {
        return false;
      }
    }

    if (png_img.GetMetadata().format != DXGI_FORMAT_R16G16B16A16_UNORM)
      return false;

    uint16_t* rgb16_pixels =
      reinterpret_cast <uint16_t*> (png_img.GetPixels());

    if (rgb16_pixels == nullptr)
      return false;

    EvaluateImage(raw_hdr_img,
      [&](const XMVECTOR* pixels, size_t width, size_t y)
      {
        UNREFERENCED_PARAMETER(y);

        static const XMVECTOR pq_range_10bpc = XMVectorReplicate(1023.0f),
          pq_range_11bpc = XMVectorReplicate(2047.0f),
          pq_range_12bpc = XMVectorReplicate(4095.0f),
          pq_range_13bpc = XMVectorReplicate(8191.0f),
          pq_range_14bpc = XMVectorReplicate(16383.0f),
          pq_range_15bpc = XMVectorReplicate(32767.0f),
          pq_range_16bpc = XMVectorReplicate(65535.0f),
          pq_range_32bpc = XMVectorReplicate(4294967295.0f);

        auto pq_range_out =
          (typeless_fmt == DXGI_FORMAT_R10G10B10A2_TYPELESS) ? pq_range_10bpc :
          Config::HDR_bitdepth == 10 ? pq_range_10bpc :
          Config::HDR_bitdepth == 11 ? pq_range_11bpc :
          Config::HDR_bitdepth == 12 ? pq_range_12bpc :
          Config::HDR_bitdepth == 13 ? pq_range_13bpc :
          Config::HDR_bitdepth == 14 ? pq_range_14bpc :
          Config::HDR_bitdepth == 15 ? pq_range_15bpc :
          pq_range_16bpc;

        const auto pq_range_in =
          (typeless_fmt == DXGI_FORMAT_R10G10B10A2_TYPELESS) ? pq_range_10bpc :
          (typeless_fmt == DXGI_FORMAT_R16G16B16A16_TYPELESS) ? pq_range_16bpc :
          pq_range_32bpc;

        int output_bits =
          (typeless_fmt == DXGI_FORMAT_R10G10B10A2_TYPELESS) ? 10 :
          (typeless_fmt == DXGI_FORMAT_R16G16B16A16_TYPELESS) ? Config::HDR_bitdepth :
          Config::HDR_bitdepth;
        int intermediate_bits = 16;

        for (size_t j = 0; j < width; ++j)
        {
          XMVECTOR v =
            *pixels++;

          // Assume scRGB for any FP32 input, though uncommon
          if (typeless_fmt == DXGI_FORMAT_R16G16B16A16_TYPELESS ||
            typeless_fmt == DXGI_FORMAT_R32G32B32A32_TYPELESS)
          {
            v =
              SKIV_Image_LinearToPQ(XMVectorMax(XMVector3Transform(v, c_scRGBtoBt2100), g_XMZero));
          }

          v = // Quantize to 10- or 12-bpc before expanding to 16-bpc in order to improve
            XMVectorRound( // compression efficiency
              XMVectorMultiply(
                XMVectorSaturate(v), pq_range_out));

          *(rgb16_pixels++) =
            static_cast <uint16_t> (DirectX::XMVectorGetX(v)) << (intermediate_bits - output_bits);
          *(rgb16_pixels++) =
            static_cast <uint16_t> (DirectX::XMVectorGetY(v)) << (intermediate_bits - output_bits);
          *(rgb16_pixels++) =
            static_cast <uint16_t> (DirectX::XMVectorGetZ(v)) << (intermediate_bits - output_bits);
          rgb16_pixels++; // We have an unused alpha channel that needs skipping
        }
      });
  }

  return true;
}

HRESULT
SKIV_Image_SaveToDisk_HDR(const DirectX::Image& image, const wchar_t* wszFileName)
{
  SKIF_RegistrySettings& _registry =
    SKIF_RegistrySettings::GetInstance();

  using namespace DirectX;

  const Image* pOutputImage = &image;

  if (image.format != DXGI_FORMAT_R10G10B10A2_UNORM &&
    image.format != DXGI_FORMAT_R16G16B16A16_FLOAT &&
    image.format != DXGI_FORMAT_R32G32B32A32_FLOAT)
  {
    // SKIV always uses scRGB internally for HDR, any other format
    //   can't be HDR...
    LOG_E << "Unsupported HDR image format: %d", image.format;

    return E_NOTIMPL;
  }

  wchar_t* wszExtension =
    PathFindExtensionW(wszFileName);

  wchar_t wszImplicitFileName[MAX_PATH] = { };
  wcscpy(wszImplicitFileName, wszFileName);

  // For doofus users who don't give us filenames...
  if (!wszExtension)
  {
    PathAddExtension(wszImplicitFileName, defaultHDRFileExt.c_str());
    wszExtension =
      PathFindExtensionW(wszImplicitFileName);
  }

  GUID wic_codec;

  if (StrStrIW(wszExtension, L"jxr"))
  {
    wic_codec = GetWICCodec(WIC_CODEC_WMP);
  }

#ifdef _M_X64
  else if (StrStrIW(wszExtension, L"exr"))
  {
    using namespace DirectX;

    if (SUCCEEDED(SaveToEXRFile(image, wszImplicitFileName)))
    {
      return S_OK;
    }
  }
#endif

  else if (StrStrIW(wszExtension, L"hdr"))
  {
    using namespace DirectX;

    if (SUCCEEDED(SaveToHDRFile(image, wszImplicitFileName)))
    {
      return S_OK;
    }
  }

  else if (StrStrIW(wszExtension, L"png"))
  {
    DirectX::ScratchImage                  png_img;
    if (SKIV_HDR_ConvertImageToPNG(image, png_img))
    {
      if (SKIV_HDR_SavePNGToDisk(wszImplicitFileName, png_img.GetImages(), &image, nullptr, true))
      {
        return S_OK;
      }
    }
  }

  else if (StrStrIW(wszExtension, L"jxl"))
  {
    if (!isJXLDecoderAvailable())
      return E_NOTIMPL;

    static HMODULE hModJXL;
    hModJXL = LoadLibraryW(L"jxl.dll");

    static HMODULE hModJXLThreads;
    hModJXLThreads = LoadLibraryW(L"jxl_threads.dll");

    using JxlEncoderCreate_pfn = JxlEncoder * (*)(const JxlMemoryManager* memory_manager);
    using JxlEncoderDestroy_pfn = void                     (*)(JxlEncoder* enc);
    using JxlEncoderCloseInput_pfn = void                     (*)(JxlEncoder* enc);
    using JxlEncoderProcessOutput_pfn = JxlEncoderStatus(*)(JxlEncoder* enc, uint8_t** next_out, size_t* avail_out);
    using JxlEncoderFrameSettingsCreate_pfn = JxlEncoderFrameSettings * (*)(JxlEncoder* enc, const JxlEncoderFrameSettings* source);
    using JxlEncoderInitBasicInfo_pfn = void                     (*)(JxlBasicInfo* info);
    using JxlEncoderSetBasicInfo_pfn = JxlEncoderStatus(*)(JxlEncoder* enc, const JxlBasicInfo* info);
    using JxlEncoderAddImageFrame_pfn = JxlEncoderStatus(*)(const JxlEncoderFrameSettings* frame_settings, const JxlPixelFormat* pixel_format, const void* buffer, size_t size);
    using JxlEncoderSetColorEncoding_pfn = JxlEncoderStatus(*)(JxlEncoder* enc, const JxlColorEncoding* color);
    using JxlEncoderFrameSettingsSetOption_pfn = JxlEncoderStatus(*)(JxlEncoderFrameSettings* frame_settings, JxlEncoderFrameSettingId option, int64_t value);
    using JxlEncoderSetParallelRunner_pfn = JxlEncoderStatus(*)(JxlEncoder* enc, JxlParallelRunner parallel_runner, void* parallel_runner_opaque);

    using JxlThreadParallelRunner_pfn = JxlParallelRetCode(*)(void* runner_opaque, void* jpegxl_opaque, JxlParallelRunInit init, JxlParallelRunFunction func, uint32_t start_range, uint32_t end_range);
    using JxlThreadParallelRunnerCreate_pfn = void* (*)(const JxlMemoryManager* memory_manager, size_t num_worker_threads);
    using JxlThreadParallelRunnerDestroy_pfn = void                     (*)(void* runner_opaque);
    using JxlThreadParallelRunnerDefaultNumWorkerThreads_pfn = size_t(*)(void);

    static JxlEncoderCreate_pfn                 jxlEncoderCreate = (JxlEncoderCreate_pfn)GetProcAddress(hModJXL, "JxlEncoderCreate");
    static JxlEncoderDestroy_pfn                jxlEncoderDestroy = (JxlEncoderDestroy_pfn)GetProcAddress(hModJXL, "JxlEncoderDestroy");
    static JxlEncoderCloseInput_pfn             jxlEncoderCloseInput = (JxlEncoderCloseInput_pfn)GetProcAddress(hModJXL, "JxlEncoderCloseInput");
    static JxlEncoderProcessOutput_pfn          jxlEncoderProcessOutput = (JxlEncoderProcessOutput_pfn)GetProcAddress(hModJXL, "JxlEncoderProcessOutput");
    static JxlEncoderFrameSettingsCreate_pfn    jxlEncoderFrameSettingsCreate = (JxlEncoderFrameSettingsCreate_pfn)GetProcAddress(hModJXL, "JxlEncoderFrameSettingsCreate");
    static JxlEncoderInitBasicInfo_pfn          jxlEncoderInitBasicInfo = (JxlEncoderInitBasicInfo_pfn)GetProcAddress(hModJXL, "JxlEncoderInitBasicInfo");
    static JxlEncoderSetBasicInfo_pfn           jxlEncoderSetBasicInfo = (JxlEncoderSetBasicInfo_pfn)GetProcAddress(hModJXL, "JxlEncoderSetBasicInfo");
    static JxlEncoderAddImageFrame_pfn          jxlEncoderAddImageFrame = (JxlEncoderAddImageFrame_pfn)GetProcAddress(hModJXL, "JxlEncoderAddImageFrame");
    static JxlEncoderSetColorEncoding_pfn       jxlEncoderSetColorEncoding = (JxlEncoderSetColorEncoding_pfn)GetProcAddress(hModJXL, "JxlEncoderSetColorEncoding");
    static JxlEncoderFrameSettingsSetOption_pfn jxlEncoderFrameSettingsSetOption = (JxlEncoderFrameSettingsSetOption_pfn)GetProcAddress(hModJXL, "JxlEncoderFrameSettingsSetOption");
    static JxlEncoderSetParallelRunner_pfn      jxlEncoderSetParallelRunner = (JxlEncoderSetParallelRunner_pfn)GetProcAddress(hModJXL, "JxlEncoderSetParallelRunner");

    static JxlThreadParallelRunner_pfn                        jxlThreadParallelRunner = (JxlThreadParallelRunner_pfn)GetProcAddress(hModJXLThreads, "JxlThreadParallelRunner");
    static JxlThreadParallelRunnerCreate_pfn                  jxlThreadParallelRunnerCreate = (JxlThreadParallelRunnerCreate_pfn)GetProcAddress(hModJXLThreads, "JxlThreadParallelRunnerCreate");
    static JxlThreadParallelRunnerDestroy_pfn                 jxlThreadParallelRunnerDestroy = (JxlThreadParallelRunnerDestroy_pfn)GetProcAddress(hModJXLThreads, "JxlThreadParallelRunnerDestroy");
    static JxlThreadParallelRunnerDefaultNumWorkerThreads_pfn jxlThreadParallelRunnerDefaultNumWorkerThreads = (JxlThreadParallelRunnerDefaultNumWorkerThreads_pfn)GetProcAddress(hModJXLThreads, "JxlThreadParallelRunnerDefaultNumWorkerThreads");

    using JxlEncoderSetFrameLossless_pfn = JxlEncoderStatus(*)(JxlEncoderFrameSettings* frame_settings, JXL_BOOL lossless);
    using JxlEncoderSetFrameDistance_pfn = JxlEncoderStatus(*)(JxlEncoderFrameSettings* frame_settings, float distance);
    using JxlEncoderSetFrameBitDepth_pfn = JxlEncoderStatus(*)(JxlEncoderFrameSettings* frame_settings, const JxlBitDepth* bit_depth);
    using JxlEncoderDistanceFromQuality_pfn = float            (*)(float quality);

    static JxlEncoderSetFrameLossless_pfn    jxlEncoderSetFrameLossless = (JxlEncoderSetFrameLossless_pfn)GetProcAddress(hModJXL, "JxlEncoderSetFrameLossless");
    static JxlEncoderSetFrameDistance_pfn    jxlEncoderSetFrameDistance = (JxlEncoderSetFrameDistance_pfn)GetProcAddress(hModJXL, "JxlEncoderSetFrameDistance");
    static JxlEncoderSetFrameBitDepth_pfn    jxlEncoderSetFrameBitDepth = (JxlEncoderSetFrameBitDepth_pfn)GetProcAddress(hModJXL, "JxlEncoderSetFrameBitDepth");
    static JxlEncoderDistanceFromQuality_pfn jxlEncoderDistanceFromQuality = (JxlEncoderDistanceFromQuality_pfn)GetProcAddress(hModJXL, "JxlEncoderDistanceFromQuality");

    bool succeeded = false;

    if (jxlEncoderCreate == nullptr ||
      jxlThreadParallelRunnerCreate == nullptr ||
      jxlThreadParallelRunnerDefaultNumWorkerThreads == nullptr)
    {
      LOG_E << "JPEG XL library unavailable";
      return E_NOINTERFACE;
    }

    auto jxl_encoder = jxlEncoderCreate(nullptr);
    auto jxl_runner = jxlThreadParallelRunnerCreate(nullptr,
      jxlThreadParallelRunnerDefaultNumWorkerThreads());

    for (;;)
    {
      if (jxl_encoder == nullptr ||
        jxl_runner == nullptr)
        break;

      if (JXL_ENC_SUCCESS !=
        jxlEncoderSetParallelRunner(jxl_encoder,
          jxlThreadParallelRunner,
          jxl_runner))
      {
        LOG << "JxlEncoderSetParallelRunner failed";
        break;
      }

      JxlDataType type = JXL_TYPE_FLOAT;
      size_t      size = sizeof(float);

      std::vector <float> fp_pixels(image.width * image.height * 3);

      auto fp_pixel_comp =
        fp_pixels.begin();

      EvaluateImage(image,
        [&](const XMVECTOR* pixels, size_t width, size_t y)
        {
          UNREFERENCED_PARAMETER(y);

          for (size_t j = 0; j < width; ++j)
          {
            XMVECTOR v =
              *pixels++;

            *fp_pixel_comp++ = XMVectorGetX(v);
            *fp_pixel_comp++ = XMVectorGetY(v);
            *fp_pixel_comp++ = XMVectorGetZ(v);
          }
        }
      );

      JxlPixelFormat pixel_format =
      { 3, type, JXL_NATIVE_ENDIAN, 0 };

      JxlBasicInfo              basic_info = { };
      jxlEncoderInitBasicInfo(&basic_info);

      const bool bLossless = (Config::Quality == 100);

      basic_info.xsize = static_cast <uint32_t> (image.width);
      basic_info.ysize = static_cast <uint32_t> (image.height);
      basic_info.bits_per_sample = static_cast <uint32_t> (DirectX::BitsPerColor(image.format));
      basic_info.exponent_bits_per_sample = DirectX::BitsPerColor(image.format) == 32 ? 8 : 5;
      basic_info.uses_original_profile = bLossless ? JXL_TRUE : JXL_FALSE;

      if (JXL_ENC_SUCCESS !=
        jxlEncoderSetBasicInfo(jxl_encoder, &basic_info))
      {
        LOG << "JxlEncoderSetBasicInfo failed";
        break;
      }

      JxlColorEncoding color_encoding = { };

      color_encoding.color_space = JXL_COLOR_SPACE_RGB;
      color_encoding.white_point = JXL_WHITE_POINT_D65;
      color_encoding.primaries = JXL_PRIMARIES_SRGB;
      color_encoding.transfer_function = JXL_TRANSFER_FUNCTION_LINEAR;
      color_encoding.rendering_intent = JXL_RENDERING_INTENT_PERCEPTUAL;

      if (JXL_ENC_SUCCESS !=
        jxlEncoderSetColorEncoding(jxl_encoder, &color_encoding))
      {
        LOG << "JxlEncoderSetColorEncoding failed";
        break;
      }

      JxlEncoderFrameSettings* frame_settings =
        jxlEncoderFrameSettingsCreate(jxl_encoder, nullptr);

      jxlEncoderSetFrameLossless(frame_settings, bLossless ? JXL_TRUE : JXL_FALSE);
      jxlEncoderSetFrameDistance(frame_settings, jxlEncoderDistanceFromQuality((float)Config::Quality));
      jxlEncoderFrameSettingsSetOption(frame_settings, JXL_ENC_FRAME_SETTING_EFFORT, Config::Speed);

      if (JXL_ENC_SUCCESS !=
        jxlEncoderAddImageFrame(frame_settings, &pixel_format,
          static_cast <const void*> (fp_pixels.data()),
          size * fp_pixels.size()))
      {
        LOG << "JxlEncoderAddImageFrame failed";
        break;
      }

      jxlEncoderCloseInput(jxl_encoder);

      std::vector <uint8_t> output(64);

      uint8_t* next_out = output.data();
      size_t   avail_out = output.size() - (next_out - output.data());

      JxlEncoderStatus process_result = JXL_ENC_NEED_MORE_OUTPUT;

      while (process_result == JXL_ENC_NEED_MORE_OUTPUT)
      {
        process_result =
          jxlEncoderProcessOutput(jxl_encoder, &next_out, &avail_out);

        if (process_result == JXL_ENC_NEED_MORE_OUTPUT)
        {
          size_t offset = next_out - output.data();

          output.resize(output.size() * 2);

          next_out = output.data() + offset;
          avail_out = output.size() - offset;
        }
      }

      output.resize(next_out - output.data());

      if (JXL_ENC_SUCCESS != process_result)
      {
        LOG_E << "JxlEncoderProcessOutput failed";
        break;
      }

      FILE* fOutput =
        _wfopen(wszImplicitFileName, L"wb");

      if (fOutput != nullptr)
      {
        fwrite(output.data(), output.size(), 1, fOutput);
        fclose(fOutput);

        LOG << "JPEG XL Encode Finished";

        succeeded = true;
      }

      break;
    }

    if (jxl_encoder != nullptr)
      jxlEncoderDestroy(jxl_encoder);

    if (jxl_runner != nullptr)
      jxlThreadParallelRunnerDestroy(jxl_runner);

    return
      succeeded ? S_OK : E_FAIL;
  }

  else if (StrStrIW(wszExtension, L"avif"))
  {
    extern bool isAVIFEncoderAvailable(void);
    if (!isAVIFEncoderAvailable())
      return E_NOTIMPL;

    using namespace DirectX;

    uint32_t width = static_cast <uint32_t> (image.width);
    uint32_t height = static_cast <uint32_t> (image.height);

    int             bit_depth = 10;
    avifPixelFormat yuv_format = AVIF_PIXEL_FORMAT_YUV444;

    const int yuv_subsampling = 444;
    switch (yuv_subsampling)
    {
    default:
      //config.screenshots.avif.yuv_subsampling = 444; // Write a valid value to INI
      [[fallthrough]];
    case 444:
      yuv_format = AVIF_PIXEL_FORMAT_YUV444;
      break;
    case 422:
      yuv_format = AVIF_PIXEL_FORMAT_YUV422;
      break;
    case 420:
      yuv_format = AVIF_PIXEL_FORMAT_YUV420;
      break;
    case 400: // lol
      yuv_format = AVIF_PIXEL_FORMAT_YUV400;
      break;
    }

    if (image.format == DXGI_FORMAT_R16G16B16A16_FLOAT ||
      image.format == DXGI_FORMAT_R32G32B32A32_FLOAT)
    {
      bit_depth =
        std::clamp(Config::HDR_bitdepth, 8, 12);

      // 8, 10, 12... nothing else.
      if (bit_depth == 9 ||
        bit_depth == 11)
        bit_depth++;
    }

    avifResult rgbToYuvResult = AVIF_RESULT_NO_CONTENT;
    avifResult addResult = AVIF_RESULT_NO_CONTENT;
    avifResult encodeResult = AVIF_RESULT_NO_CONTENT;

    avifRWData   avifOutput = AVIF_DATA_EMPTY;
    avifRGBImage rgb = { };
    avifEncoder* encoder = nullptr;
    avifImage* avif_image =
      SK_avifImageCreate(width, height, bit_depth, yuv_format);

    if (avif_image != nullptr)
    {
      avif_image->yuvRange = AVIF_RANGE_FULL;

      switch (image.format)
      {
      case DXGI_FORMAT_R10G10B10A2_UNORM:
      case DXGI_FORMAT_R16G16B16A16_FLOAT:
      case DXGI_FORMAT_R32G32B32A32_FLOAT:
        avif_image->colorPrimaries = AVIF_COLOR_PRIMARIES_BT2020;
        avif_image->transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_SMPTE2084;
        avif_image->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT2020_NCL;
        break;
      default:
        LOG_E << "Unknown encoder format for file extension '" << wszExtension << "'";
        LOG_E << -17;
        return E_UNEXPECTED;
      }

      SK_avifRGBImageSetDefaults(&rgb, avif_image);

      rgb.depth = bit_depth;
      rgb.ignoreAlpha = true;
      rgb.isFloat = false;
      rgb.format = AVIF_RGB_FORMAT_RGB;

      SK_avifRGBImageAllocatePixels(&rgb);

      switch (image.format)
      {
      case DXGI_FORMAT_R10G10B10A2_UNORM:
      {
        uint16_t* rgb_pixels = (uint16_t*)rgb.pixels;

        EvaluateImage(image,
          [&](const DirectX::XMVECTOR* pixels, size_t width, size_t y)
          {
            UNREFERENCED_PARAMETER(y);

            for (size_t j = 0; j < width; ++j)
            {
              DirectX::XMVECTOR v = *pixels++;

              *(rgb_pixels++) = static_cast <uint16_t>(std::min(1023, static_cast <int>(XMVectorGetX(v) * 1024.0f)));
              *(rgb_pixels++) = static_cast <uint16_t>(std::min(1023, static_cast <int>(XMVectorGetY(v) * 1024.0f)));
              *(rgb_pixels++) = static_cast <uint16_t>(std::min(1023, static_cast <int>(XMVectorGetZ(v) * 1024.0f)));
            }
          });
      } break;

      case DXGI_FORMAT_R16G16B16A16_FLOAT:
      case DXGI_FORMAT_R32G32B32A32_FLOAT:
      {
        uint16_t* rgb16_pixels = (uint16_t*)rgb.pixels;
        uint8_t* rgb8_pixels = (uint8_t*)rgb.pixels;

        float N = 0.0f;
        float fLumAccum = 0.0f;
        float fMaxLum = 0.0f;
        float fMinLum = 5240320.0f;

        EvaluateImage(image,
          [&](const XMVECTOR* pixels, size_t width, size_t y)
          {
            UNREFERENCED_PARAMETER(y);

            float fScanlineLum = 0.0f;

            switch (image.format)
            {
            case DXGI_FORMAT_R10G10B10A2_UNORM:
            {
              for (size_t j = 0; j < width; ++j)
              {
                XMVECTOR v =
                  *pixels++;

                v =
                  XMVector3Transform(
                    SKIV_Image_PQToLinear(XMVectorSaturate(v)), c_from2020toXYZ
                  );

                const float fLum =
                  XMVectorGetY(v);

                fMaxLum =
                  std::max(fMaxLum, fLum);

                fMinLum =
                  std::min(fMinLum, fLum);

                fScanlineLum += fLum;
              }
            } break;

            case DXGI_FORMAT_R16G16B16A16_FLOAT:
            case DXGI_FORMAT_R32G32B32A32_FLOAT:
            {
              for (size_t j = 0; j < width; ++j)
              {
                XMVECTOR v =
                  *pixels++;

                v =
                  XMVector3Transform(v, c_from709toXYZ);

                const float fLum =
                  XMVectorGetY(v);

                fMaxLum =
                  std::max(fMaxLum, fLum);

                fMinLum =
                  std::min(fMinLum, fLum);

                fScanlineLum += fLum;
              }
            } break;

            default:
              break;
            }

            fLumAccum +=
              (fScanlineLum / static_cast <float> (width));
            ++N;
          }
        );

        if (N > 0.0)
        {
          // 0 nits - 10k nits (limit imposed by PQ)
          fMinLum = std::clamp(fMinLum, 0.0f, 125.0f);
          fMaxLum = std::clamp(fMaxLum, fMinLum, 125.0f);

          const float fLumRange =
            (fMaxLum - fMinLum);

          auto        luminance_freq = std::make_unique <uint32_t[]>(65536);
          ZeroMemory(luminance_freq.get(), sizeof(uint32_t) * 65536);

          EvaluateImage(image,
            [&](const XMVECTOR* pixels, size_t width, size_t y)
            {
              UNREFERENCED_PARAMETER(y);

              for (size_t j = 0; j < width; ++j)
              {
                XMVECTOR v = *pixels++;

                v =
                  XMVectorMax(g_XMZero, XMVector3Transform(v, c_from709toXYZ));

                luminance_freq[
                  std::clamp((int)
                    std::roundf(
                      (XMVectorGetY(v) - fMinLum) /
                      (fLumRange / 65536.0f)),
                    0, 65535)]++;
              }
            });

          double percent = 100.0;
          const double img_size = (double)image.width *
            (double)image.height;

          for (auto i = 65535; i >= 0; --i)
          {
            percent -=
              100.0 * ((double)luminance_freq[i] / img_size);

            if (percent <= 99.5)
            {
              fMaxLum =
                fMinLum + (fLumRange * ((float)i / 65536.0f));

              break;
            }
          }

          avif_image->clli.maxCLL =
            static_cast <uint16_t> (80.0f * fMaxLum);
          avif_image->clli.maxPALL =
            static_cast <uint16_t> (80.0f * (fLumAccum / N));
        }

        const float fMaxVal =
          Config::HDR_bitdepth == 8 ? 256.0f :
          Config::HDR_bitdepth == 10 ? 1024.0f :
          Config::HDR_bitdepth == 12 ? 4096.0f :
          4096.0f;

        const float fClampVal =
          Config::HDR_bitdepth == 8 ? 255.0f :
          Config::HDR_bitdepth == 10 ? 1023.0f :
          Config::HDR_bitdepth == 12 ? 4095.0f :
          4095.0f;

        XMVECTOR vMaxVal =
          XMVectorReplicate(fMaxVal);
        XMVECTOR vClampVal =
          XMVectorReplicate(fClampVal);

        EvaluateImage(image,
          [&](_In_reads_(width) const XMVECTOR* pixels, size_t width, size_t y)
          {
            UNREFERENCED_PARAMETER(y);

            for (size_t j = 0; j < width; ++j)
            {
              XMVECTOR value = pixels[j];

              value =
                XMVectorSaturate(
                  SKIV_Image_LinearToPQ(
                    XMVector3Transform(value, c_scRGBtoBt2100)
                  )
                );

              value =
                XMVectorMin(XMVectorMultiply(value, vMaxVal), vClampVal);

              if (bit_depth > 8)
              {
                *(rgb16_pixels++) = static_cast <uint16_t> (DirectX::XMVectorGetX(value));
                *(rgb16_pixels++) = static_cast <uint16_t> (DirectX::XMVectorGetY(value));
                *(rgb16_pixels++) = static_cast <uint16_t> (DirectX::XMVectorGetZ(value));
              }

              else
              {
                *(rgb8_pixels++) = static_cast <uint8_t> (DirectX::XMVectorGetX(value));
                *(rgb8_pixels++) = static_cast <uint8_t> (DirectX::XMVectorGetY(value));
                *(rgb8_pixels++) = static_cast <uint8_t> (DirectX::XMVectorGetZ(value));
              }
            }
          });
      } break;
      }

      rgbToYuvResult =
        SK_avifImageRGBToYUV(avif_image, &rgb);
    }

    if (rgbToYuvResult == AVIF_RESULT_OK)
    {
      encoder =
        SK_avifEncoderCreate();

      if (encoder != nullptr)
      {
        SYSTEM_INFO     si = { };
        GetSystemInfo(&si);

        encoder->quality = Config::Quality;
        encoder->qualityAlpha = Config::Quality; // N/A?
        encoder->timescale = 1;
        encoder->repetitionCount = AVIF_REPETITION_COUNT_INFINITE;
#ifdef _M_X64
        encoder->maxThreads = std::min(64U, std::min((UINT)si.dwNumberOfProcessors, (UINT)__popcnt64(si.dwActiveProcessorMask)));
#endif
        encoder->minQuantizer = AVIF_QUANTIZER_BEST_QUALITY;
        encoder->maxQuantizer = AVIF_QUANTIZER_WORST_QUALITY;
        encoder->codecChoice = AVIF_CODEC_CHOICE_AUTO;
        encoder->speed = Config::Speed;

        sk_avif_add_icc_to_image(avif_image);
        addResult = SK_avifEncoderAddImage(encoder, avif_image, 1, AVIF_ADD_IMAGE_FLAG_SINGLE);
        encodeResult = SK_avifEncoderFinish(encoder, &avifOutput);
      }
    }

    if (rgbToYuvResult != AVIF_RESULT_OK ||
      addResult != AVIF_RESULT_OK ||
      encodeResult != AVIF_RESULT_OK)
    {
      LOG << L"rgbToYUV: " << rgbToYuvResult << L" addImage: " << addResult << L" encode: " << encodeResult;
    }

    if (encodeResult == AVIF_RESULT_OK)
    {
      FILE* fAVIF =
        _wfopen(wszImplicitFileName, L"wb");

      if (fAVIF != nullptr)
      {
        fwrite(avifOutput.data, 1, avifOutput.size, fAVIF);
        fclose(fAVIF);
      }
    }

    if (avif_image != nullptr) SK_avifImageDestroy(avif_image);
    if (encoder != nullptr) SK_avifEncoderDestroy(encoder);

    SK_avifRGBImageFreePixels(&rgb);

    return
      (encodeResult == AVIF_RESULT_OK) ? S_OK : E_FAIL;
  }

  else
  {
    // What the hell is this?
    LOG_E << "Unknown encoder format for file extension '" << wszExtension << "'";
    return E_UNEXPECTED;
  }

  return
    DirectX::SaveToWICFile(*pOutputImage, DirectX::WIC_FLAGS_NONE, wic_codec,
      wszImplicitFileName, nullptr, SK_WIC_SetMaximumQuality);
}


HRESULT
SKIV_Image_SaveToDisk_SDR(const DirectX::Image& image, const wchar_t* wszFileName, const bool force_sRGB)
{
  using namespace DirectX;

  ScratchImage
    scratch_image;
  scratch_image.InitializeFromImage(image);

  wchar_t* wszExtension =
    PathFindExtensionW(wszFileName);

  wchar_t wszImplicitFileName[MAX_PATH] = { };
  wcscpy(wszImplicitFileName, wszFileName);

  // For silly users who don't give us filenames...
  if (!wszExtension)
  {
    PathAddExtension(wszImplicitFileName, defaultSDRFileExt.c_str());
    wszExtension =
      PathFindExtensionW(wszImplicitFileName);
  }

  using namespace DirectX;

  //float mastering_max_nits = out_desc1.MaxLuminance;
  //float mastering_sdr_nits = SKIF_Util_GetSDRWhiteLevel (out_desc1.Monitor);

  const Image* pOutputImage = scratch_image.GetImages();

  XMVECTOR maxLum = XMVectorZero(),
    minLum = XMVectorSplatInfinity(),
    maxICtCp = XMVectorZero();

  bool is_hdr = false;

  ScratchImage scrgb;
  ScratchImage final_sdr;

  if (image.format == DXGI_FORMAT_R16G16B16A16_FLOAT ||
    image.format == DXGI_FORMAT_R32G32B32A32_FLOAT)
  {
    is_hdr = true;

    if (FAILED(scrgb.InitializeFromImage(*scratch_image.GetImages())))
      return E_INVALIDARG;
  }

  bool bPrefer10bpcAs48bpp = false;
  bool bPrefer10bpcAs32bpp = false;

  GUID      wic_codec;
  WIC_FLAGS wic_flags = WIC_FLAGS_DITHER_DIFFUSION | (force_sRGB ? WIC_FLAGS_FORCE_SRGB : WIC_FLAGS_NONE);

  if (StrStrIW(wszExtension, L"jpg") ||
    StrStrIW(wszExtension, L"jpeg"))
  {
    wic_codec = GetWICCodec(WIC_CODEC_JPEG);

    if (DirectX::BitsPerColor(image.format) == 10 ||
      DirectX::BitsPerColor(image.format) == 16)
    {
      if (!is_hdr)
      {
        if (FAILED(scrgb.InitializeFromImage(*scratch_image.GetImages())))
          return E_INVALIDARG;

        TransformImage(*scratch_image.GetImages(),
          [&](_Out_writes_(width)       XMVECTOR* outPixels,
            _In_reads_(width) const XMVECTOR* inPixels,
            size_t    width,
            size_t)
          {
            for (size_t j = 0; j < width; ++j)
            {
              XMVECTOR value =
                inPixels[j];
              outPixels[j] =
                XMColorRGBToSRGB(
                  XMVectorSaturate(value)
                );
            }
          }, scrgb
        );

        pOutputImage = scrgb.GetImages();
      }
    }
  }

  else if (StrStrIW(wszExtension, L"png"))
  {
    wic_codec = GetWICCodec(WIC_CODEC_PNG);
    //bPrefer10bpcAs48bpp = is_hdr;

    wic_flags |= WIC_FLAGS_FORCE_SRGB;
    wic_flags |= WIC_FLAGS_DEFAULT_SRGB;

    if (DirectX::BitsPerColor(image.format) == 10 ||
      DirectX::BitsPerColor(image.format) == 16)
    {
      if (!is_hdr)
      {
        bPrefer10bpcAs48bpp = true;

        if (FAILED(scrgb.InitializeFromImage(*scratch_image.GetImages())))
          return E_INVALIDARG;

        TransformImage(*scratch_image.GetImages(),
          [&](_Out_writes_(width)       XMVECTOR* outPixels,
            _In_reads_(width) const XMVECTOR* inPixels,
            size_t    width,
            size_t)
          {
            for (size_t j = 0; j < width; ++j)
            {
              XMVECTOR value =
                inPixels[j];
              outPixels[j] =
                XMColorRGBToSRGB(
                  XMVectorSaturate(value)
                );
            }
          }, scrgb
        );

        pOutputImage = scrgb.GetImages();
      }
    }
  }

  else if (StrStrIW(wszExtension, L"bmp"))
  {
    wic_codec = GetWICCodec(WIC_CODEC_BMP);

    if (DirectX::BitsPerColor(image.format) == 10 ||
      DirectX::BitsPerColor(image.format) == 16)
    {
      if (!is_hdr)
      {
        if (FAILED(Convert(*scratch_image.GetImages(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DirectX::TEX_FILTER_DEFAULT, 0.0f, scrgb)))
          return E_INVALIDARG;

        pOutputImage = scrgb.GetImages();
      }
    }
  }

  else if (StrStrIW(wszExtension, L"tiff"))
  {
    wic_codec = GetWICCodec(WIC_CODEC_TIFF);
    bPrefer10bpcAs48bpp = false; // ?
    bPrefer10bpcAs32bpp = false; // ?

    if (DirectX::BitsPerColor(image.format) == 10 ||
      DirectX::BitsPerColor(image.format) == 16)
    {
      if (!is_hdr)
      {
        if (FAILED(scrgb.InitializeFromImage(*scratch_image.GetImages())))
          return E_INVALIDARG;

        TransformImage(*scratch_image.GetImages(),
          [&](_Out_writes_(width)       XMVECTOR* outPixels,
            _In_reads_(width) const XMVECTOR* inPixels,
            size_t    width,
            size_t)
          {
            for (size_t j = 0; j < width; ++j)
            {
              XMVECTOR value =
                inPixels[j];
              outPixels[j] =
                value;
            }
          }, scrgb
        );

        pOutputImage = scrgb.GetImages();
      }
    }
  }

  // Probably ignore this
  else if (StrStrIW(wszExtension, L"hdp") ||
    StrStrIW(wszExtension, L"jxr"))
  {
    wic_codec = GetWICCodec(WIC_CODEC_WMP);
    bPrefer10bpcAs32bpp = is_hdr;
  }

  // AVIF technically works for SDR... do we want to support it?
  //  If we do, WIC won't help us, however.

  else
  {
    return E_UNEXPECTED;
  }

  if (is_hdr)
  {
    if (bPrefer10bpcAs48bpp ||
      bPrefer10bpcAs32bpp)
    {
      wic_flags |= WIC_FLAGS_FORCE_SRGB;
    }

    ScratchImage tonemapped_hdr;
    ScratchImage tonemapped_copy;

    LOG << "SKIV_Image_TonemapToSDR ( ): EvaluateImageBegin";

    EvaluateImage(scrgb.GetImages(),
      scrgb.GetImageCount(),
      scrgb.GetMetadata(),
      [&](const XMVECTOR* pixels, size_t width, size_t y)
      {
        UNREFERENCED_PARAMETER(y);

        for (size_t j = 0; j < width; ++j)
        {
          XMVECTOR v = *pixels++;

          v =
            XMVector3Transform(v, c_from709toXYZ);

          maxLum =
            XMVectorReplicate(XMVectorGetY(XMVectorMax(v, maxLum)));

          minLum =
            XMVectorReplicate(XMVectorGetY(XMVectorMin(v, minLum)));
        }
      });

    minLum = XMVectorMax(g_XMZero, minLum);

    auto        luminance_freq = std::make_unique <uint32_t[]>(65536);
    ZeroMemory(luminance_freq.get(), sizeof(uint32_t) * 65536);

    const float fLumRange =
      XMVectorGetY(maxLum) -
      XMVectorGetY(minLum);

    EvaluateImage(scrgb.GetImages(),
      scrgb.GetImageCount(),
      scrgb.GetMetadata(),
      [&](const XMVECTOR* pixels, size_t width, size_t y)
      {
        UNREFERENCED_PARAMETER(y);

        for (size_t j = 0; j < width; ++j)
        {
          XMVECTOR v = *pixels++;

          v =
            XMVectorMax(g_XMZero, XMVector3Transform(v, c_from709toXYZ));

          luminance_freq[
            std::clamp((int)
              std::roundf(
                (XMVectorGetY(v) - XMVectorGetY(minLum)) /
                (fLumRange / 65536.0f)),
              0, 65535)]++;
        }
      });

    double percent = 100.0;
    const double img_size = (double)scrgb.GetMetadata().width *
      (double)scrgb.GetMetadata().height;

    for (auto i = 65535; i >= 0; --i)
    {
      percent -=
        100.0 * ((double)luminance_freq[i] / img_size);

      if (percent <= 99.94)
      {
        LOG << "99.94th percentile luminance: " <<
          80.0f * (XMVectorGetY(minLum) + (fLumRange * ((float)i / 65536.0f)))
          << " nits";

        maxLum =
          XMVectorReplicate(
            XMVectorGetY(minLum) + (fLumRange * ((float)i / 65536.0f))
          );

        break;
      }
    }

    LOG << "SKIV_Image_TonemapToSDR ( ): EvaluateImageEnd";

    // After tonemapping, re-normalize the image to preserve peak white,
    //   this is important in cases where the maximum luminance was < 1000 nits
    XMVECTOR maxTonemappedRGB = g_XMZero;

    // If it's too bright, don't bother trying to tonemap the full range...
    const float _maxNitsToTonemap = 125.0f;

    const float SDR_YInPQ =
      SKIV_Image_LinearToPQY(1.5f);

    const float  maxYInPQ =
      std::max(SDR_YInPQ,
        SKIV_Image_LinearToPQY(std::min(_maxNitsToTonemap, XMVectorGetY(maxLum)))
      );

    bool needs_tonemapping = false;

    //if (SKIV_Image_LinearToPQY (XMVectorGetY (maxLum)) > SDR_YInPQ)
    {
      needs_tonemapping = true;
    }

    TransformImage(scrgb.GetImages(),
      scrgb.GetImageCount(),
      scrgb.GetMetadata(),
      [&](XMVECTOR* outPixels, const XMVECTOR* inPixels, size_t width, size_t y)
      {
        UNREFERENCED_PARAMETER(y);

        auto TonemapHDR = [](float L, float Lc, float Ld) -> float
          {
            float a = (Ld / pow(Lc, 2.0f));
            float b = (1.0f / Ld);

            return
              L * (1 + a * L) / (1 + b * L);
          };

        for (size_t j = 0; j < width; ++j)
        {
          XMVECTOR value = inPixels[j];

          if (needs_tonemapping)
          {
            XMVECTOR ICtCp =
              SKIV_Image_Rec709toICtCp(value);

            float Y_in = std::max(XMVectorGetX(ICtCp), 0.0f);
            float Y_out = 1.0f;

            Y_out =
              TonemapHDR(Y_in, maxYInPQ, SDR_YInPQ);

            if (Y_out + Y_in > 0.0f)
            {
              ICtCp.m128_f32[0] =
                std::pow(Y_in, 1.18f);

              float I0 = XMVectorGetX(ICtCp);
              float I1 = 0.0f;
              float I_scale = 0.0f;

              ICtCp.m128_f32[0] *=
                std::max((Y_out / Y_in), 0.0f);

              I1 = XMVectorGetX(ICtCp);

              if (I0 != 0.0f && I1 != 0.0f)
              {
                I_scale =
                  std::min(I0 / I1, I1 / I0);
              }

              ICtCp.m128_f32[1] *= I_scale;
              ICtCp.m128_f32[2] *= I_scale;
            }

            value =
              SKIV_Image_ICtCptoRec709(ICtCp);

            maxTonemappedRGB =
              XMVectorMax(maxTonemappedRGB, XMVectorMax(value, g_XMZero));
          }

          if (bPrefer10bpcAs48bpp || bPrefer10bpcAs32bpp)
            outPixels[j] = XMVectorSaturate(value);
          else outPixels[j] = XMVectorSaturate(value);
        }
      }, tonemapped_hdr
    );

    float fMaxR = XMVectorGetX(maxTonemappedRGB);
    float fMaxG = XMVectorGetY(maxTonemappedRGB);
    float fMaxB = XMVectorGetZ(maxTonemappedRGB);

    if (false)
    {
      float fSmallestComp =
        std::min({ fMaxR, fMaxG, fMaxB });

      float fRescale =
        (1.0f / fSmallestComp);

      XMVECTOR vNormalizationScale =
        XMVectorReplicate(fRescale);

      LOG << "SKIV_Image_TonemapToSDR ( ): TransformImageBegin";
      TransformImage(*tonemapped_hdr.GetImages(),
        [&](_Out_writes_(width)       XMVECTOR* outPixels,
          _In_reads_(width) const XMVECTOR* inPixels,
          size_t    width,
          size_t)
        {
          for (size_t j = 0; j < width; ++j)
          {
            XMVECTOR value =
              inPixels[j];
            outPixels[j] =
              XMVectorSaturate(
                XMVectorMultiply(value, vNormalizationScale)
              );
          }
        }, tonemapped_copy
      );
      std::swap(tonemapped_hdr, tonemapped_copy);
    }

    if (FAILED(DirectX::Convert(*tonemapped_hdr.GetImages(), bPrefer10bpcAs48bpp ? DXGI_FORMAT_R16G16B16A16_UNORM :
      bPrefer10bpcAs32bpp ? DXGI_FORMAT_R10G10B10A2_UNORM :
      DXGI_FORMAT_B8G8R8X8_UNORM_SRGB,
      (TEX_FILTER_FLAGS)0x200000FF, 1.0f, final_sdr)))
    {
      return E_UNEXPECTED;
    }
    pOutputImage =
      final_sdr.GetImages();
  }

  return
    DirectX::SaveToWICFile(*pOutputImage, wic_flags, wic_codec,
      wszImplicitFileName, bPrefer10bpcAs48bpp ? &GUID_WICPixelFormat48bppRGB :
      bPrefer10bpcAs32bpp ? &GUID_WICPixelFormat32bppBGR101010 :
      &GUID_WICPixelFormat24bppBGR, SK_WIC_SetMaximumQuality);
}

bool
LoadLibraryTextureCLI(std::wstring FilePath, std::wstring OutFilePath)
{
  image_s image = { };

  image.file_info.path = FilePath;
  image.file_info.path_utf8 = SK_WideCharToUTF8(OutFilePath);
  image.file_info.path_utf8 = SK_WideCharToUTF8(FilePath);
  image.file_info.size = SK_File_GetSize(FilePath.c_str());

  CoInitializeEx(nullptr, COINIT_MULTITHREADED);

  if (!LoadLibraryTexture(image))
  {
    LOG_E << L"Failed to load image: " << FilePath;
    return false;
  }

  HRESULT hr = E_UNEXPECTED;

  auto pDevice =
    SKIF_D3D11_GetDevice();

  if (pDevice && image.pRawTexSRV.p != nullptr)
  {
    CComPtr <ID3D11DeviceContext>  pDevCtx;
    pDevice->GetImmediateContext(&pDevCtx);

    if (pDevCtx)
    {
      CComPtr <ID3D11Resource>        pCoverRes;
      image.pRawTexSRV->GetResource(&pCoverRes.p);

      DirectX::ScratchImage                                                captured_img;
      if (SUCCEEDED(DirectX::CaptureTexture(pDevice, pDevCtx, pCoverRes, captured_img)))
      {
        if (image.is_hdr && !Config::SDR)
          hr = SKIV_Image_SaveToDisk_HDR(*captured_img.GetImages(), OutFilePath.data());
        else
          hr = SKIV_Image_SaveToDisk_SDR(*captured_img.GetImages(), OutFilePath.data(), false);
      }
    }
  }

  if (FAILED(hr))
  {
    LOG_E << L"Failed to save image as " << OutFilePath;
    return false;
  }
  return true;
}