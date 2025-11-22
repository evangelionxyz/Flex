// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#include "NetworkClient.h"
#include "net/ServerPacket.hpp"

#include "Scene/Scene.h"
#include "Scene/Components.h"
#include "Core/UUID.h"

#include <SDL3/SDL_log.h>

#include <glm/gtx/euler_angles.hpp>

namespace flex
{
    namespace
    {
        constexpr size_t kSendBufferSize = 256;
    }

    NetworkClient::NetworkClient()
    {
        m_SendBuffer.Allocate(kSendBufferSize);

        if (!m_InitializedCallbacks)
        {
            m_Client.SetDataReceivedCallback([this](const ignite::net::Buffer buffer)
            {
                OnDataReceived(buffer);
            });

            m_Client.SetServerConnectedCallback([]()
            {
                SDL_Log("[NetworkClient] Connected to server");
            });

            m_Client.SetServerDisconnectedCallback([]()
            {
                SDL_Log("[NetworkClient] Disconnected from server");
            });

            m_InitializedCallbacks = true;
        }
    }

    NetworkClient::~NetworkClient()
    {
        Disconnect();
        m_SendBuffer.Release();
    }

    void NetworkClient::Connect(const std::string& address)
    {
        ClearSnapshots();
        m_Client.ConnectToServer(address);
    }

    void NetworkClient::Disconnect()
    {
        if (m_Client.IsRunning())
        {
            m_Client.Disconnect();
        }
        ClearSnapshots();
    }

    bool NetworkClient::IsConnected() const
    {
        return m_Client.GetConnectionStatus() == ignite::net::Client::ConnectionStatus::Connected;
    }

    bool NetworkClient::IsConnecting() const
    {
        return m_Client.GetConnectionStatus() == ignite::net::Client::ConnectionStatus::Connecting;
    }

    ignite::net::Client::ConnectionStatus NetworkClient::GetStatus() const
    {
        return m_Client.GetConnectionStatus();
    }

    const std::string& NetworkClient::GetDebugMessage() const
    {
        return m_Client.GetConnectionDebugMessage();
    }

    void NetworkClient::SendPlayerInput(const PlayerInputMessage& input)
    {
        if (!IsConnected())
        {
            return;
        }

        std::lock_guard sendLock(m_SendBufferMutex);
        ignite::net::BufferStreamWriter writer(m_SendBuffer);
        writer.SetStreamPosition(0);
        writer.WriteRaw(ignite::net::PacketType::PlayerInput);
        writer.WriteRaw(input.moveAxes.x);
        writer.WriteRaw(input.moveAxes.y);
        writer.WriteRaw(input.yawDelta);
        writer.WriteRaw(static_cast<uint8_t>(input.jump ? 1 : 0));
        writer.WriteRaw(static_cast<uint8_t>(input.fireLeft ? 1 : 0));
        writer.WriteRaw(static_cast<uint8_t>(input.fireRight ? 1 : 0));

        m_Client.SendBuffer(writer.GetBuffer(), false);
    }

    void NetworkClient::ApplyPendingSnapshot(Scene& scene)
    {
        ApplyPendingEntitySpawns(scene);

        std::optional<PhysicsSnapshot> snapshot;
        {
            std::lock_guard lock(m_SnapshotMutex);
            if (!m_PendingSnapshot.has_value())
            {
                return;
            }
            snapshot = std::move(m_PendingSnapshot);
            m_PendingSnapshot.reset();
        }

        if (!snapshot.has_value())
        {
            return;
        }

        constexpr float kRadToDeg = 57.29577951308232f;

        for (const auto& entry : snapshot->entries)
        {
            const UUID uuid(entry.uuid);
            entt::entity entity = scene.GetEntityByUUID(uuid);
            if (entity == entt::null)
            {
                continue;
            }

            if (scene.HasComponent<TransformComponent>(entity))
            {
                auto& transform = scene.GetComponent<TransformComponent>(entity);
                transform.position = entry.position;
                glm::vec3 euler = glm::eulerAngles(entry.rotation);
                transform.rotation = euler * kRadToDeg;
            }

            if (scene.HasComponent<RigidbodyComponent>(entity))
            {
                auto& rb = scene.GetComponent<RigidbodyComponent>(entity);
                rb.bodyID = JPH::BodyID();
            }
        }
    }

    void NetworkClient::ClearSnapshots()
    {
        std::lock_guard lock(m_SnapshotMutex);
        if (m_PendingSnapshot.has_value())
        {
            m_PendingSnapshot->entries.clear();
        }
        m_PendingSnapshot.reset();

        std::lock_guard spawnLock(m_SpawnMutex);
        m_PendingSpawns.clear();
    }

    void NetworkClient::ApplyPendingEntitySpawns(Scene& scene)
    {
        std::vector<EntitySpawnEvent> spawns;
        {
            std::lock_guard lock(m_SpawnMutex);
            if (m_PendingSpawns.empty())
            {
                return;
            }
            spawns.swap(m_PendingSpawns);
        }

        for (const EntitySpawnEvent& spawn : spawns)
        {
            if (spawn.entityUUID == 0 || spawn.templateUUID == 0)
            {
                continue;
            }

            const UUID templateUUID(spawn.templateUUID);
            entt::entity templateEntity = scene.GetEntityByUUID(templateUUID);
            if (templateEntity == entt::null)
            {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[NetworkClient] Template entity %llu not found for spawn", static_cast<unsigned long long>(spawn.templateUUID));
                continue;
            }

            const UUID spawnUUID(spawn.entityUUID);
            if (entt::entity existing = scene.GetEntityByUUID(spawnUUID); existing != entt::null)
            {
                scene.DestroyEntity(existing);
            }

            entt::entity spawnedEntity = scene.DuplicateEntityWithUUID(templateEntity, spawnUUID);
            if (spawnedEntity == entt::null)
            {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[NetworkClient] Failed to spawn entity %llu", static_cast<unsigned long long>(spawn.entityUUID));
                continue;
            }

            if (scene.HasComponent<TransformComponent>(spawnedEntity))
            {
                auto& transform = scene.GetComponent<TransformComponent>(spawnedEntity);
                transform.position = spawn.position;
            }

            if (spawn.fireSoundUUID != 0)
            {
                const UUID soundUUID(spawn.fireSoundUUID);
                entt::entity soundEntity = scene.GetEntityByUUID(soundUUID);
                if (soundEntity != entt::null && scene.HasComponent<AudioComponent>(soundEntity))
                {
                    auto& audio = scene.GetComponent<AudioComponent>(soundEntity);
                    if (audio.sound)
                    {
                        audio.sound->Play();
                        audio.sound->SetVolume(audio.volume);
                        audio.sound->SetPan(audio.panning);
                    }
                }
            }
        }
    }

    void NetworkClient::OnDataReceived(const ignite::net::Buffer buffer)
    {
        ignite::net::BufferStreamReader reader(buffer);
        ignite::net::PacketType packetType = ignite::net::PacketType::None;
        if (!reader.ReadRaw(packetType))
        {
            return;
        }

        switch (packetType)
        {
        case ignite::net::PacketType::PhysicsState:
        {
            uint32_t entryCount = 0;
            if (!reader.ReadRaw(entryCount))
            {
                return;
            }

            PhysicsSnapshot snapshot;
            snapshot.entries.reserve(entryCount);
            for (uint32_t i = 0; i < entryCount; ++i)
            {
                PhysicsSnapshot::Entry entry;
                reader.ReadRaw(entry.uuid);
                reader.ReadRaw(entry.position);
                float rx = 0.0f, ry = 0.0f, rz = 0.0f, rw = 1.0f;
                reader.ReadRaw(rx);
                reader.ReadRaw(ry);
                reader.ReadRaw(rz);
                reader.ReadRaw(rw);
                entry.rotation = glm::normalize(glm::quat(rw, rx, ry, rz));
                reader.ReadRaw(entry.velocity);
                snapshot.entries.push_back(entry);
            }

            std::lock_guard lock(m_SnapshotMutex);
            m_PendingSnapshot = std::move(snapshot);
            break;
        }
        case ignite::net::PacketType::EntitySpawn:
        {
            EntitySpawnEvent spawn{};
            reader.ReadRaw(spawn.entityUUID);
            reader.ReadRaw(spawn.templateUUID);
            reader.ReadRaw(spawn.position);
            reader.ReadRaw(spawn.velocity);
            reader.ReadRaw(spawn.fireSoundUUID);

            std::lock_guard lock(m_SpawnMutex);
            m_PendingSpawns.push_back(spawn);
            break;
        }
        default:
            break;
        }
    }
}
