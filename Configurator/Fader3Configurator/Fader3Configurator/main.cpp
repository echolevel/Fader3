#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "imgui_internal.h"
#include "IconsFontAwesome6.h"
#include <d3d11.h>
#include <dxgi.h>
#include <tchar.h>
#include "Fader3Config.h"
#include <windows.h>
#include <windowsx.h>
#include <algorithm>

#if defined (_WIN32)

#include <iostream>
#include <sstream>
#include <cstdlib>
#include <vector>
#define __WINDOWS_MM__
#include "RtMidi.h"

#endif

// Data
static ID3D11Device* g_pd3dDevice = NULL;
static ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
static IDXGISwapChain* g_pSwapChain = NULL;
static ID3D11RenderTargetView* g_mainRenderTargetView = NULL;

// Forward declarations
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Global variables to track drag state
static bool isDragging = false;
static bool isMenuHovered = false;
static ImVec2 dragStartPos;

float width = 340;
float height = 550;

float baseFontSize = 13.0f;
float iconFontSize = baseFontSize;

template <typename T>
T r_clamp(T value, T lo, T hi) {
	if (value < lo) return lo;
	if (value > hi) return hi;
	return value;
}

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

// Bess Dark Theme - https://github.com/shivang51/bess/blob/main/Bess/src/settings/themes.cpp
void SetBessTheme()
{
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;

	// Base colors for a pleasant and modern dark theme with dark accents
	colors[ImGuiCol_Text] = ImVec4(0.92f, 0.93f, 0.94f, 1.00f);                  // Light grey text for readability
	colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.52f, 0.54f, 1.00f);          // Subtle grey for disabled text
	colors[ImGuiCol_WindowBg] = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);              // Dark background with a hint of blue
	colors[ImGuiCol_ChildBg] = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);               // Slightly lighter for child elements
	colors[ImGuiCol_PopupBg] = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);               // Popup background
	colors[ImGuiCol_Border] = ImVec4(0.28f, 0.29f, 0.30f, 0.60f);                // Soft border color
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);          // No border shadow
	colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.22f, 0.24f, 1.00f);               // Frame background
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.24f, 0.26f, 1.00f);        // Frame hover effect
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.26f, 0.28f, 1.00f);         // Active frame background
	colors[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);               // Title background
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);         // Active title background
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);      // Collapsed title background
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);             // Menu bar background
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);           // Scrollbar background
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.24f, 0.26f, 0.28f, 1.00f);         // Dark accent for scrollbar grab
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.28f, 0.30f, 0.32f, 1.00f);  // Scrollbar grab hover
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.32f, 0.34f, 0.36f, 1.00f);   // Scrollbar grab active
	colors[ImGuiCol_CheckMark] = ImVec4(0.46f, 0.56f, 0.66f, 1.00f);             // Dark blue checkmark
	colors[ImGuiCol_SliderGrab] = ImVec4(0.36f, 0.46f, 0.56f, 1.00f);            // Dark blue slider grab
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.40f, 0.50f, 0.60f, 1.00f);      // Active slider grab
	colors[ImGuiCol_Button] = ImVec4(0.24f, 0.34f, 0.44f, 1.00f);                // Dark blue button
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.38f, 0.48f, 1.00f);         // Button hover effect
	colors[ImGuiCol_ButtonActive] = ImVec4(0.32f, 0.42f, 0.52f, 1.00f);          // Active button
	colors[ImGuiCol_Header] = ImVec4(0.24f, 0.34f, 0.44f, 1.00f);                // Header color similar to button
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.38f, 0.48f, 1.00f);         // Header hover effect
	colors[ImGuiCol_HeaderActive] = ImVec4(0.32f, 0.42f, 0.52f, 1.00f);          // Active header
	colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.29f, 0.30f, 1.00f);             // Separator color
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.46f, 0.56f, 0.66f, 1.00f);      // Hover effect for separator
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.46f, 0.56f, 0.66f, 1.00f);       // Active separator
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.36f, 0.46f, 0.56f, 1.00f);            // Resize grip
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.40f, 0.50f, 0.60f, 1.00f);     // Hover effect for resize grip
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.44f, 0.54f, 0.64f, 1.00f);      // Active resize grip
	colors[ImGuiCol_Tab] = ImVec4(0.20f, 0.22f, 0.24f, 1.00f);                   // Inactive tab
	colors[ImGuiCol_TabHovered] = ImVec4(0.28f, 0.38f, 0.48f, 1.00f);            // Hover effect for tab
	colors[ImGuiCol_TabActive] = ImVec4(0.24f, 0.34f, 0.44f, 1.00f);             // Active tab color
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.20f, 0.22f, 0.24f, 1.00f);          // Unfocused tab
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.24f, 0.34f, 0.44f, 1.00f);    // Active but unfocused tab
	colors[ImGuiCol_PlotLines] = ImVec4(0.46f, 0.56f, 0.66f, 1.00f);             // Plot lines
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.46f, 0.56f, 0.66f, 1.00f);      // Hover effect for plot lines
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.36f, 0.46f, 0.56f, 1.00f);         // Histogram color
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.40f, 0.50f, 0.60f, 1.00f);  // Hover effect for histogram
	colors[ImGuiCol_TableHeaderBg] = ImVec4(0.20f, 0.22f, 0.24f, 1.00f);         // Table header background
	colors[ImGuiCol_TableBorderStrong] = ImVec4(0.28f, 0.29f, 0.30f, 1.00f);     // Strong border for tables
	colors[ImGuiCol_TableBorderLight] = ImVec4(0.24f, 0.25f, 0.26f, 1.00f);      // Light border for tables
	colors[ImGuiCol_TableRowBg] = ImVec4(0.20f, 0.22f, 0.24f, 1.00f);            // Table row background
	colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.22f, 0.24f, 0.26f, 1.00f);         // Alternate row background
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.24f, 0.34f, 0.44f, 0.35f);        // Selected text background
	colors[ImGuiCol_DragDropTarget] = ImVec4(0.46f, 0.56f, 0.66f, 0.90f);        // Drag and drop target
	colors[ImGuiCol_NavHighlight] = ImVec4(0.46f, 0.56f, 0.66f, 1.00f);          // Navigation highlight
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f); // Windowing highlight
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);     // Dim background for windowing
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);      // Dim background for modal windows

	// Style adjustments
	style.WindowRounding = 0.0f;    // Softer rounded corners for windows
	style.FrameRounding = 4.0f;     // Rounded corners for frames
	style.ScrollbarRounding = 6.0f; // Rounded corners for scrollbars
	style.GrabRounding = 4.0f;      // Rounded corners for grab elements
	style.ChildRounding = 4.0f;     // Rounded corners for child windows

	style.WindowTitleAlign = ImVec2(0.50f, 0.50f); // Centered window title
	style.WindowPadding = ImVec2(10.0f, 10.0f);    // Comfortable padding
	style.FramePadding = ImVec2(6.0f, 4.0f);       // Frame padding
	style.ItemSpacing = ImVec2(8.0f, 8.0f);        // Item spacing
	style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);   // Inner item spacing
	style.IndentSpacing = 22.0f;                   // Indentation spacing

	style.ScrollbarSize = 16.0f; // Scrollbar size
	style.GrabMinSize = 10.0f;   // Minimum grab size

	style.AntiAliasedLines = true; // Enable anti-aliased lines
	style.AntiAliasedFill = true;  // Enable anti-aliased fill
}


Fader3* fader3 = new Fader3();

// Setup RtMidi
RtMidiIn* midiin = 0;
RtMidiOut* midiout = 0;

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
		fader3->settings->chan[0] = *it+1; it++;
		fader3->settings->chan[1] = *it+1; it++;
		fader3->settings->chan[2] = *it+1; it++;
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
		finalhexout << std::setfill('0') << std::setw(sizeof(char)*2) << std::hex << int(*it) << " ";
	}

	fader3->Log(finalhexout.str().c_str());	

}

float GetDpiScalingFactor(HWND hwnd)
{
	UINT dpi = GetDpiForWindow(hwnd);  // Get the DPI for the current window
	float scaleFactor = (float)dpi / 96.0f;  // Convert to scaling factor
	return scaleFactor;
}


// Main code
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	//SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2); // Enforce DPI awareness

	// Create application window
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("ImGui Example"), NULL };
	RegisterClassEx(&wc);
	HWND hwnd = CreateWindow(wc.lpszClassName, _T("Fader3 Configurator"), 
		WS_POPUP,
		1280, 
		400, 
		width, 
		height, 
		NULL, NULL, wc.hInstance, NULL);

	// Initialize Direct3D
	if (!CreateDeviceD3D(hwnd))
	{
		CleanupDeviceD3D();
		UnregisterClass(wc.lpszClassName, wc.hInstance);
		return 1;
	}

	ShowWindow(hwnd, SW_SHOWDEFAULT);
	UpdateWindow(hwnd);


	if (refreshMidiPorts() < 1)
	{
		return -1;
	}


	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	SetBessTheme();

	setupImGuiFonts();

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

	// ImGui globals
	static char inputStatus[32] = "Inactive";
	static char outputStatus[32] = "Inactive";



	// Main loop
	bool done = false;
	while (!done)
	{
		MSG msg;
		while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				done = true;
		}
		if (done)
			break;

		// Start ImGui frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		static POINT dragStartPos; //Starting mouse position
		static POINT windowStartPos;    // Starting window position

		// Get the current mouse position
		POINT cursorPos;
		GetCursorPos(&cursorPos);
		
		if (ImGui::GetMousePos().y < ImGui::GetFrameHeight())
		{
			isMenuHovered = true;
		}
		else if(!isDragging)
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
					dragStartPos = cursorPos;

					// Save the initial window position
					RECT rect;
					GetWindowRect(hwnd, &rect);
					windowStartPos = { rect.left, rect.top };
				}

				// Calculate the new position based on the total mouse movement
				POINT newWindowPos;
				newWindowPos.x = windowStartPos.x + static_cast<int>(cursorPos.x - dragStartPos.x);
				newWindowPos.y = windowStartPos.y + static_cast<int>(cursorPos.y - dragStartPos.y);

				// Set the new position
				SetWindowPos(hwnd, NULL, newWindowPos.x, newWindowPos.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
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
		ImGui::SetNextWindowSize(ImVec2(viewport->Size.x + 2, viewport->Size.y + 1));

		ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y));
		
		// Simple ImGui window
		static bool window_open = true;
		if (!window_open)
		{
			done = true;
		}

		ImGui::Begin("Fader3 Configurator", &window_open, ImGuiWindowFlags_NoCollapse);
		{

			if (!fader3->MidiInNames.empty())
			{
				static int MidiInIndex = 0;

				const char* combo_preview_value = fader3->MidiInNames[MidiInIndex].c_str();

				

				// Reset this in case we've released the ports 
				if (!midiin && midiin->isPortOpen())
				{
					snprintf(inputStatus, 32, "Inactive");
				}

				static ImVec4 inputStatusCol = ImVec4(1.0, 0.0, 0.0, 1.0); // red

				ImGui::SetNextItemWidth(200.f);
				if (ImGui::BeginCombo("Input", combo_preview_value, 0))
				{
					for (int i = 0; i < fader3->MidiInNames.size(); i++)
					{
						const bool is_selected = (MidiInIndex == i);
						if (ImGui::Selectable(fader3->MidiInNames[i].c_str(), is_selected))
						{
							MidiInIndex = i;

							if (midiin->isPortOpen())
							{
								midiin->closePort();
							}

							try
							{
								midiin->openPort(MidiInIndex, fader3->MidiInNames[i]);

								fader3->Log(fader3->MidiInNames[i].c_str());

								midiin->setCallback(&midiInCallback, fader3);

								// Don't ignore sysex, timing or active sensing messages
								midiin->ignoreTypes(false, true, true);

								snprintf(inputStatus, 32, "Active");
								inputStatusCol = ImVec4(0.0, 1.0, 0.0, 1.0);
							}
							catch (RtMidiError& error) {
								error.printMessage();
								fader3->Log(error.getMessage().c_str());
								//return -1;

								snprintf(inputStatus, 32, "Inactive");
								inputStatusCol = ImVec4(1.0, 0.0, 0.0, 1.0);
							}
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
			if (!midiout && midiout->isPortOpen())
			{
				snprintf(outputStatus, 32, "Inactive");
			}

			static ImVec4 outputStatusCol = ImVec4(1.0, 0.0, 0.0, 1.0); // red

			if (!fader3->MidiOutNames.empty())
			{
				static int MidiOutIndex = 0;

				const char* combo_preview_value = fader3->MidiOutNames[MidiOutIndex].c_str();

				ImGui::SetNextItemWidth(200.f);
				if (ImGui::BeginCombo("Output", combo_preview_value, 0))
				{
					for (int i = 0; i < fader3->MidiOutNames.size(); i++)
					{
						const bool is_selected = (MidiOutIndex == i);
						if (ImGui::Selectable(fader3->MidiOutNames[i].c_str(), is_selected))
						{
							MidiOutIndex = i;

							midiout->closePort();

							try
							{
								midiout->openPort(MidiOutIndex, fader3->MidiOutNames[i]);

								fader3->Log(fader3->MidiOutNames[i].c_str());

								if (midiout->isPortOpen())
								{
									// TEMP
									std::vector<unsigned char> message;
									message.push_back(176);
									message.push_back(7);
									message.push_back(100);
									midiout->sendMessage(&message);
								}

								snprintf(outputStatus, 32, "Active");
								outputStatusCol = ImVec4(0.0, 1.0, 0.0, 1.0);
							}
							catch (RtMidiError& error) {
								error.printMessage();
								fader3->Log(error.getMessage().c_str());

								snprintf(outputStatus, 32, "Inactive");
								outputStatusCol = ImVec4(0.0, 1.0, 0.0, 1.0);

								return -1;

							}

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
						ImGui::Text("Fader #%d", i+1);

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

			if(ImGui::BeginTable("##table3", 2, buttonTableFlags))
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
			
			

			
			
			// Console log
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


		}
				
		ImGui::End();

		// Rendering
		ImGui::Render();
		const float clear_color[4] = { 0.f, 0.f, 0.f, 1.00f };
		g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
		g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		g_pSwapChain->Present(1, 0); // Present with vsync

		
	}

	// Cleanup
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CleanupDeviceD3D();
	DestroyWindow(hwnd);
	UnregisterClass(wc.lpszClassName, wc.hInstance);

	delete midiin;
	delete midiout;
	delete fader3;

	return 0;
}

// Helper functions
bool CreateDeviceD3D(HWND hWnd)
{
	DXGI_SWAP_CHAIN_DESC sd = {};
	sd.BufferCount = 2;
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0,
		D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, NULL, &g_pd3dDeviceContext);
	if (FAILED(hr))
		return false;

	CreateRenderTarget();
	return true;
}

void CleanupDeviceD3D()
{
	CleanupRenderTarget();
	if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
	if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
	if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

void CreateRenderTarget()
{
	ID3D11Texture2D* pBackBuffer;
	g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
	g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
	pBackBuffer->Release();
}

void CleanupRenderTarget()
{
	if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
	{
		return true;
	}

	switch (msg)
	{
	case WM_LBUTTONDOWN: // LMB pressed on top bar
		POINT cursorPos;
		GetCursorPos(&cursorPos);
		ScreenToClient(hWnd, &cursorPos); // screen coords to client coords
		RECT windowRect;
		GetClientRect(hWnd, &windowRect);

		if(cursorPos.y >= windowRect.top && cursorPos.y <= (windowRect.top + 20)
			&& cursorPos.x >= windowRect.left && cursorPos.x <= windowRect.right)
		{
			SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0); // Send drag message
		}		
		return 0;		
		
		
	case WM_NCHITTEST:
	{
		// Get the mouse position relative to the window
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		ScreenToClient(hWnd, &pt);

		RECT rect;
		GetClientRect(hWnd, &rect);
		ImGui::GetIO().DisplaySize = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));

		const int borderWidth = 8;  // Thickness of the resize area

		// Determine the hit-test result based on the mouse position
		if (pt.x < borderWidth && pt.y < borderWidth) return HTTOPLEFT;
		if (pt.x > rect.right - borderWidth && pt.y < borderWidth) return HTTOPRIGHT;
		if (pt.x < borderWidth && pt.y > rect.bottom - borderWidth) return HTBOTTOMLEFT;
		if (pt.x > rect.right - borderWidth && pt.y > rect.bottom - borderWidth) return HTBOTTOMRIGHT;
		if (pt.x < borderWidth) return HTLEFT;
		if (pt.x > rect.right - borderWidth) return HTRIGHT;
		if (pt.y < borderWidth) return HTTOP;
		if (pt.y > rect.bottom - borderWidth) return HTBOTTOM;

		// Default to client area
		return HTCLIENT;
	}
	case WM_SIZE:
		if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
		{
			CleanupRenderTarget();
			g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
			CreateRenderTarget();
		}
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;	
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}
