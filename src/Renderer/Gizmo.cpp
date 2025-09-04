#include "Gizmo.h"
#include <GLFW/glfw3.h>
#include <algorithm>

// GizmoColors definitions
const glm::vec3 GizmoColors::XAxis = glm::vec3(1.0f, 0.2f, 0.2f);
const glm::vec3 GizmoColors::YAxis = glm::vec3(0.2f, 1.0f, 0.2f);
const glm::vec3 GizmoColors::ZAxis = glm::vec3(0.2f, 0.2f, 1.0f);
const glm::vec3 GizmoColors::Selected = glm::vec3(1.0f, 1.0f, 0.0f);
const glm::vec3 GizmoColors::Hover = glm::vec3(1.0f, 1.0f, 1.0f);

Gizmo::Gizmo()
    : mode_(GizmoMode::TRANSLATE)
    , selectedAxis_(GizmoAxis::NONE)
    , hoveredAxis_(GizmoAxis::NONE)
    , position_(0.0f)
    , rotation_(glm::quat(1.0f, 0.0f, 0.0f, 0.0f))
    , scale_(1.0f)
    , isDragging_(false)
    , lastMousePos_(0.0f)
    , dragStartPosition_(0.0f)
    , dragStartRotation_(glm::quat(1.0f, 0.0f, 0.0f, 0.0f))
    , dragStartScale_(glm::vec3(1.0f))
{
    // Create gizmo shader
    gizmoShader_ = std::make_shared<Shader>();
    gizmoShader_->AddFromString(R"(
        #version 460 core
        layout(location = 0) in vec3 a_Position;
        layout(location = 1) in vec3 a_Normal;
        layout(location = 2) in vec3 a_Color;

        uniform mat4 u_MVP;
        uniform vec3 u_Color;

        out vec3 v_Color;
        out vec3 v_Normal;

        void main() {
            gl_Position = u_MVP * vec4(a_Position, 1.0);
            v_Color = a_Color * u_Color;
            v_Normal = a_Normal;
        }
    )", GL_VERTEX_SHADER);

    gizmoShader_->AddFromString(R"(
        #version 460 core
        in vec3 v_Color;
        in vec3 v_Normal;

        out vec4 FragColor;

        void main() {
            vec3 normal = normalize(v_Normal);
            vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
            float diff = max(dot(normal, lightDir), 0.3);
            FragColor = vec4(v_Color * diff, 1.0);
        }
    )", GL_FRAGMENT_SHADER);

    gizmoShader_->Compile();

    CreateTranslateGizmo();
}

void Gizmo::SetMode(GizmoMode mode)
{
    if (mode_ != mode)
    {
        mode_ = mode;
        ClearParts();

        switch (mode_)
        {
        case GizmoMode::TRANSLATE:
            CreateTranslateGizmo();
            break;
        case GizmoMode::ROTATE:
            CreateRotateGizmo();
            break;
        case GizmoMode::SCALE:
            CreateScaleGizmo();
            break;
        }
    }
}

void Gizmo::SetPosition(const glm::vec3& position)
{
    position_ = position;
}

void Gizmo::SetRotation(const glm::quat& rotation)
{
    rotation_ = rotation;
}

void Gizmo::SetScale(float scale)
{
    scale_ = scale;
}

void Gizmo::Update(const Camera& camera, GLFWwindow* window, float deltaTime)
{
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    glm::vec2 mousePos(mouseX, mouseY);

    // Get window size
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    // Create ray from mouse position
    Ray ray = ScreenPointToRay(mousePos, camera.projection * camera.view, width, height);

    // Update hover state
    hoveredAxis_ = PickAxis(ray, camera.projection * camera.view);

    // Handle mouse interaction
    bool leftMousePressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    if (leftMousePressed && !isDragging_)
    {
        // Start dragging
        if (hoveredAxis_ != GizmoAxis::NONE)
        {
            isDragging_ = true;
            selectedAxis_ = hoveredAxis_;
            lastMousePos_ = mousePos;
            dragStartPosition_ = position_;
            dragStartRotation_ = rotation_;
            dragStartScale_ = glm::vec3(scale_);
        }
    }
    else if (!leftMousePressed && isDragging_)
    {
        // Stop dragging
        isDragging_ = false;
        selectedAxis_ = GizmoAxis::NONE;
    }

    // Handle dragging
    if (isDragging_ && selectedAxis_ != GizmoAxis::NONE)
    {
        glm::vec2 mouseDelta = mousePos - lastMousePos_;
        lastMousePos_ = mousePos;

        switch (mode_)
        {
        case GizmoMode::TRANSLATE:
            HandleTranslate(mouseDelta, camera, width, height);
            break;
        case GizmoMode::ROTATE:
            HandleRotate(mouseDelta, camera);
            break;
        case GizmoMode::SCALE:
            HandleScale(mouseDelta, camera, width, height);
            break;
        }
    }
}

void Gizmo::Render(const glm::mat4& viewProjection)
{
    gizmoShader_->Use();

    glm::mat4 model = glm::translate(glm::mat4(1.0f), position_) *
                     glm::mat4_cast(rotation_) *
                     glm::scale(glm::mat4(1.0f), glm::vec3(scale_));

    glm::mat4 mvp = viewProjection * model;

    for (auto& part : parts_)
    {
        glm::vec3 color = part.color;

        // Highlight hovered/selected parts
        if (part.axis == hoveredAxis_ && !isDragging_)
        {
            color = GizmoColors::Hover;
        }
        else if (part.axis == selectedAxis_)
        {
            color = GizmoColors::Selected;
        }

        gizmoShader_->SetUniform("u_MVP", mvp * part.transform);
        gizmoShader_->SetUniform("u_Color", color);

        part.mesh->vertexArray->Bind();
        if (part.mesh->indexBuffer) {
            glDrawElements(GL_TRIANGLES, part.mesh->indexBuffer->GetCount(), GL_UNSIGNED_INT, nullptr);
        }
    }
}

void Gizmo::ApplyTranslation(const glm::vec3& delta)
{
    position_ += delta;
}

void Gizmo::ApplyRotation(const glm::quat& delta)
{
    rotation_ = delta * rotation_;
}

void Gizmo::ApplyScale(const glm::vec3& delta)
{
    scale_ *= (1.0f + delta.x); // Uniform scaling for simplicity
}

GizmoAxis Gizmo::PickAxis(const Ray& ray, const glm::mat4& viewProjection)
{
    float closestDistance = std::numeric_limits<float>::max();
    GizmoAxis closestAxis = GizmoAxis::NONE;

    glm::mat4 model = glm::translate(glm::mat4(1.0f), position_) *
                     glm::mat4_cast(rotation_) *
                     glm::scale(glm::mat4(1.0f), glm::vec3(scale_));

    for (const auto& part : parts_)
    {
        glm::mat4 worldTransform = model * part.transform;
        bool intersects = false;

        switch (mode_)
        {
        case GizmoMode::TRANSLATE:
            intersects = RayIntersectsArrow(ray, worldTransform, 1.0f, 0.05f);
            break;
        case GizmoMode::ROTATE:
            intersects = RayIntersectsRing(ray, worldTransform, 0.8f, 1.0f);
            break;
        case GizmoMode::SCALE:
            intersects = RayIntersectsCube(ray, worldTransform, 0.1f);
            break;
        }

        if (intersects)
        {
            // Calculate distance from ray origin to gizmo position
            glm::vec3 gizmoWorldPos = glm::vec3(worldTransform[3]);
            float distance = glm::length(gizmoWorldPos - ray.origin);

            if (distance < closestDistance)
            {
                closestDistance = distance;
                closestAxis = part.axis;
            }
        }
    }

    return closestAxis;
}

void Gizmo::CreateTranslateGizmo()
{
    // X axis arrow
    auto xArrow = CreateArrowMesh(1.0f, 0.05f);
    glm::mat4 xTransform = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    parts_.emplace_back(GizmoAxis::X, xArrow, xTransform, GizmoColors::XAxis);

    // Y axis arrow
    auto yArrow = CreateArrowMesh(1.0f, 0.05f);
    glm::mat4 yTransform = glm::mat4(1.0f);
    parts_.emplace_back(GizmoAxis::Y, yArrow, yTransform, GizmoColors::YAxis);

    // Z axis arrow
    auto zArrow = CreateArrowMesh(1.0f, 0.05f);
    glm::mat4 zTransform = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    parts_.emplace_back(GizmoAxis::Z, zArrow, zTransform, GizmoColors::ZAxis);
}

void Gizmo::CreateRotateGizmo()
{
    // X axis ring
    auto xRing = CreateRingMesh(0.8f, 1.0f);
    glm::mat4 xTransform = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    parts_.emplace_back(GizmoAxis::X, xRing, xTransform, GizmoColors::XAxis);

    // Y axis ring
    auto yRing = CreateRingMesh(0.8f, 1.0f);
    glm::mat4 yTransform = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    parts_.emplace_back(GizmoAxis::Y, yRing, yTransform, GizmoColors::YAxis);

    // Z axis ring
    auto zRing = CreateRingMesh(0.8f, 1.0f);
    glm::mat4 zTransform = glm::mat4(1.0f);
    parts_.emplace_back(GizmoAxis::Z, zRing, zTransform, GizmoColors::ZAxis);
}

void Gizmo::CreateScaleGizmo()
{
    // X axis cube
    auto xCube = CreateCubeMesh(0.1f);
    glm::mat4 xTransform = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    parts_.emplace_back(GizmoAxis::X, xCube, xTransform, GizmoColors::XAxis);

    // Y axis cube
    auto yCube = CreateCubeMesh(0.1f);
    glm::mat4 yTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    parts_.emplace_back(GizmoAxis::Y, yCube, yTransform, GizmoColors::YAxis);

    // Z axis cube
    auto zCube = CreateCubeMesh(0.1f);
    glm::mat4 zTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    parts_.emplace_back(GizmoAxis::Z, zCube, zTransform, GizmoColors::ZAxis);

    // Center cube for uniform scaling
    auto centerCube = CreateCubeMesh(0.15f);
    glm::mat4 centerTransform = glm::mat4(1.0f);
    parts_.emplace_back(GizmoAxis::XYZ, centerCube, centerTransform, glm::vec3(0.7f));
}

void Gizmo::ClearParts()
{
    parts_.clear();
}

std::shared_ptr<Mesh> Gizmo::CreateArrowMesh(float length, float radius, int segments)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Create cylinder part
    float cylinderLength = length * 0.8f;
    for (int i = 0; i <= segments; ++i)
    {
        float angle = 2.0f * glm::pi<float>() * i / segments;
        float x = cos(angle) * radius;
        float z = sin(angle) * radius;

        // Bottom vertex
        vertices.push_back({glm::vec3(x, 0.0f, z), glm::vec3(x, 0.0f, z), glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), glm::vec2(0.0f)});
        // Top vertex
        vertices.push_back({glm::vec3(x, cylinderLength, z), glm::vec3(x, 0.0f, z), glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), glm::vec2(0.0f)});
    }

    // Create cone part (arrow head)
    float coneBaseRadius = radius * 2.0f;
    float coneHeight = length * 0.2f;
    float coneStartY = cylinderLength;

    for (int i = 0; i <= segments; ++i)
    {
        float angle = 2.0f * glm::pi<float>() * i / segments;
        float x = cos(angle) * coneBaseRadius;
        float z = sin(angle) * coneBaseRadius;

        // Base vertex
        vertices.push_back({glm::vec3(x, coneStartY, z), glm::vec3(x, 0.0f, z), glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), glm::vec2(0.0f)});
    }
    // Cone tip
    vertices.push_back({glm::vec3(0.0f, coneStartY + coneHeight, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), glm::vec2(0.0f)});

    // Generate indices for cylinder
    for (int i = 0; i < segments; ++i)
    {
        int base = i * 2;
        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);

        indices.push_back(base + 1);
        indices.push_back(base + 3);
        indices.push_back(base + 2);
    }

    // Generate indices for cone
    int coneBaseStart = segments * 2 + 2;
    int coneTipIndex = vertices.size() - 1;

    for (int i = 0; i < segments; ++i)
    {
        int current = coneBaseStart + i;
        int next = coneBaseStart + (i + 1) % segments;

        indices.push_back(current);
        indices.push_back(next);
        indices.push_back(coneTipIndex);
    }

    return Mesh::Create(vertices, indices);
}

std::shared_ptr<Mesh> Gizmo::CreateRingMesh(float innerRadius, float outerRadius, int segments)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    for (int i = 0; i < segments; ++i)
    {
        float angle = 2.0f * glm::pi<float>() * i / segments;

        // Inner vertex
        float innerX = cos(angle) * innerRadius;
        float innerZ = sin(angle) * innerRadius;
        vertices.push_back({glm::vec3(innerX, 0.0f, innerZ), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), glm::vec2(0.0f)});

        // Outer vertex
        float outerX = cos(angle) * outerRadius;
        float outerZ = sin(angle) * outerRadius;
        vertices.push_back({glm::vec3(outerX, 0.0f, outerZ), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), glm::vec2(0.0f)});
    }

    // Generate quad indices
    for (int i = 0; i < segments; ++i)
    {
        int current = i * 2;
        int next = ((i + 1) % segments) * 2;

        // First triangle
        indices.push_back(current);
        indices.push_back(current + 1);
        indices.push_back(next + 1);

        // Second triangle
        indices.push_back(current);
        indices.push_back(next + 1);
        indices.push_back(next);
    }

    return Mesh::Create(vertices, indices);
}

std::shared_ptr<Mesh> Gizmo::CreateCubeMesh(float size)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float halfSize = size * 0.5f;

    // Define cube vertices
    glm::vec3 positions[8] = {
        {-halfSize, -halfSize, -halfSize},
        { halfSize, -halfSize, -halfSize},
        { halfSize,  halfSize, -halfSize},
        {-halfSize,  halfSize, -halfSize},
        {-halfSize, -halfSize,  halfSize},
        { halfSize, -halfSize,  halfSize},
        { halfSize,  halfSize,  halfSize},
        {-halfSize,  halfSize,  halfSize}
    };

    // Define cube normals for each face
    glm::vec3 normals[6] = {
        { 0.0f,  0.0f, -1.0f}, // Front
        { 0.0f,  0.0f,  1.0f}, // Back
        {-1.0f,  0.0f,  0.0f}, // Left
        { 1.0f,  0.0f,  0.0f}, // Right
        { 0.0f,  1.0f,  0.0f}, // Top
        { 0.0f, -1.0f,  0.0f}  // Bottom
    };

    // Create vertices for each face
    for (int face = 0; face < 6; ++face)
    {
        for (int i = 0; i < 4; ++i)
        {
            int vertexIndex = (face * 4) + i;
            vertices.push_back({positions[vertexIndex], normals[face], glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), glm::vec2(0.0f)});
        }
    }

    // Define cube indices
    uint32_t cubeIndices[36] = {
        0, 1, 2, 2, 3, 0, // Front
        4, 5, 6, 6, 7, 4, // Back
        8, 9, 10, 10, 11, 8, // Left
        12, 13, 14, 14, 15, 12, // Right
        16, 17, 18, 18, 19, 16, // Top
        20, 21, 22, 22, 23, 20  // Bottom
    };

    indices.assign(cubeIndices, cubeIndices + 36);

    return Mesh::Create(vertices, indices);
}

std::shared_ptr<Mesh> Gizmo::CreateCylinderMesh(float radius, float height, int segments)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float halfHeight = height * 0.5f;

    // Create cylinder vertices
    for (int i = 0; i <= segments; ++i)
    {
        float angle = 2.0f * glm::pi<float>() * i / segments;
        float x = cos(angle) * radius;
        float z = sin(angle) * radius;

        // Bottom vertex
        vertices.push_back({glm::vec3(x, -halfHeight, z), glm::vec3(x, 0.0f, z), glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), glm::vec2(0.0f)});
        // Top vertex
        vertices.push_back({glm::vec3(x, halfHeight, z), glm::vec3(x, 0.0f, z), glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), glm::vec2(0.0f)});
    }

    // Generate indices
    for (int i = 0; i < segments; ++i)
    {
        int base = i * 2;
        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);

        indices.push_back(base + 1);
        indices.push_back(base + 3);
        indices.push_back(base + 2);
    }

    return Mesh::Create(vertices, indices);
}

bool Gizmo::RayIntersectsArrow(const Ray& ray, const glm::mat4& transform, float length, float radius)
{
    // Simple bounding box intersection for arrow
    glm::vec3 center = glm::vec3(transform[3]);
    glm::vec3 extents = glm::vec3(length * 0.5f, radius, radius);

    return RayIntersectsAABB(ray, center - extents, center + extents);
}

bool Gizmo::RayIntersectsRing(const Ray& ray, const glm::mat4& transform, float innerRadius, float outerRadius)
{
    // Transform ray to local space
    glm::mat4 invTransform = glm::inverse(transform);
    glm::vec3 localOrigin = glm::vec3(invTransform * glm::vec4(ray.origin, 1.0f));
    glm::vec3 localDirection = glm::normalize(glm::vec3(invTransform * glm::vec4(ray.direction, 0.0f)));

    // Check intersection with ring plane (Y = 0)
    if (abs(localDirection.y) < 0.001f) return false;

    float t = -localOrigin.y / localDirection.y;
    if (t < 0.0f) return false;

    glm::vec3 intersection = localOrigin + localDirection * t;
    float distanceFromCenter = sqrt(intersection.x * intersection.x + intersection.z * intersection.z);

    return distanceFromCenter >= innerRadius && distanceFromCenter <= outerRadius;
}

bool Gizmo::RayIntersectsCube(const Ray& ray, const glm::mat4& transform, float size)
{
    glm::vec3 center = glm::vec3(transform[3]);
    glm::vec3 extents = glm::vec3(size * 0.5f);

    return RayIntersectsAABB(ray, center - extents, center + extents);
}

glm::vec3 Gizmo::ScreenToWorld(const glm::vec2& screenPos, float depth, const glm::mat4& viewProjection, int screenWidth, int screenHeight)
{
    glm::vec4 ndc(
        (screenPos.x / screenWidth) * 2.0f - 1.0f,
        1.0f - (screenPos.y / screenHeight) * 2.0f,
        depth * 2.0f - 1.0f,
        1.0f
    );

    glm::vec4 world = glm::inverse(viewProjection) * ndc;
    return glm::vec3(world) / world.w;
}

Ray Gizmo::ScreenPointToRay(const glm::vec2& screenPos, const glm::mat4& viewProjection, int screenWidth, int screenHeight)
{
    glm::vec3 nearPoint = ScreenToWorld(screenPos, 0.0f, viewProjection, screenWidth, screenHeight);
    glm::vec3 farPoint = ScreenToWorld(screenPos, 1.0f, viewProjection, screenWidth, screenHeight);

    return Ray(nearPoint, farPoint - nearPoint);
}

glm::vec2 Gizmo::WorldToScreen(const glm::vec3& worldPos, const glm::mat4& viewProjection, int screenWidth, int screenHeight)
{
    glm::vec4 clip = viewProjection * glm::vec4(worldPos, 1.0f);
    glm::vec3 ndc = glm::vec3(clip) / clip.w;

    return glm::vec2(
        ((ndc.x + 1.0f) * 0.5f) * screenWidth,
        ((1.0f - ndc.y) * 0.5f) * screenHeight
    );
}

// Helper function for AABB ray intersection
bool RayIntersectsAABB(const Ray& ray, const glm::vec3& min, const glm::vec3& max)
{
    glm::vec3 invDir = 1.0f / ray.direction;

    glm::vec3 tMin = (min - ray.origin) * invDir;
    glm::vec3 tMax = (max - ray.origin) * invDir;

    glm::vec3 t1 = glm::min(tMin, tMax);
    glm::vec3 t2 = glm::max(tMin, tMax);

    float tNear = glm::max(glm::max(t1.x, t1.y), t1.z);
    float tFar = glm::min(glm::min(t2.x, t2.y), t2.z);

    return tNear <= tFar && tFar >= 0.0f;
}

// Gizmo interaction handlers
void Gizmo::HandleTranslate(const glm::vec2& mouseDelta, const Camera& camera, int screenWidth, int screenHeight)
{
    if (selectedAxis_ == GizmoAxis::NONE) return;

    // Get camera matrices
    glm::mat4 viewProjection = camera.projection * camera.view;

    // Calculate movement direction based on selected axis
    glm::vec3 axis;
    switch (selectedAxis_)
    {
    case GizmoAxis::X: axis = glm::vec3(1.0f, 0.0f, 0.0f); break;
    case GizmoAxis::Y: axis = glm::vec3(0.0f, 1.0f, 0.0f); break;
    case GizmoAxis::Z: axis = glm::vec3(0.0f, 0.0f, 1.0f); break;
    default: return;
    }

    // Transform axis to world space
    axis = glm::mat3(rotation_) * axis;

    // Project axis onto screen
    glm::vec3 axisStart = position_;
    glm::vec3 axisEnd = position_ + axis;

    glm::vec2 screenStart = WorldToScreen(axisStart, viewProjection, screenWidth, screenHeight);
    glm::vec2 screenEnd = WorldToScreen(axisEnd, viewProjection, screenWidth, screenHeight);
    glm::vec2 screenAxis = glm::normalize(screenEnd - screenStart);

    // Calculate movement along the axis
    float movement = glm::dot(mouseDelta, screenAxis) * 0.01f;

    // Apply translation
    position_ += axis * movement;
}

void Gizmo::HandleRotate(const glm::vec2& mouseDelta, const Camera& camera)
{
    if (selectedAxis_ == GizmoAxis::NONE) return;

    float rotationSpeed = 0.01f;
    glm::vec3 axis;

    switch (selectedAxis_)
    {
    case GizmoAxis::X: axis = glm::vec3(1.0f, 0.0f, 0.0f); break;
    case GizmoAxis::Y: axis = glm::vec3(0.0f, 1.0f, 0.0f); break;
    case GizmoAxis::Z: axis = glm::vec3(0.0f, 0.0f, 1.0f); break;
    default: return;
    }

    // Transform axis to world space
    axis = glm::mat3(rotation_) * axis;

    // Calculate rotation
    float angle = (mouseDelta.x - mouseDelta.y) * rotationSpeed;
    glm::quat deltaRotation = glm::angleAxis(angle, axis);

    // Apply rotation
    rotation_ = deltaRotation * rotation_;
}

void Gizmo::HandleScale(const glm::vec2& mouseDelta, const Camera& camera, int screenWidth, int screenHeight)
{
    if (selectedAxis_ == GizmoAxis::NONE) return;

    float scaleSpeed = 0.01f;
    float scaleDelta = (mouseDelta.x + mouseDelta.y) * scaleSpeed;

    if (selectedAxis_ == GizmoAxis::XYZ)
    {
        // Uniform scaling
        scale_ += scaleDelta;
        scale_ = glm::max(scale_, 0.1f);
    }
    else
    {
        // Axis-specific scaling would require more complex implementation
        // For now, just do uniform scaling
        scale_ += scaleDelta;
        scale_ = glm::max(scale_, 0.1f);
    }
}

// Utility functions
Ray CreateRayFromScreen(const glm::vec2& screenPos, const Camera& camera, int screenWidth, int screenHeight)
{
    glm::mat4 viewProjection = camera.projection * camera.view;

    glm::vec4 ndc(
        (screenPos.x / screenWidth) * 2.0f - 1.0f,
        1.0f - (screenPos.y / screenHeight) * 2.0f,
        -1.0f, // Near plane
        1.0f
    );

    glm::vec4 worldNear = glm::inverse(viewProjection) * ndc;
    worldNear /= worldNear.w;

    ndc.z = 1.0f; // Far plane
    glm::vec4 worldFar = glm::inverse(viewProjection) * ndc;
    worldFar /= worldFar.w;

    return Ray(glm::vec3(worldNear), glm::normalize(glm::vec3(worldFar - worldNear)));
}

glm::vec3 ProjectPointOnPlane(const glm::vec3& point, const glm::vec3& planeNormal, const glm::vec3& planePoint)
{
    float distance = glm::dot(planeNormal, point - planePoint);
    return point - planeNormal * distance;
}

glm::vec3 ProjectPointOnLine(const glm::vec3& point, const glm::vec3& lineStart, const glm::vec3& lineEnd)
{
    glm::vec3 lineDir = glm::normalize(lineEnd - lineStart);
    float t = glm::dot(point - lineStart, lineDir);
    return lineStart + lineDir * t;
}
