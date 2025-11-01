// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef UI_HELPER_H
#define UI_HELPER_H

#include "Renderer/Texture.h"

#include <memory>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

namespace flex
{
    inline void UIDrawImage(const std::shared_ptr<Texture2D> &texture, float width, float height, const std::string &text = "")
    {
        ImTextureID texID = static_cast<ImTextureID>(texture->GetHandle());
        ImGui::Image(texID, {width, height}, {0, 1}, {1, 0});
    
        if (!text.empty())
        {
            ImGui::SameLine(); ImGui::Text("%s", text.c_str());
        }
    }
}


#endif