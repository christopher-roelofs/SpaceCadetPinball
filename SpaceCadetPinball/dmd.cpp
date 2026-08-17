#include "pch.h"
#include "dmd.h"

constexpr int DotMatrix::GlyphWidth, DotMatrix::GlyphHeight, DotMatrix::GlyphSpacing;

/*
 * 5x7 font, one byte per column, bit 0 is the top row.
 * Covers ASCII 0x20 - 0x5F; lowercase is folded to uppercase when drawing.
 */
static const uint8_t DmdFont[][DotMatrix::GlyphWidth] =
{
	{0x00, 0x00, 0x00, 0x00, 0x00}, // space
	{0x00, 0x00, 0x5F, 0x00, 0x00}, // !
	{0x00, 0x07, 0x00, 0x07, 0x00}, // "
	{0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
	{0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
	{0x23, 0x13, 0x08, 0x64, 0x62}, // %
	{0x36, 0x49, 0x55, 0x22, 0x50}, // &
	{0x00, 0x05, 0x03, 0x00, 0x00}, // '
	{0x00, 0x1C, 0x22, 0x41, 0x00}, // (
	{0x00, 0x41, 0x22, 0x1C, 0x00}, // )
	{0x14, 0x08, 0x3E, 0x08, 0x14}, // *
	{0x08, 0x08, 0x3E, 0x08, 0x08}, // +
	{0x00, 0x50, 0x30, 0x00, 0x00}, // ,
	{0x08, 0x08, 0x08, 0x08, 0x08}, // -
	{0x00, 0x60, 0x60, 0x00, 0x00}, // .
	{0x20, 0x10, 0x08, 0x04, 0x02}, // /
	{0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
	{0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
	{0x42, 0x61, 0x51, 0x49, 0x46}, // 2
	{0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
	{0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
	{0x27, 0x45, 0x45, 0x45, 0x39}, // 5
	{0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
	{0x01, 0x71, 0x09, 0x05, 0x03}, // 7
	{0x36, 0x49, 0x49, 0x49, 0x36}, // 8
	{0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
	{0x00, 0x36, 0x36, 0x00, 0x00}, // :
	{0x00, 0x56, 0x36, 0x00, 0x00}, // ;
	{0x00, 0x08, 0x14, 0x22, 0x41}, // <
	{0x14, 0x14, 0x14, 0x14, 0x14}, // =
	{0x41, 0x22, 0x14, 0x08, 0x00}, // >
	{0x02, 0x01, 0x51, 0x09, 0x06}, // ?
	{0x32, 0x49, 0x79, 0x41, 0x3E}, // @
	{0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
	{0x7F, 0x49, 0x49, 0x49, 0x36}, // B
	{0x3E, 0x41, 0x41, 0x41, 0x22}, // C
	{0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
	{0x7F, 0x49, 0x49, 0x49, 0x41}, // E
	{0x7F, 0x09, 0x09, 0x01, 0x01}, // F
	{0x3E, 0x41, 0x41, 0x51, 0x32}, // G
	{0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
	{0x00, 0x41, 0x7F, 0x41, 0x00}, // I
	{0x20, 0x40, 0x41, 0x3F, 0x01}, // J
	{0x7F, 0x08, 0x14, 0x22, 0x41}, // K
	{0x7F, 0x40, 0x40, 0x40, 0x40}, // L
	{0x7F, 0x02, 0x04, 0x02, 0x7F}, // M
	{0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
	{0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
	{0x7F, 0x09, 0x09, 0x09, 0x06}, // P
	{0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
	{0x7F, 0x09, 0x19, 0x29, 0x46}, // R
	{0x46, 0x49, 0x49, 0x49, 0x31}, // S
	{0x01, 0x01, 0x7F, 0x01, 0x01}, // T
	{0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
	{0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
	{0x7F, 0x20, 0x18, 0x20, 0x7F}, // W
	{0x63, 0x14, 0x08, 0x14, 0x63}, // X
	{0x03, 0x04, 0x78, 0x04, 0x03}, // Y
	{0x61, 0x51, 0x49, 0x45, 0x43}, // Z
	{0x00, 0x00, 0x7F, 0x41, 0x41}, // [
	{0x02, 0x04, 0x08, 0x10, 0x20}, // backslash
	{0x41, 0x41, 0x7F, 0x00, 0x00}, // ]
	{0x04, 0x02, 0x01, 0x02, 0x04}, // ^
	{0x40, 0x40, 0x40, 0x40, 0x40}, // _
};

static constexpr char DmdFontFirstChar = ' ';
static constexpr char DmdFontLastChar = '_';

DotMatrix::DotMatrix(int columns, int rows) : columns(0), rows(0)
{
	Resize(columns, rows);
}

void DotMatrix::Resize(int newColumns, int newRows)
{
	newColumns = std::max(1, newColumns);
	newRows = std::max(1, newRows);
	if (newColumns == columns && newRows == rows)
		return;

	columns = newColumns;
	rows = newRows;
	dots.assign(static_cast<size_t>(columns) * rows, 0);
}

void DotMatrix::Clear()
{
	std::fill(dots.begin(), dots.end(), static_cast<uint8_t>(0));
}

void DotMatrix::SetDot(int x, int y, uint8_t level)
{
	if (x < 0 || y < 0 || x >= columns || y >= rows)
		return;
	dots[static_cast<size_t>(y) * columns + x] = level;
}

uint8_t DotMatrix::GetDot(int x, int y) const
{
	if (x < 0 || y < 0 || x >= columns || y >= rows)
		return 0;
	return dots[static_cast<size_t>(y) * columns + x];
}

const uint8_t* DotMatrix::GetGlyph(char ch)
{
	auto c = static_cast<unsigned char>(ch);
	if (c >= 'a' && c <= 'z')
		c -= 'a' - 'A';
	if (c < DmdFontFirstChar || c > DmdFontLastChar)
		c = '?';
	return DmdFont[c - DmdFontFirstChar];
}

int DotMatrix::TextWidth(const char* text, int scale)
{
	if (!text || !*text)
		return 0;

	scale = std::max(1, scale);
	auto charCount = static_cast<int>(strlen(text));
	return (charCount * (GlyphWidth + GlyphSpacing) - GlyphSpacing) * scale;
}

void DotMatrix::DrawText(int x, int y, const char* text, int scale, uint8_t level)
{
	if (!text)
		return;

	scale = std::max(1, scale);
	for (auto ptr = text; *ptr; ++ptr)
	{
		auto glyph = GetGlyph(*ptr);
		for (auto col = 0; col < GlyphWidth; col++)
		{
			auto bits = glyph[col];
			for (auto row = 0; row < GlyphHeight; row++)
			{
				if (!(bits & (1u << row)))
					continue;

				// Scale up by drawing a scale x scale block per dot
				for (auto dy = 0; dy < scale; dy++)
					for (auto dx = 0; dx < scale; dx++)
						SetDot(x + (col * scale) + dx, y + (row * scale) + dy, level);
			}
		}
		x += (GlyphWidth + GlyphSpacing) * scale;
	}
}

void DotMatrix::DrawText(Align align, int y, const char* text, int scale, uint8_t level)
{
	auto width = TextWidth(text, scale);
	auto x = 0;
	switch (align)
	{
	case Align::Center:
		x = (columns - width) / 2;
		break;
	case Align::Right:
		x = columns - width;
		break;
	case Align::Left:
	default:
		break;
	}
	DrawText(x, y, text, scale, level);
}

void DotMatrix::Present(SDL_Renderer* renderer, const SDL_Rect& dest, ColorRgba onColor, ColorRgba offColor,
                        bool drawUnlitDots) const
{
	if (!renderer || dest.w <= 0 || dest.h <= 0)
		return;

	// Square dots, grid centered in dest
	auto pitch = std::min(dest.w / columns, dest.h / rows);
	if (pitch < 1)
		pitch = 1;
	auto gridWidth = pitch * columns, gridHeight = pitch * rows;
	auto originX = dest.x + (dest.w - gridWidth) / 2;
	auto originY = dest.y + (dest.h - gridHeight) / 2;

	// Leave a gap between dots so that individual dots stay visible
	auto dotSize = std::max(1, pitch - std::max(1, pitch / 8));
	auto dotOffset = (pitch - dotSize) / 2;

	SDL_BlendMode prevBlendMode;
	SDL_GetRenderDrawBlendMode(renderer, &prevBlendMode);
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

	for (auto row = 0; row < rows; row++)
	{
		for (auto col = 0; col < columns; col++)
		{
			auto level = dots[static_cast<size_t>(row) * columns + col];
			if (!level && !drawUnlitDots)
				continue;

			ColorRgba color = level ? onColor : offColor;
			if (level && level != 255)
			{
				// Dim lit dots by fading them towards the unlit color
				auto blend = [level](uint8_t on, uint8_t off)
				{
					return static_cast<uint8_t>((on * level + off * (255 - level)) / 255);
				};
				color = ColorRgba{
					blend(onColor.GetRed(), offColor.GetRed()),
					blend(onColor.GetGreen(), offColor.GetGreen()),
					blend(onColor.GetBlue(), offColor.GetBlue()),
					onColor.GetAlpha()
				};
			}

			SDL_SetRenderDrawColor(renderer, color.GetRed(), color.GetGreen(), color.GetBlue(), color.GetAlpha());
			SDL_Rect dotRect
			{
				originX + col * pitch + dotOffset,
				originY + row * pitch + dotOffset,
				dotSize, dotSize
			};
			SDL_RenderFillRect(renderer, &dotRect);
		}
	}

	SDL_SetRenderDrawBlendMode(renderer, prevBlendMode);
}
