#pragma once
#include "gdrv.h"

/*
 * Simulated dot matrix display.
 *
 * Text is rasterized into a virtual grid of dots, which is then drawn as discrete
 * square dots on any renderer. The dot grid is resolution independent: the same
 * grid looks right on a 128x32 DMD panel and on a 1920x540 monitor.
 */
class DotMatrix
{
public:
	static constexpr int GlyphWidth = 5, GlyphHeight = 7, GlyphSpacing = 1;

	enum class Align
	{
		Left,
		Center,
		Right
	};

	DotMatrix(int columns, int rows);

	int Columns() const { return columns; }
	int Rows() const { return rows; }

	void Resize(int columns, int rows);
	void Clear();
	void SetDot(int x, int y, uint8_t level);
	uint8_t GetDot(int x, int y) const;

	/*Cheap hash of the dot pattern, for skipping redraws of unchanged content.*/
	uint64_t ContentHash() const;

	/*Text is drawn uppercase, the way real dot matrix displays do it.*/
	void DrawText(int x, int y, const char* text, int scale = 1, uint8_t level = 255);
	void DrawText(Align align, int y, const char* text, int scale = 1, uint8_t level = 255);

	static int TextWidth(const char* text, int scale = 1);
	static int TextHeight(int scale = 1) { return GlyphHeight * scale; }

	/*Draws the grid into dest, keeping dots square and the grid centered.*/
	void Present(SDL_Renderer* renderer, const SDL_Rect& dest, ColorRgba onColor, ColorRgba offColor,
	             bool drawUnlitDots) const;

private:
	int columns, rows;
	std::vector<uint8_t> dots;

	static const uint8_t* GetGlyph(char ch);
};
