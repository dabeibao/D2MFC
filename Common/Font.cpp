#include <windows.h>
#include "Font.hpp"

#include <math.h>
#include <ft2build.h>
#include <map>
#include <algorithm>
#include FT_FREETYPE_H

#include "FontLoader.hpp"

#define FtAss(e_) ((void) (!(e_) || (Abort("FreeType call failed: " # e_ ""), 0)))

void Font::Clear() {
  fill(Glyphs.begin(), Glyphs.end(), nullptr);
  Pals.clear();
  Faces.clear();
  Size = 0;
  LnSpacingOff = 1;
  CapHeightOff = 0;
  DescentPadding = -1;
  LnSpacing = 0;
  CapHeight = 0;
  UnkHZ = 0;
}

void Font::FromSprTbl(Sprite& Spr, FontTable& Tbl) {
  if (Spr.NDir() != 1)
    Abort("The number of directions should be 1 instead of %zu", Spr.NDir());
  if (Tbl.Hdr.NChar != Spr.NFrm())
    Abort("The number of images (%zu) should be the same with characters (%u)", Spr.NFrm(), Tbl.Hdr.NChar);
  Clear();
  Size = Tbl.Hdr.LnSpacing;
  LnSpacing = Tbl.Hdr.LnSpacing;
  CapHeight = Tbl.Hdr.CapHeight;
  UnkHZ = Tbl.Hdr.UnkHZ;
  for (auto i = 0u; i < Spr.NFrm(); ++i) {
    auto& C = Tbl.Chrs[i];
    auto& G = Glyphs[C.Char];
    Assert(!G);
    G.reset(new FontGlyph);
    G->Char = C.Char;
    G->Size = Tbl.Hdr.LnSpacing;
    G->UnkTwo = C.UnkTwo;
    G->HasBmp = 2;
    G->BearY = C.Height;
    G->Advance = C.Width;
    if (C.Dc6Index >= Spr.NFrm())
      Abort("DC6 index (%u) is too large for char (%u): should be less than %zu", C.Dc6Index, G->Char, Spr.NFrm());
    G->Bmp = move(Spr[0][C.Dc6Index]);
  }
}

static size_t shrink(Bitmap& bmp)
{
    return bmp.Height();
    if (bmp.Count() == 0) {
        return 0;
    }

    size_t y;

#define POS     1
#if POS
    for (y = bmp.Height() - 1; y >= 0; y -= 1) {
        for (size_t x = 0; x < bmp.Width(); x += 1) {
            if (bmp[y][x].Rgb() != 0) {
                goto out;
            }
        }
    }
#else
    for (y = 0; y < bmp.Height(); y += 1) {
        for (size_t x = 0; x < bmp.Width(); x += 1) {
            if (bmp[y][x].Rgb() != 0) {
                goto out;
            }
        }
    }
#endif
    return 0;

out:
#if POS
    auto height = y + 1;
    //bmp.Resize(bmp.Width(), height);
    for (; y < bmp.Height(); y += 1) {
        for (size_t x = 0; x < bmp.Width(); x += 1) {
            if (bmp[y][x].Rgb() == 0) {
                bmp[y][x] = Pixel(255, 255, 255);
            }
        }
    }
    return bmp.Height();
#else
    if (y == 0) {
        return bmp.Height();
    }
    auto newHeight = bmp.Height() - y;
    auto newBmp = Bitmap(bmp.Width(), newHeight);
    for (size_t i = 0; i < newHeight; i += 1) {
        copy(&bmp[i+y][0], &bmp[i+y][bmp.Width()], &newBmp[i][0]);
    }
    bmp = std::move(newBmp);
    return newHeight;

#endif
}

static double unitToPixel(uint32_t size, int unit, int unitsPerEM)
{
    const double dpi = 72.0;
    return unit * (int)size * dpi / 72 / unitsPerEM;
}

static int getFontHeight(FT_Face face, uint32_t size)
{
    return (int)unitToPixel(size, face->height, face->units_per_EM);
}

static void getFontMetrics(FT_Face face, FT_GlyphSlot glyph, uint32_t size, int * width, int * height, int * offsetX, int * offsetY)
{
    int fontHeight = (int)unitToPixel(size, face->height, face->units_per_EM);
    int fontDescender = (int)unitToPixel(size, face->descender, face->units_per_EM);
    int base = glyph->bitmap_top - glyph->bitmap.rows;
    int offset = base - fontDescender;

    printf("OFFSET:\n");
    printf("    base:           %d\n", base);
    printf("    descend:        %d\n", fontDescender);
    printf("    X:              %d\n", glyph->bitmap_left);
    printf("    Y:              %d\n", offset);
    printf("    W:              %d\n", glyph->metrics.horiAdvance / 64);
    printf("    H:              %d\n", fontHeight);

    if (width)
        *width = glyph->metrics.horiAdvance / 64;
    if (height)
        *height = fontHeight;
    if (offsetX)
        *offsetX = glyph->bitmap_left;
    if (offsetY)
        *offsetY = offset;
}

static void dumpGlyphy(wchar_t c, FT_Face face, FT_GlyphSlot glyph, uint32_t size)
{
    auto & metrics = glyph->metrics;
    auto & bitmap = glyph->bitmap;

    if (c == L'e' || c == L'l') {
    } else {
        return;
    }
    printf("Face (0x%x):\n", c);
    // height: (ymax - ymin) * size / units_per_EM
    printf("    Height:         %d\n", face->height);
    printf("    UnitsPerEM:     %d\n", face->units_per_EM);
    printf("    ascender:       %d\n", face->ascender);
    printf("    descender:      %d\n", face->descender);
    printf("    xMin:           %ld\n", face->bbox.xMin);
    printf("    xMax:           %ld\n", face->bbox.xMax);
    printf("    yMin:           %ld\n", face->bbox.yMin);
    printf("    yMax:           %ld\n", face->bbox.yMax);
    printf("Size: %d Left: %d, Top: %d\n", size, glyph->bitmap_left, glyph->bitmap_top);
    printf("Metrics:\n");
    printf("    Width:          %ld\n", metrics.width);
    printf("    Height:         %ld\n", metrics.height);
    printf("    BearX:          %ld\n", metrics.horiBearingX);
    printf("    BearY:          %ld\n", metrics.horiBearingY);
    printf("    Adv:            %ld\n", metrics.horiAdvance);
    printf("Bitmap:\n");
    printf("    Rows:           %d\n", bitmap.rows);
    printf("    Width:          %d\n", bitmap.width);
    printf("    Pitch:          %d\n", bitmap.pitch);

    getFontMetrics(face, glyph, size, nullptr, nullptr, nullptr, nullptr);
}

static void fillEmpty(FontGlyph * G)
{
    G->Valid = false;
    G->BearX = 0;
    G->BearY = 1;
    G->Advance = 1;
    G->HasBmp = 1;
    G->Bmp.Resize(1, 1);
    G->Bmp.Fill({});
}

wchar_t ToSimple(wchar_t _str)
{
    wchar_t src[] = { _str, L'\0', };
    wchar_t dest[2] = { L'\0' };
    int ret = LCMapStringW(0x0804,LCMAP_SIMPLIFIED_CHINESE, &src[0], 1, &dest[0], 2);
    if (ret == 0) {
        Warn("Convert failed, error %lu\n", GetLastError());
        return _str;
    }
    return dest[0];
}

class FontInfoImpl {
public:
    FontInfoImpl(const LOGFONTW& lf, int size):
        mInfo(lf, size)
    {
        mFallbacks.push_back(new FontInfo(L"Microsoft Sans Serif", size));
        mFallbacks.push_back(new FontInfo(L"Microsoft YaHei UI", size));
    }

    ~FontInfoImpl()
    {
        for (auto fallbacks: mFallbacks) {
            delete fallbacks;
        }
    }

    const TEXTMETRIC& metric() { return mInfo.metric(); }

    bool getBitmap(wchar_t c, Bitmap& bitmap, GLYPHMETRICS * metrics=nullptr)
    {
        bool ok = false;

        ok = mInfo.getBitmap(c, bitmap, metrics);
        if (ok) {
            return true;
        }
        for (auto& fallback: mFallbacks) {
            ok = fallback->getBitmap(c, bitmap, metrics);
            if (ok) {
                Warn("Replace char %u(0x%x) with fallback", c, c);
                return true;
            }
        }
        return false;
    }

private:
    FontInfo                    mInfo;
    std::vector<FontInfo *>     mFallbacks;

};

void Font::RenderGlyphsGDI(int size)
{
  vector<FontGlyph*> ToRender;

  for (auto Ch = 0u; Ch < Glyphs.size(); ++Ch) {
      auto& G = Glyphs[Ch];
      if (!G) {
          continue;
      }
      Assert(G->Char == Ch);
      if (G->HasBmp) {
          continue;
      }
      ToRender.emplace_back(Glyphs[Ch].get());
  }
  string name = Faces[0];
  wstring wName = wstring(name.begin(), name.end());
  FontLoader loader;
  if (!loader.load(wName.c_str())) {
      Abort("Load %lS failed\n", wName.c_str());
  }
  FontInfoImpl impl(loader.logFont(), size);
  printf("FONT height: %ld\n", impl.metric().tmHeight);

  for (auto& G: ToRender) {
      GLYPHMETRICS gm;
      wchar_t c = ToSimple(G->Char);
      bool ok = impl.getBitmap(c, G->Bmp, &gm);
      if (!ok) {
          if (c == 0x3000) {
              Warn("A1A1 not found, generate default blank character");
              ok = impl.getBitmap(0x7530, G->Bmp, &gm);
              if (ok) {
                  Warn("Generate a1 w %zd h %zd", G->Bmp.Width(), G->Bmp.Height());
                  G->Bmp.Fill({});
              }
          }
          if (!ok && (c >= 32 && c < 127)) {
              ok = impl.getBitmap(L'.', G->Bmp, &gm);
              if (ok) {
                  Warn("Replace %u(0x%x) with space w %zd h %zd", c, c, G->Bmp.Width(), G->Bmp.Height());
                  G->Bmp.Fill({});
              }
          }
      }
      if (!ok) {
          Warn("No glyph found for char (%u), a dummy (1x1) bitmap will be generated", G->Char);
          fillEmpty(G);
          continue;
      }
      //Warn("Char %lc %zd %zd", G->Char, G->Bmp.Width(), G->Bmp.Height());
      G->BearX = gm.gmBlackBoxX;
      G->BearY = gm.gmBlackBoxY;
      G->HasBmp = 2;
      G->Advance = gm.gmCellIncX;
  }

  LnSpacing = impl.metric().tmHeight;
  CapHeight = 1;
}

void Font::RenderGlyphs() {
  vector<FontGlyph*> ToRender;

  for (auto Ch = 0u; Ch < Glyphs.size(); ++Ch) {
    auto& G = Glyphs[Ch];
    if (!G)
      continue;
    Assert(G->Char == Ch);
    if (G->HasBmp)
      continue;
    if (G->FaceIdx < 0)
      Abort("No font face specified for char (%u)", Ch);
    if ((size_t) G->FaceIdx >= Faces.size())
      Abort("Face index for char (%u) is too large: %d > %zu", Ch, G->FaceIdx, Faces.size());
    if (!Glyphs[Ch]->Size)
      Abort("The size of char (%u) should not be 0", Ch);
    ToRender.emplace_back(Glyphs[Ch].get());
  }
  sort(ToRender.begin(), ToRender.end(),
    [](FontGlyph* A, FontGlyph* B) {
      return A->FaceIdx != B->FaceIdx ? A->FaceIdx < B->FaceIdx : A->Size < B->Size;
    }
  );
  int32_t LastFace{-1};
  uint32_t LastSize{};
  FT_Library Lib{};
  FtAss(FT_Init_FreeType(&Lib));
  FT_Face Face{};
  for (auto& G : ToRender) {
    if (G->FaceIdx != LastFace) {
      if (Face)
        FtAss(FT_Done_Face(Face));
      FtAss(FT_New_Face(Lib, Faces[G->FaceIdx].c_str(), 0, &Face));
      LastFace = G->FaceIdx;
      LastSize = 0;
    }
    if (G->Size != LastSize) {
      FtAss(FT_Set_Pixel_Sizes(Face, 0, G->Size));
      LastSize = G->Size;
    }
    auto FtgIdx = FT_Get_Char_Index(Face, G->Char);
    if (!FtgIdx) {
      Warn("No glyph found for char (%u), a dummy (1x1) bitmap will be generated", G->Char);
      G->Valid = false;
      G->BearX = 0;
      G->BearY = 1;
      G->Advance = 1;
      G->HasBmp = 1;
      G->Bmp.Resize(1, 1);
      G->Bmp.Fill({});
    }
    else {
      FtAss(FT_Load_Glyph(Face, FtgIdx, G->AntiAliasing ? FT_LOAD_DEFAULT : FT_LOAD_TARGET_MONO | FT_LOAD_MONOCHROME));
      if (Face->glyph->format != FT_GLYPH_FORMAT_BITMAP)
        FtAss( FT_Render_Glyph(Face->glyph, G->AntiAliasing ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO));
      auto& Ftg = Face->glyph;
      auto& Ftb = Face->glyph->bitmap;
      auto& metrics = Face->glyph->metrics;
      if (!Ftb.width || !Ftb.rows) {
        Warn("Empty bitmap generated for char (%u), a dummy (1x1) bitmap will be generated", G->Char);
        G->Valid = false;
        G->BearX = 0;
        G->BearY = 1;
        G->Advance = Ftg->advance.x >> 6;
        G->HasBmp = 1;
        G->Bmp.Resize(1, 1);
        G->Bmp.Fill({});
      }
      else {
        dumpGlyphy(G->Char, Face, Ftg, G->Size);
        G->BearX = Ftg->bitmap_left;
        G->BearY = Ftg->bitmap_top;
        G->Advance = Ftg->advance.x >> 6;
        G->HasBmp = 2;
        G->Bmp.Resize(Ftb.width, Ftb.rows);
        if (G->AntiAliasing) {
          for (auto i = 0u; i < Ftb.rows; ++i)
            for (auto j = 0u; j < Ftb.width; ++j) {
              auto Col = Ftb.buffer[i * Ftb.pitch + j];
              G->Bmp[i][j].R = Col;
              G->Bmp[i][j].G = Col;
              G->Bmp[i][j].B = Col;
#ifdef BMP_ALPHA
              G->Bmp[i][j].A = Col ? 255 : 0;
#endif
            }
        }
        else {
          for (auto i = 0u; i < Ftb.rows; ++i)
            for (auto j = 0u; j < Ftb.width; ++j) {
              auto Col = Ftb.buffer[i * Ftb.pitch + (j >> 3)] & (1u << ((j & 7) ^ 7));
              G->Bmp[i][j].R = Col ? 255 : 0;
              G->Bmp[i][j].G = Col ? 255 : 0;
              G->Bmp[i][j].B = Col ? 255 : 0;
#ifdef BMP_ALPHA
              G->Bmp[i][j].A = Col ? 255 : 0;
#endif
            }
        }
      }
    }
  }
  if (Face)
    FtAss(FT_Done_Face(Face));
  FtAss(FT_Done_FreeType(Lib));
  auto MaxDescent = int32_t{};
  for (auto& G : ToRender)
    if (G->HasBmp == 2)
      MaxDescent = max(MaxDescent, G->Descent());
  auto MaxPadding = ~DescentPadding ? DescentPadding : MaxDescent + OriginOffset + DescentOffset;
  auto MaxH = size_t{};
  printf("LastSize: %d\n", LastSize);
  auto fontHeight = getFontHeight(Face, LastSize);

  std::map<int, int> heightCount;
  for (auto& G : ToRender) {
    if (G->HasBmp != 2)
      continue;
    if (G->BearX < 0) {
      Warn("BearX is negative (%d) for char (%u), set it to 0", G->BearX, G->Char);
      G->BearX = 0;
    }
    if (G->BearX || G->Descent() != MaxPadding) {
      auto W = G->BearX + (int32_t) G->Bmp.Width();
      auto H = fontHeight;
      if (W <= 0 || H <= 0) {
        Warn("The bitmap of char (%u) is completely cropped out, a dummy (1x1) bitmap will be generated", G->Char);
        G->HasBmp = 1;
        G->Bmp.Resize(1, 1);
        G->Bmp.Fill({});
        continue;
      }
      auto Bmp = Bitmap(W, H);
      Bmp.Fill({});
      //int offsetY = MaxPadding + G->BearY - G->Bmp.Height();
      int offsetY = fontHeight - G->BearY - MaxPadding;
      if (G->Char == L'e' || G->Char == L'l') {
          printf("Char 0x%x W: %d, H: %d, X: %d, Y: %d, bmW: %zd, bmH: %zd\n",
                 G->Char,
                 W, H, G->BearX, offsetY, G->Bmp.Width(), G->Bmp.Height());
      }
      Bmp.Draw(G->Bmp, G->BearX, offsetY);
      // auto height = shrink(Bmp);
      // if (height == 0) {
      //   Warn("The bitmap of char (%u) is shrinked out, a dummy (1x1) bitmap will be generated", G->Char);
      //   G->HasBmp = 1;
      //   G->Bmp.Resize(1, 1);
      //   G->Bmp.Fill({});
      //   continue;
      // }
      G->Bmp = move(Bmp);
    }
    heightCount[(int)G->Bmp.Height()] += 1;
    MaxH = max(MaxH, G->Bmp.Height());
  }
  auto ptr = std::max_element(heightCount.begin(), heightCount.end(), [](const auto& x, const auto &y) {
      return x.second < y.second;
  });
  auto mostH = ptr->first;
  auto count = ptr->second;
  if ((int)MaxH != mostH) {
      Warn("Update MAXH: %zd, mostH: %d (%d)\n", MaxH, mostH, count);
      MaxH = mostH;
      // for (auto& G: ToRender) {
      //     if (G->Bmp.Height() > MaxH) {
      //         auto oH = G->Bmp.Height();
      //         G->Bmp.Shrink(MaxH);
      //         Warn("Shrink (%u) from %zd to %zd", G->Char, oH, G->Bmp.Height());
      //     }
      // }
  }
  if (!LnSpacing)
    LnSpacing = (uint32_t)(ceil((float)(MaxH - MaxDescent * 10 / HeightConstant)) + LnSpacingOff);
  auto ActualSpacing = HeightConstant * LnSpacing / 10;
  if (ActualSpacing < MaxH)
    Warn("The maximum height (%zu) of newly generated glyphs is larger than Actual Spacing (%u)", MaxH, ActualSpacing);
  if (!CapHeight)
    CapHeight = 1; // CapHeightOff + (Size / 2);

  printf("MaxH %zd MaxDescent %d HeightConstant %d LnSpacingOff %d\n",
         MaxH, MaxDescent, HeightConstant, LnSpacing);
}

pair<size_t, size_t> Font::Extent(wstring_view Str) {
  auto NLine = (uint32_t) count(Str.begin(), Str.end(), L'\n') + 1;
  auto W = size_t{0};
  auto H = size_t{NLine * LnSpacing};
  auto X = size_t{0};
  auto XMax = size_t{0};
  auto HCur = H - LnSpacing;
  for (auto Ch : Str) {
    if (Ch == L'\n') {
      --NLine;
      W = max(W, XMax);
      XMax = 0u;
      HCur -= LnSpacing;
      X = 0u;
      continue;
    }
    auto& G = Glyphs[Ch];
    if (!G->HasBmp)
      Abort("No bitmap for char (%d)", (int) Ch);
    H = max(H, HCur + G->Bmp.Height());
    XMax = max(XMax, X + G->BearX + G->Bmp.Width());
    X += G->Advance;
  }
  W = max(W, XMax);
  return {W, H};
}

Bitmap Font::Render(wstring_view Str) {
  auto [W, H] = Extent(Str);
  auto NLine_ = (int32_t) count(Str.begin(), Str.end(), L'\n');
  auto X = (int32_t) 0;
  auto Y = (int32_t) (H - NLine_ * LnSpacing);
  Bitmap Bmp(W, H);
  Bmp.Fill({});
  for (auto Ch : Str) {
    if (Ch == L'\n') {
      X = 0u;
      Y += LnSpacing;
      continue;
    }
    auto& G = Glyphs[Ch];
    if (!G->HasBmp)
      Abort("No bitmap for char (%d)", (int) Ch);
    Bmp.Draw(G->Bmp, X + G->BearX, Y - G->BearY);
    X += G->Advance;
  }
  return Bmp;
}

void Font::Dump(Sprite& Spr, FontTable& Tbl) {
  auto NChar = 0u;
  for (auto Ch = 0u; Ch < Glyphs.size(); ++Ch)
    if (Glyphs[Ch])
      ++NChar;
  Tbl.Hdr.Sign = TblSign;
  Tbl.Hdr.One = 1;
  Tbl.Hdr.UnkHZ = UnkHZ;
  Tbl.Hdr.NChar = Cast<uint16_t>(NChar, "Too many chars (%u)", NChar);
  Tbl.Hdr.LnSpacing = LnSpacing;
  Tbl.Hdr.CapHeight = CapHeight;
  Tbl.Chrs.reset(new TblChar[NChar]);
  Spr.Resize(1, NChar);
  auto Id = 0u;
  for (auto Ch = 0u; Ch < Glyphs.size(); ++Ch)
    if (Glyphs[Ch]) {
      auto& C = Tbl.Chrs[Id];
      auto& G = Glyphs[Ch];
      if (!G->HasBmp)
        Abort("No bitmap for char (%u)", Ch);
      C.Char = G->Char;
      C.UnkCZ1 = 0;
      C.Width = Cast<uint8_t>(G->Advance, "The advance of char (%u) is too large (%u)", Ch, G->Advance);
      C.Height = Cast<uint8_t>(G->Bmp.Height(), "The height of char (%u) is too large (%zu)", Ch, G->Bmp.Height());
      C.UnkTwo = G->UnkTwo;
      C.UnkCZ2 = 0;
      C.Dc6Index = G->Valid == true ? (uint16_t) Id : (uint16_t) 0;
      C.ZPad1 = 0;
      C.ZPad2 = 0;
      Spr[0][Id] = move(G->Bmp);
      ++Id;
    }
  Assert(Id == NChar);
}
