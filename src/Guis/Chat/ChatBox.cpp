// src/Guis/Chat/ChatBox.cpp

#include "ChatBox.h"
#include "../../Events/EventBus.h"
#include "../../Events/Event.h"

#include <imgui.h>
#include <cstring>
#include <iostream>

ChatBox& ChatBox::instance() {
    static ChatBox inst;
    return inst;
}

void ChatBox::init() {
    // Subscribe to chat messages arriving from the network.
    EventBus::instance().subscribe<ChatReceivedEvent>([this](const ChatReceivedEvent& e) {
        appendMessage(e.senderName, e.message);
    });
}

void ChatBox::appendMessage(const std::string& sender, const std::string& msg) {
    lines_.push_back({sender, msg});
    if (static_cast<int>(lines_.size()) > kMaxLines) {
        lines_.pop_front();
    }
    scrollToBottom = true;
}

void ChatBox::render() {
    if (!visible_) return;

    const float windowW  = 420.0f;
    const float windowH  = 160.0f;

    // Anchor to the bottom-left of the viewport.
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(
        ImVec2(8.0f, io.DisplaySize.y - windowH - 8.0f),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(windowW, windowH), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.75f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
                           | ImGuiWindowFlags_NoResize
                           | ImGuiWindowFlags_NoMove
                           | ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("##ChatBox", nullptr, flags)) {
        // -----------------------------------------------------------------
        // Scrollable message history
        // -----------------------------------------------------------------
        const float inputHeight = ImGui::GetTextLineHeightWithSpacing() + 8.0f;
        ImGui::BeginChild("ChatScroll",
                          ImVec2(0.0f, -(inputHeight)),
                          false,
                          ImGuiWindowFlags_HorizontalScrollbar);

        for (const auto& line : lines_) {
            ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f),
                               "%s:", line.senderName.c_str());
            ImGui::SameLine();
            ImGui::TextUnformatted(line.message.c_str());
        }

        if (scrollToBottom) {
            ImGui::SetScrollHereY(1.0f);
            scrollToBottom = false;
        }
        ImGui::EndChild();

        // -----------------------------------------------------------------
        // Text input field
        // -----------------------------------------------------------------
        ImGui::Separator();
        ImGui::SetNextItemWidth(-1.0f); // fill remaining width

        ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue;
        bool submitted = ImGui::InputText("##ChatInput", inputBuf_, kInputBuf, inputFlags);
        inputFocused_  = ImGui::IsItemActive();

        if (submitted && inputBuf_[0] != '\0') {
            // Publish a local "send chat" event.
            // The ChatBox subscription in init() handles the local echo; the
            // NetworkSystem subscription relays the message to the server.
            ChatReceivedEvent outgoing{};
            outgoing.senderNetworkId = 0; // 0 = local player (server fills this in)
            outgoing.senderName      = "You";
            outgoing.message         = inputBuf_;

            // Publish for local echo (via ChatBox::init subscription) and
            // for NetworkSystem to relay to server.
            EventBus::instance().publish(outgoing);

            // Clear the input buffer and re-focus for rapid chatting.
            std::memset(inputBuf_, 0, sizeof(inputBuf_));
            ImGui::SetKeyboardFocusHere(-1);
        }
    }
    ImGui::End();
}
