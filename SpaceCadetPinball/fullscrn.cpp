#include "pch.h"
#include "fullscrn.h"


#include "cabinet.h"
#include "options.h"
#include "pb.h"
#include "render.h"
#include "TPinballTable.h"
#include "winmain.h"


int fullscrn::screen_mode;
int fullscrn::display_changed;

int fullscrn::resolution = 0;
const resolution_info fullscrn::resolution_array[3] =
{
	{640, 480, 600, 416, 501},
	{800, 600, 752, 520, 502},
	{1024, 768, 960, 666, 503},
};
float fullscrn::ScaleX = 1;
float fullscrn::ScaleY = 1;
int fullscrn::OffsetX = 0;
int fullscrn::OffsetY = 0;
SDL_Rect fullscrn::SourceRect{0, 0, 1, 1};

void fullscrn::init()
{
	window_size_changed();
}

void fullscrn::shutdown()
{
	if (display_changed)
		set_screen_mode(0);
}

int fullscrn::set_screen_mode(int isFullscreen)
{
	int result = isFullscreen;
	if (isFullscreen == screen_mode)
		return result;
	screen_mode = isFullscreen;
	if (isFullscreen)
	{
		enableFullscreen();
		result = 1;
	}
	else
	{
		disableFullscreen();
		result = 1;
	}
	return result;
}

int fullscrn::enableFullscreen()
{
	if (!display_changed)
	{
		if (SDL_SetWindowFullscreen(winmain::MainWindow, SDL_WINDOW_FULLSCREEN_DESKTOP) == 0)
		{
			display_changed = 1;
			return 1;
		}
	}
	return 0;
}

int fullscrn::disableFullscreen()
{
	if (display_changed)
	{
		if (SDL_SetWindowFullscreen(winmain::MainWindow, 0) == 0)
			display_changed = 0;
	}

	return 0;
}

void fullscrn::activate(int flag)
{
	if (screen_mode)
	{
		if (!flag)
		{
			set_screen_mode(0);
		}
	}
}

int fullscrn::GetResolution()
{
	return resolution;
}

void fullscrn::SetResolution(int value)
{
	if (!pb::FullTiltMode || pb::FullTiltDemoMode)
		value = 0;
	assertm(value >= 0 && value <= 2, "Resolution value out of bounds");
	resolution = value;
}

int fullscrn::GetMaxResolution()
{
	return pb::FullTiltMode && !pb::FullTiltDemoMode ? 2 : 0;
}

void fullscrn::window_size_changed()
{
	int width, height;
	SDL_GetRendererOutputSize(winmain::Renderer, &width, &height);
	int menuHeight = options::Options.ShowMenu ? winmain::MainMenuHeight : 0;
	height -= menuHeight;
	auto res = &resolution_array[resolution];

	// Cabinet mode presents the board only, the score sidebar moves to the other screens.
	// Only TPinballTable::Width is in screen space; it is where the sidebar starts.
	SourceRect = SDL_Rect{0, 0, res->TableWidth, res->TableHeight};
	if (cabinet::HideSidebar() && pb::MainTable && pb::MainTable->Width > 0 &&
		pb::MainTable->Width < SourceRect.w)
	{
		SourceRect.w = pb::MainTable->Width;
	}

	// A quarter turn puts the table's X axis along the window's Y axis
	auto quarterTurn = cabinet::PlayfieldRotation() % 180 != 0;
	auto availableX = quarterTurn ? height : width;
	auto availableY = quarterTurn ? width : height;

	ScaleX = static_cast<float>(availableX) / SourceRect.w;
	ScaleY = static_cast<float>(availableY) / SourceRect.h;

	if (options::Options.IntegerScaling)
	{
		ScaleX = ScaleX < 1 ? ScaleX : std::floor(ScaleX);
		ScaleY = ScaleY < 1 ? ScaleY : std::floor(ScaleY);
	}

	if (options::Options.UniformScaling)
	{
		ScaleY = ScaleX = std::min(ScaleX, ScaleY);
	}

	auto tableWidth = static_cast<int>(floor(SourceRect.w * ScaleX));
	auto tableHeight = static_cast<int>(floor(SourceRect.h * ScaleY));
	OffsetX = (availableX - tableWidth) / 2;
	OffsetY = (availableY - tableHeight) / 2;

	// Kept centered in the window: rotating around its own center then keeps it centered
	render::DestinationRect = SDL_Rect
	{
		(width - tableWidth) / 2,
		menuHeight + (height - tableHeight) / 2,
		tableWidth, tableHeight
	};
}

SDL_Rect fullscrn::GetScreenRectFromPinballRect(SDL_Rect rect)
{
	// The result positions ImGui windows, and ImGui lays out in the rotated canvas, so this
	// maps into canvas space. The table is centered there, just as it is in the window.
	auto& destination = render::DestinationRect;
	auto originX = destination.x, originY = destination.y;
	if (cabinet::PlayfieldRotation() % 180 != 0)
	{
		int width, height;
		SDL_GetRendererOutputSize(winmain::Renderer, &width, &height);
		originX = (height - destination.w) / 2;
		originY = (width - destination.h) / 2;
	}

	SDL_Rect converted_rect;
	converted_rect.x = (rect.x - SourceRect.x) * destination.w / SourceRect.w + originX;
	converted_rect.y = (rect.y - SourceRect.y) * destination.h / SourceRect.h + originY;

	converted_rect.w = rect.w * destination.w / SourceRect.w;
	converted_rect.h = rect.h * destination.h / SourceRect.h;

	return converted_rect;
}

float fullscrn::GetScreenToPinballRatio()
{
	return (float) render::DestinationRect.w / SourceRect.w;
}