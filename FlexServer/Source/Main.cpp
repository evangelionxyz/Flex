#include <chrono>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <utility>

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
#include <glm/gtc/matrix_transform.hpp>

using namespace ignite;
using namespace flex;

namespace
{
    constexpr uint16_t kDefaultPort = 8192;
    constexpr double kFixedDeltaSeconds = 1.0 / 60.0;

    struct ClientRuntimeState
    {
        PlayerController::NetworkInputState input;
        entt::entity playerEntity = entt::null;
        PlayerController* controller = nullptr;
    };

    std::mutex g_InputMutex;
    std::unordered_map<uint32_t, ClientRuntimeState> g_ClientStates;

    std::mutex g_AssignmentMutex;
    std::vector<uint32_t> g_PendingSpawnRequests;
    std::vector<uint32_t> g_PendingReleaseRequests;

    std::vector<entt::entity> g_AvailablePlayers;

    struct BulletSpawnEvent
    {
        uint64_t bulletUUID = 0;
        uint64_t templateUUID = 0;
        glm::vec3 position{0.0f};
        glm::vec3 velocity{0.0f};
        uint64_t fireSoundUUID = 0;
    };

    std::mutex g_EventMutex;
    std::vector<BulletSpawnEvent> g_PendingBulletEvents;

    std::mutex g_EventBufferMutex;
    net::Buffer g_EventBuffer;

    std::mutex g_ScratchMutex;
    net::Buffer g_ScratchBuffer;

    Ref<Scene> g_RuntimeScene;
    entt::entity g_PlayerTemplate = entt::null;
    TransformComponent g_PlayerSpawnTransform{};
    bool g_HasPlayerSpawnTransform = false;
    uint32_t g_SpawnCounter = 0;

    PlayerController* GetPlayerControllerForEntity(entt::entity entity)
    {
        if (!g_RuntimeScene || entity == entt::null)
        {
            return nullptr;
        }

        if (!g_RuntimeScene->HasComponent<NativeScriptComponent>(entity))
        {
            return nullptr;
        }

        auto& nsc = g_RuntimeScene->GetComponent<NativeScriptComponent>(entity);
        if (!nsc.instance && nsc.InstantiateScript)
        {
            nsc.instance = nsc.InstantiateScript(g_RuntimeScene.get(), entity);
            if (nsc.instance)
            {
                nsc.instance->OnStart();
            }
        }
        return dynamic_cast<PlayerController*>(nsc.instance);
    }

    void ResetPlayerEntity(entt::entity entity)
    {
        if (!g_RuntimeScene || entity == entt::null || !g_HasPlayerSpawnTransform)
        {
            return;
        }

        if (g_RuntimeScene->HasComponent<TransformComponent>(entity))
        {
            g_RuntimeScene->GetComponent<TransformComponent>(entity) = g_PlayerSpawnTransform;
        }

        if (g_RuntimeScene->HasComponent<RigidbodyComponent>(entity) && g_RuntimeScene->joltPhysicsScene)
        {
            auto& rb = g_RuntimeScene->GetComponent<RigidbodyComponent>(entity);
            if (!rb.bodyID.IsInvalid())
            {
                g_RuntimeScene->joltPhysicsScene->SetLinearVelocity(rb.bodyID, glm::vec3(0.0f));
                g_RuntimeScene->joltPhysicsScene->SetAngularVelocity(rb.bodyID, glm::vec3(0.0f));
                g_RuntimeScene->joltPhysicsScene->SetPosition(rb.bodyID, g_PlayerSpawnTransform.position, true);
                const glm::quat rotation = glm::quat(glm::radians(g_PlayerSpawnTransform.rotation));
                g_RuntimeScene->joltPhysicsScene->SetRotation(rb.bodyID, rotation, true);
            }
        }
    }

    void SetupPlayerSpawnTransform(entt::entity entity, uint32_t spawnIndex)
    {
        if (!g_RuntimeScene || entity == entt::null || !g_HasPlayerSpawnTransform)
        {
            return;
        }

        glm::vec3 spawnPosition = g_PlayerSpawnTransform.position;
        constexpr float spacing = 2.5f;
        const uint32_t row = spawnIndex / 4;
        const uint32_t column = spawnIndex % 4;
        spawnPosition += glm::vec3(static_cast<float>(column) * spacing, 0.0f, static_cast<float>(row) * spacing);

        if (g_RuntimeScene->HasComponent<TransformComponent>(entity))
        {
            auto& transform = g_RuntimeScene->GetComponent<TransformComponent>(entity);
            transform = g_PlayerSpawnTransform;
            transform.position = spawnPosition;
        }

        if (g_RuntimeScene->HasComponent<RigidbodyComponent>(entity) && g_RuntimeScene->joltPhysicsScene)
        {
            auto& rb = g_RuntimeScene->GetComponent<RigidbodyComponent>(entity);
            if (rb.bodyID.IsInvalid())
            {
                g_RuntimeScene->joltPhysicsScene->InstantiateEntity(entity);
            }

            if (!rb.bodyID.IsInvalid())
            {
                g_RuntimeScene->joltPhysicsScene->SetLinearVelocity(rb.bodyID, glm::vec3(0.0f));
                g_RuntimeScene->joltPhysicsScene->SetAngularVelocity(rb.bodyID, glm::vec3(0.0f));
                g_RuntimeScene->joltPhysicsScene->SetPosition(rb.bodyID, spawnPosition, true);
                const glm::quat rotation = glm::quat(glm::radians(g_PlayerSpawnTransform.rotation));
                g_RuntimeScene->joltPhysicsScene->SetRotation(rb.bodyID, rotation, true);
            }
        }
    }

    entt::entity AcquirePlayerEntity()
    {
        entt::entity entity = entt::null;
        if (!g_AvailablePlayers.empty())
        {
            entity = g_AvailablePlayers.back();
            g_AvailablePlayers.pop_back();
        }
        else if (g_RuntimeScene && g_PlayerTemplate != entt::null)
        {
            entity = g_RuntimeScene->DuplicateEntity(g_PlayerTemplate);
        }

        if (entity != entt::null)
        {
            SetupPlayerSpawnTransform(entity, g_SpawnCounter++);
        }

        return entity;
    }

    void AssignClientPlayer(uint32_t clientId)
    {
        entt::entity playerEntity = AcquirePlayerEntity();
        if (playerEntity == entt::null)
        {
            std::cerr << "No player entity available for client " << clientId << std::endl;
            return;
        }

        PlayerController* controller = GetPlayerControllerForEntity(playerEntity);
        if (controller)
        {
            controller->EnableNetworkInput(true);
        }
        else
        {
            std::cerr << "PlayerController missing for entity assigned to client " << clientId << std::endl;
        }

        std::lock_guard lock(g_InputMutex);
        auto it = g_ClientStates.find(clientId);
        if (it == g_ClientStates.end())
        {
            if (controller)
            {
                controller->EnableNetworkInput(false);
            }
            g_AvailablePlayers.push_back(playerEntity);
            return;
        }

        it->second.playerEntity = playerEntity;
        it->second.controller = controller;
        it->second.input = {};
    }

    void ReleaseClientPlayer(uint32_t clientId)
    {
        entt::entity playerEntity = entt::null;
        PlayerController* controller = nullptr;

        {
            std::lock_guard lock(g_InputMutex);
            auto it = g_ClientStates.find(clientId);
            if (it == g_ClientStates.end())
            {
                return;
            }

            playerEntity = it->second.playerEntity;
            controller = it->second.controller;
            g_ClientStates.erase(it);
        }

        if (controller)
        {
            controller->EnableNetworkInput(false);
        }

        if (playerEntity != entt::null)
        {
            ResetPlayerEntity(playerEntity);
            g_AvailablePlayers.push_back(playerEntity);
        }
    }

    void ProcessPendingAssignments()
    {
        std::vector<uint32_t> spawnRequests;
        std::vector<uint32_t> releaseRequests;
        {
            std::lock_guard lock(g_AssignmentMutex);
            if (g_PendingSpawnRequests.empty() && g_PendingReleaseRequests.empty())
            {
                return;
            }
            spawnRequests.swap(g_PendingSpawnRequests);
            releaseRequests.swap(g_PendingReleaseRequests);
        }

        for (uint32_t clientId : releaseRequests)
        {
            ReleaseClientPlayer(clientId);
        }

        for (uint32_t clientId : spawnRequests)
        {
            AssignClientPlayer(clientId);
        }
    }

    void BroadcastSpawnEvents(net::Server& server)
    {
        std::vector<BulletSpawnEvent> events;
        {
            std::lock_guard lock(g_EventMutex);
            if (g_PendingBulletEvents.empty())
            {
                return;
            }
            events.swap(g_PendingBulletEvents);
        }

        for (const BulletSpawnEvent& event : events)
        {
            std::lock_guard bufferLock(g_EventBufferMutex);
            net::BufferStreamWriter writer(g_EventBuffer);
            writer.SetStreamPosition(0);
            writer.WriteRaw(net::PacketType::EntitySpawn);
            writer.WriteRaw(event.bulletUUID);
            writer.WriteRaw(event.templateUUID);
            writer.WriteRaw(event.position);
            writer.WriteRaw(event.velocity);
            writer.WriteRaw(event.fireSoundUUID);
            server.SendBufferToAllClients(writer.GetBuffer(), 0, false);
        }
    }

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
        const uint32_t clientId = static_cast<uint32_t>(clientInfo.ID);
        {
            std::lock_guard lock(g_InputMutex);
            g_ClientStates[clientId] = ClientRuntimeState{};
        }
        {
            std::lock_guard lock(g_AssignmentMutex);
            g_PendingSpawnRequests.push_back(clientId);
        }

        std::cout << "Client connected: " << clientInfo.ID << std::endl;
    }

    void OnClientDisconnected(const net::ClientInfo& clientInfo)
    {
        const uint32_t clientId = static_cast<uint32_t>(clientInfo.ID);
        {
            std::lock_guard lock(g_AssignmentMutex);
            g_PendingReleaseRequests.push_back(clientId);
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

            const uint32_t clientId = static_cast<uint32_t>(clientInfo.ID);
            std::lock_guard lock(g_InputMutex);
            auto it = g_ClientStates.find(clientId);
            if (it != g_ClientStates.end())
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
    g_EventBuffer.Allocate(4 * 1024);

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
        g_PlayerTemplate = playerEntity;

        if (g_RuntimeScene->HasComponent<TransformComponent>(playerEntity))
        {
            g_PlayerSpawnTransform = g_RuntimeScene->GetComponent<TransformComponent>(playerEntity);
            g_HasPlayerSpawnTransform = true;
        }
    }

    g_RuntimeScene->Start();

    PlayerController::SetBulletSpawnCallback([](const PlayerController::BulletSpawnInfo& info)
    {
        if (static_cast<uint64_t>(info.bulletUUID) == 0 || static_cast<uint64_t>(info.templateUUID) == 0)
        {
            return;
        }

        BulletSpawnEvent event;
        event.bulletUUID = static_cast<uint64_t>(info.bulletUUID);
        event.templateUUID = static_cast<uint64_t>(info.templateUUID);
        event.position = info.spawnPosition;
        event.velocity = info.spawnVelocity;
        event.fireSoundUUID = static_cast<uint64_t>(info.fireSoundUUID);

        std::lock_guard lock(g_EventMutex);
        g_PendingBulletEvents.push_back(event);
    });

    if (g_PlayerTemplate != entt::null)
    {
        PlayerController* templateController = GetPlayerControllerForEntity(g_PlayerTemplate);
        if (templateController)
        {
            templateController->EnableNetworkInput(false);
        }
        g_AvailablePlayers.push_back(g_PlayerTemplate);
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

        ProcessPendingAssignments();

        while (accumulator >= kFixedDeltaSeconds)
        {
            std::vector<std::pair<PlayerController*, PlayerController::NetworkInputState>> stepInputs;
            {
                std::lock_guard lock(g_InputMutex);
                stepInputs.reserve(g_ClientStates.size());
                for (auto& [clientId, state] : g_ClientStates)
                {
                    if (!state.controller)
                    {
                        continue;
                    }

                    stepInputs.emplace_back(state.controller, state.input);
                    state.input.yawDelta = 0.0f;
                }
            }

            for (auto& [controller, input] : stepInputs)
            {
                if (controller)
                {
                    controller->ApplyNetworkInput(input);
                }
            }

            g_RuntimeScene->Update(static_cast<float>(kFixedDeltaSeconds));
            BroadcastPhysicsState(server);
            accumulator -= kFixedDeltaSeconds;
        }

        BroadcastSpawnEvents(server);

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (g_RuntimeScene && g_RuntimeScene->IsPlaying())
    {
        g_RuntimeScene->Stop();
    }

    PlayerController::SetBulletSpawnCallback(nullptr);

    g_RuntimeScene.reset();

    JoltPhysics::Shutdown();
    return 0;
}
