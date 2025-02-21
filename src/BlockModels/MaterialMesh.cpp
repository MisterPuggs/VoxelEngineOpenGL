//
// Created by cew05 on 26/07/2024.
//

#include "MaterialMesh.h"

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

void MaterialMesh::AddVerticies(const std::vector<UniqueVertex>& _verticies, const glm::vec3& _position) {
    if (_verticies.empty()) return;

    for (const UniqueVertex& vertex : _verticies) {
        vertexArray.push_back(vertex);
        vertexArray.back().chunkPosition = _position;
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

    // Enough space for all verticies to be bound
    if ((int)vertexArray.size() <= bufferVerticiesSize) {
        return UpdateMesh();
    }

    // (else) More space is required in Buffers

    // Ensure minimum buffer size
    bufferVerticiesSize = std::max(400, bufferVerticiesSize);
    bufferIndiciesSize = std::max(600, bufferIndiciesSize);

    // Determine new buffer sizes
    bufferVerticiesSize = (int)vertexArray.size() * 2;
    bufferIndiciesSize = (bufferVerticiesSize / 4) * 6;

    // bind vao
    glBindVertexArray(vertexArrayObject);

    // bind vertex buffer object
    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject);
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(bufferVerticiesSize * sizeof(UniqueVertex)), nullptr, GL_DYNAMIC_DRAW);

    // Vertex Position, Model, FaceAxis Attributes
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct UniqueVertex), (const GLvoid*)offsetof(UniqueVertex, chunkPosition));
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(struct UniqueVertex), (const GLvoid*)offsetof(UniqueVertex, modelVertex));
    glVertexAttribPointer(2, 3, GL_BYTE, GL_FALSE, sizeof(struct UniqueVertex), (const GLvoid*)offsetof(UniqueVertex, blockOffset));
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(struct UniqueVertex), (const GLvoid*)offsetof(UniqueVertex, normalAxis));

    // Vertex Texture, Rotation, Light attributes
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(struct UniqueVertex), (const GLvoid*)offsetof(UniqueVertex, textureCoord));
    glVertexAttribPointer(5, 2, GL_BYTE, GL_FALSE, sizeof(struct UniqueVertex), (const GLvoid*)offsetof(UniqueVertex, blockRotation));
    glVertexAttribPointer(6, 1, GL_BYTE, GL_FALSE, sizeof(struct UniqueVertex), (const GLvoid*)offsetof(UniqueVertex, occlusion));
    glVertexAttribPointer(7, 1, GL_BYTE, GL_FALSE, sizeof(struct UniqueVertex), (const GLvoid*)offsetof(UniqueVertex, lightLevel));

    // Bind index buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferObject);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(bufferIndiciesSize * sizeof(GLuint)), nullptr, GL_DYNAMIC_DRAW);

    // update bound value
    boundFaces = (int)vertexArray.size() / 4;

    // Unbind arrays / buffers
    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    UpdateMesh();
}

void MaterialMesh::UpdateMesh() {
    boundFaces = (int)vertexArray.size() / 4;

    // populate indexArray
    std::vector<GLuint> indexArray {};
    for (int f = 0; f < boundFaces; f++) {
        GLbyte o0 = vertexArray[f*4 + 0].occlusion, o1 = vertexArray[f*4 + 1].occlusion;
        GLbyte o2 = vertexArray[f*4 + 2].occlusion, o3 = vertexArray[f*4 + 3].occlusion;

        if ((o1 + o2 > o0 + o3) || (o3 == 0 && (o1 + o2 == o0 + o3))) {
            indexArray.push_back(f*4 + 3);
            indexArray.push_back(f*4 + 2);
            indexArray.push_back(f*4 + 1);
            indexArray.push_back(f*4 + 1);
            indexArray.push_back(f*4 + 2);
            indexArray.push_back(f*4 + 0);
        }
        else {
            indexArray.push_back(f * 4 + 1);
            indexArray.push_back(f * 4 + 3);
            indexArray.push_back(f * 4 + 0);
            indexArray.push_back(f * 4 + 0);
            indexArray.push_back(f * 4 + 3);
            indexArray.push_back(f * 4 + 2);
        }
    }

    // Bind face verticies to buffer
    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject);
    glBufferSubData(GL_ARRAY_BUFFER, 0, GLsizeiptr(vertexArray.size() * sizeof(UniqueVertex)), vertexArray.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Bind indicies to buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferObject);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, GLsizeiptr(indexArray.size()*sizeof(GLuint)), indexArray.data());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    oldMesh = false;
}

void MaterialMesh::DrawMesh(const Transformation& _transformation) const {
    window.SetShader(Window::BASEMESH);

    GLint uniformLocation;

    // Update model matrix with chunk transformation
    uniformLocation = glGetUniformLocation(window.GetShader(), "matricies.uModelMatrix");
    if (uniformLocation >= 0) glUniformMatrix4fv(uniformLocation, 1, GL_FALSE, &_transformation.GetModelMatrix()[0][0]);

    // Set blockModel
    uniformLocation = glGetUniformLocation(window.GetShader(), "uBlockModel");
    if (uniformLocation >= 0) glUniform1i(uniformLocation, block->GetSharedAttribute(BLOCKATTRIBUTE::BLOCKMODEL));

    // Set texture information
    textureManager->EnableTextureSheet(block->GetTextureSheet());
    uniformLocation = glGetUniformLocation(window.GetShader(), "uVertexTextureCoordOffset");
    if (uniformLocation >= 0) glUniform2fv(uniformLocation, 1, &block->GetTextureOrigin()[0]);

    // Enable / Disable fog translucency of block
    uniformLocation = glGetUniformLocation(window.GetShader(), "uCanFog");
    if (uniformLocation >= 0) glUniform1i(uniformLocation, 1);

    // Bind and draw block mesh
    glBindVertexArray(vertexArrayObject);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
    glEnableVertexAttribArray(4);
    glEnableVertexAttribArray(5);
    glEnableVertexAttribArray(6);
    glEnableVertexAttribArray(7);

    glDrawElements(GL_TRIANGLES, 6 * boundFaces, GL_UNSIGNED_INT, nullptr);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glDisableVertexAttribArray(3);
    glDisableVertexAttribArray(4);
    glDisableVertexAttribArray(5);
    glDisableVertexAttribArray(6);
    glDisableVertexAttribArray(7);
    glBindVertexArray(0);
}