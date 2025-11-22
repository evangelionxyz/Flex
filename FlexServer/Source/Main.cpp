#include <chrono>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "net/BufferStream.hpp"
#include "net/Server.hpp"
#include "net/ServerPacket.hpp"

#include "Physics/JoltPhysics.h"
#include "Scene/Scene.h"
#include "Scene/Serializer.h"
#include "Scene/Components.h"
#include "GameTest/PlayerController.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace ignite;
using namespace flex;

namespace
{
    constexpr uint16_t kDefaultPort = 8192;
    constexpr double kFixedDeltaSeconds = 1.0 / 60.0;

    struct ClientInputState
    {
        PlayerController::NetworkInputState input;
    };

    std::mutex g_InputMutex;
    std::unordered_map<uint32_t, ClientInputState> g_ClientInputs;

    std::mutex g_ScratchMutex;
    net::Buffer g_ScratchBuffer;

    Ref<Scene> g_RuntimeScene;
    entt::entity g_PlayerEntity = entt::null;
    PlayerController* g_PlayerController = nullptr;
    std::optional<uint32_t> g_ControlClient;

    void BroadcastPhysicsState(net::Server& server)
    {
        if (!g_RuntimeScene || !g_RuntimeScene->joltPhysicsScene)
        {
            return;
        }

        struct SnapshotEntry
        {
            uint64_t uuid;
            glm::vec3 position;
            glm::quat rotation;
            glm::vec3 velocity;
        };

        std::vector<SnapshotEntry> entries;
        auto view = g_RuntimeScene->registry->view<TagComponent, RigidbodyComponent>();
        for (auto entity : view)
        {
            const auto& tag = view.get<TagComponent>(entity);
            auto& rb = view.get<RigidbodyComponent>(entity);
            if (rb.isStatic || rb.bodyID.IsInvalid())
            {
                continue;
            }

            SnapshotEntry entry;
            entry.uuid = static_cast<uint64_t>(tag.uuid);
            entry.position = g_RuntimeScene->joltPhysicsScene->GetPosition(rb.bodyID);
            entry.rotation = g_RuntimeScene->joltPhysicsScene->GetRotation(rb.bodyID);
            entry.velocity = g_RuntimeScene->joltPhysicsScene->GetLinearVelocity(rb.bodyID);
            entries.push_back(entry);
        }

        if (entries.empty())
        {
            return;
        }

        std::lock_guard sendLock(g_ScratchMutex);
        net::BufferStreamWriter writer(g_ScratchBuffer);
        writer.SetStreamPosition(0);
        writer.WriteRaw(net::PacketType::PhysicsState);
        writer.WriteRaw<uint32_t>(static_cast<uint32_t>(entries.size()));

        for (const SnapshotEntry& entry : entries)
        {
            writer.WriteRaw(entry.uuid);
            writer.WriteRaw(entry.position);
            writer.WriteRaw(entry.rotation.x);
            writer.WriteRaw(entry.rotation.y);
            writer.WriteRaw(entry.rotation.z);
            writer.WriteRaw(entry.rotation.w);
            writer.WriteRaw(entry.velocity);
        }

        server.SendBufferToAllClients(writer.GetBuffer(), 0, false);
    }

    void OnClientConnected(const net::ClientInfo& clientInfo)
    {
        {
            std::lock_guard lock(g_InputMutex);
            g_ClientInputs[static_cast<uint32_t>(clientInfo.ID)] = ClientInputState{};
            if (!g_ControlClient.has_value())
            {
                g_ControlClient = static_cast<uint32_t>(clientInfo.ID);
            }
        }

        std::cout << "Client connected: " << clientInfo.ID << std::endl;
    }

    void OnClientDisconnected(const net::ClientInfo& clientInfo)
    {
        std::lock_guard lock(g_InputMutex);
        g_ClientInputs.erase(static_cast<uint32_t>(clientInfo.ID));
        if (g_ControlClient == static_cast<uint32_t>(clientInfo.ID))
        {
            g_ControlClient.reset();
        }

        std::cout << "Client disconnected: " << clientInfo.ID << std::endl;
    }

    void OnDataReceived(const net::ClientInfo& clientInfo, const net::Buffer buffer)
    {
        net::BufferStreamReader reader(buffer);
        net::PacketType packetType;
        if (!reader.ReadRaw(packetType))
        {
            return;
        }

        switch (packetType)
        {
        case net::PacketType::PlayerInput:
        {
            float moveX = 0.0f;
            float moveY = 0.0f;
            float yawDelta = 0.0f;
            uint8_t jump = 0;
            uint8_t fireLeft = 0;
            uint8_t fireRight = 0;

            reader.ReadRaw(moveX);
            reader.ReadRaw(moveY);
            reader.ReadRaw(yawDelta);
            reader.ReadRaw(jump);
            reader.ReadRaw(fireLeft);
            reader.ReadRaw(fireRight);

            PlayerController::NetworkInputState input;
            input.moveAxes = { moveX, moveY };
            input.yawDelta = yawDelta;
            input.jump = jump != 0;
            input.fireLeft = fireLeft != 0;
            input.fireRight = fireRight != 0;

            std::lock_guard lock(g_InputMutex);
            auto it = g_ClientInputs.find(static_cast<uint32_t>(clientInfo.ID));
            if (it != g_ClientInputs.end())
            {
                auto& state = it->second.input;
                state.moveAxes = input.moveAxes;
                state.jump = input.jump;
                state.fireLeft = input.fireLeft;
                state.fireRight = input.fireRight;
                state.yawDelta += input.yawDelta;
            }
            break;
        }
        default:
            break;
        }
    }
}

int main(int argc, char** argv)
{
    std::filesystem::path scenePath;
    if (argc > 1)
    {
        scenePath = argv[1];
    }
    else
    {
        scenePath = std::filesystem::current_path() / "../Flex/Resources/scenes/test_scene.json";
    }

    scenePath = std::filesystem::weakly_canonical(scenePath);

    g_ScratchBuffer.Allocate(64 * 1024);

    JoltPhysics::Init();

    SceneLoadOptions loadOptions;
    loadOptions.loadGraphics = false;
    loadOptions.loadAudio = false;

    g_RuntimeScene = CreateRef<Scene>();
    SceneSerializer serializer(g_RuntimeScene, loadOptions);
    if (!serializer.Deserialize(scenePath))
    {
        std::cerr << "Failed to load scene: " << scenePath << std::endl;
        JoltPhysics::Shutdown();
        return 1;
    }

    entt::entity playerEntity = g_RuntimeScene->GetEntityByName("Player");
    if (playerEntity == entt::null)
    {
        std::cerr << "Player entity not found in scene." << std::endl;
    }
    else
    {
        if (!g_RuntimeScene->HasComponent<NativeScriptComponent>(playerEntity))
        {
            g_RuntimeScene->AddComponent<NativeScriptComponent>(playerEntity);
        }
        auto& nsc = g_RuntimeScene->GetComponent<NativeScriptComponent>(playerEntity);
        nsc.Bind<PlayerController>();
        g_PlayerEntity = playerEntity;
    }

    g_RuntimeScene->Start();

    if (g_PlayerEntity != entt::null)
    {
        auto& nsc = g_RuntimeScene->GetComponent<NativeScriptComponent>(g_PlayerEntity);
        g_PlayerController = dynamic_cast<PlayerController*>(nsc.instance);
        if (g_PlayerController)
        {
            g_PlayerController->EnableNetworkInput(true);
        }
    }

    net::Server server(kDefaultPort);
    server.SetClientConnectedCallback(OnClientConnected);
    server.SetClientDisconnectedCallback(OnClientDisconnected);
    server.SetDataReceivedCallback(OnDataReceived);
    server.Start();

    auto previousTime = std::chrono::steady_clock::now();
    double accumulator = 0.0;

    while (true)
    {
        const auto now = std::chrono::steady_clock::now();
        const double frameTime = std::chrono::duration<double>(now - previousTime).count();
        previousTime = now;
        accumulator += frameTime;

        PlayerController::NetworkInputState pendingInput;
        bool hasInput = false;
        {
            std::lock_guard lock(g_InputMutex);
            if (g_ControlClient && g_PlayerController)
            {
                auto it = g_ClientInputs.find(*g_ControlClient);
                if (it != g_ClientInputs.end())
                {
                    pendingInput = it->second.input;
                    it->second.input.yawDelta = 0.0f;
                    hasInput = true;
                }
            }
        }

        while (accumulator >= kFixedDeltaSeconds)
        {
            if (g_PlayerController && hasInput)
            {
                g_PlayerController->ApplyNetworkInput(pendingInput);
                pendingInput.yawDelta = 0.0f;
            }

            g_RuntimeScene->Update(static_cast<float>(kFixedDeltaSeconds));
            BroadcastPhysicsState(server);
            accumulator -= kFixedDeltaSeconds;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (g_RuntimeScene && g_RuntimeScene->IsPlaying())
    {
        g_RuntimeScene->Stop();
    }

    g_RuntimeScene.reset();

    JoltPhysics::Shutdown();
    return 0;
}
