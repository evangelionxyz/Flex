// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef FLEX_APP_NETWORK_CLIENT_H
#define FLEX_APP_NETWORK_CLIENT_H

#include "net/Client.hpp"
#include "net/BufferStream.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <cstdint>

namespace flex
{
    class Scene;

    class NetworkClient
    {
    public:
        struct PlayerInputMessage
        {
            glm::vec2 moveAxes{0.0f};
            float yawDelta = 0.0f;
            bool jump = false;
            bool fireLeft = false;
            bool fireRight = false;
        };

        NetworkClient();
        ~NetworkClient();

        void Connect(const std::string& address);
        void Disconnect();

        bool IsConnected() const;
        bool IsConnecting() const;
        ignite::net::Client::ConnectionStatus GetStatus() const;
        const std::string& GetDebugMessage() const;

        void SendPlayerInput(const PlayerInputMessage& input);
        void ApplyPendingSnapshot(Scene& scene);
        void ClearSnapshots();

    private:
        struct PhysicsSnapshot
        {
            struct Entry
            {
                uint64_t uuid = 0;
                glm::vec3 position{0.0f};
                glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                glm::vec3 velocity{0.0f};
            };

            std::vector<Entry> entries;
        };

        void OnDataReceived(const ignite::net::Buffer buffer);

        ignite::net::Client m_Client;
        ignite::net::Buffer m_SendBuffer;

        std::mutex m_SendBufferMutex;
        std::mutex m_SnapshotMutex;
        std::optional<PhysicsSnapshot> m_PendingSnapshot;

        bool m_InitializedCallbacks = false;
    };
}

#endif
