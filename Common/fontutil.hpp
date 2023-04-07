#pragma once

#include <windows.h>
#include "Bitmap.hpp"

bool ttfGetFamilyName(const WCHAR * path, WCHAR name[LF_FACESIZE]);
HRESULT GetLogFontFromFileName(WCHAR const* fontFileName, LOGFONTW* logFont);
bool saveBitmap(const wchar_t * fileName, const Bitmap& bitmap);
