#include "stdafx.h"

#include <GWCA/Utilities/Debug.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Macros.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/Managers/Module.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/ChatMgr.h>

#define COLOR_ARGB(a, r, g, b) (GW::Chat::Color)((((a) & 0xff) << 24) | (((r) & 0xff) << 16) | (((g) & 0xff) << 8) | ((b) & 0xff))
#define COLOR_RGB(r, g, b) COLOR_ARGB(0xff, r, g, b)

namespace {
    using namespace GW;

    bool ShowTimestamps = false;
    bool Timestamp_24hFormat = false;
    bool Timestamp_seconds = false;
    Chat::Color TimestampsColor = COLOR_RGB(0xff, 0xff, 0xff);

    // 08 01 07 01 [Time] 01 00 02 00
    // ChatBuffer **ChatBuffer_Addr;
    Chat::ChatBuffer** ChatBuffer_Addr = nullptr;
    uint32_t* IsTyping_FrameId = nullptr;

    // There is maybe more.
    // Though, we can probably fix this.
    bool ChannelThatParseColorTag[] = {
        true, true, true, true, true, true, true,
        false, // WARNING
        true, true, true, true, true,
        false, // ADVISORY
        true
    };

    std::map<Chat::Channel, Chat::Color> ChatSenderColor;
    std::map<Chat::Channel, Chat::Color> ChatMessageColor;

    void wcs_tolower(wchar_t* s)
    {
        for (size_t i = 0; s[i]; i++)
            s[i] = towlower(s[i]);
    }

    typedef Chat::Color* (__cdecl* GetChannelColor_pt)(Chat::Color* color, uint32_t chan);
    GetChannelColor_pt GetSenderColor_Func = 0, GetSenderColor_Ret = 0;
    GetChannelColor_pt GetMessageColor_Func = 0, GetMessageColor_Ret = 0;

    struct ChatCommandCallbackHandler {
        Chat::ChatCommandCallback voidcb = nullptr;
        Chat::BoolChatCommandCallback boolcb = nullptr;
    };
    std::unordered_map<std::wstring, ChatCommandCallbackHandler> chat_command_hook_entries;

    Chat::Color* __cdecl OnGetSenderColor_Func(Chat::Color* color, Chat::Channel chan) {
        HookBase::EnterHook();
        GW::UI::UIPacket::kGetColor packet = { color, chan };
        *packet.color = ChatSenderColor[chan];
        GW::UI::SendUIMessage(GW::UI::UIMessage::kGetSenderColor, &packet);
        HookBase::LeaveHook();
        return packet.color;
    }

    Chat::Color* __cdecl OnGetMessageColor_Func(Chat::Color* color, Chat::Channel chan) {
        HookBase::EnterHook();
        GW::UI::UIPacket::kGetColor packet = { color, chan };
        *packet.color = ChatMessageColor[chan];
        GW::UI::SendUIMessage(GW::UI::UIMessage::kGetMessageColor, &packet);
        HookBase::LeaveHook();
        return packet.color;
    }

    typedef void(__cdecl* SendChat_pt)(wchar_t* message, uint32_t agent_id);
    SendChat_pt SendChat_Func = 0, SendChat_Ret = 0;

    void __cdecl OnSendChat_Func(wchar_t *message, uint32_t agent_id) {
        HookBase::EnterHook();
        GW::UI::UIPacket::kSendChatMessage packet = { message, agent_id };
        GW::UI::SendUIMessage(GW::UI::UIMessage::kSendChatMessage, &packet);
        HookBase::LeaveHook();
    }

    typedef void(__cdecl *RecvWhisper_pt)(uint32_t transaction_id, wchar_t *player_name, wchar_t* message);
    RecvWhisper_pt RecvWhisper_Func = 0, RecvWhisper_Ret = 0;

    void __cdecl OnRecvWhisper_Func(uint32_t transaction_id, wchar_t *player_name, wchar_t* message) {
        HookBase::EnterHook();
        uint32_t packet[] = { transaction_id, (uint32_t)player_name, (uint32_t)message };
        GW::UI::SendUIMessage(GW::UI::UIMessage::kRecvWhisper, packet);
        HookBase::LeaveHook();
    }

    typedef void(__fastcall* StartWhisper_pt)(GW::UI::Frame* ctx, uint32_t edx, wchar_t* name);
    StartWhisper_pt StartWhisper_Func = 0, StartWhisper_Ret = 0;

    void __fastcall OnStartWhisper_Func(GW::UI::Frame* ctx, uint32_t, wchar_t* name) {
        GW::HookBase::EnterHook();
        wchar_t* packet[] = {name};
        GW::UI::SendUIMessage(GW::UI::UIMessage::kStartWhisper, packet, ctx);
        GW::HookBase::LeaveHook();
    }

    typedef void(_cdecl* AddToChatLog_pt)(wchar_t* message, uint32_t channel);
    AddToChatLog_pt AddToChatLog_Func = 0, AddToChatLog_Ret = 0;

    void __cdecl OnAddToChatLog_Func(wchar_t* message, uint32_t channel) {
        GW::HookBase::EnterHook();
        uint32_t packet[] = { (uint32_t)message, channel };
        GW::UI::SendUIMessage(GW::UI::UIMessage::kLogChatMessage, packet);
        GW::HookBase::LeaveHook();
    }

    typedef void (__fastcall *PrintChat_pt)(UI::Frame *frame, uint32_t edx, Chat::Channel channel, wchar_t *message, FILETIME timestamp, bool is_reprint);
    PrintChat_pt PrintChat_Func = 0, PrintChat_Ret = 0;

    void __fastcall OnPrintChat_Func(UI::Frame *frame, uint32_t, Chat::Channel channel, wchar_t *message, FILETIME timestamp, bool is_reprint)
    {
        HookBase::EnterHook();
        GW::UI::UIPacket::kPrintChatMessage packet = { channel, message, timestamp, is_reprint };
        GW::UI::SendUIMessage(GW::UI::UIMessage::kPrintChatMessage, &packet, frame);
        HookBase::LeaveHook();
    }

    wchar_t* rewritten_message_buffer = nullptr;

    void OnUIMessage_Pre_AddTimestampToChatMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void* wparam, void*) {
        GWCA_ASSERT(message_id == GW::UI::UIMessage::kPrintChatMessage);
        const auto packet = (GW::UI::UIPacket::kPrintChatMessage*)wparam;
        if (!ShowTimestamps)
            return;
        if (packet->message == rewritten_message_buffer)
            return;

        FILETIME   timestamp2;
        SYSTEMTIME localtime;

        FileTimeToLocalFileTime(&packet->timestamp, &timestamp2);
        FileTimeToSystemTime(&timestamp2, &localtime);

        WORD hour = localtime.wHour;
        WORD minute = localtime.wMinute;
        WORD second = localtime.wSecond;

        if(!Timestamp_24hFormat)
            hour %= 12;


        wchar_t time_buffer[29];
        if (localtime.wYear == 0) {
            Timestamp_seconds ? std::memcpy(time_buffer, L"[lbracket]--:--:--[rbracket]", sizeof(wchar_t) * 29)
                : std::memcpy(time_buffer, L"[lbracket]--:--[rbracket]", sizeof(wchar_t) * 26);
        }
        else {
            if(Timestamp_seconds)
                swprintf(time_buffer, 29, L"[lbracket]%02d:%02d:%02d[rbracket]", hour, minute, second);
            else
                swprintf(time_buffer, 29, L"[lbracket]%02d:%02d[rbracket]", hour, minute);
        }
        size_t buf_len = 21 + 29 + wcslen(packet->message);

        rewritten_message_buffer = new wchar_t[buf_len];
        if (ChannelThatParseColorTag[packet->channel]) {
            swprintf(rewritten_message_buffer, buf_len, L"\x108\x107<c=#%06x>%s </c>\x01\x02%s", (TimestampsColor & 0x00FFFFFF), time_buffer, packet->message);
        } else {
            swprintf(rewritten_message_buffer, buf_len, L"\x108\x107%s \x01\x02%s", time_buffer, packet->message);
        }
        packet->message = rewritten_message_buffer;

    }
    void OnUIMessage_Post_AddTimestampToChatMessage(GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
        if (rewritten_message_buffer)
            delete[] rewritten_message_buffer;
        rewritten_message_buffer = nullptr;
    }


    UI::UIInteractionCallback UICallback_AssignEditableText_Func = nullptr;
    UI::UIInteractionCallback UICallback_AssignEditableText_Ret = nullptr;
    // When a control is terminated ( message 0xB ) it doesn't clear the IsTyping_FrameId that we're using. Clear it manually.
    void OnUICallback_AssignEditableText(UI::InteractionMessage* message, void* wParam, void* lParam) {
        HookBase::EnterHook();
        if (message->message_id == UI::UIMessage::kDestroyFrame && IsTyping_FrameId && *IsTyping_FrameId == message->frame_id) {
            *IsTyping_FrameId = 0;
            //GWCA_INFO("IsTyping_FrameId manually cleared");
        }
        UICallback_AssignEditableText_Ret(message, wParam, lParam);
        HookBase::LeaveHook();
    }

    GW::HookEntry UIMessage_Entry;
    const UI::UIMessage ui_messages_to_hook[] = {
        UI::UIMessage::kSendChatMessage,
        UI::UIMessage::kStartWhisper,
        UI::UIMessage::kPrintChatMessage,
        UI::UIMessage::kLogChatMessage,
        UI::UIMessage::kRecvWhisper
    };
    void OnUIMessage(GW::HookStatus* status, UI::UIMessage message_id, void* wparam, void* lparam) {
        if (status->blocked)
            return;
        uint32_t* pack = (uint32_t*)wparam;
        switch (message_id) {
        case UI::UIMessage::kSendChatMessage: {
            const auto msg = reinterpret_cast<UI::UIPacket::kSendChatMessage*>(pack);
            if (Chat::GetChannel(*msg->message) == Chat::CHANNEL_COMMAND) {
                int argc = 0;
                LPWSTR* argv = CommandLineToArgvW(msg->message + 1, &argc);
                GWCA_ASSERT(argv && argc);
                wcs_tolower(*argv);

                for (auto [command_str, callback_handler] : chat_command_hook_entries) {
                    if (command_str != *argv)
                        continue;
                    if (callback_handler.voidcb) {
                        callback_handler.voidcb(msg->message, argc, argv);
                        status->blocked = true;
                    }
                    else {
                        if (callback_handler.boolcb(msg->message, argc, argv)) {
                            status->blocked = true;
                        }
                    }
                }
                LocalFree(argv);
            }
            if (!status->blocked && SendChat_Ret) {
                SendChat_Ret(msg->message, msg->agent_id);
                return;
            }
        } break;
        case UI::UIMessage::kStartWhisper: {
            if (StartWhisper_Ret) {
                const auto frame = lparam ? (UI::Frame*)lparam : UI::GetFrameByLabel(L"Chat");
                if (frame) {
                    StartWhisper_Ret(frame, 0, *(wchar_t**)wparam);
                    return;
                }
            }
        } break;
        case UI::UIMessage::kPrintChatMessage: {
            const auto packet = (GW::UI::UIPacket::kPrintChatMessage*)wparam;
            if (PrintChat_Ret) {
                const auto frame = lparam ? (UI::Frame*)lparam : UI::GetFrameByLabel(L"Log");
                if (frame) {
                    PrintChat_Ret(frame, 0, packet->channel,packet->message,packet->timestamp,packet->is_reprint);
                    return;
                }
            }
        } break;
        case UI::UIMessage::kLogChatMessage: {
            if (AddToChatLog_Ret) {
                AddToChatLog_Ret((wchar_t*)pack[0], pack[1]);
                return;
            }
        } break;
        case UI::UIMessage::kRecvWhisper: {
            if (RecvWhisper_Ret) {
                RecvWhisper_Ret(pack[0], (wchar_t*)pack[1], (wchar_t*)pack[2]);
                return;
            }
        } break;
        }
        status->blocked = true;
    }

    void DisableHook(void* hook) {
        if (hook)
            HookBase::DisableHooks(hook);
    }
    void EnableHook(void* hook) {
        if (hook)
            HookBase::EnableHooks(hook);
    }


    void Init() {
        GetSenderColor_Func = (GetChannelColor_pt)Scanner::Find("\xC7\x00\x60\xC0\xFF\xFF\x5D\xC3", "xxxxxxxx", -0x1C);
        GetMessageColor_Func = (GetChannelColor_pt)Scanner::Find("\xC7\x00\xB0\xB0\xB0\xFF\x5D\xC3", "xxxxxxxx", -0x27);
        SendChat_Func = (SendChat_pt)Scanner::Find("\x8D\x85\xE0\xFE\xFF\xFF\x50\x68\x1C\x01", "xxxxxxxxx", -0x3E);
        StartWhisper_Func = (StartWhisper_pt)GW::Scanner::Find("\xFC\x53\x56\x8B\xF1\x57\x6A\x05\xFF\x36\xE8", "xxxxxxxxxxx", -0xF);
        PrintChat_Func = (PrintChat_pt)Scanner::Find("\x3D\x00\x00\x00\x00\x73\x2B\x6A", "x??xxxxx", -0x46);
        AddToChatLog_Func = (AddToChatLog_pt)GW::Scanner::Find("\x40\x25\xff\x01\x00\x00", "xxxxxx", -0x97);
        ChatBuffer_Addr = *(Chat::ChatBuffer***)Scanner::Find("\x8B\x45\x08\x83\x7D\x0C\x07\x74", "xxxxxxxx", -4);
        RecvWhisper_Func = (RecvWhisper_pt)Scanner::Find("\x83\xc4\x04\x8d\x58\x2e\x8b\xc3", "xxxxxxxx", -0x18);

        uintptr_t address = Scanner::FindAssertion("p:\\code\\engine\\controls\\ctledit.cpp","charCount >= 1",0x37);
        if (address && Scanner::IsValidPtr(*(uintptr_t*) address))
            IsTyping_FrameId = *(uint32_t **)address;

        address = Scanner::Find("\x6a\x06\x68\x00\x03\x80\x00","xxxxxxx",-0x4);
        if (address && Scanner::IsValidPtr(*(uintptr_t*)address, Scanner::TEXT))
            UICallback_AssignEditableText_Func = *(UI::UIInteractionCallback*)address;

        GWCA_INFO("[SCAN] GetSenderColor = %p", GetSenderColor_Func);
        GWCA_INFO("[SCAN] GetMessageColor = %p", GetMessageColor_Func);
        GWCA_INFO("[SCAN] SendChat = %p", SendChat_Func);
        GWCA_INFO("[SCAN] StartWhisper = %p", StartWhisper_Func);
        GWCA_INFO("[SCAN] RecvWhisper_Func = %p", RecvWhisper_Func);
        GWCA_INFO("[SCAN] PrintChat = %p", PrintChat_Func);
        GWCA_INFO("[SCAN] AddToChatLog_Func = %p", AddToChatLog_Func);
        GWCA_INFO("[SCAN] ChatBuffer_Addr = %p", ChatBuffer_Addr);
        GWCA_INFO("[SCAN] IsTyping_FrameId = %p", IsTyping_FrameId);
        GWCA_INFO("[SCAN] UICallback_AssignEditableText_Func = %p", UICallback_AssignEditableText_Func);

#ifdef _DEBUG
        GWCA_ASSERT(GetSenderColor_Func);
        GWCA_ASSERT(GetMessageColor_Func);
        GWCA_ASSERT(SendChat_Func);
        GWCA_ASSERT(StartWhisper_Func);
        GWCA_ASSERT(RecvWhisper_Func);
        GWCA_ASSERT(PrintChat_Func);
        GWCA_ASSERT(AddToChatLog_Func);
        GWCA_ASSERT(ChatBuffer_Addr);
        GWCA_ASSERT(IsTyping_FrameId);
        GWCA_ASSERT(UICallback_AssignEditableText_Func);
#endif

        HookBase::CreateHook(StartWhisper_Func, OnStartWhisper_Func, (void**)& StartWhisper_Ret);
        HookBase::CreateHook(GetSenderColor_Func, OnGetSenderColor_Func, (void **)&GetSenderColor_Ret);
        HookBase::CreateHook(GetMessageColor_Func, OnGetMessageColor_Func, (void **)&GetMessageColor_Ret);
        HookBase::CreateHook(SendChat_Func, OnSendChat_Func, (void **)&SendChat_Ret);
        HookBase::CreateHook(RecvWhisper_Func, OnRecvWhisper_Func, (void **)&RecvWhisper_Ret);
        HookBase::CreateHook(PrintChat_Func, OnPrintChat_Func, (void **)&PrintChat_Ret);
        HookBase::CreateHook(AddToChatLog_Func, OnAddToChatLog_Func, (void**)&AddToChatLog_Ret);
        HookBase::CreateHook(UICallback_AssignEditableText_Func, OnUICallback_AssignEditableText, (void**)& UICallback_AssignEditableText_Ret);

        for (size_t i = 0; i < (size_t)GW::Chat::Channel::CHANNEL_COUNT; i++) {
            const auto chan = (GW::Chat::Channel)i;
            ChatSenderColor[chan] = 0;
            GetSenderColor_Ret(&ChatSenderColor[chan], chan);
            ChatMessageColor[chan] = 0;
            GetMessageColor_Ret(&ChatMessageColor[chan], chan);
        }
    }

    void EnableHooks() {
        EnableHook(StartWhisper_Func);
        EnableHook(GetSenderColor_Func);
        EnableHook(GetMessageColor_Func);
        EnableHook(SendChat_Func);
        EnableHook(RecvWhisper_Func);
        EnableHook(PrintChat_Func);
        EnableHook(AddToChatLog_Func);
        EnableHook(UICallback_AssignEditableText_Func);

        UI::RegisterUIMessageCallback((GW::HookEntry*)OnUIMessage_Pre_AddTimestampToChatMessage, UI::UIMessage::kPrintChatMessage, OnUIMessage_Pre_AddTimestampToChatMessage, -0x4000);
        UI::RegisterUIMessageCallback((GW::HookEntry*)OnUIMessage_Post_AddTimestampToChatMessage, UI::UIMessage::kPrintChatMessage, OnUIMessage_Post_AddTimestampToChatMessage, 0x4000);

        for (auto ui_message : ui_messages_to_hook) {
            UI::RegisterUIMessageCallback(&UIMessage_Entry, ui_message, OnUIMessage, 0x1);
        }
    }
    void DisableHooks() {
        DisableHook(StartWhisper_Func);
        DisableHook(GetSenderColor_Func);
        DisableHook(GetMessageColor_Func);
        DisableHook(SendChat_Func);
        DisableHook(RecvWhisper_Func);
        DisableHook(PrintChat_Func);
        DisableHook(AddToChatLog_Func);
        DisableHook(UICallback_AssignEditableText_Func);

        UI::RemoveUIMessageCallback((GW::HookEntry*)OnUIMessage_Pre_AddTimestampToChatMessage);
        UI::RemoveUIMessageCallback((GW::HookEntry*)OnUIMessage_Post_AddTimestampToChatMessage);

        UI::RemoveUIMessageCallback(&UIMessage_Entry);
    }

    void Exit() {
        HookBase::RemoveHook(StartWhisper_Func);
        HookBase::RemoveHook(GetSenderColor_Func);
        HookBase::RemoveHook(GetMessageColor_Func);
        HookBase::RemoveHook(SendChat_Func);
        HookBase::RemoveHook(RecvWhisper_Func);
        HookBase::RemoveHook(PrintChat_Func);
        HookBase::RemoveHook(AddToChatLog_Func);
        HookBase::RemoveHook(UICallback_AssignEditableText_Func);

        while (chat_command_hook_entries.size()) {
            Chat::DeleteCommand(chat_command_hook_entries.begin()->first.c_str());
        }
    }
}

namespace GW {

    Module ChatModule = {
        "ChatModule",   // name
        NULL,           // param
        ::Init,         // init_module
        ::Exit,         // exit_module
        ::EnableHooks,           // enable_hooks
        ::DisableHooks, // disable_hooks
    };

    Chat::Channel Chat::GetChannel(char opcode) {
        switch (opcode) {
        case '!': return Chat::Channel::CHANNEL_ALL;
        case '@': return Chat::Channel::CHANNEL_GUILD;
        case '#': return Chat::Channel::CHANNEL_GROUP;
        case '$': return Chat::Channel::CHANNEL_TRADE;
        case '%': return Chat::Channel::CHANNEL_ALLIANCE;
        case '"': return Chat::Channel::CHANNEL_WHISPER;
        case '/': return Chat::Channel::CHANNEL_COMMAND;
        default:  return Chat::Channel::CHANNEL_UNKNOW;
        }
    }
    Chat::Channel Chat::GetChannel(wchar_t opcode) {
        return GetChannel((char)opcode);
    }

    Chat::ChatBuffer* Chat::GetChatLog() {
        return *ChatBuffer_Addr;
    }

    bool Chat::AddToChatLog(wchar_t* message, uint32_t channel) {
        uint32_t packet[] = { (uint32_t)message, channel };
        return GW::UI::SendUIMessage(UI::UIMessage::kLogChatMessage, packet);
    }

    Chat::Color Chat::SetSenderColor(Channel chan, Color col) {
        Color old = 0;
        GetChannelColors(chan, &old, 0);
        ChatSenderColor[chan] = col;
        return old;
    }

    Chat::Color Chat::SetMessageColor(Channel chan, Color col) {
        Color old = 0;
        GetChannelColors(chan, 0, &old);
        ChatMessageColor[chan] = col;
        return old;
    }

    void Chat::GetChannelColors(Channel chan, Color *sender, Color *message) {
        GW::UI::UIPacket::kGetColor packet = { sender, chan };
        if (sender && GetSenderColor_Func) {
            GetSenderColor_Func(sender, chan);
        }
        if (message && GetMessageColor_Func) {
            GetMessageColor_Func(message, chan);
        }
    }
    void Chat::GetDefaultColors(Channel chan, Color* sender, Color* message) {
        if (sender && GetSenderColor_Ret) {
            GetSenderColor_Ret(sender, chan);
        }
        if (message && GetMessageColor_Ret) {
            GetMessageColor_Ret(message, chan);
        }
    }

    bool Chat::GetIsTyping() {
        return IsTyping_FrameId && *IsTyping_FrameId != 0;
    }

    bool Chat::SendChat(char channel, const wchar_t *msg) {
        if (!(SendChat_Func && msg && *msg && GetChannel(channel) != Channel::CHANNEL_UNKNOW))
            return false;

        wchar_t buffer[140];

        // We could take 140 char long, but the chat only allow 120 ig.
        size_t len = wcslen(msg);
        len = len > 120 ? 120 : len;

        buffer[0] = static_cast<wchar_t>(channel);
        wcsncpy(&buffer[1], msg, len);
        buffer[len + 1] = 0;
        uint32_t packet[] = { (uint32_t)buffer, 0 };
        return GW::UI::SendUIMessage(UI::UIMessage::kSendChatMessage, packet);
    }

    bool Chat::SendChat(char channel, const char *msg) {
        wchar_t buffer[140];
        int written = swprintf(buffer, _countof(buffer), L"%S", msg);
        if (!(written > 0 && written < 140))
            return false;
        buffer[written] = 0;
        return SendChat(channel, buffer);
    }

    bool Chat::SendChat(const wchar_t *from, const wchar_t *msg) {
        wchar_t buffer[140];
        if (!(SendChat_Func && from && *from && msg && *msg))
            return false;
        int written = swprintf(buffer, _countof(buffer), L"\"%s,%s", from, msg);
        if (!(written > 0 && written < 140))
            return false;
        buffer[written] = 0;
        uint32_t packet[] = { (uint32_t)buffer, 0 };
        return GW::UI::SendUIMessage(UI::UIMessage::kSendChatMessage, packet);
    }

    bool Chat::SendChat(const char *from, const char *msg) {
        GWCA_ASSERT(SendChat_Func);
        wchar_t buffer[140];
        if (!(SendChat_Func && from && *from && msg && *msg))
            return false;
        int written = swprintf(buffer, _countof(buffer), L"\"%S,%S", from, msg);
        if (!(written > 0 && written < 140))
            return false;
        buffer[written] = 0;
        uint32_t packet[] = { (uint32_t)buffer, 0 };
        return GW::UI::SendUIMessage(UI::UIMessage::kSendChatMessage, packet);
    }

    // Change to WriteChatF(Channel chan, const wchar_t *from, const wchar_t *frmt, ..)
    // and       WriteChat(Channel chan, const wchar_t *from, const wchar_t *msg)

    void Chat::WriteChatF(Channel channel, const wchar_t* format, ...) {
        va_list vl;
        va_start(vl, format);
        size_t szbuf = vswprintf(NULL,0,format, vl) + 1;
        wchar_t* chat = new wchar_t[szbuf];
        vswprintf(chat, szbuf, format, vl);
        va_end(vl);

        WriteChat(channel, chat);
        delete[] chat;
    }



    void Chat::WriteChat(Channel channel, const wchar_t *message_unencoded, const wchar_t *sender_unencoded, bool transient) {
        size_t len = wcslen(message_unencoded) + 4;
        wchar_t* message_encoded = new wchar_t[len];
        GWCA_ASSERT(swprintf(message_encoded, len, L"\x108\x107%s\x1", message_unencoded) >= 0);
        wchar_t* sender_encoded = 0;
        if (sender_unencoded) {
            len = wcslen(sender_unencoded) + 4;
            sender_encoded = new wchar_t[len];
            GWCA_ASSERT(swprintf(sender_encoded, len, L"\x108\x107%s\x1", sender_unencoded) >= 0);
        }
        WriteChatEnc(channel, message_encoded, sender_encoded, transient);
        delete[] message_encoded;
        if(sender_encoded)
            delete[] sender_encoded;
    }
    void Chat::WriteChatEnc(Channel channel, const wchar_t* message_encoded, const wchar_t* sender_encoded, bool) {
        UI::UIChatMessage param;
        param.channel = param.channel2 = channel;
        param.message = (wchar_t*)message_encoded;
        bool delete_message = false;
        if (sender_encoded) {
            // If message contains link (<a=1>), manually create the message string
            const wchar_t* format = L"\x76b\x10a%s\x1\x10b%s\x1";
            size_t len = wcslen(message_encoded) + wcslen(sender_encoded) + 6;
            bool has_link_in_message = wcsstr(message_encoded, L"<a=1>") != 0;
            bool has_markup = has_link_in_message || wcsstr(message_encoded, L"<c=") != 0;
            if (has_markup) {
                // NB: When not using this method, any skill templates etc are NOT rendered by the game
                if (has_link_in_message) {
                    format = L"\x108\x107<a=2>\x1\x2%s\x2\x108\x107</a>\x1\x2\x108\x107: \x1\x2%s";
                }
                else {
                    format = L"\x108\x107<a=1>\x1\x2%s\x2\x108\x107</a>\x1\x2\x108\x107: \x1\x2%s";
                }
                len += 19;
            }
            param.message = new wchar_t[len];
            delete_message = true;
            GWCA_ASSERT(swprintf(param.message, len, format, sender_encoded, message_encoded) >= 0);
        }
        UI::SendUIMessage(UI::UIMessage::kWriteToChatLog, &param);
        if (delete_message)
            delete[] param.message;
    }

    void Chat::CreateCommand(const wchar_t* cmd, Chat::ChatCommandCallback callback) {
        ChatCommandCallbackHandler h;
        h.voidcb = callback;
        chat_command_hook_entries[cmd] = h;
    }
    void Chat::CreateCommand(const wchar_t* cmd, Chat::BoolChatCommandCallback callback) {
        ChatCommandCallbackHandler h; 
        h.boolcb = callback;
        chat_command_hook_entries[cmd] = h;
    }
    void Chat::DeleteCommand(const wchar_t* cmd) {
        const auto found = chat_command_hook_entries.find(cmd);
        if (found != chat_command_hook_entries.end())
            chat_command_hook_entries.erase(found);
    }

    void Chat::ToggleTimestamps(bool enable) {
        ShowTimestamps = enable;
        if (ShowTimestamps)
            UI::SetPreference(UI::FlagPreference::ShowChatTimestamps, 0);
    }

    void Chat::SetTimestampsFormat(bool use_24h, bool show_timestamp_seconds) {
        Timestamp_24hFormat = use_24h;
        Timestamp_seconds = show_timestamp_seconds;
    }

    void Chat::SetTimestampsColor(Color color) {
        TimestampsColor = color;
    }

} // namespace GW
