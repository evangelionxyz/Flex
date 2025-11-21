#include <gtest/gtest.h>

#include "Math/Math.hpp"
#include "Scene/Scene.h"
#include "Core/Types.h"
#include "Scene/Components.h"

namespace
{
    struct Sample
    {
        int value;
        explicit Sample(int v) : value(v) {}
    };
}

TEST(CoreTypesTest, CreateRefConstructsSharedInstance)
{
    auto object = flex::CreateRef<Sample>(42);
    ASSERT_NE(object, nullptr);
    EXPECT_EQ(object->value, 42);
    EXPECT_EQ(object.use_count(), 1);
}

TEST(CoreTypesTest, CreateScopeConstructsUniqueInstance)
{
    auto object = flex::CreateScope<Sample>(7);
    ASSERT_NE(object, nullptr);
    EXPECT_EQ(object->value, 7);
}

TEST(UUIDTest, ExplicitValueIsPreserved)
{
    constexpr uint64_t expected = 0x1234ABCDEFu;
    const flex::UUID uuid(expected);
    EXPECT_EQ(static_cast<uint64_t>(uuid), expected);
}

namespace
{
    constexpr float kEpsilon = 1e-4f;

    void ExpectVec3Near(const glm::vec3& lhs, const glm::vec3& rhs)
    {
        EXPECT_NEAR(lhs.x, rhs.x, kEpsilon);
        EXPECT_NEAR(lhs.y, rhs.y, kEpsilon);
        EXPECT_NEAR(lhs.z, rhs.z, kEpsilon);
    }

    void ExpectMat4Near(const glm::mat4& lhs, const glm::mat4& rhs)
    {
        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                EXPECT_NEAR(lhs[column][row], rhs[column][row], kEpsilon);
            }
        }
    }
}

class JoltPhysicsTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        if (!flex::JoltPhysics::Get())
        {
            flex::JoltPhysics::Init();
        }
    }

    static void TearDownTestSuite()
    {
        if (flex::JoltPhysics::Get())
        {
            flex::JoltPhysics::Shutdown();
        }
    }
};

class SceneTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        if (!flex::JoltPhysics::Get())
        {
            flex::JoltPhysics::Init();
        }
    }

    static void TearDownTestSuite()
    {
        if (flex::JoltPhysics::Get())
        {
            flex::JoltPhysics::Shutdown();
        }
    }
};

TEST(MathTest, ComposeTransformMatchesTranslationRotationScale)
{
    flex::TransformComponent transform;
    transform.position = { 3.0f, -2.0f, 5.0f };
    transform.rotation = { 45.0f, 30.0f, 15.0f };
    transform.scale = { 2.0f, 0.5f, 1.5f };

    const glm::mat4 composed = flex::math::ComposeTransform(transform);

    const glm::mat4 translation = glm::translate(glm::mat4(1.0f), transform.position);
    const glm::quat orientation = glm::quat(glm::radians(transform.rotation));
    const glm::mat4 rotation = glm::toMat4(orientation);
    const glm::mat4 scale = glm::scale(glm::mat4(1.0f), transform.scale);
    const glm::mat4 expected = translation * rotation * scale;

    ExpectMat4Near(composed, expected);
}

TEST(MathTest, DecomposeTransformRestoresOriginalComponents)
{
    flex::TransformComponent original;
    original.position = { -10.0f, 4.0f, 12.5f };
    original.rotation = { -20.0f, 60.0f, 5.0f };
    original.scale = { 0.25f, 3.0f, 1.2f };

    const glm::mat4 matrix = flex::math::ComposeTransform(original);

    flex::TransformComponent decomposed;
    flex::math::DecomposeTransform(matrix, decomposed);

    ExpectVec3Near(decomposed.position, original.position);
    ExpectVec3Near(decomposed.scale, original.scale);
    ExpectVec3Near(decomposed.rotation, original.rotation);
}

TEST_F(JoltPhysicsTest, DynamicBodyFallsUnderGravity)
{
    flex::Scene scene;
    auto entity = scene.CreateEntity("Falling Body");

    auto& transform = scene.AddComponent<flex::TransformComponent>(entity);
    transform.position = { 0.0f, 10.0f, 0.0f };
    transform.scale = { 1.0f, 1.0f, 1.0f };

    auto& rb = scene.AddComponent<flex::RigidbodyComponent>(entity);
    rb.useGravity = true;

    auto& collider = scene.AddComponent<flex::BoxColliderComponent>(entity);
    collider.scale = { 0.5f, 0.5f, 0.5f };

    const float initialY = transform.position.y;

    scene.Start();
    EXPECT_FALSE(rb.bodyID.IsInvalid());

    constexpr int steps = 120;
    constexpr float dt = 1.0f / 120.0f;
    for (int i = 0; i < steps; ++i)
    {
        scene.Update(dt);
    }

    scene.Stop();

    EXPECT_LT(transform.position.y, initialY);
}

TEST_F(JoltPhysicsTest, AddForceAcceleratesBody)
{
    flex::Scene scene;
    auto entity = scene.CreateEntity("Dynamic Body");

    auto& transform = scene.AddComponent<flex::TransformComponent>(entity);
    transform.position = { 0.0f, 0.5f, 0.0f };
    transform.scale = { 1.0f, 1.0f, 1.0f };

    auto& rb = scene.AddComponent<flex::RigidbodyComponent>(entity);
    rb.useGravity = false;

    auto& collider = scene.AddComponent<flex::BoxColliderComponent>(entity);
    collider.scale = { 0.5f, 0.5f, 0.5f };

    scene.Start();
    ASSERT_FALSE(rb.bodyID.IsInvalid());

    const glm::vec3 force = { 25.0f, 0.0f, 0.0f };
    scene.joltPhysicsScene->AddForce(rb.bodyID, force);

    constexpr int steps = 60;
    constexpr float dt = 1.0f / 120.0f;
    for (int i = 0; i < steps; ++i)
    {
        scene.Update(dt);
    }

    const glm::vec3 velocity = scene.joltPhysicsScene->GetLinearVelocity(rb.bodyID);
    scene.Stop();

    EXPECT_GT(velocity.x, 0.0f);
    EXPECT_NEAR(velocity.y, 0.0f, kEpsilon);
    EXPECT_NEAR(velocity.z, 0.0f, kEpsilon);
}


TEST_F(SceneTest, SceneInitializesRegistryAndPhysics)
{
    flex::Scene scene;
    EXPECT_TRUE(scene.registry != nullptr);
    EXPECT_TRUE(scene.joltPhysicsScene != nullptr);
    EXPECT_FALSE(scene.IsPlaying());

    scene.Start();
    EXPECT_TRUE(scene.IsPlaying());

    scene.Stop();
    EXPECT_FALSE(scene.IsPlaying());
}


TEST_F(SceneTest, CreateEntityAddsTagAndUUIDMapping)
{
    flex::Scene scene;
    const std::string name = "Test Entity";

    entt::entity entity = scene.CreateEntity(name);
    ASSERT_TRUE(scene.IsValid(entity));

    auto& tag = scene.GetComponent<flex::TagComponent>(entity);
    EXPECT_EQ(tag.name, name);
    EXPECT_EQ(tag.scene, &scene);
    EXPECT_TRUE(scene.entities.contains(tag.uuid));
    EXPECT_EQ(static_cast<uint32_t>(scene.entities[tag.uuid]), static_cast<uint32_t>(entity));
}

TEST_F(SceneTest, DuplicateEntityCopiesComponentsAndMaintainsUUID)
{
    flex::Scene scene;
    entt::entity original = scene.CreateEntity("Original");
    auto& originalTransform = scene.AddComponent<flex::TransformComponent>(original);
    originalTransform.position = { 1.0f, 2.0f, 3.0f };
    originalTransform.rotation = { 10.0f, 20.0f, 30.0f };
    originalTransform.scale = { 2.0f, 2.0f, 2.0f };

    entt::entity duplicate = scene.DuplicateEntity(original);
    ASSERT_TRUE(scene.IsValid(duplicate));
    ASSERT_NE(duplicate, original);

    const auto& originalTag = scene.GetComponent<flex::TagComponent>(original);
    const auto& duplicateTag = scene.GetComponent<flex::TagComponent>(duplicate);
    EXPECT_NE(duplicateTag.uuid, originalTag.uuid);
    EXPECT_NE(duplicateTag.name, "");
    EXPECT_NE(duplicateTag.name, originalTag.name);

    ASSERT_TRUE(scene.HasComponent<flex::TransformComponent>(duplicate));
    const auto& duplicateTransform = scene.GetComponent<flex::TransformComponent>(duplicate);
    ExpectVec3Near(duplicateTransform.position, originalTransform.position);
    ExpectVec3Near(duplicateTransform.rotation, originalTransform.rotation);
    ExpectVec3Near(duplicateTransform.scale, originalTransform.scale);
}
