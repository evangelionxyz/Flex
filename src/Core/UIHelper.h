#pragma once

#include "Renderer/Texture.h"

#include <memory>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

inline void UIDrawImage(const std::shared_ptr<Texture2D> &texture, float width, float height, const std::string &text = "")
{
    ImTextureID texID = static_cast<ImTextureID>(texture->GetHandle());
    ImGui::Image(texID, {width, height}, {0, 1}, {1, 0});

    if (!text.empty())
    {
        ImGui::SameLine(); ImGui::Text(text.c_str());
    }
}