#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include "imgui_internal.h"
#include "IconsFontAwesome6.h"
#include "Fader3Config.h"
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <sstream>
#include <cstdlib>
#include <vector>
#include <unordered_map>

#if defined (_WIN32)
#define __WINDOWS_MM__
#include <windows.h>
#endif

#include "RtMidi.h"


std::string getExecutableDirectory()
{
	char path[1024] = {0};

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

void writeAppSettings(const std::string& fileName, const std::unordered_map<std::string, std::string>& keyValuePairs)
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

void readAppSettings(const std::string& fileName, std::unordered_map<std::string, std::string>& keyValuePairs) {
	std::string filePath = getExecutableDirectory() + "/" + fileName;
	std::ifstream file(filePath);
	
	if (file.is_open()) {
		std::string line;
		std::string currentSection;

		while (std::getline(file, line))
		{
			if(line.empty() || line[0] == ';' || line[0] == '#')
				continue;

			if (line[0] == '[' && line[line.size() - 1] == ']')
			{
				currentSection = line.substr(1, line.size()-2);
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

void findConfigValue(const std::string keyname, std::unordered_map<std::string, std::string>& configPairsLoading, int* outputint = nullptr, std::string* outputstring = nullptr)
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

// GLFW Event Handlers - to replicate WndProc stuff
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);

		int windowWidth, windowHeight;
		glfwGetWindowSize(window, &windowWidth, &windowHeight);

		// Check if the mouse is in the top bar area
		if (ypos >= 0 && ypos <= 20) { // Assuming the top bar height is 20 pixels
			glfwSetWindowAttrib(window, GLFW_RESIZABLE, GLFW_FALSE); // Temporarily disable resizing
			glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE); // Make window draggable
		}
	}
}

void WindowCloseCallback(GLFWwindow* window) {
	glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_LEFT_ALT || key == GLFW_KEY_RIGHT_ALT) {
		// Handle ALT key logic if needed
	}

	if (key == GLFW_KEY_ESCAPE)
	{
		glfwSetWindowShouldClose(window, true);
	}
}

void CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
	const int borderWidth = 8;
	int windowWidth, windowHeight;
	glfwGetWindowSize(window, &windowWidth, &windowHeight);

	if (xpos < borderWidth && ypos < borderWidth) {
		// Top-left corner
	}
	else if (xpos > windowWidth - borderWidth && ypos < borderWidth) {
		// Top-right corner
	}
	else if (xpos < borderWidth && ypos > windowHeight - borderWidth) {
		// Bottom-left corner
	}
	else if (xpos > windowWidth - borderWidth && ypos > windowHeight - borderWidth) {
		// Bottom-right corner
	}
	else if (xpos < borderWidth) {
		// Left edge
	}
	else if (xpos > windowWidth - borderWidth) {
		// Right edge
	}
	else if (ypos < borderWidth) {
		// Top edge
	}
	else if (ypos > windowHeight - borderWidth) {
		// Bottom edge
	}
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);
}

void SyncWindowSizeWithImGui(GLFWwindow* glfwWindow) {
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


static bool isDragging = false;
static bool isMenuHovered = false;
static ImVec2 dragStartPos;

int width = 320;
int height = 400;

float baseFontSize = 13.0f;
float iconFontSize = baseFontSize;


ImFont* iconFont;

void setupImGuiFonts() {
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


Fader3* fader3 = new Fader3();

// Setup RtMidi
RtMidiIn* midiin = 0;
RtMidiOut* midiout = 0;

// ImGui globals
static char inputStatus[32] = "Inactive";
static char outputStatus[32] = "Inactive";

int refreshMidiPorts()
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
			fader3->Log(error.getMessage().c_str());
			//exit(EXIT_FAILURE);
			//return -1;
		}
	}



	// Check Midi inputs
	if (midiin)
	{
		// Clear old names
		fader3->MidiInNames.clear();

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
				fader3->Log(error.getMessage().c_str());
				return -1;
			}
			std::cout << " Input Port #" << i + 1 << ": " << portName << '\n';
			fader3->MidiInNames.push_back(portName);
			fader3->Log(portName.c_str());
			fader3->scrollToBottom = true;
		}
	}


	if (midiout)
	{
		// Clear old names
		fader3->MidiOutNames.clear();

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
				fader3->Log(error.getMessage().c_str());
				return -1;
			}
			std::cout << "  Output Port #" << i + 1 << ": " << portName << '\n';
			fader3->MidiOutNames.push_back(portName);
			fader3->Log(portName.c_str());
		}
		std::cout << '\n';
	}

	return 1;
}

void midiInCallback(double deltatime, std::vector<unsigned char>* message, void* userData)
{
	if (!fader3 || message->size() < 1)
	{
		return;
	}
	std::stringstream debugout;

	for (unsigned char byte : *message)
	{
		debugout << (int)byte << " ";
	}

	fader3->Log(debugout.str().c_str());

	std::stringstream finalhexout;

	// Is this a sysex message?
	if (((int)message->at(0) == 0xF0) && message->size() >= 14)
	{
		std::vector<unsigned char>::iterator it = message->begin();

		fader3->Log("Received config dump from Fader3");

		it += 2; // start at the second byte		
		fader3->settings->chan[0] = *it + 1; it++;
		fader3->settings->chan[1] = *it + 1; it++;
		fader3->settings->chan[2] = *it + 1; it++;
		fader3->settings->ccNum[0] = *it; it++;
		fader3->settings->ccNum[1] = *it; it++;
		fader3->settings->ccNum[2] = *it; it++;
		fader3->settings->f14bit[0] = *it; it++;
		fader3->settings->f14bit[1] = *it; it++;
		fader3->settings->f14bit[2] = *it; it++;
		fader3->settings->smooth14bit = *it; it++;
		fader3->settings->smooth7bit = *it; it++;
		fader3->settings->thresh14bit = *it; it++;
		fader3->settings->thresh7bit = *it; it++;
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
		if ((ccNum == (fader3->lastCCnum + 32))
			&& (ccChan == fader3->lastChannel))
		{
			is14bit = true;
			value14bit = (fader3->lastCCvalue << 7) | ccValue;
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
		fader3->lastChannel = ccChan;
		fader3->lastCCnum = ccNum;
		fader3->lastCCvalue = ccValue;
	}

	// It's not CC or sysex so just display raw bytes
	else for (auto it = message->begin(); it != message->end(); it++)
	{
		finalhexout << std::setfill('0') << std::setw(sizeof(char) * 2) << std::hex << int(*it) << " ";
	}

	fader3->Log(finalhexout.str().c_str());

}

int openMidiInPort(int InIndex)
{
	if (midiin->isPortOpen())
		midiin->closePort();

	try
	{
		midiin->openPort(InIndex, fader3->MidiInNames[InIndex]);
		fader3->Log(fader3->MidiInNames[InIndex].c_str());
		midiin->setCallback(&midiInCallback, fader3);
		// Don't ignore sysex, timing or active sensing messages
		midiin->ignoreTypes(false, true, true);
		snprintf(inputStatus, 32, "Active");
		return 1;
	}
	catch (RtMidiError& error) {
		error.printMessage();
		fader3->Log(error.getMessage().c_str());
		snprintf(inputStatus, 32, "Inactive");
		return 0;
	}

	return 0;
}

int openMidiOutPort(int OutIndex)
{	
	if (midiout->isPortOpen())
		midiout->closePort();

	try
	{
		midiout->openPort(OutIndex, fader3->MidiOutNames[OutIndex]);
		fader3->Log(fader3->MidiOutNames[OutIndex].c_str());		
		snprintf(outputStatus, 32, "Active");
		return 1;
	}
	catch (RtMidiError& error) {
		error.printMessage();
		fader3->Log(error.getMessage().c_str());
		snprintf(outputStatus, 32, "Inactive");
		return 0;
	}

	return 0;
}

void releaseMidiPorts(char* inputStatus, char* outputStatus)
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

	fader3->Log("MIDI ports closed");
}

inline float mapToRange(const float x, const float in_min, const float in_max, const float out_min, const float out_max)
{
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}





int main() {
	if (!glfwInit()) {
		std::cerr << "Failed to initialize GLFW" << std::endl;
		return -1;
	}

	// Create a GLFW window
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_DECORATED, 1);

	glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);  // Disable window decorations (title bar, borders)
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);   // Make window resizable (optional)
	

	GLFWwindow* window = glfwCreateWindow(width, height, "Fader3 Editor", nullptr, nullptr);
	if (!window) {
		std::cerr << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	glfwSetMouseButtonCallback(window, MouseButtonCallback);
	glfwSetWindowCloseCallback(window, WindowCloseCallback);
	glfwSetKeyCallback(window, KeyCallback);
	glfwSetCursorPosCallback(window, CursorPosCallback);

	const std::string iniFileName = "fader3settings.ini";
	std::unordered_map<std::string, std::string> pairs;
	readAppSettings(iniFileName, pairs);
	
	findConfigValue("width", pairs, &width, nullptr);
	findConfigValue("height", pairs, &height, nullptr);

	


	// Get the monitor's dimensions so we can start the app bang in the middle of it
	// (We can't rely on ImGui's saved position in the ini, because we override that
	// and it always reads -1,0)
	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* vidmode = glfwGetVideoMode(monitor);
	int wPosX = (vidmode->width - width) / 2;
	int wPosY = (vidmode->height - height) / 2;

	findConfigValue("xpos", pairs, &wPosX, nullptr);
	findConfigValue("ypos", pairs, &wPosY, nullptr);

	glfwSetWindowPos(window, wPosX, wPosY);

	// Load OpenGL functions with GLAD
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		std::cerr << "Failed to initialize GLAD" << std::endl;
		return -1;
	}


	if (refreshMidiPorts() < 1)
	{
		return -1;
	}

	std::string PrevMidiIn = "";
	std::string PrevMidiOut = "";

	findConfigValue("inmidi", pairs, nullptr, &PrevMidiIn);
	findConfigValue("outmidi", pairs, nullptr, &PrevMidiOut);

	for (int i = 0; i < fader3->MidiInNames.size(); i++)
	{
		if (fader3->MidiInNames[i] == PrevMidiIn)
		{
			fader3->MidiInIndex = i;
			openMidiInPort(i);
		}
	}

	for (int i = 0; i < fader3->MidiOutNames.size(); i++)
	{
		if (fader3->MidiOutNames[i] == PrevMidiOut)
		{
			fader3->MidiOutIndex = i;
			openMidiOutPort(i);
		}
	}

	// Setup ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	SetBessTheme();

	setupImGuiFonts();

	// Setup ImGui backends
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 460");

	

	// Main loop
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		

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

		if (ImGui::GetMousePos().y < ImGui::GetFrameHeight())
		{
			isMenuHovered = true;
		}
		else if (!isDragging)
		{
			isMenuHovered = false;
		}

		if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
		{
			if (isMenuHovered)
			{
				if (!isDragging)
				{
					// Start dragging
					isDragging = true;

					// Save the initial mouse position
					dragStartPos = ImVec2(cursorX, cursorY);

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

		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x - 1, viewport->Pos.y));		
		

		static bool window_open = true;
		if (!window_open)
		{
			glfwSetWindowShouldClose(window, true);
		}
		if(ImGui::Begin("Fader3 Editor", &window_open, ImGuiWindowFlags_NoCollapse))
		{

			if (!fader3->MidiInNames.empty())
			{
				

				const char* combo_preview_value = fader3->MidiInNames[fader3->MidiInIndex].c_str();

				// Reset this in case we've released the ports 
				if ((midiin != nullptr) && !midiin->isPortOpen())
				{
					snprintf(inputStatus, 32, "Inactive");
				}

				static ImVec4 inputStatusCol = ImVec4(1.0, 0.0, 0.0, 1.0); // red

				ImGui::SetNextItemWidth(200.f);
				if (ImGui::BeginCombo("Input", combo_preview_value, 0))
				{
					for (int i = 0; i < fader3->MidiInNames.size(); i++)
					{
						const bool is_selected = (fader3->MidiInIndex == i);
						if (ImGui::Selectable(fader3->MidiInNames[i].c_str(), is_selected))
						{
							fader3->MidiInIndex = i;
							openMidiInPort(fader3->MidiInIndex);
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

			if (!fader3->MidiOutNames.empty())
			{
				
				const char* combo_preview_value = fader3->MidiOutNames[fader3->MidiOutIndex].c_str();

				ImGui::SetNextItemWidth(200.f);
				if (ImGui::BeginCombo("Output", combo_preview_value, 0))
				{
					for (int i = 0; i < fader3->MidiOutNames.size(); i++)
					{
						const bool is_selected = (fader3->MidiOutIndex == i);
						if (ImGui::Selectable(fader3->MidiOutNames[i].c_str(), is_selected))
						{
							fader3->MidiOutIndex = i;

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

						bool f14bit = fader3->settings->f14bit[i];
						if (ImGui::Checkbox("##14bit", &f14bit))
						{
							fader3->settings->f14bit[i] = f14bit;
						}

						ImGui::DragInt("##Chan", &fader3->settings->chan[i], 1.0f, 1, 16);
						ImGui::SetItemTooltip("Ctrl+Click to enter value manually");

						ImGui::DragInt("##CC #", &fader3->settings->ccNum[i], 1.0f, 0, 127);

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
							ImGui::DragInt("14-bit##smooth", &fader3->settings->smooth14bit, 1.0f, 0, 63);
							ImGui::SetItemTooltip("Ctrl+Click to enter value manually");

							ImGui::DragInt("14-bit##thresh", &fader3->settings->thresh14bit, 1.0f, 0, 127);
							ImGui::SetItemTooltip("Ctrl+Click to enter value manually");
						}
						else if (i == 2)
						{
							ImGui::DragInt("7-bit##smooth", &fader3->settings->smooth7bit, 1.0f, 0, 63);
							ImGui::SetItemTooltip("Ctrl+Click to enter value manually");

							ImGui::DragInt("7-bit##thresh", &fader3->settings->thresh7bit, 1.0f, 0, 127);
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
						fader3->Log("Reading Fader3 sysex config...");

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
						std::vector<unsigned char> message = fader3->settings->getSysexBytes();

						fader3->Log("Writing Fader3 sysex config...");
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
			for (int i = fader3->MidiLogs.size() - 1; i > 0; i--)
			{
				if (i >= LINESMAX) continue;

				multilines += fader3->MidiLogs[i] + '\n';
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
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(window);
	}

	// Cleanup
	std::unordered_map<std::string, std::string> configPairs;
	int glW, glH, glXpos, glYpos;
	glfwGetWindowSize(window, &glW, &glH);
	glfwGetWindowPos(window, &glXpos, &glYpos);
	configPairs["width"] = std::to_string(glW);
	configPairs["height"] = std::to_string(glH);
	configPairs["xpos"] = std::to_string(glXpos);
	configPairs["ypos"] = std::to_string(glYpos);
	configPairs["inmidi"] = fader3->MidiInNames[fader3->MidiInIndex];
	configPairs["outmidi"] = fader3->MidiOutNames[fader3->MidiOutIndex];
	writeAppSettings(iniFileName, configPairs);
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();
	delete midiin;
	delete midiout;
	delete fader3;
	return 0;
}


