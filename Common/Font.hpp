#pragma once

#include "Common.hpp"

#include "Bitmap.hpp"
#include "FontTable.hpp"
#include "Sprite.hpp"

struct FontGlyph {
  // In, not used by TBL/DC6
  uint16_t    Char{};
  bool        AntiAliasing{true};
  int32_t     FaceIdx{-1}; // -1: no face
  uint32_t    Size{0};
  Pixel       FgCol{255,255,255};
  Pixel       BgCol{0,0,0};
  // Tbl Specific - also by config
  uint8_t     UnkTwo{1};
  // Out
  uint8_t     HasBmp{0}; // 0: no bmp; 1: dummy 1x1; 2: normal
  int32_t     BearX{};
  int32_t     BearY{};
  uint32_t    Advance{};
  Bitmap      Bmp{};

  bool        Valid{ true }; // valid glyph

  constexpr int32_t Descent() { return (int32_t) Bmp.Height() - BearY; }
};

struct Font {
  vector<unique_ptr<FontGlyph>> Glyphs{65536};
  // By Config
  vector<Palette> Pals{};
  vector<string> Faces{};
  uint32_t Size{};          // The first entry
  int32_t LnSpacingOff{0};
  int32_t CapHeightOff{0};
  int32_t DescentPadding{-1}; // -1 for automatic
  int32_t OriginOffset{ 0 }; // pop
  // Tbl Specific - also by config
  int32_t DescentOffset{ 0 }; // pop
  int32_t HeightConstant{ 14 }; // 14 ENG 15 JPN 17 CHI
  uint32_t LnSpacing{};
  uint32_t CapHeight{};
  uint16_t UnkHZ{};

  void Clear();
  void FromSprTbl(Sprite& Spr, FontTable& Tbl);
  //void ReadYml(const char* Path);

  void RenderGlyphs();
  void RenderGlyphsGDI(int size);
  void Dump(Sprite& Spr, FontTable& Tbl);

  pair<size_t, size_t> Extent(wstring_view Str);
  Bitmap Render(wstring_view Str);
};
