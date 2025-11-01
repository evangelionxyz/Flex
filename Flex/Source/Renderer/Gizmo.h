// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef GIZMO_H
#define GIZMO_H

#ifndef GLM_ENABLE_EXPERIMENTAL
    #define GLM_ENABLE_EXPERIMENTAL
#endif

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Math/Math.hpp"
#include "Renderer/Mesh.h"
#include "Renderer/Shader.h"
#include "Core/Camera.h"

// Forward declarations
struct GLFWwindow;

namespace flex
{
    enum class GizmoMode
    {
        TRANSLATE,
        ROTATE,
        SCALE
    };

    enum class GizmoAxis
    {
        NONE,
        X,
        Y,
        Z,
        XY,
        XZ,
        YZ,
        XYZ
    };

    struct GizmoColors
    {
        static const glm::vec3 XAxis;
        static const glm::vec3 YAxis;
        static const glm::vec3 ZAxis;
        static const glm::vec3 Selected;
        static const glm::vec3 Hover;
    };

    struct Ray
    {
        glm::vec3 origin;
        glm::vec3 direction;

        Ray(const glm::vec3& origin, const glm::vec3& direction)
            : origin(origin), direction(glm::normalize(direction)) {}
    };

    struct GizmoPart
    {
        GizmoAxis axis;
        std::shared_ptr<Mesh> mesh;
        glm::mat4 transform;
        glm::vec3 color;
        bool hovered = false;
        bool selected = false;

        GizmoPart(GizmoAxis axis, std::shared_ptr<Mesh> mesh, const glm::mat4& transform, const glm::vec3& color)
            : axis(axis), mesh(mesh), transform(transform), color(color) {}
    };

    class Gizmo
    {
    public:
        Gizmo();
        ~Gizmo() = default;

        // Core functionality
        void SetMode(GizmoMode mode);
        void SetPosition(const glm::vec3& position);
        void SetRotation(const glm::quat& rotation);
        void SetScale(float scale);

        // Interaction
        void Update(const Camera& camera, GLFWwindow* window, float deltaTime);
        void Render(const glm::mat4& viewProjection);

        // State queries
        GizmoMode GetMode() const { return mode_; }
        GizmoAxis GetSelectedAxis() const { return selectedAxis_; }
        GizmoAxis GetHoveredAxis() const { return hoveredAxis_; }
        const glm::vec3& GetPosition() const { return position_; }
        const glm::quat& GetRotation() const { return rotation_; }
        float GetScale() const { return scale_; }

        // Transform application
        void ApplyTranslation(const glm::vec3& delta);
        void ApplyRotation(const glm::quat& delta);
        void ApplyScale(const glm::vec3& delta);

        // Mouse picking
        GizmoAxis PickAxis(const Ray& ray, const glm::mat4& viewProjection);

    private:
        // Gizmo parts
        std::vector<GizmoPart> parts_;
        GizmoMode mode_;
        GizmoAxis selectedAxis_;
        GizmoAxis hoveredAxis_;

        // Transform
        glm::vec3 position_;
        glm::quat rotation_;
        float scale_;

        // Interaction state
        bool isDragging_;
        glm::vec2 lastMousePos_;
        glm::vec3 dragStartPosition_;
        glm::quat dragStartRotation_;
        glm::vec3 dragStartScale_;

        // Shaders
        std::shared_ptr<Shader> gizmoShader_;

        // Geometry creation
        void CreateTranslateGizmo();
        void CreateRotateGizmo();
        void CreateScaleGizmo();
        void ClearParts();

        // Helper functions
        std::shared_ptr<Mesh> CreateArrowMesh(float length, float radius, int segments = 8);
        std::shared_ptr<Mesh> CreateRingMesh(float innerRadius, float outerRadius, int segments = 32);
        std::shared_ptr<Mesh> CreateCubeMesh(float size);
        std::shared_ptr<Mesh> CreateCylinderMesh(float radius, float height, int segments = 8);

        // Ray intersection
        bool RayIntersectsArrow(const Ray& ray, const glm::mat4& transform, float length, float radius);
        bool RayIntersectsRing(const Ray& ray, const glm::mat4& transform, float innerRadius, float outerRadius);
        bool RayIntersectsCube(const Ray& ray, const glm::mat4& transform, float size);

        // Interaction handlers
        void HandleTranslate(const glm::vec2& mouseDelta, const Camera& camera, int screenWidth, int screenHeight);
        void HandleRotate(const glm::vec2& mouseDelta, const Camera& camera);
        void HandleScale(const glm::vec2& mouseDelta, const Camera& camera, int screenWidth, int screenHeight);

        // Screen space utilities
        glm::vec3 ScreenToWorld(const glm::vec2& screenPos, float depth, const glm::mat4& viewProjection, int screenWidth, int screenHeight);
        Ray ScreenPointToRay(const glm::vec2& screenPos, const glm::mat4& viewProjection, int screenWidth, int screenHeight);
        glm::vec2 WorldToScreen(const glm::vec3& worldPos, const glm::mat4& viewProjection, int screenWidth, int screenHeight);
    };

    // Utility functions
    Ray CreateRayFromScreen(const glm::vec2& screenPos, const Camera& camera, int screenWidth, int screenHeight);
    glm::vec3 ProjectPointOnPlane(const glm::vec3& point, const glm::vec3& planeNormal, const glm::vec3& planePoint);
    glm::vec3 ProjectPointOnLine(const glm::vec3& point, const glm::vec3& lineStart, const glm::vec3& lineEnd);
    bool RayIntersectsAABB(const Ray& ray, const glm::vec3& min, const glm::vec3& max);

}
#endif // GIZMO_H
