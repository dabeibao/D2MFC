#include "FontLoader.hpp"
#include <stdio.h>
#include <windows.h>
#include <memory>
#include "fontutil.hpp"


//#define FIDBG

#ifdef FIDBG
#define FiDbg(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#else
#define FiDbg(fmt, ...)
#endif


FontResource::FontResource(): mName(nullptr)
{
}

bool FontResource::load(const wchar_t * fileName)
{
    unload();
    int cnt = AddFontResourceExW(fileName, FR_PRIVATE, 0);
    if (cnt == 0) {
        return false;
    }
    mName = _wcsdup(fileName);

    return true;
}

void FontResource::unload()
{
    if (mName != nullptr) {
        RemoveFontResourceExW(mName, FR_PRIVATE, 0);
        free(mName);
        mName = nullptr;
    }
}

FontResource::~FontResource()
{
    unload();
}

FontLoader::FontLoader()
{
    ZeroMemory(&mLf, sizeof(mLf));
}

namespace {

BYTE normalizeGray(BYTE gray)
{
    static BYTE map[65];
    static bool isInited;
    if (!isInited) {
        for (int i = 0; i < ARRAYSIZE(map); i += 1) {
            double output = 255 * pow(i / 64.0, 1 / 2.2);
            int o = (int)output;
            if (o > 255) {
                o = 255;
            }
            map[i] = o;
        }
    }

    if (gray >= ARRAYSIZE(map)) {
        return 0xff;
    }
    return map[gray];
}

bool adjustFontSize(int fontWidth, int fontHeight, int offsetX, int offsetY, const GLYPHMETRICS& gm,
                    int *outWidth, int *outHeight)
{
    if (fontWidth <= 0 || fontHeight <= 0) {
        // cannot support this character
        return false;
    }
    if (offsetX >= fontWidth || offsetY >= fontHeight) {
        return false;
    }
    int realWidth = gm.gmBlackBoxX;
    int realHeight = gm.gmBlackBoxY;

    *outWidth = realWidth;
    *outHeight = realHeight;

    return true;
}

}

bool FontLoader::load(const wchar_t *fileName)
{
    if (!mRes.load(fileName)) {
        return false;
    }
    auto result = GetLogFontFromFileName(fileName, &mLf);
    if (result < 0) {
        return false;
    }

    return true;
}

FontInfo::FontInfo(const wchar_t * fontName, int height)
{
    HFONT hFont = CreateFontW(-height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, fontName);
    init(hFont);
}

FontInfo::FontInfo(const LOGFONTW & lf, int height):
    mHdc(nullptr), mHFont(nullptr)
{
    LOGFONTW logFont = lf;
    logFont.lfHeight = -height;
    //logFont.lfQuality = PROOF_QUALITY;
    HFONT font = CreateFontIndirectW(&logFont);
    init(font);
}

void FontInfo::init(HFONT font)
{
    if (font == nullptr) {
        mHFont = nullptr;
        mHdc = nullptr;
        return;
    }

    mHdc = CreateCompatibleDC(GetDC(nullptr));
    mHFont = font;
    SelectObject(mHdc, font);
    GetTextMetrics(mHdc, &mMetric);
}

FontInfo::~FontInfo()
{
    if (mHFont != nullptr) {
        DeleteObject(mHFont);
        DeleteDC(mHdc);
    }
}

void FontInfo::dumpMetric()
{
    printf("TM info:\n");
    printf("    Height:         %ld\n", mMetric.tmHeight);
    printf("    Descent:        %ld\n", mMetric.tmDescent);
    printf("    Ascent:         %ld\n", mMetric.tmAscent);
    printf("    Leading:        %ld\n", mMetric.tmExternalLeading);
}

bool FontInfo::getBitmap(wchar_t c, Bitmap &bitmap, GLYPHMETRICS * outGm)
{
    DWORD num;
    WORD gi;

    num = GetGlyphIndicesW(mHdc, &c, 1, &gi, GGI_MARK_NONEXISTING_GLYPHS);
    if (num != 1 || gi == 0xffff) {
        FiDbg("Char 0x%x not found\n", c);
        return false;
    }
    MAT2 mat2 = { {0, 1}, {0, 0}, {0, 0}, {0, 1} };
    GLYPHMETRICS gm = { 0 };
    DWORD size = GetGlyphOutlineW(mHdc, c, GGO_GRAY8_BITMAP, &gm, 0, NULL, &mat2);
    if (size == GDI_ERROR) {
        FiDbg("Char 0x%x get glyph failed\n", c);
        return false;
    }
    // FiDbg("Bitmap size %ld\n", size);
    std::unique_ptr<BYTE> bitmapData(new BYTE[size]);
    ZeroMemory(bitmapData.get(), size);
    if (GetGlyphOutlineW(mHdc, c, GGO_GRAY8_BITMAP, &gm, size, bitmapData.get(), &mat2) == GDI_ERROR) {
        FiDbg("Char 0x%x get glyph 2 failed\n", c);
        return false;
    }
    for (DWORD i = 0; i < size; i += 1) {
        bitmapData.get()[i] = normalizeGray(bitmapData.get()[i]);
    }

    int bmWidth = (gm.gmBlackBoxX + 3) / 4 * 4;
    int bmHeight = gm.gmBlackBoxY;
    if ((int)size != bmWidth * bmHeight) {
        FiDbg("Invalid size: w %d h %d size %ld\n", bmWidth, bmHeight, size);
        return false;
    }

    int fontHeight = mMetric.tmHeight;
    int fontWidth = gm.gmCellIncX;
    int offsetX = gm.gmptGlyphOrigin.x;
    int offsetY = fontHeight - mMetric.tmDescent - gm.gmptGlyphOrigin.y;
    int realWidth;
    int realHeight;

    bool ok = adjustFontSize(fontWidth, fontHeight, offsetX, offsetY, gm, &realWidth, &realHeight);
    // printf("Font 0x%x W %d H %d, ox %d oy %d, Bw %d BH %d RW %d RH %d\n",
    //        c, fontWidth, fontHeight, offsetX, offsetY,
    //        gm.gmBlackBoxX, gm.gmBlackBoxY, realWidth, realHeight);
    if (!ok) {
        printf("Unsupport Font 0x%x W %d H %d, ox %d oy %d, Bw %d BH %d\n",
               c, fontWidth, fontHeight, offsetX, offsetY,
               gm.gmBlackBoxX, gm.gmBlackBoxY);
        return false;
    }

    bitmap.Resize(fontWidth, fontHeight);
    bitmap.Fill({});
    // FiDbg("BMP %zd %zd\n", bitmap.Width(), bitmap.Height());

    for (int y = 0; y < realHeight; y += 1) {
        int nY = y + offsetY;
        if (nY < 0) {
            continue;
        }
        if (nY >= fontHeight) {
            break;
        }
        auto row = bitmap[nY];
        // printf("Fill row %d, begin: %p now %p %d\n", y, bitmap.Raw(), row, (int)(row - bitmap.Raw()));
        for (int x = 0; x < realWidth; x += 1) {
            int nX = x + offsetX;
            if (nX < 0) {
                continue;
            }
            if (nX >= fontWidth) {
                break;
            }
            int bmOffset = y * bmWidth + x;
            BYTE data = bitmapData.get()[bmOffset];
            // FiDbg("Fill %d -> %d(%zd)\n", bmOffset, (y + offsetY) * (int)bitmap.Width() + x + offsetX, bitmap.Count());
            row[nX] = {data, data, data};
        }
    }
    if (outGm) {
        *outGm = gm;
    }

    return true;
}
