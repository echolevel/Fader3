#include <GLFW/glfw3.h>
#include "Fader3Config.h"
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include "IconsFontAwesome6.h"
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <vector>
#include <unordered_map>

#if defined (_WIN32)
#define __WINDOWS_MM__
#include <windows.h>
#endif

#include "RtMidi.h"

Fader3::Fader3()
{
	width = 330;
	height = 500;

	MidiInIndex = 0;
	MidiOutIndex = 0;

	lastChannel = 0;
	lastCCnum = 0;
	lastCCvalue = 0;
}

Fader3::~Fader3()
{

}

void Fader3::Update(GLFWwindow* window)
{
	// Start ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// Starting mouse position
	static double dragStartX;
	static double dragStartY;
	glfwGetCursorPos(window, &dragStartX, &dragStartY);

	// Starting window position
	static int windowStartX;
	static int windowStartY;
	glfwGetWindowPos(window, &windowStartX, &windowStartY);

	// Get the current mouse position
	static double cursorX;
	static double cursorY;
	glfwGetCursorPos(window, &cursorX, &cursorY);

	if (ImGui::IsMouseClicked(0))
	{
		if (ImGui::GetMousePos().y < ImGui::GetFrameHeight())
		{
			dragStartedOnTitlebar = true;
		}
	}

	if (ImGui::GetMousePos().y < ImGui::GetFrameHeight())
	{
		isMenuHovered = true;
	}
	else if (!isDragging)
	{
		//isMenuHovered = false;
	}

	if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
	{
		if (isMenuHovered && dragStartedOnTitlebar)
		{
			if (!isDragging)
			{
				// Start dragging
				isDragging = true;

				// Save the initial mouse position
				dragStartPos = ImVec2((float)cursorX, (float)cursorY);

				// Save the initial window position
				glfwGetWindowPos(window, &windowStartX, &windowStartX);
			}

			// Calculate the new position based on the total mouse movement
			int newWindowPosX;
			int newWindowPosY;
			newWindowPosX = windowStartX + static_cast<int>(cursorX - dragStartPos.x);
			newWindowPosY = windowStartY + static_cast<int>(cursorY - dragStartPos.y);

			// Set the new position
			glfwSetWindowPos(window, newWindowPosX, newWindowPosY);
		}
	}
	else
	{
		// Stop dragging
		isDragging = false;
		isMenuHovered = false;
	}

	if (!ImGui::IsMouseDown(0))
	{
		dragStartedOnTitlebar = false;
	}

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x - 1, viewport->Pos.y));
	ImGui::SetNextWindowSize(ImVec2(width, height));

	static bool window_open = true;
	if (!window_open)
	{
		glfwSetWindowShouldClose(window, true);
	}
	if (ImGui::Begin("Fader3 Editor", &window_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_MenuBar))
	{
		// Unnecessary File menu, with unnecessary theme switcher

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Read"))
				{

				}
				if (ImGui::MenuItem("Write"))
				{

				}

				ImGui::Separator();

				if (ImGui::MenuItem("Quit"))
				{
					glfwSetWindowShouldClose(window, true);
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Theme"))
			{
				if (ImGui::MenuItem("Fader3"))
				{
					Fader3ImGuiStyle();
				}

				if (ImGui::MenuItem("VisualStudio"))
				{
					SetupImGuiStyle();
				}

				if (ImGui::MenuItem("Bess Dark"))
				{
					SetBessTheme();
				}

				if (ImGui::MenuItem("ImGui Dark"))
				{
					ImGui::StyleColorsDark();
				}

				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}


		// MIDI in/out port dropdowns

		if (!MidiInNames.empty())
		{
			const char* combo_preview_value = MidiInNames[MidiInIndex].c_str();

			// Reset this in case we've released the ports 
			if ((midiin != nullptr) && !midiin->isPortOpen())
			{
				snprintf(inputStatus, 32, "Inactive");
			}

			static ImVec4 inputStatusCol = ImVec4(1.0, 0.0, 0.0, 1.0); // red

			ImGui::SetNextItemWidth(200.f);
			if (ImGui::BeginCombo("Input", combo_preview_value, 0))
			{
				for (int i = 0; i < MidiInNames.size(); i++)
				{
					const bool is_selected = (MidiInIndex == i);
					if (ImGui::Selectable(MidiInNames[i].c_str(), is_selected))
					{
						MidiInIndex = i;
						openMidiInPort(MidiInIndex);
					}
					if (is_selected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			if (strcmp(inputStatus, "Active") == 0)
			{
				inputStatusCol = ImVec4(0.0, 1.0, 0.0, 1.0);
			}
			else
			{
				inputStatusCol = ImVec4(1.0, 0.0, 0.0, 1.0);
			}
			ImGui::TextColored(inputStatusCol, inputStatus);
		}



		// Reset this in case we've released the ports 
		if ((midiout != nullptr) && !midiout->isPortOpen())
		{
			snprintf(outputStatus, 32, "Inactive");
		}

		static ImVec4 outputStatusCol = ImVec4(1.0, 0.0, 0.0, 1.0); // red

		if (!MidiOutNames.empty())
		{

			const char* combo_preview_value = MidiOutNames[MidiOutIndex].c_str();

			ImGui::SetNextItemWidth(200.f);
			if (ImGui::BeginCombo("Output", combo_preview_value, 0))
			{
				for (int i = 0; i < MidiOutNames.size(); i++)
				{
					const bool is_selected = (MidiOutIndex == i);
					if (ImGui::Selectable(MidiOutNames[i].c_str(), is_selected))
					{
						MidiOutIndex = i;

						openMidiOutPort(i);
					}
					if (is_selected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			if (strcmp(outputStatus, "Active") == 0)
			{
				outputStatusCol = ImVec4(0.0, 1.0, 0.0, 1.0);
			}
			else
			{
				outputStatusCol = ImVec4(1.0, 0.0, 0.0, 1.0);
			}
			ImGui::TextColored(outputStatusCol, outputStatus);
		}

		if (ImGui::Button(ICON_FA_RECYCLE" Refresh"))
		{
			refreshMidiPorts();
		}

		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_PLUG_CIRCLE_XMARK " Release"))
		{
			releaseMidiPorts(inputStatus, outputStatus);
		}


		// Settings table

		static ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingFixedFit;

		if (ImGui::BeginTable("##Table1", 4, tableFlags))
		{
			ImGui::TableNextRow();

			for (int i = 0; i < 4; i++)
			{
				ImGui::TableSetColumnIndex(i);
				if (i == 3)
				{
					{
						ImGui::AlignTextToFramePadding();
						ImGui::NewLine();

						ImGui::AlignTextToFramePadding();
						ImGui::Text("14-bit mode");

						ImGui::AlignTextToFramePadding();
						ImGui::Text("Channel number");

						ImGui::AlignTextToFramePadding();
						ImGui::Text("CC number");
					}
				}
				else
				{
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Fader #%d", i + 1);

					ImGui::PushID(i);
					ImGui::PushItemWidth(40.f);

					bool f14bit = settings->f14bit[i];
					if (ImGui::Checkbox("##14bit", &f14bit))
					{
						settings->f14bit[i] = f14bit;
					}

					ImGui::DragInt("##Chan", &settings->chan[i], 1.0f, 1, 16);
					ImGui::SetItemTooltip("Ctrl+Click to enter value manually");

					ImGui::DragInt("##CC #", &settings->ccNum[i], 1.0f, 0, 127);

					ImGui::SetItemTooltip("Ctrl+Click to enter value manually");

					ImGui::PopID();
					ImGui::PopItemWidth();
				}
			}

			ImGui::EndTable();
		}

		ImGui::NewLine();

		if (ImGui::BeginTable("##Table2", 3, tableFlags))
		{
			ImGui::TableNextRow();

			for (int i = 0; i < 3; i++)
			{
				ImGui::TableSetColumnIndex(i);
				if (i == 0)
				{
					{
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Smoothing samples");

						ImGui::AlignTextToFramePadding();
						ImGui::Text("Delta threshold");
					}
				}
				else
				{
					ImGui::PushID(i);
					ImGui::PushItemWidth(40.f);

					if (i == 1)
					{
						ImGui::DragInt("14-bit##smooth", &settings->smooth14bit, 1.0f, 0, 63);
						ImGui::SetItemTooltip("Ctrl+Click to enter value manually");

						ImGui::DragInt("14-bit##thresh", &settings->thresh14bit, 1.0f, 0, 127);
						ImGui::SetItemTooltip("Ctrl+Click to enter value manually");
					}
					else if (i == 2)
					{
						ImGui::DragInt("7-bit##smooth", &settings->smooth7bit, 1.0f, 0, 63);
						ImGui::SetItemTooltip("Ctrl+Click to enter value manually");

						ImGui::DragInt("7-bit##thresh", &settings->thresh7bit, 1.0f, 0, 127);
						ImGui::SetItemTooltip("Ctrl+Click to enter value manually");
					}
					ImGui::PopID();
					ImGui::PopItemWidth();
				}



			}
			ImGui::EndTable();
		}

		ImGui::NewLine();

		ImGuiTableFlags buttonTableFlags = ImGuiTableFlags_SizingStretchSame;

		if (ImGui::BeginTable("##table3", 2, buttonTableFlags))
		{
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			{
				if (ImGui::Button(ICON_FA_DOWNLOAD " Read config"))
				{
					Log("Reading Fader3 sysex config...");

					std::vector<unsigned char> message;

					// Start the message
					message.push_back(0xF0);

					// Manufacturer ID (I use Fairlight CMI's)
					message.push_back(0x7d);

					// Dump Request header
					message.push_back(0x00);
					message.push_back(0x0A);

					// End the message
					message.push_back(0xF7);
					midiout->sendMessage(&message);
				}
			}


			// Hardware read/write, including confirmation popup on write

			static bool showConfirmPopup = false;

			ImGui::TableSetColumnIndex(1);
			{
				if (ImGui::Button(ICON_FA_UPLOAD " Write config"))
				{
					showConfirmPopup = true;
					ImGui::OpenPopup("Write Config");


				}
				ImGui::SetItemTooltip("Write config to device");
			}

			if (ImGui::BeginPopupModal("Write Config", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::Text("Are you sure?");
				ImGui::Separator();
				if (ImGui::Button("Yes"))
				{
					// do the thing
					std::vector<unsigned char> message = settings->getSysexBytes();

					Log("Writing Fader3 sysex config...");
					midiout->sendMessage(&message);

					ImGui::CloseCurrentPopup();
					showConfirmPopup = false;
				}
				ImGui::SameLine();
				if (ImGui::Button("No"))
				{
					// cancel
					ImGui::CloseCurrentPopup();
					showConfirmPopup = false;

				}
				ImGui::EndPopup();
			}

			ImGui::EndTable();
		}


		ImGui::Text("Midi Monitor");
		static ImGuiInputTextFlags flags = ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoHorizontalScroll;

		std::string multilines;
		for (int i = (int)MidiLogs.size() - 1; i > 0; i--)
		{
			if (i >= LINESMAX) continue;

			multilines += MidiLogs[i] + '\n';
		};

		static std::vector<char> multibuffer;
		multibuffer.resize(multilines.size() + 1); // leave space for null terminator
		std::copy(multilines.begin(), multilines.end(), multibuffer.begin());
		multibuffer[multilines.size()] = '\0'; // terminate buffer

		ImGui::InputTextMultiline("##logoutput", multibuffer.data(), multibuffer.size(), ImVec2(-FLT_MIN, -1.0), flags);


		// Main ImGui window ENDS
		ImGui::End();
	}
	// ImGui demo window
	//ImGui::ShowDemoWindow();

	// Synchronize GLFW window size with ImGui window
	SyncWindowSizeWithImGui(window);

	// Render
	ImGui::Render();
}

std::string Fader3::getExecutableDirectory()
{
	char path[1024] = { 0 };

#ifdef _WIN32 
	GetModuleFileNameA(nullptr, path, sizeof(path));
	std::string exePath(path);
	size_t pos = exePath.find_last_of("\\/");
	return exePath.substr(0, pos); // Get directory of executable
#else
	// Mac or Linux
	char result[1024];
	SSIZE_T count = readlink("/proc/self/exe", result, sizeof(result));
	if (count != -1)
	{
		std::string exePath(result, count);
		size_t pos = exePath.find_last_of("/\\");
		return exePath.substr(0, pos); // return dir of exe
	}
	return "";
#endif
}

void Fader3::writeAppSettings(const std::string& fileName, const std::unordered_map<std::string, std::string>& keyValuePairs)
{
	std::string filePath = getExecutableDirectory() + "/" + fileName;
	std::ofstream file(filePath);
	if (file.is_open())
	{
		file << "[Settings]" << std::endl;

		for (const auto& pair : keyValuePairs)
		{
			file << pair.first << "=" << pair.second << std::endl;
		}

		file.close();
		std::cout << "Data written to " << filePath << std::endl;
	}
	else
	{
		std::cerr << "Failed to open file for writing!" << std::endl;
	}
}

void Fader3::readAppSettings(const std::string& fileName, std::unordered_map<std::string, std::string>& keyValuePairs)
{
	std::string filePath = getExecutableDirectory() + "/" + fileName;
	std::ifstream file(filePath);

	if (file.is_open()) {
		std::string line;
		std::string currentSection;

		while (std::getline(file, line))
		{
			if (line.empty() || line[0] == ';' || line[0] == '#')
				continue;

			if (line[0] == '[' && line[line.size() - 1] == ']')
			{
				currentSection = line.substr(1, line.size() - 2);
				continue;
			}

			// Process key/value pairs
			size_t delimiterPos = line.find('=');
			if (delimiterPos != std::string::npos)
			{
				std::string key = line.substr(0, delimiterPos);
				std::string value = line.substr(delimiterPos + 1);

				// Store in the map
				keyValuePairs[key] = value;
			}
		}


		std::cout << "Data read from " << filePath << std::endl;
		file.close();
	}
	else {
		std::cerr << "Failed to open file for reading!" << std::endl;
	}
	file.close();
}


void Fader3::findConfigValue(const std::string keyname, std::unordered_map<std::string, std::string>& configPairsLoading, int* outputint /*= nullptr*/, std::string* outputstring /*= nullptr*/)
{
	if (configPairsLoading.find(keyname) != configPairsLoading.end())
	{
		try
		{
			if (outputint != nullptr)
			{
				// Only overwrite output if we find a valid keyname
				*outputint = std::stoi(configPairsLoading[keyname]);
			}
			if (outputstring != nullptr)
			{
				// Only overwrite output if we find a valid keyname
				*outputstring = configPairsLoading[keyname];
			}

		}
		catch (const std::invalid_argument& e)
		{
			std::cerr << "Invalid argument: " << e.what() << std::endl;
		}
		catch (const std::out_of_range& e) {
			std::cerr << "Out of range: " << e.what() << std::endl;
		}

	}
}



void Fader3::SyncWindowSizeWithImGui(GLFWwindow* glfwWindow)
{
	ImGuiIO& io = ImGui::GetIO();

	// Assuming the ImGui window is named "Main"
	if (ImGui::Begin("Fader3 Editor")) {
		ImVec2 imguiSize = ImGui::GetWindowSize();

		// Convert ImGui size to integer dimensions for GLFW
		int newWidth = static_cast<int>(imguiSize.x);
		int newHeight = static_cast<int>(imguiSize.y);

		// Retrieve the current GLFW window size
		int currentWidth, currentHeight;
		glfwGetWindowSize(glfwWindow, &currentWidth, &currentHeight);

		// Resize GLFW window only if the size has changed
		if (newWidth != currentWidth || newHeight != currentHeight) {
			glfwSetWindowSize(glfwWindow, newWidth, newHeight);
		}
	}
	ImGui::End();
}

void Fader3::setupImGuiFonts()
{
	ImGuiIO& io = ImGui::GetIO();

	io.Fonts->AddFontDefault();

	static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
	ImFontConfig config;
	config.MergeMode = true;
	config.PixelSnapH = true;
	config.GlyphMaxAdvanceX = iconFontSize;
	config.GlyphOffset.y = 1.0f;
	iconFont = io.Fonts->AddFontFromFileTTF(FONT_ICON_FILE_NAME_FAS, iconFontSize, &config, icon_ranges);

	io.Fonts->Build();
}

int Fader3::refreshMidiPorts()
{
	if (!midiin || !midiout)
	{
		try
		{
			midiin = new RtMidiIn();
			midiin->setBufferSize(2048, 4);
			midiout = new RtMidiOut();
		}
		catch (RtMidiError& error)
		{
			std::cerr << "RtMidiError: ";
			error.printMessage();
			Log(error.getMessage().c_str());
			//exit(EXIT_FAILURE);
			//return -1;
		}
	}

	// Check Midi inputs
	if (midiin)
	{
		// Clear old names
		MidiInNames.clear();

		unsigned int nPorts = midiin->getPortCount();
		std::cout << "\nThere are " << nPorts << " MIDI input sources available. \n";
		std::string portName;
		for (unsigned int i = 0; i < nPorts; i++)
		{
			try {
				portName = midiin->getPortName(i);
			}
			catch (RtMidiError& error)
			{
				std::cerr << "RtMidiError: ";
				error.printMessage();
				Log(error.getMessage().c_str());
				return -1;
			}
			std::cout << " Input Port #" << i + 1 << ": " << portName << '\n';
			MidiInNames.push_back(portName);
			Log(portName.c_str());
		}
	}


	if (midiout)
	{
		// Clear old names
		MidiOutNames.clear();

		// Check outputs.
		unsigned int nPorts = midiout->getPortCount();
		std::string portName;
		std::cout << "\nThere are " << nPorts << " MIDI output ports available.\n";
		for (unsigned int i = 0; i < nPorts; i++) {
			try {
				portName = midiout->getPortName(i);
			}
			catch (RtMidiError& error) {
				error.printMessage();
				Log(error.getMessage().c_str());
				return -1;
			}
			std::cout << "  Output Port #" << i + 1 << ": " << portName << '\n';
			MidiOutNames.push_back(portName);
			Log(portName.c_str());
		}
		std::cout << '\n';
	}

	return 1;
}

// We need to use a static var for the main Fader3 instance because it's for some reason 
// impossible to pass RtMidi our callback function when it's a member of a class, and I 
// couldn't get it to work as a lambda either.
static Fader3* globalInstance = nullptr;

static void midiInCallback(double deltatime, std::vector<unsigned char>* message, void* userData )
{
	if (globalInstance == nullptr)
	{
		return;
	}

	if (message->size() < 1)
	{
		return;
	}
	std::stringstream debugout;

	for (unsigned char byte : *message)
	{
		debugout << (int)byte << " ";
	}

	globalInstance->Log(debugout.str().c_str());

	std::stringstream finalhexout;

	// Is this a sysex message?
	if (((int)message->at(0) == 0xF0) && message->size() >= 14)
	{
		std::vector<unsigned char>::iterator it = message->begin();

		globalInstance->Log("Received config dump from Fader3");

		it += 2; // start at the second byte		
		globalInstance->settings->chan[0] = *it + 1; it++;
		globalInstance->settings->chan[1] = *it + 1; it++;
		globalInstance->settings->chan[2] = *it + 1; it++;
		globalInstance->settings->ccNum[0] = *it; it++;
		globalInstance->settings->ccNum[1] = *it; it++;
		globalInstance->settings->ccNum[2] = *it; it++;
		globalInstance->settings->f14bit[0] = *it; it++;
		globalInstance->settings->f14bit[1] = *it; it++;
		globalInstance->settings->f14bit[2] = *it; it++;
		globalInstance->settings->smooth14bit = *it; it++;
		globalInstance->settings->smooth7bit = *it; it++;
		globalInstance->settings->thresh14bit = *it; it++;
		globalInstance->settings->thresh7bit = *it; it++;
	}

	// Is this a control message status byte? Check if MSB is 0B
	else if (((int)message->at(0) >> 4) == 0x0b)
	{
		int ccNum = (int)message->at(1);
		int ccChan = ((int)message->at(0) & 0x0F) + 1;
		int ccValue = (int)message->at(2);

		bool is14bit = false;
		int value14bit = 0;
		// If this CC number is equal to the last one + 32, and the channel is the same, it's 14-bit o'clock
		if ((ccNum == (globalInstance->lastCCnum + 32)) && (ccChan == globalInstance->lastChannel))
		{
			is14bit = true;
			value14bit = (globalInstance->lastCCvalue << 7) | ccValue;
		}

		// It is, so get the channel from the LSB. We add 1 for display purposes.
		finalhexout << "Chan:" << ccChan << " ";

		// Control message
		finalhexout << "CC";

		// Next byte will be CC number
		finalhexout << ccNum;

		// Then value
		if (is14bit)
		{
			finalhexout << " Value 14bit: " << value14bit;
		}
		else
		{
			finalhexout << " Value 7bit: " << ccValue;
		}

		// Cache channel and CC num for later 14-bit checks
		globalInstance->lastChannel = ccChan;
		globalInstance->lastCCnum = ccNum;
		globalInstance->lastCCvalue = ccValue;
	}

	// It's not CC or sysex so just display raw bytes
	else for (auto it = message->begin(); it != message->end(); it++)
	{
		finalhexout << std::setfill('0') << std::setw(sizeof(char) * 2) << std::hex << int(*it) << " ";
	}

	globalInstance->Log(finalhexout.str().c_str());

}

int Fader3::openMidiInPort(int InIndex)
{
	if (midiin->isPortOpen())
		midiin->closePort();

	try
	{
		midiin->openPort(InIndex, MidiInNames[InIndex]);
		Log(MidiInNames[InIndex].c_str());

		// Use the static globalInstance of the active Fader3 class
		globalInstance = this;
		if (globalInstance != nullptr)
		{
			midiin->setCallback(midiInCallback, nullptr);			
			// Don't ignore sysex, timing or active sensing messages
			midiin->ignoreTypes(false, true, true);
			snprintf(inputStatus, 32, "Active");
			return 1;
		}
		
	}
	catch (RtMidiError& error) {
		error.printMessage();
		Log(error.getMessage().c_str());
		snprintf(inputStatus, 32, "Inactive");
		return 0;
	}

	return 0;
}


int Fader3::openMidiOutPort(int OutIndex)
{
	if (midiout->isPortOpen())
		midiout->closePort();

	try
	{
		midiout->openPort(OutIndex, MidiOutNames[OutIndex]);
		Log(MidiOutNames[OutIndex].c_str());
		snprintf(outputStatus, 32, "Active");
		return 1;
	}
	catch (RtMidiError& error) {
		error.printMessage();
		Log(error.getMessage().c_str());
		snprintf(outputStatus, 32, "Inactive");
		return 0;
	}

	return 0;
}

void Fader3::releaseMidiPorts(char* inputStatus, char* outputStatus)
{
	if (midiin && midiin->isPortOpen())
	{
		midiin->closePort();
		sprintf_s(inputStatus, 32, "Inactive");
	}

	if (midiout && midiout->isPortOpen())
	{
		midiout->closePort();
		sprintf_s(outputStatus, 32, "Inactive");
	}

	Log("MIDI ports closed");
}

void Fader3::Log(const char* fmt) {
	
	std::time_t now = std::time(nullptr);
	std::tm localTime;
	localtime_s(&localTime, &now);

	std::ostringstream timestamp;
	timestamp << "[" << std::setfill('0') << std::setw(2) << localTime.tm_hour << ":"
		<< std::setfill('0') << std::setw(2) << localTime.tm_min << ":"
		<< std::setfill('0') << std::setw(2) << localTime.tm_sec << "] ";

	std::string timestampedFmt = timestamp.str() + fmt;

	char buffer[LINEBUFFERMAX];
	snprintf(buffer, sizeof(buffer), "%s", timestampedFmt.c_str());
	buffer[sizeof(buffer) - 1] = 0; // Ensure null-termination
	MidiLogs.push_back(buffer);

	// Lose surplus lines
	if (MidiLogs.size() > LINESMAX)
	{
		MidiLogs.erase(MidiLogs.begin(), MidiLogs.begin() + (MidiLogs.size() - LINESMAX ));
	}
	
}
