#include "stdafx.h"

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/TextParser.h>

#include <GWCA/Utilities/Debug.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Macros.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/GameContainers/Array.h>

#include <GWCA/Managers/Module.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/CtoSMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/RenderMgr.h>

namespace {
    using namespace GW;

    typedef void (__cdecl *SendUIMessage_pt)(UI::UIMessage msgid, void *wParam, void *lParam);
    SendUIMessage_pt SendUIMessage_Func = 0;
    SendUIMessage_pt RetSendUIMessage = 0;

    struct TooltipObj {
        UI::TooltipInfo* tooltip;
    };

    typedef void(__cdecl* SetTooltip_pt)(UI::TooltipInfo** tooltip);
    SetTooltip_pt SetTooltip_Func = 0;
    SetTooltip_pt RetSetTooltip = 0;


    typedef uint32_t(__cdecl* CreateUIComponent_pt)(uint32_t frame_id, uint32_t component_flags, uint32_t tab_index, void* event_callback, wchar_t* name_enc, wchar_t* component_label);
    CreateUIComponent_pt CreateUIComponent_Func = 0;
    CreateUIComponent_pt CreateUIComponent_Ret = 0;
    std::unordered_map<HookEntry*, UI::CreateUIComponentCallback> OnCreateUIComponent_callbacks;


    uint32_t __cdecl OnCreateUIComponent(uint32_t frame_id, uint32_t component_flags, uint32_t tab_index, void* event_callback, wchar_t* name_enc, wchar_t* component_label) {
        GW::Hook::EnterHook();
        UI::CreateUIComponentPacket packet = {frame_id,component_flags, tab_index, event_callback, name_enc, component_label};
        for (auto& it : OnCreateUIComponent_callbacks) {
            it.second(&packet);
        }
        uint32_t out = CreateUIComponent_Ret(packet.frame_id,packet.component_flags,packet.tab_index,packet.event_callback,packet.name_enc,packet.component_label);
        GW::Hook::LeaveHook();
        return out;
    }

    typedef void(__cdecl* SetWindowVisible_pt)(uint32_t window_id, uint32_t is_visible, void* wParam, void* lParam);
    SetWindowVisible_pt SetWindowVisible_Func = 0;

    typedef void(__cdecl* SetVolume_pt)(uint32_t volume_id, float amount);
    SetVolume_pt SetVolume_Func = 0;

    typedef void(__cdecl* SetMasterVolume_pt)(float amount);
    SetMasterVolume_pt SetMasterVolume_Func = 0;


    typedef void(__cdecl* SetFloatingWindowVisible_pt)(uint32_t unk, uint32_t window_id, uint32_t visible, void* unk1, void* unk2, void* unk3);
    SetFloatingWindowVisible_pt SetFloatingWindowVisible_Func = 0;
    SetFloatingWindowVisible_pt RetSetFloatingWindowVisible = 0;

    typedef void(__cdecl* SetWindowPosition_pt)(uint32_t window_id, UI::WindowPosition* info, void* wParam, void* lParam);
    SetWindowPosition_pt SetWindowPosition_Func = 0;

    typedef void(__fastcall* DoAction_pt)(void* ecx, void* edx, uint32_t msgid, void* arg1, void* arg2);
    DoAction_pt DoAction_Func = 0;
    DoAction_pt RetDoAction = 0;

    typedef void(__cdecl* DrawOnCompass_pt)(uint32_t session_id, uint32_t pt_count, uint32_t* pts);
    DrawOnCompass_pt DrawOnCompass_Func = 0;

    struct KeypressPacket {
        uint32_t key = 0;
        uint32_t unk1 = 0x4000;
        uint32_t unk2 = 0;
    };
    GW::Array<uintptr_t>* s_FrameCache = nullptr;
    static uintptr_t GetActionContext()
    {
        if (!(s_FrameCache && s_FrameCache->size() > 1))
            return 0;
        return (*s_FrameCache)[1] + 0xA0;
    }

    typedef void (__cdecl *LoadSettings_pt)(uint32_t size, uint8_t *data);
    LoadSettings_pt LoadSettings_Func = 0;


    typedef void(__cdecl* ValidateAsyncDecodeStr_pt)(const wchar_t* s, GW::UI::DecodeStr_Callback cb, void* wParam);
    typedef uint32_t(__fastcall* DoAsyncDecodeStr_pt)(void* ecx, void* edx, wchar_t* encoded_str, GW::UI::DecodeStr_Callback cb, void* wParam);
    ValidateAsyncDecodeStr_pt ValidateAsyncDecodeStr = 0;
    // NB: This is a __thiscall, but the function that calls it is a __cdecl - we can't hook it because theres not enough room but would be nice.
    DoAsyncDecodeStr_pt AsyncDecodeStringPtr = 0;
    DoAsyncDecodeStr_pt RetAsyncDecodeStr = 0;


    bool open_links = false;
    HookEntry open_template_hook;

    uintptr_t CommandAction_Addr = 0;
    uintptr_t GameSettings_Addr;
    uintptr_t ui_drawn_addr;
    uintptr_t shift_screen_addr;
    uintptr_t WorldMapState_Addr;

    UI::TooltipInfo*** CurrentTooltipPtr = 0;

    // Get command line parameters that were assigned when GW started
    typedef bool (__cdecl *GetFlagParameter_pt)(uint32_t flag_parameter_id);
    GetFlagParameter_pt GetFlagParameter_Func = 0;
    typedef wchar_t* (__cdecl *GetStringParameter_pt)(uint32_t string_parameter_id_plus_0x27);
    GetStringParameter_pt GetStringParameter_Func = 0;

    // Get in-game preferences assigd during gameplay
    typedef bool (__cdecl *GetFlagPreference_pt)(uint32_t flag_pref_id);
    GetFlagPreference_pt GetFlagPreference_Func = 0;
    typedef void (__cdecl *SetFlagPreference_pt)(uint32_t flag_pref_id, bool value);
    SetFlagPreference_pt SetFlagPreference_Func = 0;

    typedef wchar_t* (__cdecl *GetStringPreference_pt)(uint32_t string_pref_id);
    GetStringPreference_pt GetStringPreference_Func = 0;
    typedef void (__cdecl *SetStringPreference_pt)(uint32_t string_pref_id, wchar_t* value);
    SetStringPreference_pt SetStringPreference_Func = 0;

    typedef uint32_t (__cdecl *GetEnumPreference_pt)(uint32_t choice_pref_id);
    GetEnumPreference_pt GetEnumPreference_Func = 0;
    typedef void (__cdecl *SetEnumPreference_pt)(uint32_t choice_pref_id, uint32_t value);
    SetEnumPreference_pt SetEnumPreference_Func = 0;

    typedef uint32_t (__cdecl *GetNumberPreference_pt)(uint32_t number_pref_id);
    GetNumberPreference_pt GetNumberPreference_Func = 0;
    typedef void (__cdecl *SetNumberPreference_pt)(uint32_t number_pref_id, uint32_t value);
    SetNumberPreference_pt SetNumberPreference_Func = 0;

    UI::WindowPosition* window_positions_array = 0;
    UI::FloatingWindow* floating_windows_array = 0;

    static void OnOpenTemplate_UIMessage(HookStatus *hook_status, UI::UIMessage msgid, void *wParam, void *)
    {
        GWCA_ASSERT(msgid == UI::UIMessage::kOpenTemplate && wParam);
        UI::ChatTemplate *info = static_cast<UI::ChatTemplate *>(wParam);
        if (!(open_links && info && info->code.valid() && info->name))
            return;
        if (!wcsncmp(info->name, L"http://", 7) || !wcsncmp(info->name, L"https://", 8)) {
            hook_status->blocked = true;
            ShellExecuteW(NULL, L"open", info->name, NULL, NULL, SW_SHOWNORMAL);
        }
    }
    // Callbacks are triggered by weighting
    struct CallbackEntry {
        int altitude;
        HookEntry* entry;
        UI::UIMessageCallback callback;
    };
    std::map<UI::UIMessage,std::vector<CallbackEntry>> UIMessage_callbacks;

    static void __cdecl OnSendUIMessage(UI::UIMessage msgid, void *wParam, void *lParam)
    {
        HookBase::EnterHook();
        UI::SendUIMessage(msgid, wParam, lParam);
        HookBase::LeaveHook();
    }

    std::unordered_map<HookEntry*, UI::KeyCallback> OnKeydown_callbacks;
    std::unordered_map<HookEntry*, UI::KeyCallback> OnKeyup_callbacks;
    static void __fastcall OnDoAction(void* ecx, void* edx, uint32_t action_type, void* arg1, void* arg2) {
        HookBase::EnterHook();
        switch (action_type) {
        case 0x1E: // Keydown
        case 0x20: // Keyup
        {
            GW::HookStatus status;
            uint32_t key_pressed = *(uint32_t*)arg1;
            auto* callbacks = action_type == 0x1e ? &OnKeydown_callbacks : &OnKeyup_callbacks;
            for (auto& it : *callbacks) {
                it.second(&status, key_pressed);
                ++status.altitude;
            }
            if (!status.blocked)
                RetDoAction(ecx, edx, action_type, arg1, arg2);
        }
            break;
        default:
            RetDoAction(ecx, edx, action_type, arg1, arg2);
            break;
        }
        HookBase::LeaveHook();
    }

    struct AsyncBuffer {
        void *buffer;
        size_t size;
    };

    void __cdecl __callback_copy_char(void *param, wchar_t *s) {
        GWCA_ASSERT(param && s);
        AsyncBuffer *abuf = (AsyncBuffer *)param;
        char *outstr = (char *)abuf->buffer;
        for (size_t i = 0; i < abuf->size; i++) {
            outstr[i] = s[i] & 0x7F;
            if (!s[i]) break;
        }
        delete abuf;
    }

    void __cdecl __callback_copy_wchar(void *param, wchar_t *s) {
        GWCA_ASSERT(param && s);
        AsyncBuffer *abuf = (AsyncBuffer *)param;
        wcsncpy((wchar_t *)abuf->buffer, s, abuf->size);
        delete abuf;
    }

    void __cdecl __calback_copy_wstring(void *param, wchar_t *s) {
        GWCA_ASSERT(param && s);
        std::wstring *str = (std::wstring *)param;
        *str = s;
    }



    void Init() {
        uintptr_t address;

        uintptr_t FrameCache_addr = Scanner::Find("\x68\x00\x10\x00\x00\x8B\x1C\x98\x8D", "xxxxxxxxx", -4);
        if (Verify(FrameCache_addr))
            s_FrameCache = *(GW::Array<uintptr_t>**)FrameCache_addr;

        address = Scanner::Find("\x81\x0D\xFF\xFF\xFF\xFF\x00\x00\x08\x00", "xx????xxxx", 2);
        if (Verify(address))
            WorldMapState_Addr = *(uintptr_t*)address;

        DoAction_Func = (DoAction_pt)Scanner::Find("\x8B\x75\x08\x57\x8B\xF9\x83\xFE\x09\x75", "xxxxxxxxxx", -0x4);

        SendUIMessage_Func = (SendUIMessage_pt)Scanner::Find(
            "\xE8\x00\x00\x00\x00\x5D\xC3\x89\x45\x08\x5D\xE9", "x????xxxxxxx", -0x1A);

        LoadSettings_Func = (LoadSettings_pt)Scanner::Find(
            "\xE8\x00\x00\x00\x00\xFF\x75\x0C\xFF\x75\x08\x6A\x00", "x????xxxxxxxx", -0x1E);

        address = Scanner::FindAssertion(R"(p:\code\gw\ui\uiroot.cpp)", "!s_count++", -0xD);
        if (Verify(address))
            ui_drawn_addr = *(uintptr_t*)address - 0x10;

        address = Scanner::Find(
            "\x75\x19\x6A\x00\xC7\x05\x00\x00\x00\x00\x01\x00", "xxxxxx????xx", +6);
        if (Verify(address))
            shift_screen_addr = *(uintptr_t *)address;

        address = GW::Scanner::Find("\x83\x0e\x02\x57\x6a\x2a", "xxxxxx", -0x2c); // Initialise preferences function start
        if (address) {
            GetFlagParameter_Func = (GetFlagParameter_pt)GW::Scanner::FunctionFromNearCall(address + 0xf);
            GetStringParameter_Func = (GetStringParameter_pt)GW::Scanner::FunctionFromNearCall(address + 0x32);

            GetStringPreference_Func = (GetStringPreference_pt)GW::Scanner::FunctionFromNearCall(address + 0x5c);
            GetFlagPreference_Func = (GetFlagPreference_pt)GW::Scanner::FunctionFromNearCall(address + 0x10b);
            GetEnumPreference_Func = (GetEnumPreference_pt)GW::Scanner::FunctionFromNearCall(address + 0x118);
            GetNumberPreference_Func = (GetNumberPreference_pt)GW::Scanner::FunctionFromNearCall(address + 0x13f);
        }

        //TODO: RVA fix
        //address = GW::Scanner::Find("\x50\x68\x50\x00\x00\x10", "xxxxxx", -0x3f);
        //SetStringPreference_Func = (SetStringPreference_pt)GW::Scanner::FunctionFromNearCall(address);

        address = GW::Scanner::FindAssertion(R"(p:\code\gw\ui\dialog\dlgoptgr.cpp)", "No valid case for switch variable 'quality'");
        SetEnumPreference_Func = (SetEnumPreference_pt)GW::Scanner::FunctionFromNearCall(address - 0x84);
        SetFlagPreference_Func = (SetFlagPreference_pt)GW::Scanner::FunctionFromNearCall(address - 0x3b);
        SetNumberPreference_Func = (SetNumberPreference_pt)GW::Scanner::FunctionFromNearCall(address - 0x61);

        address = GW::Scanner::FindAssertion(R"(p:\code\engine\frame\frtip.cpp)", "CMsg::Validate(id)");
        if(address)
            address = GW::Scanner::FindInRange("\x56\x8B\xF7", "xxx", -0x13, address, address - 0x200);
        if (address) {
            SetTooltip_Func = (SetTooltip_pt)address;
            address += 0x9;
            CurrentTooltipPtr = (UI::TooltipInfo***)(*(uintptr_t*)address);
        }

        address = Scanner::Find("\x8D\x4B\x28\x89\x73\x24\x8B\xD7", "xxxxxxx", +0x10);
        if (Verify(address))
            GameSettings_Addr = *(uintptr_t*)address;

        // NB: 0x39 is the size of the floating window array
        SetFloatingWindowVisible_Func = (SetFloatingWindowVisible_pt)Scanner::Find("\x8B\x75\x0C\x57\x83\xFE\x39", "xxxxxxx", -0x5);
        if (Verify(SetFloatingWindowVisible_Func)) {
            //HookBase::CreateHook(SetFloatingWindowVisible_Func, OnToggleFloatingWindow, (void**)&RetSetFloatingWindowVisible);
            address = (uintptr_t)SetFloatingWindowVisible_Func + 0x41;
            if (Verify(address)) {
                address = *(uintptr_t*)address;
                floating_windows_array = reinterpret_cast<UI::FloatingWindow*>(address - 0x10);
            }
        }

        // NB: 0x66 is the size of the window info array
        SetWindowVisible_Func = (SetWindowVisible_pt)Scanner::Find("\x8B\x75\x08\x83\xFE\x66\x7C\x19\x68", "xxxxxxxxx", -0x7);
        if (SetWindowVisible_Func) {
            SetWindowPosition_Func = reinterpret_cast<SetWindowPosition_pt>((uintptr_t)SetWindowVisible_Func - 0xE0);
            address = (uintptr_t)SetWindowVisible_Func + 0x49;
            if (Verify(address)) {
                address = *(uintptr_t*)address;
                window_positions_array = reinterpret_cast<UI::WindowPosition*>(address);
            }
        }

        ValidateAsyncDecodeStr = (ValidateAsyncDecodeStr_pt)Scanner::Find("\x83\xC4\x10\x3B\xC6\x5E\x74\x14", "xxxxxxxx", -0x70);
        AsyncDecodeStringPtr = (DoAsyncDecodeStr_pt)Scanner::Find("\x8b\x47\x14\x8d\x9f\x80\xfe\xff\xff", "xxxxxxxxx", -0x8);

        // NB: "p:\\code\\engine\\sound\\sndmain.cpp", "(unsigned)type < arrsize(s_volume)" works, but also matches SetVolume()
        SetVolume_Func = (SetVolume_pt)GW::Scanner::Find("\x8b\x75\x08\x83\xfe\x05\x72\x14\x68\x5b\x04\x00\x00\xba", "xxxxxxxxxxxxxx", -0x4);

        SetMasterVolume_Func = (SetMasterVolume_pt)GW::Scanner::Find("\xd9\x45\x08\x83\xc6\x1c\x83\xef\x01\x75\xea\x5f\xdd\xd8\x5e\x5d", "xxxxxxxxxxxxxxxx", -0x4b);
        DrawOnCompass_Func = (DrawOnCompass_pt)GW::Scanner::FindAssertion(R"(p:\code\gw\char\charmsg.cpp)", "knotCount <= arrsize(message.knotData)",-0x2e);

        CreateUIComponent_Func = (CreateUIComponent_pt)GW::Scanner::Find("\x33\xd2\x89\x45\x08\xb9\xac\x01\x00\x00", "xxxxxxxxxx", -0x27);

        GWCA_INFO("[SCAN] FrameCache_addr = %p", FrameCache_addr);
        GWCA_INFO("[SCAN] WorldMapState_Addr = %p", WorldMapState_Addr);
        GWCA_INFO("[SCAN] DoAction = %p", DoAction_Func);
        GWCA_INFO("[SCAN] SendUIMessage = %p", SendUIMessage_Func);
        GWCA_INFO("[SCAN] LoadSettings = %p", LoadSettings_Func);
        GWCA_INFO("[SCAN] ui_drawn_addr = %p", ui_drawn_addr);
        GWCA_INFO("[SCAN] shift_screen_addr = %p", shift_screen_addr);
        GWCA_INFO("[SCAN] GetStringPreference_Func = %p", GetStringPreference_Func);
        GWCA_INFO("[SCAN] GetEnumPreference_Func = %p", GetEnumPreference_Func);
        GWCA_INFO("[SCAN] GetNumberPreference_Func = %p", GetNumberPreference_Func);
        GWCA_INFO("[SCAN] GetFlagPreference_Func = %p", GetFlagPreference_Func);
        GWCA_INFO("[SCAN] SetStringPreference_Func = %p", SetStringPreference_Func);
        GWCA_INFO("[SCAN] SetEnumPreference_Func = %p", SetEnumPreference_Func);
        GWCA_INFO("[SCAN] SetNumberPreference_Func = %p", SetNumberPreference_Func);
        GWCA_INFO("[SCAN] SetFlagPreference_Func = %p", SetFlagPreference_Func);
        GWCA_INFO("[SCAN] SetTooltip_Func = %p", SetTooltip_Func);
        GWCA_INFO("[SCAN] CurrentTooltipPtr = %p", CurrentTooltipPtr);
        GWCA_INFO("[SCAN] GameSettings = %p", GameSettings_Addr);
        GWCA_INFO("[SCAN] SetFloatingWindowVisible_Func = %p", SetFloatingWindowVisible_Func);
        GWCA_INFO("[SCAN] floating_windows_array = %p", floating_windows_array);
        GWCA_INFO("[SCAN] SetWindowVisible_Func = %p", SetWindowVisible_Func);
        GWCA_INFO("[SCAN] SetWindowPosition_Func = %p", SetWindowPosition_Func);
        GWCA_INFO("[SCAN] window_positions_array = %p", window_positions_array);
        GWCA_INFO("[SCAN] ValidateAsyncDecodeStr = %p", ValidateAsyncDecodeStr);
        GWCA_INFO("[SCAN] AsyncDecodeStringPtr = %p", AsyncDecodeStringPtr);
        GWCA_INFO("[SCAN] SetVolume_Func = %p", SetVolume_Func);
        GWCA_INFO("[SCAN] SetMasterVolume_Func = %p", SetMasterVolume_Func);
        GWCA_INFO("[SCAN] DrawOnCompass_Func = %p", DrawOnCompass_Func);
        GWCA_INFO("[SCAN] CreateUIComponent_Func = %p", CreateUIComponent_Func);

#if _DEBUG
        GWCA_ASSERT(GetStringPreference_Func);
        GWCA_ASSERT(GetEnumPreference_Func);
        GWCA_ASSERT(GetNumberPreference_Func);
        GWCA_ASSERT(GetFlagPreference_Func);
        //GWCA_ASSERT(SetStringPreference_Func);
        GWCA_ASSERT(SetEnumPreference_Func);
        GWCA_ASSERT(SetNumberPreference_Func);
        GWCA_ASSERT(SetFlagPreference_Func);
        GWCA_ASSERT(FrameCache_addr);
        GWCA_ASSERT(WorldMapState_Addr);
        GWCA_ASSERT(DoAction_Func);
        GWCA_ASSERT(SendUIMessage_Func);
        GWCA_ASSERT(LoadSettings_Func);
        GWCA_ASSERT(ui_drawn_addr);
        GWCA_ASSERT(shift_screen_addr);
        GWCA_ASSERT(SetTooltip_Func);
        GWCA_ASSERT(CurrentTooltipPtr);
        GWCA_ASSERT(GameSettings_Addr);
        GWCA_ASSERT(floating_windows_array);
        GWCA_ASSERT(SetWindowVisible_Func);
        GWCA_ASSERT(SetWindowPosition_Func);
        GWCA_ASSERT(window_positions_array);
        GWCA_ASSERT(ValidateAsyncDecodeStr);
        GWCA_ASSERT(AsyncDecodeStringPtr);
        GWCA_ASSERT(SetVolume_Func);
        GWCA_ASSERT(SetMasterVolume_Func);
        GWCA_ASSERT(DrawOnCompass_Func);
        GWCA_ASSERT(CreateUIComponent_Func);
#endif
        HookBase::CreateHook(SendUIMessage_Func, OnSendUIMessage, (void **)&RetSendUIMessage);
        HookBase::CreateHook(DoAction_Func, OnDoAction, (void**)&RetDoAction);
        HookBase::CreateHook(CreateUIComponent_Func, OnCreateUIComponent, (void**)&CreateUIComponent_Ret);

    }

    void EnableHooks() {
        if (AsyncDecodeStringPtr)
            HookBase::EnableHooks(AsyncDecodeStringPtr);
        if (DoAction_Func)
            HookBase::EnableHooks(DoAction_Func);
        if (SetTooltip_Func)
            HookBase::EnableHooks(SetTooltip_Func);
        if (SendUIMessage_Func)
            HookBase::EnableHooks(SendUIMessage_Func);
        if (CreateUIComponent_Func)
            HookBase::EnableHooks(CreateUIComponent_Func);
        UI::RegisterUIMessageCallback(&open_template_hook, UI::UIMessage::kOpenTemplate, OnOpenTemplate_UIMessage);
    }
    void DisableHooks() {
        UI::RemoveUIMessageCallback(&open_template_hook);
        if (AsyncDecodeStringPtr)
            HookBase::DisableHooks(AsyncDecodeStringPtr);
        if (DoAction_Func)
            HookBase::DisableHooks(DoAction_Func);
        if (SetTooltip_Func)
            HookBase::DisableHooks(SetTooltip_Func);
        if (SendUIMessage_Func)
            HookBase::DisableHooks(SendUIMessage_Func);
        if (CreateUIComponent_Func)
            HookBase::DisableHooks(CreateUIComponent_Func);
    }

    void Exit()
    {
        HookBase::RemoveHook(AsyncDecodeStringPtr);
        HookBase::RemoveHook(DoAction_Func);
        HookBase::RemoveHook(SetTooltip_Func);
        HookBase::RemoveHook(SendUIMessage_Func);
        HookBase::RemoveHook(CreateUIComponent_Func);
    }
}

namespace GW {

    Module UIModule = {
        "UIModule",     // name
        NULL,           // param
        ::Init,         // init_module
        ::Exit,         // exit_module
        ::EnableHooks,           // enable_hooks
        ::DisableHooks,           // disable_hooks
    };
    namespace UI {
        Vec2f WindowPosition::yAxis(float multiplier) const {
            const float h = static_cast<float>(Render::GetViewportHeight());
            Vec2f y;
            float correct;
            switch (state ^ 0x1) {
            case 0x20:
            case 0x24:
            case 0x30:
                y = { h - p1.y * multiplier, h - p2.y * multiplier };
                break;
            case 0x4:
            case 0x10:
            case 0x0:
                correct = (h / 2.f);
                y = { correct - p1.y * multiplier, correct + p2.y * multiplier };
                break;
            default:
                y = { p1.y * multiplier, p2.y * multiplier };
                break;
            }
            return y;
        }
        Vec2f WindowPosition::xAxis(float multiplier) const {
            const auto w = static_cast<float>(Render::GetViewportWidth());
            const auto middle = w / 2.f;
            switch (state ^ 0x1) {
            case 0x10:
            case 0x18:
            case 0x30:
                return { std::roundf(w - p1.x * multiplier), std::roundf(w - p2.x * multiplier) };
            case 0x8:
            case 0x20:
            case 0x0:
                return { std::roundf(middle - p1.x * multiplier), std::roundf(middle + p2.x * multiplier) };
            default:
                return {std::roundf(p1.x * multiplier), std::roundf(p2.x * multiplier)};
            }
        }
        float WindowPosition::top(float multiplier) const {
            return yAxis(multiplier).x;
        }
        float WindowPosition::left(float multiplier) const {
            return xAxis(multiplier).x;
        }
        float WindowPosition::bottom(float multiplier) const {
            return yAxis(multiplier).y;
        }
        float WindowPosition::right(float multiplier) const {
            return xAxis(multiplier).y;
        }

        bool SendUIMessage(UIMessage msgid, void *wParam, void *lParam)
        {
            auto forward_call = [msgid, wParam, lParam]() {
                if (!RetSendUIMessage)
                    return false;
                HookBase::EnterHook();
                RetSendUIMessage(msgid, wParam, lParam);
                HookBase::LeaveHook();
                return true;
            };
            HookStatus status;
            auto found = UIMessage_callbacks.find(msgid);
            if (found == UIMessage_callbacks.end()) {
                return forward_call();
            }

            auto it = found->second.begin();
            auto end = found->second.end();
            // Pre callbacks
            while (it != end) {
                if (it->altitude > 0)
                    break;
                it->callback(&status, msgid, wParam, lParam);
                ++status.altitude;
                it++;
            }

            bool result = !status.blocked && forward_call();

            // Post callbacks
            while (it != end) {
                it->callback(&status, msgid, wParam, lParam);
                ++status.altitude;
                it++;
            }
            return result;
        }
        bool Keydown(ControlAction key) {
            uintptr_t ecx = GetActionContext();
            if (!(ecx && RetDoAction))
                return false;
            KeypressPacket action;
            action.key = key;
            OnDoAction((void*)ecx, 0, 0x1E, &action, 0);
            return true;
        }
        bool Keyup(ControlAction key) {
            uintptr_t ecx = GetActionContext();
            if (!(ecx && RetDoAction))
                return false;
            KeypressPacket action;
            action.key = key;
            OnDoAction((void*)ecx, 0, 0x20, &action, 0);
            return true;
        }

        bool SetWindowVisible(WindowID window_id,bool is_visible) {
            if (!SetWindowVisible_Func || window_id >= WindowID::WindowID_Count)
                return false;
            SetWindowVisible_Func(window_id, is_visible ? 1u : 0u, 0, 0);
            return true;
        }
        bool SetWindowPosition(WindowID window_id, WindowPosition* info) {
            if (!SetWindowPosition_Func || window_id >= WindowID::WindowID_Count)
                return false;
            SetWindowPosition_Func(window_id, info, 0, 0);
            return true;
        }
        WindowPosition* GetWindowPosition(WindowID window_id) {
            if (!window_positions_array || window_id >= WindowID::WindowID_Count)
                return nullptr;
            return &window_positions_array[window_id];
        }

        bool Keypress(ControlAction key) {
            if (!Keydown(key))
                return false;
            GW::GameThread::Enqueue([key]() {
                Keyup(key);
                });
            return true;
        }
        bool DrawOnCompass(unsigned session_id, unsigned pt_count, CompassPoint *pts)
        {
            if (!DrawOnCompass_Func)
                return false;
            uint32_t* pts_conv = (uint32_t*)malloc(pt_count * sizeof(uint32_t) * 2);
            // Legacy code was to pass short* for coordinates direct to CtoS. New hook needs them in int* coordinates, fill with 0xf
            for (unsigned i = 0; i < pt_count; i++) {
                pts_conv[i * 2] = pts[i].x | 0xffff0000;
                pts_conv[i * 2 + 1] = pts[i].y | 0xffff0000;
            }
            DrawOnCompass_Func(session_id, pt_count, pts_conv);
            free(pts_conv);
            return true;
        }

        void LoadSettings(size_t size, uint8_t *data) {
            if (Verify(LoadSettings_Func))
                LoadSettings_Func(size, data);
        }

        ArrayByte* GetSettings() {
            return (ArrayByte *)GameSettings_Addr;
        }

        bool GetIsUIDrawn() {
            uint32_t *ui_drawn = (uint32_t *)ui_drawn_addr;
            if (Verify(ui_drawn))
                return (*ui_drawn == 0);
            else
                return true;
        }
        bool GetIsWorldMapShowing() {
            uint32_t* WorldMapState = (uint32_t*)WorldMapState_Addr;
            if (Verify(WorldMapState))
                return (*WorldMapState & 0x80000) != 0;
            else
                return false;
        }

        bool GetIsShiftScreenShot() {
            uint32_t *shift_screen = (uint32_t *)shift_screen_addr;
            if (Verify(shift_screen))
                return (*shift_screen != 0);
            else
                return false;
        }

        void AsyncDecodeStr(const wchar_t *enc_str, wchar_t *buffer, size_t size) {
            if (!ValidateAsyncDecodeStr)
                return;
            // @Enhancement: Should use a pool of this buffer, but w/e for now
            AsyncBuffer *abuf = new AsyncBuffer;
            abuf->buffer = buffer;
            abuf->size = size;
            ValidateAsyncDecodeStr((wchar_t*)enc_str, __callback_copy_wchar, abuf);
        }

        void AsyncDecodeStr(const wchar_t *enc_str, char *buffer, size_t size) {
            if (!ValidateAsyncDecodeStr)
                return;
            // @Enhancement: Should use a pool of this buffer, but w/e for now
            AsyncBuffer *abuf = new AsyncBuffer;
            abuf->buffer = buffer;
            abuf->size = size;
            ValidateAsyncDecodeStr((wchar_t*)enc_str, __callback_copy_char, abuf);
        }

        void AsyncDecodeStr(const wchar_t* enc_str, DecodeStr_Callback callback, void* callback_param, uint32_t language_id) {
            if (!ValidateAsyncDecodeStr)
                return;
            auto& textParser = GameContext::instance()->text_parser;
            uint32_t prev_language_id = textParser->language_id;
            if (language_id != -1) {
                textParser->language_id = language_id;
            }
            ValidateAsyncDecodeStr((wchar_t*)enc_str, callback, callback_param);
            textParser->language_id = prev_language_id;
        }

        void AsyncDecodeStr(const wchar_t *enc_str, std::wstring *out, uint32_t language_id) {
            if (!ValidateAsyncDecodeStr)
                return;
            auto& textParser = GameContext::instance()->text_parser;
            uint32_t prev_language_id = textParser->language_id;
            if (language_id != -1) {
                textParser->language_id = language_id;
            }
            ValidateAsyncDecodeStr((wchar_t*)enc_str, __calback_copy_wstring, out);
            textParser->language_id = prev_language_id;
        }

#define WORD_BIT_MORE       (0x8000)
#define WORD_VALUE_BASE     (0x100)
#define WORD_VALUE_RANGE    (WORD_BIT_MORE - WORD_VALUE_BASE)
        bool UInt32ToEncStr(uint32_t value, wchar_t *buffer, size_t count) {
            // Each "case" in the array of wchar_t contains a value in the range [0, WORD_VALUE_RANGE)
            // This value is offseted by WORD_VALUE_BASE and if it take more than 1 "case" it set the bytes WORD_BIT_MORE
            size_t case_required = (value + WORD_VALUE_RANGE - 1) / WORD_VALUE_RANGE;
            if (case_required + 1 > count)
                return false;
            buffer[case_required] = 0;
            for (size_t i = case_required - 1; i < case_required; i--) {
                buffer[i] = WORD_VALUE_BASE + (value % WORD_VALUE_RANGE);
                value /= WORD_VALUE_RANGE;
                if (i != case_required - 1)
                    buffer[i] |= WORD_BIT_MORE;
            }
            return true;
        }

        uint32_t EncStrToUInt32(const wchar_t *enc_str) {
            uint32_t val = 0;
            do {
                GWCA_ASSERT(*enc_str >= WORD_VALUE_BASE);
                val *= WORD_VALUE_RANGE;
                val += (*enc_str & ~WORD_BIT_MORE) - WORD_VALUE_BASE;
            } while (*enc_str++ & WORD_BIT_MORE);
            return val;
        }

        void SetOpenLinks(bool toggle)
        {
            open_links = toggle;
        }

        uint32_t GetPreference(EnumPreference pref)
        {
            return GetEnumPreference_Func && pref < EnumPreference::Count ? GetEnumPreference_Func((uint32_t)pref) : 0;
        }
        uint32_t GetPreference(NumberPreference pref)
        {
            return GetNumberPreference_Func && pref < NumberPreference::Count ? GetNumberPreference_Func((uint32_t)pref) : 0;
        }
        wchar_t* GetPreference(StringPreference pref)
        {
            return GetStringPreference_Func && pref < StringPreference::Count ? GetStringPreference_Func((uint32_t)pref) : 0;
        }
        bool GetPreference(FlagPreference pref)
        {
            return GetFlagPreference_Func && pref < FlagPreference::Count ? GetFlagPreference_Func((uint32_t)pref) : 0;
        }
        bool SetPreference(EnumPreference pref, uint32_t value)
        {
            return SetEnumPreference_Func && pref < EnumPreference::Count ? SetEnumPreference_Func((uint32_t)pref, value), true : false;
        }
        bool SetPreference(NumberPreference pref, uint32_t value)
        {
            bool ok = SetNumberPreference_Func && pref < NumberPreference::Count ? SetNumberPreference_Func((uint32_t)pref, value), true : false;
            if (!ok)
                return ok;
            // Set in-game volume
            switch (pref) {
            case NumberPreference::EffectsVolume:
                if(SetVolume_Func) SetVolume_Func(0, (float)((float)value / 100.f));
                break;
            case NumberPreference::DialogVolume:
                if (SetVolume_Func) SetVolume_Func(4, (float)((float)value / 100.f));
                break;
            case NumberPreference::BackgroundVolume:
                if (SetVolume_Func) SetVolume_Func(1, (float)((float)value / 100.f));
                break;
            case NumberPreference::MusicVolume:
                if (SetVolume_Func) SetVolume_Func(3, (float)((float)value / 100.f));
                break;
            case NumberPreference::UIVolume:
                if (SetVolume_Func) SetVolume_Func(2, (float)((float)value / 100.f));
                break;
            case NumberPreference::MasterVolume:
                if(SetMasterVolume_Func) SetMasterVolume_Func((float)((float)value / 100.f));
                break;
            default:
                break;
            }
            return ok;
        }
        bool SetPreference(StringPreference pref, wchar_t* value)
        {
            return SetStringPreference_Func && pref < StringPreference::Count ? SetStringPreference_Func((uint32_t)pref, value), true : false;
        }
        bool SetPreference(FlagPreference pref, bool value)
        {
            return SetFlagPreference_Func && pref < FlagPreference::Count ? SetFlagPreference_Func((uint32_t)pref, value), true : false;
        }

        void RegisterKeyupCallback(HookEntry* entry, const KeyCallback& callback) {
            OnKeyup_callbacks.insert({ entry, callback });
        }
        void RemoveKeyupCallback(HookEntry* entry) {
            auto it = OnKeyup_callbacks.find(entry);
            if (it != OnKeyup_callbacks.end())
                OnKeyup_callbacks.erase(it);
        }

        void RegisterKeydownCallback(HookEntry* entry, const KeyCallback& callback) {
            OnKeydown_callbacks.insert({ entry, callback });
        }
        void RemoveKeydownCallback(HookEntry* entry) {
            auto it = OnKeydown_callbacks.find(entry);
            if (it != OnKeydown_callbacks.end())
                OnKeydown_callbacks.erase(it);
        }

        void RegisterUIMessageCallback(
            HookEntry *entry,
            UIMessage message_id,
            const UIMessageCallback& callback,
            int altitude)
        {
            if (!UIMessage_callbacks.contains(message_id)) {
                UIMessage_callbacks[message_id] = std::vector<CallbackEntry>();
            }
            auto it = UIMessage_callbacks[message_id].begin();
            while (it != UIMessage_callbacks[message_id].end()) {
                if (it->altitude > altitude)
                    break;
                it++;
            }
            UIMessage_callbacks[message_id].insert(it, { altitude, entry, callback});
        }

        void RemoveUIMessageCallback(
            HookEntry *entry)
        {
            for (auto& it : UIMessage_callbacks) {
                auto it2 = it.second.begin();
                while (it2 != it.second.end()) {
                    if (it2->entry == entry) {
                        it.second.erase(it2);
                        return;
                    }
                    it2++;
                }
            }
        }

        TooltipInfo* GetCurrentTooltip() {
            return CurrentTooltipPtr && *CurrentTooltipPtr ? **CurrentTooltipPtr : 0;
        }

        void RegisterCreateUIComponentCallback(HookEntry* entry, const CreateUIComponentCallback& callback)
        {
            if (!OnCreateUIComponent_callbacks.contains(entry))
                OnCreateUIComponent_callbacks[entry] = callback;
        }
        void RemoveCreateUIComponentCallback(HookEntry* entry)
        {
            if (OnCreateUIComponent_callbacks.contains(entry))
                OnCreateUIComponent_callbacks.erase(entry);
        }
    }

} // namespace GW
