#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include "Fader3Config.h"

#if defined (_WIN32)
#define __WINDOWS_MM__
#include <windows.h>
#endif

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

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);
}


int main(int argc, char** argv) {
	if (!glfwInit()) {
		std::cerr << "Failed to initialize GLFW" << std::endl;
		return -1;
	}

	Fader3* fader3 = new Fader3();

	bool use_default_settings = false;
		

	// Create a GLFW window
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_DECORATED, 1);
	glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);

	glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);  // Disable window decorations (title bar, borders)
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);   // Make window resizable (optional)
	

	GLFWwindow* window = glfwCreateWindow(fader3->width, fader3->height, "Fader3 Editor", nullptr, nullptr);
	if (!window) {
		std::cerr << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);	

	glfwSetMouseButtonCallback(window, MouseButtonCallback);
	glfwSetWindowCloseCallback(window, WindowCloseCallback);
	glfwSetKeyCallback(window, KeyCallback);
	
	// Attempt to load stored settings from the ini file
	const std::string iniFileName = use_default_settings ? "" : "fader3settings.ini";
	std::unordered_map<std::string, std::string> pairs;
	fader3->readAppSettings(iniFileName, pairs);
	// Try to get width and height
	fader3->findConfigValue("width", pairs, &fader3->width, nullptr);
	fader3->findConfigValue("height", pairs, &fader3->height, nullptr);

	
	// Get the monitor's dimensions so we can start the app bang in the middle of it,
	// in case there are no previously stored settings
	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* vidmode = glfwGetVideoMode(monitor);
	int wPosX = (vidmode->width - fader3->width) / 2;
	int wPosY = (vidmode->height - fader3->height) / 2;

	// Now look for previously stored settings
	fader3->findConfigValue("xpos", pairs, &wPosX, nullptr);
	fader3->findConfigValue("ypos", pairs, &wPosY, nullptr);

	// And update window pos either way
	glfwSetWindowPos(window, wPosX, wPosY);

	// Load OpenGL functions with GLAD
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		std::cerr << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	// Enable blending
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (fader3->refreshMidiPorts() < 1)
	{
		return -1;
	}

	// Attempt to open the previously used MIDI device ports
	std::string PrevMidiIn = "";
	std::string PrevMidiOut = "";

	fader3->findConfigValue("inmidi", pairs, nullptr, &PrevMidiIn);
	fader3->findConfigValue("outmidi", pairs, nullptr, &PrevMidiOut);

	for (int i = 0; i < fader3->MidiInNames.size(); i++)
	{
		if (fader3->MidiInNames[i] == PrevMidiIn)
		{
			fader3->MidiInIndex = i;
			fader3->openMidiInPort(i);
		}
	}

	for (int i = 0; i < fader3->MidiOutNames.size(); i++)
	{
		if (fader3->MidiOutNames[i] == PrevMidiOut)
		{
			fader3->MidiOutIndex = i;
			fader3->openMidiOutPort(i);
		}
	}

	// Setup ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	// Disable imgui's ini, since we're handling window size/pos ourselves and the true values aren't written to it
	io.IniFilename = NULL;

	// Setup Dear ImGui style
	Fader3ImGuiStyle();

	fader3->setupImGuiFonts();

	// Setup ImGui backends
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 460");


	// Main loop
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		// Do most of the work
		fader3->Update(window);

		glClearColor(0.1f, 0.1f, 0.1f, 0.0f);
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
	fader3->writeAppSettings(iniFileName, configPairs);
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();
	fader3->midiin->closePort();
	fader3->midiout->closePort();
	delete fader3->midiin;
	delete fader3->midiout;
	delete fader3;
	return 0;
}



