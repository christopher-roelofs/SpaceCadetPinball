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

SDL_Rect fullscrn::ApplyRotation(SDL_Rect rect)
{
	auto angle = cabinet::PlayfieldRotation();
	if (angle == 0)
		return rect;

	auto centerX = render::DestinationRect.x + render::DestinationRect.w / 2;
	auto centerY = render::DestinationRect.y + render::DestinationRect.h / 2;

	// Screen space is y down, so a clockwise turn maps (u, v) to (-v, u)
	auto rotate = [angle, centerX, centerY](int x, int y, int& outX, int& outY)
	{
		auto u = x - centerX, v = y - centerY;
		switch (angle)
		{
		case 90:
			outX = centerX - v;
			outY = centerY + u;
			break;
		case 180:
			outX = centerX - u;
			outY = centerY - v;
			break;
		case 270:
			outX = centerX + v;
			outY = centerY - u;
			break;
		default:
			outX = x;
			outY = y;
			break;
		}
	};

	int x1, y1, x2, y2;
	rotate(rect.x, rect.y, x1, y1);
	rotate(rect.x + rect.w, rect.y + rect.h, x2, y2);
	return SDL_Rect{std::min(x1, x2), std::min(y1, y2), std::abs(x2 - x1), std::abs(y2 - y1)};
}

SDL_Rect fullscrn::GetScreenRectFromPinballRect(SDL_Rect rect)
{
	SDL_Rect converted_rect;

	converted_rect.x = (rect.x - SourceRect.x) * render::DestinationRect.w / SourceRect.w + render::DestinationRect.x;
	converted_rect.y = (rect.y - SourceRect.y) * render::DestinationRect.h / SourceRect.h + render::DestinationRect.y;

	converted_rect.w = rect.w * render::DestinationRect.w / SourceRect.w;
	converted_rect.h = rect.h * render::DestinationRect.h / SourceRect.h;

	return ApplyRotation(converted_rect);
}

float fullscrn::GetScreenToPinballRatio()
{
	return (float) render::DestinationRect.w / SourceRect.w;
}