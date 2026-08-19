#include "pch.h"
#include "cabinet.h"

#include "fullscrn.h"
#include "high_score.h"
#include "options.h"
#include "pb.h"
#include "render.h"
#include "score.h"
#include "TPinballTable.h"
#include "TTextBox.h"
#include "TTextBoxMessage.h"
#include "winmain.h"

#ifdef PB_USE_SDL_IMAGE
#include <SDL_image.h>
#endif

bool cabinet::ShowSettingsDialog = false;

cabinet::Screen cabinet::Backglass{};
cabinet::Screen cabinet::Dmd{};
SDL_Texture* cabinet::BackglassImage = nullptr;
DotMatrix cabinet::DmdCanvas{128, 48};
// Matches the colour of the original score reels and text boxes on the sidebar
static constexpr ColorRgba DefaultDmdColor{120, 88, 220, 255};
ColorRgba cabinet::DmdColor = DefaultDmdColor;
bool cabinet::Initialized = false, cabinet::Requested = false;

// The backglass is a static image: repaint it only when it or its window changes
static bool BackglassDirty = true;

static ColorRgba ParseHexColor(const std::string& text, ColorRgba defaultColor)
{
	auto start = text.find_first_not_of(" \t#");
	if (start == std::string::npos)
		return defaultColor;

	auto value = text.substr(start);
	if (value.size() < 6)
		return defaultColor;

	unsigned red = 0, green = 0, blue = 0, alpha = 255;
	auto parsed = sscanf(value.c_str(), "%2x%2x%2x%2x", &red, &green, &blue, &alpha);
	if (parsed < 3)
		return defaultColor;

	return ColorRgba
	{
		static_cast<uint8_t>(red), static_cast<uint8_t>(green),
		static_cast<uint8_t>(blue), static_cast<uint8_t>(alpha)
	};
}

static std::string Trim(const std::string& text)
{
	auto first = text.find_first_not_of(" \t\r\n");
	if (first == std::string::npos)
		return "";
	auto last = text.find_last_not_of(" \t\r\n");
	return text.substr(first, last - first + 1);
}

void cabinet::Screen::Destroy()
{
	if (Renderer)
		SDL_DestroyRenderer(Renderer);
	if (Window)
		SDL_DestroyWindow(Window);
	Renderer = nullptr;
	Window = nullptr;
}

void cabinet::RequestCabinetMode()
{
	Requested = true;
}

bool cabinet::CabinetModeRequested()
{
	return Requested;
}

bool cabinet::CabinetModeActive()
{
	return Requested && Initialized;
}

bool cabinet::HideSidebar()
{
	return CabinetModeActive() && options::Options.PlayfieldHideSidebar;
}

int cabinet::PlayfieldRotation()
{
	auto rotation = options::Options.PlayfieldRotation.V;
	rotation = ((rotation % 360) + 360) % 360;
	// Snap to the four supported orientations
	return (rotation / 90) * 90;
}

const char* cabinet::AnchorName(CabinetAnchor anchor)
{
	switch (anchor)
	{
	case CabinetAnchor::Custom: return "Custom (X/Y)";
	case CabinetAnchor::Centered: return "Centered";
	case CabinetAnchor::TopLeft: return "Top Left";
	case CabinetAnchor::TopRight: return "Top Right";
	case CabinetAnchor::BottomLeft: return "Bottom Left";
	case CabinetAnchor::BottomRight: return "Bottom Right";
	default: return "Unknown";
	}
}

std::string cabinet::MediaPath()
{
	auto& path = options::Options.CabinetMediaPath.V;
	if (path.empty())
		return pb::BasePath;

	auto absolute = path[0] == '/' || path[0] == '\\' ||
		(path.size() > 1 && path[1] == ':');
	auto result = absolute ? path : pb::make_path_name(path);
	if (result.back() != PathSeparator && result.back() != '/')
		result += PathSeparator;
	return result;
}

/*Resolves a display index into desktop bounds; falls back to the primary display.*/
static SDL_Rect GetDisplayBounds(int display)
{
	SDL_Rect bounds{0, 0, 640, 480};
	auto displayCount = SDL_GetNumVideoDisplays();
	if (displayCount <= 0)
		return bounds;
	if (display < 0 || display >= displayCount)
		display = 0;
	if (SDL_GetDisplayBounds(display, &bounds) != 0)
		SDL_ClearError();
	return bounds;
}

void cabinet::ApplyMainWindowLayout()
{
	auto window = winmain::MainWindow;
	if (!window)
		return;

	auto& opt = options::Options;
	auto cabinetMode = Requested;
	auto display = cabinetMode ? opt.PlayfieldDisplay.V : opt.WindowDisplay.V;
	auto width = cabinetMode ? opt.PlayfieldWidth.V : opt.WindowWidth.V;
	auto height = cabinetMode ? opt.PlayfieldHeight.V : opt.WindowHeight.V;
	auto bounds = GetDisplayBounds(display);

	if (cabinetMode && opt.PlayfieldFullscreen)
	{
		// Move onto the target display first, fullscreen desktop follows the window
		SDL_SetWindowPosition(window, bounds.x, bounds.y);
		opt.FullScreen = true;
		fullscrn::set_screen_mode(true);
		return;
	}
	if (opt.FullScreen)
		return;

	if (width <= 0 || height <= 0)
	{
		if (cabinetMode)
		{
			width = bounds.w;
			height = bounds.h;
		}
		else
		{
			SDL_GetWindowSize(window, &width, &height);
		}
	}
	SDL_SetWindowSize(window, width, height);

	int x, y;
	if (cabinetMode)
	{
		x = bounds.x + opt.PlayfieldX;
		y = bounds.y + opt.PlayfieldY;
	}
	else
	{
		auto anchor = static_cast<CabinetAnchor>(opt.WindowAnchor.V);
		switch (anchor)
		{
		case CabinetAnchor::TopLeft:
			x = bounds.x;
			y = bounds.y;
			break;
		case CabinetAnchor::TopRight:
			x = bounds.x + bounds.w - width;
			y = bounds.y;
			break;
		case CabinetAnchor::BottomLeft:
			x = bounds.x;
			y = bounds.y + bounds.h - height;
			break;
		case CabinetAnchor::BottomRight:
			x = bounds.x + bounds.w - width;
			y = bounds.y + bounds.h - height;
			break;
		case CabinetAnchor::Custom:
			x = bounds.x + opt.WindowX;
			y = bounds.y + opt.WindowY;
			break;
		case CabinetAnchor::Centered:
		default:
			x = bounds.x + (bounds.w - width) / 2;
			y = bounds.y + (bounds.h - height) / 2;
			break;
		}
	}
	SDL_SetWindowPosition(window, x, y);
}

bool cabinet::CreateScreen(Screen& screen, const char* title, int display, int x, int y, int width, int height,
                           bool fullscreen)
{
	auto bounds = GetDisplayBounds(display);
	if (width <= 0)
		width = bounds.w;
	if (height <= 0)
		height = bounds.h;

#if SDL_VERSION_ATLEAST(2, 0, 22)
	// Aux windows are output only: taking focus would mute and pause the game
	UsingSdlHint noActivation{SDL_HINT_WINDOW_NO_ACTIVATION_WHEN_SHOWN, "1"};
#endif

	screen.Window = SDL_CreateWindow
	(
		title,
		bounds.x + x, bounds.y + y,
		width, height,
		SDL_WINDOW_BORDERLESS | (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0)
	);
	if (!screen.Window)
	{
		printf("Cabinet: could not create %s window.\nSDL Error: %s\n", title, SDL_GetError());
		SDL_ClearError();
		return false;
	}

	for (auto i = 0; i < 2 && !screen.Renderer; i++)
	{
		screen.Renderer = SDL_CreateRenderer(screen.Window, -1,
		                                     i == 0 ? SDL_RENDERER_ACCELERATED : SDL_RENDERER_SOFTWARE);
	}
	if (!screen.Renderer)
	{
		printf("Cabinet: could not create %s renderer.\nSDL Error: %s\n", title, SDL_GetError());
		SDL_ClearError();
		screen.Destroy();
		return false;
	}

	SDL_RendererInfo info{};
	if (!SDL_GetRendererInfo(screen.Renderer, &info))
		printf("Cabinet: %s using SDL renderer: %s\n", title, info.name);

	SDL_SetRenderDrawColor(screen.Renderer, 0, 0, 0, 255);
	SDL_RenderClear(screen.Renderer);
	SDL_RenderPresent(screen.Renderer);
	return true;
}

void cabinet::Init()
{
	auto& opt = options::Options;
	Initialized = false;
	if (!Requested)
	{
		ApplyMainWindowLayout();
		return;
	}

#ifdef PB_USE_SDL_IMAGE
	static auto imgInitialized = false;
	if (!imgInitialized)
	{
		auto flags = IMG_INIT_PNG | IMG_INIT_JPG;
		if ((IMG_Init(flags) & flags) != flags)
		{
			printf("Cabinet: SDL_image init incomplete, some formats may not load.\nSDL Error: %s\n", SDL_GetError());
			SDL_ClearError();
		}
		imgInitialized = true;
	}
#endif

	if (opt.BackglassEnabled)
	{
		CreateScreen(Backglass, "Pinball Backglass", opt.BackglassDisplay, opt.BackglassX, opt.BackglassY,
		             opt.BackglassWidth, opt.BackglassHeight, opt.BackglassFullscreen);
	}
	if (opt.DmdEnabled)
	{
		CreateScreen(Dmd, "Pinball DMD", opt.DmdDisplay, opt.DmdX, opt.DmdY,
		             opt.DmdWidth, opt.DmdHeight, opt.DmdFullscreen);
	}

	DmdCanvas.Resize(std::max(16, opt.DmdColumns.V), std::max(7, opt.DmdRows.V));
	DmdColor = ParseHexColor(opt.DmdDotColor.V, DefaultDmdColor);

	Initialized = true;

	if (opt.CabinetHideUi)
	{
		// A cabinet has no mouse and no room for a menu bar over the playfield
		opt.ShowMenu = false;
		opt.HideCursor = true;
	}

	LoadBackglassImage();
	ApplyMainWindowLayout();

	// The playfield window must keep input focus, aux windows are output only
	SDL_RaiseWindow(winmain::MainWindow);
}

void cabinet::Shutdown()
{
	UnloadBackglassImage();
	Backglass.Destroy();
	Dmd.Destroy();
	Initialized = false;
}

void cabinet::ApplyVSync(int enabled)
{
#if SDL_VERSION_ATLEAST(2, 0, 18)
	if (Backglass.Renderer)
		SDL_RenderSetVSync(Backglass.Renderer, enabled);
	if (Dmd.Renderer)
		SDL_RenderSetVSync(Dmd.Renderer, enabled);
#else
	(void)enabled;
#endif
}

void cabinet::Reload()
{
	Shutdown();
	Init();
}


/*Loads an image from the cabinet media folder. Without SDL2_image, only BMP works.*/
static SDL_Texture* LoadImageTexture(SDL_Renderer* renderer, const std::string& path)
{
#ifdef PB_USE_SDL_IMAGE
	auto surface = IMG_Load(path.c_str());
#else
	auto surface = SDL_LoadBMP(path.c_str());
#endif
	if (!surface)
	{
		printf("Cabinet: could not load image '%s'.\nSDL Error: %s\n", path.c_str(), SDL_GetError());
		SDL_ClearError();
		return nullptr;
	}

	auto texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_FreeSurface(surface);
	if (!texture)
	{
		printf("Cabinet: could not create texture for '%s'.\nSDL Error: %s\n", path.c_str(), SDL_GetError());
		SDL_ClearError();
	}
	else
	{
		printf("Cabinet: loaded backglass image %s\n", path.c_str());
	}
	return texture;
}

void cabinet::UnloadBackglassImage()
{
	if (BackglassImage)
		SDL_DestroyTexture(BackglassImage);
	BackglassImage = nullptr;
}

void cabinet::LoadBackglassImage()
{
	UnloadBackglassImage();
	BackglassDirty = true;
	if (!Backglass.Valid())
		return;

	auto& fileName = options::Options.BackglassImage.V;
	if (fileName.empty())
		return;

	// An absolute path is used as given, everything else is relative to the media folder
	auto absolute = fileName[0] == '/' || fileName[0] == '\\' || (fileName.size() > 1 && fileName[1] == ':');
	auto path = absolute ? fileName : MediaPath() + fileName;
	if (auto file = fopenu(path.c_str(), "rb"))
	{
		fclose(file);
		BackglassImage = LoadImageTexture(Backglass.Renderer, path);
		return;
	}

	// Art is often supplied in a different format than the configured name suggests
	auto stem = path.substr(0, path.find_last_of('.'));
	for (auto extension : {".png", ".jpg", ".jpeg", ".bmp"})
	{
		auto candidate = stem + extension;
		if (auto file = fopenu(candidate.c_str(), "rb"))
		{
			fclose(file);
			BackglassImage = LoadImageTexture(Backglass.Renderer, candidate);
			return;
		}
	}

	printf("Cabinet: no backglass image found at %s\n", path.c_str());
}

/*Formats a score with thousand separators, including the billions overflow counter.*/
static std::string FormatScore(int score, int scoreE9)
{
	auto value = static_cast<int64_t>(scoreE9) * 1000000000ll + score;
	char buffer[32];
	snprintf(buffer, sizeof buffer, "%lld", static_cast<long long>(value));

	std::string digits = buffer;
	std::string result;
	auto count = 0;
	for (auto i = static_cast<int>(digits.size()) - 1; i >= 0; i--)
	{
		result.insert(result.begin(), digits[i]);
		if (++count % 3 == 0 && i > 0)
			result.insert(result.begin(), ',');
	}
	return result;
}

std::string cabinet::GetFieldText(TextField field)
{
	auto table = pb::MainTable;
	char buffer[128];

	switch (field)
	{
	case TextField::Score:
		if (!table)
			return "0";
		return FormatScore(table->CurScore, table->CurScoreE9);
	case TextField::Player:
		if (!table || pb::game_mode != GameModes::InGame)
			return "";
		// Mirrors the player_number1 reel, which is shown even in single player
		snprintf(buffer, sizeof buffer, "PLAYER %d", table->CurrentPlayer + 1);
		return buffer;
	case TextField::Ball:
		if (!table)
			return "";
		if (pb::game_mode != GameModes::InGame)
			return "GAME OVER";
		// BallCount is balls remaining; the ball in play is what a cabinet shows.
		// Same expression the original sidebar reel uses.
		snprintf(buffer, sizeof buffer, "BALL %d",
		         Clamp(table->MaxBallCount - table->BallCount + 1, 1, table->MaxBallCount));
		return buffer;
	case TextField::Info:
	case TextField::Mission:
		{
			// The sidebar shows both boxes at once. Despite the name, pb::MissTextBox is
			// the "mission_text_box" component; pb::InfoTextBox is the short status line.
			auto box = field == TextField::Info ? pb::InfoTextBox : pb::MissTextBox;
			if (box && box->CurrentMessage && box->CurrentMessage->Text)
			{
				std::string message = box->CurrentMessage->Text;
				std::replace(message.begin(), message.end(), '\n', ' ');
				std::replace(message.begin(), message.end(), '\r', ' ');
				return Trim(message);
			}
			return "";
		}
	case TextField::HighScore:
		snprintf(buffer, sizeof buffer, "HIGH %s", FormatScore(high_score::highscore_table[0].Score, 0).c_str());
		return buffer;
	case TextField::Title:
		return "3D PINBALL";
	default:
		return "";
	}
}

void cabinet::RenderBackglass()
{
	if (!Backglass.Valid() || !BackglassDirty)
		return;
	BackglassDirty = false;

	SDL_SetRenderDrawColor(Backglass.Renderer, 0, 0, 0, 255);
	SDL_RenderClear(Backglass.Renderer);

	if (BackglassImage)
	{
		int width, height, imageWidth, imageHeight;
		SDL_GetRendererOutputSize(Backglass.Renderer, &width, &height);
		SDL_QueryTexture(BackglassImage, nullptr, nullptr, &imageWidth, &imageHeight);

		// Fit the art without distorting it; sizing the window to the art fills the screen
		auto scale = std::min(static_cast<float>(width) / imageWidth, static_cast<float>(height) / imageHeight);
		SDL_Rect dest
		{
			0, 0,
			static_cast<int>(imageWidth * scale), static_cast<int>(imageHeight * scale)
		};
		dest.x = (width - dest.w) / 2;
		dest.y = (height - dest.h) / 2;
		SDL_RenderCopy(Backglass.Renderer, BackglassImage, nullptr, &dest);
	}

	SDL_RenderPresent(Backglass.Renderer);
}

/*Breaks text into lines that fit the given dot width, splitting on spaces.*/
static std::vector<std::string> WrapText(const std::string& text, int scale, int maxWidth)
{
	std::vector<std::string> lines;
	if (text.empty())
		return lines;

	std::string current;
	size_t pos = 0;
	while (pos <= text.size())
	{
		auto space = text.find(' ', pos);
		auto word = text.substr(pos, space == std::string::npos ? std::string::npos : space - pos);
		if (!word.empty())
		{
			auto candidate = current.empty() ? word : current + " " + word;
			if (DotMatrix::TextWidth(candidate.c_str(), scale) <= maxWidth || current.empty())
			{
				current = candidate;
			}
			else
			{
				lines.push_back(current);
				current = word;
			}
		}
		if (space == std::string::npos)
			break;
		pos = space + 1;
	}
	if (!current.empty())
		lines.push_back(current);
	return lines;
}

void cabinet::RenderDmd()
{
	if (!Dmd.Valid())
		return;

	auto columns = DmdCanvas.Columns(), rows = DmdCanvas.Rows();
	DmdCanvas.Clear();

	// Entering initials takes over the whole panel, the way a real cabinet does it
	if (high_score::CabinetEntryActive())
	{
		auto initials = high_score::GetEntryInitials();
		auto slot = high_score::GetEntrySlot();

		// Blink the character being picked
		if ((SDL_GetTicks() / 250) % 2 == 0 && slot < static_cast<int>(initials.size()))
			initials[slot] = '_';

		auto titleScale = std::max(1, columns / 128);
		auto bigScale = std::max(2, (rows - DotMatrix::TextHeight(titleScale) - 6) / DotMatrix::GlyphHeight);
		DmdCanvas.DrawText(DotMatrix::Align::Center, 1, "ENTER YOUR INITIALS", titleScale);

		// Wide spacing so the three slots read as separate characters
		auto spacing = DotMatrix::TextWidth("W", bigScale) + bigScale * 3;
		auto totalWidth = spacing * static_cast<int>(initials.size()) - bigScale * 3;
		auto x = (columns - totalWidth) / 2;
		auto y = DotMatrix::TextHeight(titleScale) + 4;
		for (auto ch : initials)
		{
			char text[2]{ch, 0};
			DmdCanvas.DrawText(x, y, text, bigScale);
			x += spacing;
		}
		PresentDmdCanvas();
		return;
	}

	// Three bands: a status line, the score, then the message lines.
	// Small text is sized by panel width, not height: at 128 columns scale 1 gives 21
	// characters per line, and scaling it up with the row count would starve the text.
	auto smallScale = std::max(1, columns / 128);
	auto smallHeight = DotMatrix::TextHeight(smallScale);
	auto lineStep = smallHeight + 1;

	// Message lines are reserved before the score claims what is left, so the mission text
	// never gets squeezed out. DesiredMessageLines is the info line plus two mission lines.
	constexpr auto DesiredMessageLines = 3;
	auto messageLines = 0, scoreScale = 1;
	for (auto candidate = DesiredMessageLines; candidate >= 0; candidate--)
	{
		auto scoreSpace = rows - lineStep - candidate * lineStep - 2;
		auto scale = scoreSpace / DotMatrix::GlyphHeight;
		if (scale >= 1)
		{
			messageLines = candidate;
			scoreScale = std::min(scale, 6);
			break;
		}
	}

	auto table = pb::MainTable;
	auto tilted = table && table->TiltLockFlag;

	// Status line: player on the left, ball (or GAME OVER) on the right
	auto player = GetFieldText(TextField::Player);
	auto ball = tilted && (SDL_GetTicks() / 300) % 2 == 0 ? std::string("TILT") : GetFieldText(TextField::Ball);
	if (player.empty() && ball.empty())
		player = GetFieldText(TextField::HighScore);
	DmdCanvas.DrawText(1, 0, player.c_str(), smallScale);
	DmdCanvas.DrawText(columns - DotMatrix::TextWidth(ball.c_str(), smallScale) - 1, 0, ball.c_str(), smallScale);

	// Score, shrunk until it fits the panel width
	auto scoreText = GetFieldText(TextField::Score);
	auto scoreWidth = DotMatrix::TextWidth(scoreText.c_str(), scoreScale);
	while (scoreScale > 1 && scoreWidth > columns - 2)
	{
		scoreScale--;
		scoreWidth = DotMatrix::TextWidth(scoreText.c_str(), scoreScale);
	}
	auto scoreY = lineStep;
	DmdCanvas.DrawText(columns - scoreWidth - 1, scoreY, scoreText.c_str(), scoreScale);

	// Message area: the sidebar shows the status box and the mission box at the same time,
	// so both get a share of the reserved lines here.
	auto messageY = scoreY + DotMatrix::TextHeight(scoreScale) + 2;
	auto messageRows = std::min(messageLines, (rows - messageY) / lineStep);
	if (messageRows > 0)
	{
		std::vector<std::string> lines;
		auto info = GetFieldText(TextField::Info);
		auto mission = GetFieldText(TextField::Mission);
		if (info.empty() && mission.empty())
			mission = GetFieldText(TextField::HighScore);

		// Sidebar order: the short status line, then the mission text below it
		if (!info.empty())
			lines = WrapText(info, smallScale, columns - 2);
		if (!mission.empty() && static_cast<int>(lines.size()) < messageRows)
		{
			for (auto& line : WrapText(mission, smallScale, columns - 2))
				lines.push_back(line);
		}

		if (static_cast<int>(lines.size()) <= messageRows)
		{
			for (auto i = 0; i < static_cast<int>(lines.size()); i++)
			{
				auto lineWidth = DotMatrix::TextWidth(lines[i].c_str(), smallScale);
				DmdCanvas.DrawText((columns - lineWidth) / 2, messageY + i * lineStep, lines[i].c_str(),
				                   smallScale);
			}
		}
		else
		{
			// More text than lines: show what fits, then scroll the remainder on the last line
			auto staticLines = messageRows - 1;
			for (auto i = 0; i < staticLines; i++)
			{
				auto lineWidth = DotMatrix::TextWidth(lines[i].c_str(), smallScale);
				DmdCanvas.DrawText((columns - lineWidth) / 2, messageY + i * lineStep, lines[i].c_str(),
				                   smallScale);
			}

			std::string remainder;
			for (auto i = staticLines; i < static_cast<int>(lines.size()); i++)
				remainder += (remainder.empty() ? "" : " ") + lines[i];

			auto span = DotMatrix::TextWidth(remainder.c_str(), smallScale) + columns;
			auto offset = static_cast<int>((SDL_GetTicks() / 30) % static_cast<Uint32>(span));
			DmdCanvas.DrawText(columns - offset, messageY + staticLines * lineStep, remainder.c_str(),
			                   smallScale);
		}
	}

	PresentDmdCanvas();
}

void cabinet::PresentDmdCanvas()
{
	// Drawing thousands of dots on a second GL context costs milliseconds a frame, and the
	// panel only changes when the text does. Redraw only when the dot pattern differs.
	static uint64_t lastContentHash = 0;
	static ColorRgba lastColor = ColorRgba{0};
	static bool lastUnlitDots = false;
	auto contentHash = DmdCanvas.ContentHash();
	if (contentHash == lastContentHash && lastColor.Color == DmdColor.Color &&
		lastUnlitDots == options::Options.DmdShowUnlitDots.V)
		return;
	lastContentHash = contentHash;
	lastColor = DmdColor;
	lastUnlitDots = options::Options.DmdShowUnlitDots;

	int width, height;
	SDL_GetRendererOutputSize(Dmd.Renderer, &width, &height);
	SDL_SetRenderDrawColor(Dmd.Renderer, 0, 0, 0, 255);
	SDL_RenderClear(Dmd.Renderer);

	auto unlit = ColorRgba
	{
		static_cast<uint8_t>(DmdColor.GetRed() / 12),
		static_cast<uint8_t>(DmdColor.GetGreen() / 12),
		static_cast<uint8_t>(DmdColor.GetBlue() / 12),
		255
	};
	DmdCanvas.Present(Dmd.Renderer, SDL_Rect{0, 0, width, height}, DmdColor, unlit,
	                  options::Options.DmdShowUnlitDots);
	SDL_RenderPresent(Dmd.Renderer);
}


void cabinet::Render()
{
	if (!CabinetModeActive())
		return;

	RenderBackglass();
	RenderDmd();
}

bool cabinet::AuxWindowHasFocus()
{
	if (!Initialized)
		return false;

	auto focused = SDL_GetKeyboardFocus();
	return focused && (focused == Backglass.Window || focused == Dmd.Window);
}

bool cabinet::OwnsWindow(Uint32 windowId)
{
	if (Backglass.Window && SDL_GetWindowID(Backglass.Window) == windowId)
		return true;
	if (Dmd.Window && SDL_GetWindowID(Dmd.Window) == windowId)
		return true;
	return false;
}

bool cabinet::HandleWindowEvent(const SDL_Event& event)
{
	if (event.type != SDL_WINDOWEVENT || !OwnsWindow(event.window.windowID))
		return false;

	switch (event.window.event)
	{
	case SDL_WINDOWEVENT_CLOSE:
		// Closing an aux window must not take the game down with it
		if (Backglass.Window && SDL_GetWindowID(Backglass.Window) == event.window.windowID)
			SDL_HideWindow(Backglass.Window);
		else if (Dmd.Window && SDL_GetWindowID(Dmd.Window) == event.window.windowID)
			SDL_HideWindow(Dmd.Window);
		break;
	case SDL_WINDOWEVENT_FOCUS_GAINED:
		// Aux windows are output only, hand focus back to the playfield
		SDL_RaiseWindow(winmain::MainWindow);
		break;
	case SDL_WINDOWEVENT_EXPOSED:
	case SDL_WINDOWEVENT_SHOWN:
	case SDL_WINDOWEVENT_RESIZED:
	case SDL_WINDOWEVENT_SIZE_CHANGED:
		BackglassDirty = true;
		break;
	default:
		break;
	}
	return true;
}

/*Helper for the settings dialog: a display picker with a "default" entry.*/
static void DisplayCombo(const char* label, IntOption& option)
{
	auto displayCount = SDL_GetNumVideoDisplays();
	char preview[64];
	if (option.V < 0 || option.V >= displayCount)
		snprintf(preview, sizeof preview, "Default");
	else
		snprintf(preview, sizeof preview, "Display %d", option.V);

	if (ImGui::BeginCombo(label, preview))
	{
		if (ImGui::Selectable("Default", option.V < 0))
			option.V = -1;
		for (auto i = 0; i < displayCount; i++)
		{
			SDL_Rect bounds{};
			SDL_GetDisplayBounds(i, &bounds);
			char item[96];
			snprintf(item, sizeof item, "Display %d (%dx%d at %d,%d)", i, bounds.w, bounds.h, bounds.x, bounds.y);
			if (ImGui::Selectable(item, option.V == i))
				option.V = i;
		}
		ImGui::EndCombo();
	}
}

/*Helper for the settings dialog: X/Y/W/H, where 0 size means "fill the display".*/
static void GeometryControls(const char* idPrefix, IntOption& x, IntOption& y, IntOption& width, IntOption& height)
{
	ImGui::PushID(idPrefix);
	int position[2]{x.V, y.V};
	if (ImGui::DragInt2("Position (X, Y)", position, 1.0f, -32000, 32000))
	{
		x.V = position[0];
		y.V = position[1];
	}
	int size[2]{width.V, height.V};
	if (ImGui::DragInt2("Size (W, H)", size, 1.0f, 0, 32000))
	{
		width.V = std::max(0, size[0]);
		height.V = std::max(0, size[1]);
	}
	ImGui::TextDisabled("Position is relative to the chosen display. Size 0 fills that display.");
	ImGui::PopID();
}

static void StringInput(const char* label, StringOption& option, size_t maxLength = 260)
{
	std::vector<char> buffer(maxLength, 0);
	snprintf(buffer.data(), maxLength, "%s", option.V.c_str());
	if (ImGui::InputText(label, buffer.data(), maxLength))
		option.V = buffer.data();
}

void cabinet::RenderSettingsUi()
{
	if (!ShowSettingsDialog)
		return;

	auto& opt = options::Options;

	// Options only reach the settings store on a clean shutdown, so anything edited here
	// would be lost to a crash or a kill. Snapshot, then flush if the user changed something.
	auto signature = [&opt]
	{
		const int values[]
		{
			opt.WindowAnchor.V, opt.WindowDisplay.V, opt.WindowX.V, opt.WindowY.V,
			opt.WindowWidth.V, opt.WindowHeight.V,
			opt.PlayfieldRotation.V, opt.PlayfieldDisplay.V, opt.PlayfieldX.V, opt.PlayfieldY.V,
			opt.PlayfieldWidth.V, opt.PlayfieldHeight.V, opt.PlayfieldFullscreen.V, opt.PlayfieldHideSidebar.V,
			opt.BackglassEnabled.V, opt.BackglassDisplay.V, opt.BackglassX.V, opt.BackglassY.V,
			opt.BackglassWidth.V, opt.BackglassHeight.V, opt.BackglassFullscreen.V,
			opt.DmdEnabled.V, opt.DmdDisplay.V, opt.DmdX.V, opt.DmdY.V, opt.DmdWidth.V, opt.DmdHeight.V,
			opt.DmdFullscreen.V, opt.DmdColumns.V, opt.DmdRows.V, opt.DmdShowUnlitDots.V,
			opt.CabinetHideUi.V,
			opt.ShowMenu.V, opt.HideCursor.V,
		};

		std::string result;
		for (auto value : values)
			result += std::to_string(value) + '|';
		return result + opt.CabinetMediaPath.V + '|' + opt.BackglassImage.V + '|' + opt.DmdDotColor.V;
	};
	auto signatureBefore = signature();

	ImGui::SetNextWindowSize(ImVec2{560, 640}, ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Cabinet Settings", &ShowSettingsDialog))
	{
		auto layoutChanged = false;
		auto screensChanged = false;
		auto mediaChanged = false;

		if (CabinetModeRequested())
			ImGui::TextUnformatted("Cabinet mode is active for this run.");
		else
			ImGui::TextDisabled("Single window. Start the game with -cabinet for the three screen layout.");

		if (ImGui::Checkbox("Hide menu bar and cursor in cabinet mode", &opt.CabinetHideUi.V) &&
			opt.CabinetHideUi && CabinetModeRequested())
		{
			opt.ShowMenu = false;
			opt.HideCursor = true;
		}

		ImGui::Separator();
		if (ImGui::CollapsingHeader("Playfield", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const char* rotations[]{"0 (landscape)", "90 (portrait, rotated right)", "180", "270 (portrait, rotated left)"};
			auto rotationIndex = Clamp(PlayfieldRotation() / 90, 0, 3);
			if (ImGui::Combo("Rotation", &rotationIndex, rotations, IM_ARRAYSIZE(rotations)))
			{
				opt.PlayfieldRotation.V = rotationIndex * 90;
				layoutChanged = true;
			}

			layoutChanged |= ImGui::Checkbox("Hide score sidebar (cabinet mode)", &opt.PlayfieldHideSidebar.V);
			DisplayCombo("Playfield display", opt.PlayfieldDisplay);
			layoutChanged |= ImGui::Checkbox("Fullscreen##playfield", &opt.PlayfieldFullscreen.V);
			if (!opt.PlayfieldFullscreen)
				GeometryControls("playfield", opt.PlayfieldX, opt.PlayfieldY, opt.PlayfieldWidth,
				                 opt.PlayfieldHeight);
			ImGui::TextDisabled("Playfield placement is used in cabinet mode.");
		}

		if (ImGui::CollapsingHeader("Single Window", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const char* anchors[static_cast<int>(CabinetAnchor::Count)]{};
			for (auto i = 0; i < static_cast<int>(CabinetAnchor::Count); i++)
				anchors[i] = AnchorName(static_cast<CabinetAnchor>(i));

			auto anchorIndex = Clamp(opt.WindowAnchor.V, 0, static_cast<int>(CabinetAnchor::Count) - 1);
			if (ImGui::Combo("Window position", &anchorIndex, anchors, IM_ARRAYSIZE(anchors)))
			{
				opt.WindowAnchor.V = anchorIndex;
				layoutChanged = true;
			}
			DisplayCombo("Window display", opt.WindowDisplay);
			GeometryControls("window", opt.WindowX, opt.WindowY, opt.WindowWidth, opt.WindowHeight);
			ImGui::TextDisabled("Used when the game runs without -cabinet.");
		}

		{
			if (ImGui::CollapsingHeader("Backglass", ImGuiTreeNodeFlags_DefaultOpen))
			{
				screensChanged |= ImGui::Checkbox("Enabled##backglass", &opt.BackglassEnabled.V);
				ImGui::SameLine();
				ImGui::TextDisabled("(turn off if a frontend draws the backglass)");

				if (opt.BackglassEnabled)
				{
					DisplayCombo("Backglass display", opt.BackglassDisplay);
					screensChanged |= ImGui::Checkbox("Fullscreen##backglass", &opt.BackglassFullscreen.V);
					if (!opt.BackglassFullscreen)
						GeometryControls("backglass", opt.BackglassX, opt.BackglassY, opt.BackglassWidth,
						                 opt.BackglassHeight);

					StringInput("Media folder", opt.CabinetMediaPath);
					StringInput("Background image", opt.BackglassImage);
					ImGui::TextDisabled("PNG, JPG or BMP. Scaled to fit, keeping its aspect ratio.");
					if (ImGui::Button("Reload backglass image"))
						mediaChanged = true;
				}
			}

			if (ImGui::CollapsingHeader("DMD", ImGuiTreeNodeFlags_DefaultOpen))
			{
				screensChanged |= ImGui::Checkbox("Enabled##dmd", &opt.DmdEnabled.V);
				if (opt.DmdEnabled)
				{
					DisplayCombo("DMD display", opt.DmdDisplay);
					screensChanged |= ImGui::Checkbox("Fullscreen##dmd", &opt.DmdFullscreen.V);
					if (!opt.DmdFullscreen)
						GeometryControls("dmd", opt.DmdX, opt.DmdY, opt.DmdWidth, opt.DmdHeight);

					int grid[2]{opt.DmdColumns.V, opt.DmdRows.V};
					if (ImGui::DragInt2("Dot grid (columns, rows)", grid, 1.0f, 16, 512))
					{
						opt.DmdColumns.V = std::max(16, grid[0]);
						opt.DmdRows.V = std::max(7, grid[1]);
						DmdCanvas.Resize(opt.DmdColumns.V, opt.DmdRows.V);
					}
					StringInput("Dot color (RRGGBB)", opt.DmdDotColor, 16);
					if (ImGui::IsItemDeactivatedAfterEdit())
						DmdColor = ParseHexColor(opt.DmdDotColor.V, DefaultDmdColor);
					ImGui::Checkbox("Show unlit dots", &opt.DmdShowUnlitDots.V);
				}
			}
		}

		ImGui::Separator();
		if (ImGui::Button("Apply"))
			screensChanged = true;
		ImGui::SameLine();
		ImGui::TextDisabled("Window changes apply immediately; use Apply after editing geometry.");

		if (screensChanged)
		{
			Reload();
			fullscrn::window_size_changed();
		}
		else
		{
			if (layoutChanged)
			{
				ApplyMainWindowLayout();
				fullscrn::window_size_changed();
			}
			if (mediaChanged)
				LoadBackglassImage();
		}
	}

	if (signature() != signatureBefore)
		options::SaveAll();
	ImGui::End();
}
