#include <windows.h>
#include <stdio.h>
#include <DWrite.h>
#include <wrl.h>
#include <memory>
#include "fontutil.hpp"

#pragma comment(lib, "dwrite.lib")

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

#define IFR(expr)       do {    \
    HRESULT res = expr;         \
    if (FAILED(res)) {          \
        printf("Failed %s 0x%lx\n", #expr, res); \
        return res;             \
    }                           \
} while(0)

HRESULT GetLogFontFromFileName(WCHAR const* fontFileName, LOGFONTW* logFont)
{
    // DWrite objects
    ComPtr<IDWriteFactory> dwriteFactory;
    ComPtr<IDWriteFontFace> fontFace;
    ComPtr<IDWriteFontFile> fontFile;
    ComPtr<IDWriteGdiInterop> gdiInterop;

    // Set up our DWrite factory and interop interface.
    IFR(DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            &dwriteFactory));
    IFR(dwriteFactory->GetGdiInterop(&gdiInterop));

    // Open the file and determine the font type.
    IFR(dwriteFactory->CreateFontFileReference(fontFileName, nullptr, &fontFile));
    BOOL isSupportedFontType = false;
    DWRITE_FONT_FILE_TYPE fontFileType;
    DWRITE_FONT_FACE_TYPE fontFaceType;
    UINT32 numberOfFaces = 0;
    IFR(fontFile->Analyze(&isSupportedFontType, &fontFileType, &fontFaceType, &numberOfFaces));

    if (!isSupportedFontType)
        return DWRITE_E_FILEFORMAT;

    // Set up a font face from the array of font files (just one)
    IDWriteFontFile * fontFileArray[] = {fontFile.Get()};
    IFR(dwriteFactory->CreateFontFace(
            fontFaceType,
            ARRAYSIZE(fontFileArray), // file count
            &fontFileArray[0], // or GetAddressOf if WRL ComPtr
            0, // faceIndex
            DWRITE_FONT_SIMULATIONS_NONE,
            &fontFace));

    // Get the necessary logical font information.
    IFR(gdiInterop->ConvertFontFaceToLOGFONT(fontFace.Get(), logFont));

    return S_OK;
}

bool saveBitmap(const wchar_t * fileName, const Bitmap &bitmap)
{
    int width = (int)bitmap.Width();
    int height = (int)bitmap.Height();
    width = (width + 3) / 4 * 4;

    std::unique_ptr<BYTE> newData(new BYTE[width * height]);
    for (int y = 0; y < height; y += 1) {
        for (int x = 0; x < bitmap.Width(); x += 1) {
            newData.get()[y * width + x] = bitmap[y][x].B;
        }
    }

    BITMAPINFO bmi = { {0} };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = (int)bitmap.Width();
    bmi.bmiHeader.biHeight = -height; // negative height to indicate top-down bitmap
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 8;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage = (int)bitmap.Width() * height;
    bmi.bmiHeader.biClrUsed = 0;
    bmi.bmiHeader.biClrImportant = 0;

    HANDLE hFile = CreateFileW(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("Create file failed\n");
        return false;
    }

    BITMAPFILEHEADER bmf = { 0 };
    bmf.bfType = 0x4D42; // "BM"
    bmf.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD);
    bmf.bfSize = bmf.bfOffBits + bmi.bmiHeader.biSizeImage;


    DWORD bytesWritten;
    WriteFile(hFile, &bmf, sizeof(bmf), &bytesWritten, NULL);
    WriteFile(hFile, &bmi.bmiHeader, sizeof(bmi.bmiHeader), &bytesWritten, NULL);

    // Write the grayscale palette to the file
    RGBQUAD grayscalePalette[256];
    for (int i = 0; i < 256; i++) {
        grayscalePalette[i].rgbRed = i;
        grayscalePalette[i].rgbGreen = i;
        grayscalePalette[i].rgbBlue = i;
        grayscalePalette[i].rgbReserved = 0;
    }
    WriteFile(hFile, grayscalePalette, 256 * sizeof(RGBQUAD), &bytesWritten, NULL);
    WriteFile(hFile, newData.get(), width * height, &bytesWritten, NULL);

    CloseHandle(hFile);

    return true;
}
