#include "drbackup.h"
#define GLFW_INCLUDE_NONE
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <iostream>
#include <vector>
#include <string>

static GLFWwindow* window = nullptr;
static std::string backupCreatedMsg;
static std::string lastRestoreResultMsg;
static bool showBackupsModal = false;

// the backups folder, in here is %LOCALAPPDATA%\DRBackupCreator
static fs::path getBackupDir()
{
    auto base = getLocalAppData();
    if (!base) return fs::path{};
    return *base / "DRBackupCreator";
}

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "[GLFW] %d: %s\n", error, description);
}

static int refreshSlots(std::vector<SlotFile>& slots)
{
    auto dirOpt = getDeltaruneDir(false);
    if (!dirOpt) return -1;

    slots = scanSlots(*dirOpt);
    groupBySave(slots);
    return slots.empty() ? 0 : 1;
}

int runGui()
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        std::cerr << "[!] glfwInit() failed\n";
        return 1;
    }

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    window = glfwCreateWindow(800, 600, "DRBackupCreator GUI", nullptr, nullptr);
    if (!window) {
        std::cerr << "[!] Uh, Window couldn't be created, report this issue on GitHub\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // here i create backup folder if isnt created :#
    auto backupDir = getBackupDir();
    std::error_code ec;
    fs::create_directories(backupDir, ec);

    std::vector<SlotFile> slots;
    refreshSlots(slots);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("DRBackupCreator", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        
        auto dirOpt = getDeltaruneDir(true);
        if (dirOpt) {
            ImGui::Text("The DELTARUNE folder: %s", dirOpt->string().c_str());
        }
        ImGui::Text("The DRBC folder: %s", backupDir.string().c_str());

        if (!backupCreatedMsg.empty()) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), 
                "%s", backupCreatedMsg.c_str());
        }

        if (!lastRestoreResultMsg.empty()) {
            ImGui::TextColored(lastRestoreResultMsg[0] == '+' ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                "%s", lastRestoreResultMsg.c_str());
        }

        if (refreshSlots(slots) == 1 && !slots.empty()) {
            ImGui::Separator();
            ImGui::Text("%d slots:", (int)slots.size());
            ImGui::Separator();

            for (const auto& s : slots) {
                ImGui::Text("%s | Chap.%d Slot.%d | %s | %s",
                    s.index < 5 ? "SAVE     " : "COMPLETION",
                    s.chapter,
                    s.index,
                    formatFileSize(s.size).c_str(),
                    s.path.filename().string().c_str());
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), 
                "[!] Uh, i didn't found any slots, create an save file on DELTARUNE, then come back.");
        }

        ImGui::Separator();
        
        if (ImGui::Button("Create Backup")) {
            if (isGameRunning()) {
                backupCreatedMsg = "[!] Uh, DELTARUNE is running on this PC. Is recommended closing it to avoid corrupting your save file.";
            } else {
                if (slots.empty()) {
                    backupCreatedMsg = "[!] Theres no slots to restore.";
                } else {
                    std::string name = createBackup(backupDir, slots);
                    backupCreatedMsg = name.empty() ? "[!] Error creating backup." : "[+] Backup creado: " + name;
                }
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Show Backups")) {
            showBackupsModal = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Exit")) {
            break;
        }

        if (isGameRunning()) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                "[!] Uh, DELTARUNE is running on this PC. Is recommended closing it to avoid corrupting your save file.");
        }

        ImGui::End();

        // MODAL DE BACKUPS
        if (showBackupsModal) {
            ImGui::OpenPopup("Available Backups");
            showBackupsModal = false;
        }

        if (ImGui::BeginPopupModal("Available Backups", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            std::vector<BackupInfo> backups = listBackups(backupDir);
            
            if (backups.empty()) {
                ImGui::Text("[!] Theres no backups.");
            } else {
                ImGui::Text("%d backups encontrados:", (int)backups.size());
                ImGui::Separator();

                for (size_t i = 0; i < backups.size() && i < 10; i++) {
                    ImGui::PushID(i);
                    ImGui::Text("[%s] %d slots | %s", 
                        backups[i].timestamp.c_str(),
                        backups[i].slotCount,
                        formatFileSize(backups[i].totalSize).c_str());
                    
                    if (ImGui::SmallButton("Restore##restore")) {
                        int restoredCount = 0;
                        if (restoreBackup(backups[i].path, *dirOpt, restoredCount)) {
                            lastRestoreResultMsg = "[+] Restored " + std::to_string(restoredCount) + " archivos.";
                        } else {
                            lastRestoreResultMsg = "[!] Error restoring backup.";
                        }
                    }
                    ImGui::PopID();
                }
            }

            if (ImGui::Button("Close")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::Render();

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}