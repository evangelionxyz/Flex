// Copyright (c) 2025 Evangelion Manuhutu

#ifndef PLAYER_CONTROLLER_H
#define PLAYER_CONTROLLER_H

#include "Scene/ScriptableEntity.h"
#include "Scene/Components.h"
#include "SDL3/SDL_keyboard.h"
#include "SDL3/SDL_mouse.h"
#include "SDL3/SDL_log.h"
#include "Renderer/Window.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <functional>
#include <utility>

namespace flex
{
    class PlayerController : public ScriptableEntity
    {
    public:
        PlayerController(Scene* scene, entt::entity entity)
            : ScriptableEntity(scene, entity)
        {
        }

        ~PlayerController() override = default;

        struct BulletSpawnInfo
        {
            UUID bulletUUID = UUID(0);
            UUID templateUUID = UUID(0);
            glm::vec3 spawnPosition = glm::vec3(0.0f);
            glm::vec3 spawnVelocity = glm::vec3(0.0f);
            UUID fireSoundUUID = UUID(0);
        };

        using BulletSpawnCallback = std::function<void(const BulletSpawnInfo&)>;

        static void SetBulletSpawnCallback(BulletSpawnCallback callback)
        {
            s_BulletSpawnCallback = std::move(callback);
        }

        void OnMouseMotion(const glm::vec2& delta)
        {
            m_MouseDelta = delta;
        }

        struct NetworkInputState
        {
            glm::vec2 moveAxes = glm::vec2(0.0f); // x = strafe, y = forward
            float yawDelta = 0.0f;
            bool jump = false;
            bool fireLeft = false;
            bool fireRight = false;
        };

        void EnableNetworkInput(bool enabled)
        {
            m_NetworkInputEnabled = enabled;
        }

        void ApplyNetworkInput(const NetworkInputState& input)
        {
            m_NetworkInput.moveAxes = input.moveAxes;
            m_NetworkInput.jump = input.jump;
            m_NetworkInput.fireLeft = input.fireLeft;
            m_NetworkInput.fireRight = input.fireRight;
            m_NetworkInput.yawDelta += input.yawDelta;
        }

    protected:
        void OnStart() override
        {
            SDL_Log("PlayerController: Started");
            
            // Find fire points and bullet template
            m_FirePointL = m_Scene->GetEntityByName("FirePoint L");
            m_FirePointR = m_Scene->GetEntityByName("FirePoint R");
            m_BulletTemplate = m_Scene->GetEntityByName("bullet");
            m_FireSound = m_Scene->GetEntityByName("bulletSound");
            
            if (m_FirePointL == entt::null)
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "PlayerController: FirePoint L not found!");
            if (m_FirePointR == entt::null)
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "PlayerController: FirePoint R not found!");
            if (m_BulletTemplate == entt::null)
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "PlayerController: bullet template not found!");
            if (m_FireSound == entt::null)
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "PlayerController: Firesound!");
        }

        void OnStop() override
        {
            SDL_Log("PlayerController: Stopped");
        }

        void OnUpdate(float deltaTime) override
        {
            if (!m_Scene || !m_Scene->IsValid(m_Entity))
                return;

            // Only process input if we have a rigidbody (physics-based movement)
            if (!m_Scene->HasComponent<RigidbodyComponent>(m_Entity))
                return;

            auto& rb = m_Scene->GetComponent<RigidbodyComponent>(m_Entity);
            
            // Skip if body is static
            if (rb.isStatic)
                return;

            if (rb.bodyID.IsInvalid())
            {
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, 
                    "PlayerController: Rigidbody has invalid BodyID. Physics not initialized?");
                return;
            }

            auto& physicsScene = m_Scene->joltPhysicsScene;
            if (!physicsScene)
                return;

            if (!m_Scene->HasComponent<TransformComponent>(m_Entity))
                return;

            auto& transform = m_Scene->GetComponent<TransformComponent>(m_Entity);

            // Handle mouse rotation
            if (m_NetworkInputEnabled)
            {
                if (std::abs(m_NetworkInput.yawDelta) > 0.0f)
                {
                    m_MouseDelta.x += m_NetworkInput.yawDelta;
                    m_NetworkInput.yawDelta = 0.0f;
                }
            }

            if (glm::length(m_MouseDelta) > 0.0f)
            {
                m_Yaw += m_MouseDelta.x * m_MouseSensitivity;
                
                // Apply rotation using physics
                glm::quat rotation = glm::quat(glm::vec3(0.0f, glm::radians(-m_Yaw), 0.0f));
                physicsScene->SetRotation(rb.bodyID, rotation, true);
                
                m_MouseDelta = glm::vec2(0.0f);
            }

            // Get current rotation from physics body to calculate correct forward/right vectors
            glm::quat currentRotation = physicsScene->GetRotation(rb.bodyID);
            glm::vec3 forward = currentRotation * glm::vec3(0.0f, 0.0f, -1.0f);
            glm::vec3 right = currentRotation * glm::vec3(1.0f, 0.0f, 0.0f);
            
            // Flatten to horizontal plane
            forward.y = 0.0f;
            right.y = 0.0f;
            if (glm::length(forward) > 0.0f)
                forward = glm::normalize(forward);
            if (glm::length(right) > 0.0f)
                right = glm::normalize(right);

            glm::vec3 moveDirection(0.0f);
            bool jumpHeld = false;
            bool leftMousePressed = false;
            bool rightMousePressed = false;

            if (m_NetworkInputEnabled)
            {
                moveDirection += forward * m_NetworkInput.moveAxes.y;
                moveDirection += right * m_NetworkInput.moveAxes.x;
                jumpHeld = m_NetworkInput.jump;
                leftMousePressed = m_NetworkInput.fireLeft;
                rightMousePressed = m_NetworkInput.fireRight;
            }
            else
            {
                const bool* keyState = SDL_GetKeyboardState(nullptr);
                if (keyState)
                {
                    if (keyState[SDL_SCANCODE_W])
                        moveDirection += forward;
                    if (keyState[SDL_SCANCODE_S])
                        moveDirection -= forward;
                    if (keyState[SDL_SCANCODE_A])
                        moveDirection -= right;
                    if (keyState[SDL_SCANCODE_D])
                        moveDirection += right;
                    jumpHeld = keyState[SDL_SCANCODE_SPACE];
                }

                if (auto* window = Window::Get())
                {
                    leftMousePressed = window->IsMouseButtonPressed(SDL_BUTTON_LEFT);
                    rightMousePressed = window->IsMouseButtonPressed(SDL_BUTTON_RIGHT);
                }
            }

            if (glm::length(moveDirection) > 0.0f)
            {
                moveDirection = glm::normalize(moveDirection);
            }

            // Get current velocity and apply movement
            glm::vec3 currentVelocity = physicsScene->GetLinearVelocity(rb.bodyID);
            
            // Apply horizontal movement while preserving vertical velocity (gravity)
            glm::vec3 targetVelocity = moveDirection * m_MoveSpeed;
            targetVelocity.y = currentVelocity.y; // Keep vertical velocity for gravity/jumping
            
            // Set the new velocity
            physicsScene->SetLinearVelocity(rb.bodyID, targetVelocity);

            // Lock angular velocity to prevent rotation from collisions (only allow Y-axis rotation)
            glm::vec3 angularVel = physicsScene->GetAngularVelocity(rb.bodyID);
            angularVel.x = 0.0f; // Lock X-axis rotation (pitch)
            angularVel.z = 0.0f; // Lock Z-axis rotation (roll)
            physicsScene->SetAngularVelocity(rb.bodyID, angularVel);

            // Ground detection - check if vertical velocity is near zero and position is low
            m_IsGrounded = std::abs(currentVelocity.y) < 0.1f && transform.position.y <= 1.0f;

            // Jump
            if (jumpHeld && !m_WasSpacePressed && m_IsGrounded)
            {
                // Apply upward impulse for jumping
                glm::vec3 jumpImpulse(0.0f, m_JumpForce * rb.mass, 0.0f);
                physicsScene->AddImpulse(rb.bodyID, jumpImpulse);
                
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "PlayerController: Jump!");
            }
            m_WasSpacePressed = jumpHeld;

            // Shooting
            HandleShooting(forward, leftMousePressed, rightMousePressed);

            m_LeftMouseWasPressed = leftMousePressed;
            m_RightMouseWasPressed = rightMousePressed;
        }

    private:
        void HandleShooting(const glm::vec3& forward, bool leftMousePressed, bool rightMousePressed)
        {
            // Fire from left weapon
            if (leftMousePressed && !m_LeftMouseWasPressed)
            {
                FireBullet(m_FirePointL, forward);
            }

            // Fire from right weapon
            if (rightMousePressed && !m_RightMouseWasPressed)
            {
                FireBullet(m_FirePointR, forward);
            }
        }

        void FireBullet(entt::entity firePoint, const glm::vec3& forward)
        {
            if (firePoint == entt::null || m_BulletTemplate == entt::null)
                return;

            if (!m_Scene->IsValid(firePoint) || !m_Scene->IsValid(m_BulletTemplate))
                return;

            // Duplicate the bullet template
            entt::entity bullet = m_Scene->DuplicateEntity(m_BulletTemplate);
            if (bullet == entt::null)
            {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "PlayerController: Failed to duplicate bullet!");
                return;
            }

            // Get fire point world position
            glm::mat4 firePointWorld = m_Scene->GetWorldTransform(firePoint);
            glm::vec3 spawnPosition;
            glm::quat spawnRotation;
            glm::vec3 scale;
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(firePointWorld, scale, spawnRotation, spawnPosition, skew, perspective);

            // Set bullet position to fire point position
            if (m_Scene->HasComponent<TransformComponent>(bullet))
            {
                auto& bulletTransform = m_Scene->GetComponent<TransformComponent>(bullet);
                bulletTransform.position = spawnPosition;
            }

            // Apply velocity to bullet
            if (m_Scene->HasComponent<RigidbodyComponent>(bullet))
            {
                auto& bulletRb = m_Scene->GetComponent<RigidbodyComponent>(bullet);

                m_Scene->joltPhysicsScene->InstantiateEntity(bullet);
                
                // Wait for physics to initialize the body
                if (!bulletRb.bodyID.IsInvalid() && m_Scene->joltPhysicsScene)
                {
                    glm::vec3 bulletVelocity = forward * m_BulletSpeed;
                    m_Scene->joltPhysicsScene->SetLinearVelocity(bulletRb.bodyID, bulletVelocity);
                    
                    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, 
                        "PlayerController: Fired bullet at (%.2f, %.2f, %.2f) with velocity (%.2f, %.2f, %.2f)",
                        spawnPosition.x, spawnPosition.y, spawnPosition.z,
                        bulletVelocity.x, bulletVelocity.y, bulletVelocity.z);

                    if (s_BulletSpawnCallback)
                    {
                        BulletSpawnInfo info;
                        if (m_Scene->HasComponent<TagComponent>(bullet))
                        {
                            info.bulletUUID = m_Scene->GetComponent<TagComponent>(bullet).uuid;
                        }
                        if (m_Scene->IsValid(m_BulletTemplate) && m_Scene->HasComponent<TagComponent>(m_BulletTemplate))
                        {
                            info.templateUUID = m_Scene->GetComponent<TagComponent>(m_BulletTemplate).uuid;
                        }
                        info.spawnPosition = spawnPosition;
                        info.spawnVelocity = bulletVelocity;
                        if (m_FireSound != entt::null && m_Scene->IsValid(m_FireSound) && m_Scene->HasComponent<TagComponent>(m_FireSound))
                        {
                            info.fireSoundUUID = m_Scene->GetComponent<TagComponent>(m_FireSound).uuid;
                        }
                        s_BulletSpawnCallback(info);
                    }
                }

                // Play Fire Sound
                if (m_FireSound != entt::null && m_Scene->HasComponent<AudioComponent>(m_FireSound))
                {
                    auto &audio = m_Scene->GetComponent<AudioComponent>(m_FireSound);
                    if (audio.sound)
                    {
                        audio.sound->Play();
                        
                        audio.sound->SetVolume(audio.volume);
                        audio.sound->SetPan(audio.panning);
                    }
                }
            }
        }

        float m_MoveSpeed = 12.0f;
        float m_JumpForce = 8.0f;
        float m_MouseSensitivity = 0.1f;
        float m_BulletSpeed = 50.0f;
        float m_Yaw = 0.0f;
        glm::vec2 m_MouseDelta = glm::vec2(0.0f);
        bool m_IsGrounded = false;
        bool m_WasSpacePressed = false;
        bool m_LeftMouseWasPressed = false;
        bool m_RightMouseWasPressed = false;
        bool m_NetworkInputEnabled = false;
        NetworkInputState m_NetworkInput;
        
        entt::entity m_FirePointL = entt::null;
        entt::entity m_FirePointR = entt::null;
        entt::entity m_BulletTemplate = entt::null;
        entt::entity m_FireSound = entt::null;

        inline static BulletSpawnCallback s_BulletSpawnCallback = nullptr;
    };
}

#endif
