#pragma once
#include "dmd.h"
#include "gdrv.h"

/*
 * Virtual pinball cabinet support.
 *
 * In cabinet mode the game is split over up to three screens:
 *   - playfield: the main window, with the score sidebar cropped away and
 *     optionally rotated for a portrait mounted monitor;
 *   - backglass: a static user supplied image;
 *   - DMD: a simulated dot matrix display carrying score, player, ball and messages.
 *
 * In single window mode (the default) only the main window is used; its position
 * and size are still configurable through the same options.
 */

enum class CabinetAnchor : int
{
	Custom = 0,
	Centered,
	TopLeft,
	TopRight,
	BottomLeft,
	BottomRight,
	Count
};

class cabinet
{
public:
	static bool ShowSettingsDialog;

	/*True when cabinet mode is enabled and at least the playfield was set up for it.*/
	static bool CabinetModeActive();
	/*True when the score sidebar should be cropped off of the playfield.*/
	static bool HideSidebar();
	/*Playfield rotation in degrees: 0, 90, 180 or 270.*/
	static int PlayfieldRotation();

	static void Init();
	static void Shutdown();

	/*Places and sizes the main window according to the current options.*/
	static void ApplyMainWindowLayout();

	/*Draws and presents the backglass and DMD windows.*/
	static void Render();

	static bool OwnsWindow(Uint32 windowId);
	/*True when keyboard focus sits on one of the cabinet's own aux windows.*/
	static bool AuxWindowHasFocus();
	/*Handles a window event addressed to a cabinet window; returns true if consumed.*/
	static bool HandleWindowEvent(const SDL_Event& event);

	/*Rebuilds the aux windows and reloads backglass media, used after option changes.*/
	static void Reload();

	static void RenderSettingsUi();

	static const char* AnchorName(CabinetAnchor anchor);
	static std::string MediaPath();

private:
	enum class TextField
	{
		Score,
		Player,
		Ball,
		Message,
		HighScore,
		Title,
	};

	struct Screen
	{
		SDL_Window* Window = nullptr;
		SDL_Renderer* Renderer = nullptr;

		bool Valid() const { return Window && Renderer; }
		void Destroy();
	};

	static Screen Backglass, Dmd;
	static SDL_Texture* BackglassImage;
	static DotMatrix DmdCanvas;
	static ColorRgba DmdColor;
	static bool Initialized;

	static bool CreateScreen(Screen& screen, const char* title, int display, int x, int y, int width, int height,
	                         bool fullscreen);
	static void LoadBackglassImage();
	static void UnloadBackglassImage();

	static void RenderBackglass();
	static void RenderDmd();

	static std::string GetFieldText(TextField field);
};
