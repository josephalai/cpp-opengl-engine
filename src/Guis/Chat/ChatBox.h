// src/Guis/Chat/ChatBox.h
//
// Phase 3 — Spatial chat UI.
//
// ChatBox is an ImGui-based scrollable chat log with an input field.
// While the input field is focused, keyboard WASD events are suppressed so
// the player cannot accidentally walk while typing.
//
// The chat box subscribes to ChatReceivedEvent on construction; received
// messages are appended to the scroll buffer automatically.
// When the player presses Enter, the typed text is published via an internal
// SendChatEvent that the NetworkSystem picks up and wraps in a ChatMessagePacket.

#ifndef ENGINE_CHATBOX_H
#define ENGINE_CHATBOX_H

#include <string>
#include <vector>
#include <deque>
#include <cstdint>

struct ChatLine {
    std::string senderName;
    std::string message;
};

class ChatBox {
public:
    /// Singleton accessor.
    static ChatBox& instance();

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    /// Register EventBus subscriptions.  Call once after the EventBus is ready.
    void init();

    /// Per-frame ImGui rendering.  Call from UISystem::update().
    void render();

    /// Returns true when the text-input field currently has keyboard focus.
    /// InputDispatcher polls this to suppress WASD movement while typing.
    bool isTyping() const { return inputFocused_; }

    /// Append a received message to the log (also called by the EventBus handler).
    void appendMessage(const std::string& sender, const std::string& msg);

    // ------------------------------------------------------------------
    // Visibility toggle (e.g. press Enter to open)
    // ------------------------------------------------------------------
    void show() { visible_ = true; }
    void hide() { visible_ = false; }
    bool isVisible() const { return visible_; }
    void toggle() { visible_ = !visible_; }

private:
    ChatBox() = default;

    static constexpr int kMaxLines  = 200;  ///< Maximum lines kept in scroll buffer.
    static constexpr int kInputBuf  = 256;  ///< Chat input field character limit.

    bool visible_       = true;
    bool inputFocused_  = false;
    bool scrollToBottom = true;

    char inputBuf_[kInputBuf]{};

    std::deque<ChatLine> lines_;
};

#endif // ENGINE_CHATBOX_H
