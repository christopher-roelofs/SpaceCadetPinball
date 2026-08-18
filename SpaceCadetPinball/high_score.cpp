#include "pch.h"
#include "high_score.h"

#include "cabinet.h"
#include "options.h"
#include "pb.h"
#include "score.h"
#include "translations.h"

bool high_score::dlg_enter_name;
bool high_score::ShowDialog = false;
high_score_entry high_score::DlgData;
std::vector<high_score_entry> high_score::ScoreQueue;
high_score_struct high_score::highscore_table[5];
bool high_score::CabinetEntry = false, high_score::EntrySubmit = false;
int high_score::EntrySlot = 0;
int high_score::EntryChars[high_score::EntrySlots]{};
constexpr int high_score::EntrySlots;

// Wheel of characters the flippers cycle through. '<' rubs out the previous slot.
static const char EntryCharset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 <";
static constexpr int EntryCharCount = sizeof EntryCharset - 1;
static constexpr int RuboutIndex = EntryCharCount - 1;

bool high_score::CabinetEntryActive()
{
	return CabinetEntry;
}

std::string high_score::GetEntryInitials()
{
	std::string result;
	for (auto i = 0; i < EntrySlots; i++)
		result += i <= EntrySlot ? EntryCharset[EntryChars[i]] : ' ';
	return result;
}

bool high_score::HandleCabinetInput(const std::vector<GameBindings>& bindings)
{
	if (!CabinetEntry)
		return false;

	auto handled = false;
	for (const auto binding : bindings)
	{
		switch (binding)
		{
		case GameBindings::LeftFlipper:
			EntryChars[EntrySlot] = (EntryChars[EntrySlot] + EntryCharCount - 1) % EntryCharCount;
			handled = true;
			break;
		case GameBindings::RightFlipper:
			EntryChars[EntrySlot] = (EntryChars[EntrySlot] + 1) % EntryCharCount;
			handled = true;
			break;
		case GameBindings::Plunger:
		case GameBindings::NewGame:
			if (EntryChars[EntrySlot] == RuboutIndex)
			{
				// Rubout steps back instead of entering a character
				if (EntrySlot > 0)
				{
					EntryChars[EntrySlot] = 0;
					EntrySlot--;
				}
			}
			else if (EntrySlot + 1 < EntrySlots)
			{
				EntrySlot++;
			}
			else
			{
				EntrySubmit = true;
			}
			handled = true;
			break;
		default:
			break;
		}
	}
	return handled;
}

int high_score::read()
{
	char Buffer[20];

	int checkSum = 0;
	clear_table();
	for (auto position = 0; position < 5; ++position)
	{
		auto& tablePtr = highscore_table[position];

		snprintf(Buffer, sizeof Buffer, "%d", position);
		strcat(Buffer, ".Name");
		auto name = options::GetSetting(Buffer, "");
		strncpy(tablePtr.Name, name.c_str(), sizeof tablePtr.Name);

		snprintf(Buffer, sizeof Buffer, "%d", position);
		strcat(Buffer, ".Score");
		tablePtr.Score = options::get_int(Buffer, tablePtr.Score);

		for (int i = static_cast<int>(strlen(tablePtr.Name)); --i >= 0; checkSum += tablePtr.Name[i])
		{
		}
		checkSum += tablePtr.Score;
	}

	auto verification = options::get_int("Verification", 7);
	if (checkSum != verification)
		clear_table();
	return 0;
}

int high_score::write()
{
	char Buffer[20];

	int checkSum = 0;
	for (auto position = 0; position < 5; ++position)
	{
		auto& tablePtr = highscore_table[position];

		snprintf(Buffer, sizeof Buffer, "%d", position);
		strcat(Buffer, ".Name");
		options::SetSetting(Buffer, tablePtr.Name);

		snprintf(Buffer, sizeof Buffer, "%d", position);
		strcat(Buffer, ".Score");
		options::set_int(Buffer, tablePtr.Score);

		for (int i = static_cast<int>(strlen(tablePtr.Name)); --i >= 0; checkSum += tablePtr.Name[i])
		{
		}
		checkSum += tablePtr.Score;
	}

	options::set_int("Verification", checkSum);
	return 0;
}

void high_score::clear_table()
{
	for (auto& table : highscore_table)
	{
		table.Score = -999;
		table.Name[0] = 0;
	}
}

int high_score::get_score_position(int score)
{
	if (score <= 0)
		return -1;

	for (int position = 0; position < 5; position++)
	{
		if (highscore_table[position].Score < score)
			return position;
	}
	return -1;
}

void high_score::place_new_score_into(high_score_entry data)
{
	if (data.Position >= 0 && data.Position < 5)
	{
		for (int i = 4; i > data.Position; i--)
		{
			highscore_table[i] = highscore_table[i - 1];
		}

		data.Entry.Name[31] = 0;
		highscore_table[data.Position] = data.Entry;
	}
}

void high_score::show_high_score_dialog()
{
	ShowDialog = true;
}

void high_score::show_and_set_high_score_dialog(high_score_entry score)
{
	ScoreQueue.insert(ScoreQueue.begin(), score);
	ShowDialog = true;
}

void high_score::RenderHighScoreDialog()
{
	if (ShowDialog == true)
	{
		ShowDialog = false;
		if (!ImGui::IsPopupOpen(pb::get_rc_string(Msg::HIGHSCORES_Caption)))
		{
			dlg_enter_name = false;
			while (!ScoreQueue.empty())
			{
				DlgData = ScoreQueue.back();
				ScoreQueue.pop_back();
				if (DlgData.Position < 0 || DlgData.Position > 4)
				{
					DlgData.Position = get_score_position(DlgData.Entry.Score);
				}

				if (DlgData.Position != -1)
				{
					dlg_enter_name = true;
					break;
				}
			}

			// A cabinet has no keyboard, so initials are picked with the flippers
			CabinetEntry = dlg_enter_name && cabinet::CabinetModeActive();
			EntrySubmit = false;
			EntrySlot = 0;
			std::fill(std::begin(EntryChars), std::end(EntryChars), 0);

			ImGui::OpenPopup(pb::get_rc_string(Msg::HIGHSCORES_Caption));
		}
	}

	bool unused_open = true, textBoxSubmit = false;
	if (ImGui::BeginPopupModal(pb::get_rc_string(Msg::HIGHSCORES_Caption), &unused_open, ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (ImGui::BeginTable("table1", 3, ImGuiTableFlags_Borders))
		{
			char buf[36];
			ImGui::TableSetupColumn(pb::get_rc_string(Msg::HIGHSCORES_Rank));
			ImGui::TableSetupColumn(pb::get_rc_string(Msg::HIGHSCORES_Name));
			ImGui::TableSetupColumn(pb::get_rc_string(Msg::HIGHSCORES_Score));
			ImGui::TableHeadersRow();

			for (int offset = 0, row = 0; row < 5; row++)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				snprintf(buf, sizeof buf, "%d", row + 1);
				ImGui::TextUnformatted(buf);

				auto currentRow = &highscore_table[row + offset];
				auto score = currentRow->Score;
				ImGui::TableNextColumn();
				if (dlg_enter_name && DlgData.Position == row)
				{
					offset = -1;
					score = DlgData.Entry.Score;
					if (CabinetEntry)
					{
						// Slots are drawn one character at a time so the active one can blink
						for (auto slot = 0; slot < EntrySlots; slot++)
						{
							if (slot)
								ImGui::SameLine(0, 8);

							char label[2]{EntryCharset[EntryChars[slot]], 0};
							if (slot > EntrySlot)
								label[0] = '_';

							auto active = slot == EntrySlot;
							if (active && (SDL_GetTicks() / 250) % 2 == 0)
								label[0] = '_';
							if (active)
								ImGui::PushStyleColor(ImGuiCol_Text, pb::TextBoxColor);
							ImGui::TextUnformatted(label);
							if (active)
								ImGui::PopStyleColor();
						}
						textBoxSubmit = EntrySubmit;
					}
					else
					{
						ImGui::PushItemWidth(200);

						if (ImGui::IsWindowAppearing())
						{
							ImGui::SetKeyboardFocusHere(0);
						}
						if (ImGui::InputText("", DlgData.Entry.Name, IM_ARRAYSIZE(DlgData.Entry.Name),
							ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
						{
							textBoxSubmit = true;
						}
					}
				}
				else
				{
					ImGui::TextUnformatted(currentRow->Name);
				}

				ImGui::TableNextColumn();
				score::string_format(score, buf);
				ImGui::TextUnformatted(buf);
			}
			ImGui::EndTable();
		}

		if (CabinetEntry)
			ImGui::TextDisabled("Flippers change the letter, plunger accepts it. '<' rubs out.");

		if (ImGui::Button(pb::get_rc_string(Msg::GenericOk)) || textBoxSubmit)
		{
			if (dlg_enter_name)
			{
				if (CabinetEntry)
				{
					auto initials = GetEntryInitials();
					while (!initials.empty() && initials.back() == ' ')
						initials.pop_back();
					snprintf(DlgData.Entry.Name, sizeof DlgData.Entry.Name, "%s", initials.c_str());
				}
				place_new_score_into(DlgData);
			}
			CabinetEntry = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();
		if (ImGui::Button(pb::get_rc_string(Msg::GenericCancel)))
		{
			CabinetEntry = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();
		if (ImGui::Button(pb::get_rc_string(Msg::HIGHSCORES_Clear)))
			ImGui::OpenPopup("Confirm");
		if (ImGui::BeginPopupModal("Confirm", nullptr, ImGuiWindowFlags_MenuBar))
		{
			ImGui::TextUnformatted(pb::get_rc_string(Msg::STRING141));
			if (ImGui::Button(pb::get_rc_string(Msg::GenericOk), ImVec2(120, 0)))
			{
				clear_table();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SetItemDefaultFocus();
			ImGui::SameLine();
			if (ImGui::Button(pb::get_rc_string(Msg::GenericCancel), ImVec2(120, 0)))
			{
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		ImGui::EndPopup();

		// Reenter dialog for the next score in the queue
		if (!ImGui::IsPopupOpen(pb::get_rc_string(Msg::HIGHSCORES_Caption)) && !ScoreQueue.empty())
		{
			ShowDialog = true;
		}
	}
}
