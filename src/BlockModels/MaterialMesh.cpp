//
// Created by cew05 on 26/07/2024.
//

#include "MaterialMesh.h"

#include "../Window.h"

MaterialMesh::MaterialMesh(Block* _block) {
    glGenVertexArrays(1, &vertexArrayObject);
    glGenBuffers(1, &vertexBufferObject);
    glGenBuffers(1, &indexBufferObject);

    block = _block;
}

MaterialMesh::~MaterialMesh() {
    glDeleteBuffers(1, &vertexBufferObject);
    glDeleteBuffers(1, &indexBufferObject);
    glDeleteVertexArrays(1, &vertexArrayObject);
}

void MaterialMesh::AddVerticies(const std::vector<Vertex>& _verticies, const glm::vec3& _position) {
    for (const Vertex& vertex : _verticies) {
        vertexArray.push_back({vertex.position + _position, vertex.textureCoord});
    }

    oldMesh = true;
    readyToBind = false;
}

void MaterialMesh::ResetVerticies() {
    vertexArray.clear();
    oldMesh = true;
    readyToBind = false;
}

void MaterialMesh::BindMesh() {
    if (vertexArrayObject == 0) {
        // For when the material mesh is created by the chunk meshing thread
        glGenVertexArrays(1, &vertexArrayObject);
        glGenBuffers(1, &vertexBufferObject);
        glGenBuffers(1, &indexBufferObject);
    }

    // Recreate the buffers only when more verticies than current size need to be written. Else glBufferSubData
    if ((int)vertexArray.size() < bufferVerticiesSize) {
        return UpdateMesh();
    }

    // Ensure a minimum buffer size of 300 verticies is created to prevent excessive recreation for small buffers
    bufferVerticiesSize = std::max(300, (int)vertexArray.size() * 2);

    glBindVertexArray(vertexArrayObject);

    // bind vertex buffer object
    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject);
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(bufferVerticiesSize * sizeof(Vertex)), nullptr, GL_DYNAMIC_DRAW);

    // Vertex Position Attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct Vertex), (const GLvoid*)offsetof(Vertex, position));

    // Vertex TextureData attributes
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(struct Vertex), (const GLvoid*)offsetof(Vertex, textureCoord));

    // Bind index buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferObject);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(bufferVerticiesSize * sizeof(GLuint)), nullptr, GL_DYNAMIC_DRAW);

    // Unbind arrays / buffers
    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    UpdateMesh();
}

void MaterialMesh::UpdateMesh() {


    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject);
    glBufferSubData(GL_ARRAY_BUFFER, 0, GLsizeiptr(vertexArray.size() * sizeof(Vertex)), vertexArray.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // populate indexArray
    std::vector<GLuint> indexArray {};
    for (int f = 0; f < (int)vertexArray.size() / 4; f++) {
        indexArray.push_back(f*4 + 1);
        indexArray.push_back(f*4 + 3);
        indexArray.push_back(f*4 + 0);
        indexArray.push_back(f*4 + 0);
        indexArray.push_back(f*4 + 3);
        indexArray.push_back(f*4 + 2);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferObject);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, GLsizeiptr(indexArray.size()*sizeof(GLuint)), indexArray.data());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);


    boundFaces = (int)vertexArray.size()/4;
    oldMesh = false;
}

void MaterialMesh::DrawMesh(const Transformation& _transformation) const {
    // Bind object
    glBindVertexArray(vertexArrayObject);

    // Update uniform
    GLint modelMatrixLocation = glGetUniformLocation(window.GetShader(), "matricies.uModelMatrix");
    if (modelMatrixLocation < 0) printf("mesh location not found [matricies.uModelMatrix]\n");
    if (modelMatrixLocation >= 0) glUniformMatrix4fv(modelMatrixLocation, 1, GL_FALSE, &_transformation.GetModelMatrix()[0][0]);

    // Set texture information
    textureManager->EnableTextureSheet(block->GetTextureSheet());
    GLint vtcOffsetLocation = glGetUniformLocation(window.GetShader(), "uVertexTextureCoordOffset");
    if (vtcOffsetLocation < 0) printf("sun location not found [uVertexTextureCoordOffset]\n");
    if (vtcOffsetLocation >= 0) glUniform2fv(vtcOffsetLocation, 1, &block->GetTextureOrigin()[0]);

    GLint canFogLocation = glGetUniformLocation(window.GetShader(), "uCanFog");
    if (canFogLocation < 0) printf("block location not found [uCanFog]\n");
    if (canFogLocation >= 0) glUniform1i(canFogLocation, 1);

    // Draw block mesh
    glDrawElements(GL_TRIANGLES, 6 * boundFaces, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}