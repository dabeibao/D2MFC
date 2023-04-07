#pragma once

#include <windows.h>
#include "Bitmap.hpp"

class FontResource {
public:
    explicit FontResource();
    bool load(const wchar_t * fileName);
    void unload();
    ~FontResource();

    bool ok() { return mName != nullptr; }

private:
    wchar_t * mName;
};

class FontLoader {
public:
    explicit FontLoader();
    bool load(const wchar_t * fileName);
    const LOGFONTW&      logFont() { return mLf; }

private:
    FontResource        mRes;
    LOGFONTW             mLf;
};

class FontInfo {
public:
    explicit FontInfo(const wchar_t * fontName, int height);
    explicit FontInfo(const LOGFONTW & lf, int height);
    ~FontInfo();
    bool ok() { return mHFont != nullptr; }
    const TEXTMETRIC& metric() { return mMetric; }
    void dumpMetric();
    bool getBitmap(wchar_t c, Bitmap& bitmap, GLYPHMETRICS * metrics=nullptr);

private:
    void        init(HFONT font);

private:
    HDC         mHdc;
    HFONT       mHFont;
    TEXTMETRIC  mMetric;
};
