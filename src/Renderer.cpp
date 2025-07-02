#include "Renderer.h"
#include "VertexArray.h"
#include "IndexBuffer.h"

#include <glad/glad.h>

void Renderer::Draw(std::shared_ptr<VertexArray> vertexArray, uint32_t count)
{
    vertexArray->Bind();
    glDrawArrays(GL_TRIANGLES, 0, count);
}

void Renderer::DrawIndexed(std::shared_ptr<VertexArray> vertexArray, std::shared_ptr<IndexBuffer> indexBuffer)
{
    uint32_t count = 0;
    if (indexBuffer)
    {
        indexBuffer->Bind();
        count = indexBuffer->GetCount();
    }
    else
    {
        vertexArray->GetIndexBuffer()->Bind();
        count = vertexArray->GetIndexBuffer()->GetCount();
    }

    vertexArray->Bind();
    glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
}