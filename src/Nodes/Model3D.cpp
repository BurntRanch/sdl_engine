#include "Node/Node3D/Model3D/Model3D.hpp"
#include "BulletCollision/CollisionDispatch/btCollisionObject.h"
#include "BulletCollision/CollisionShapes/btCollisionShape.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "LinearMath/btDefaultMotionState.h"
#include "LinearMath/btMotionState.h"
#include "LinearMath/btQuaternion.h"
#include "LinearMath/btTransform.h"
#include "LinearMath/btVector3.h"

#include "material.hpp"
#include "util.hpp"
#include <SDL3/SDL_stdinc.h>
#include <assimp/Importer.hpp>
#include <assimp/metadata.h>
#include <assimp/quaternion.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <assimp/vector3.h>
#include <functional>
#include <glm/fwd.hpp>
#include <glm/trigonometric.hpp>
#include <memory>

void Mesh3D::SetMaterial(const Material &material) {
    m_Material = material;
}
const Material &Mesh3D::GetMaterial() const {
    return m_Material;
}

const std::vector<Vertex> &Mesh3D::GetVertices() const {
    return m_Vertices;
}
const std::vector<Uint32> &Mesh3D::GetIndices() const {
    return m_Indices;
}

void Model3D::ImportFromAssimpNode(const aiNode *aiNode, const aiScene *aiScene) {
    UTILASSERT(aiNode->mNumMeshes > 0);

    m_Meshes.clear();

    for (Uint32 i = 0; i < aiNode->mNumMeshes; ++i) {
        aiMesh *aiMesh = aiScene->mMeshes[aiNode->mMeshes[i]];

        ProcessAndAddMesh(aiMesh, aiScene);
    }
}

const std::vector<Mesh3D> &Model3D::GetMeshes() const {
    return m_Meshes;
}

const glm::mat4 Model3D::GetModelMatrix() const {
    glm::mat4 modelMatrix;

    // Update the model matrix with the absolute position/rotation.
    modelMatrix = glm::translate(glm::mat4(1.0f), GetAbsolutePosition());
    modelMatrix *= glm::mat4_cast(GetAbsoluteRotation());
    modelMatrix *= glm::scale(glm::mat4(1.0f), GetAbsoluteScale());

    return modelMatrix;
}

void Model3D::ProcessAndAddMesh(const aiMesh *mesh, const aiScene *scene) {
    std::vector<Vertex> vertices;
    std::vector<Uint32> indices;
    std::string diffuseMap;

    glm::vec3 diffuse;
    //float shininess = 0.0;

    for(size_t i = 0; i < mesh->mNumVertices; ++i)
    {
        Vertex vertex;
        // process vertex positions, normals and texture coordinates
        glm::vec3 vector; // we declare a placeholder vector since assimp uses its own vector class that doesn't directly convert to glm's vec3 class.
        // positions
        vector.x = mesh->mVertices[i].x;
        vector.y = mesh->mVertices[i].y;
        vector.z = mesh->mVertices[i].z;
        vertex.Position = vector;

        // normals
        if (mesh->HasNormals())
        {
            vector.x = mesh->mNormals[i].x;
            vector.y = mesh->mNormals[i].y;
            vector.z = mesh->mNormals[i].z;
            vertex.Normal = vector;
        }

        // texture coordinates
        if(mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
        {
            glm::vec2 vec;
            // a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't 
            // use models where a vertex can have multiple texture coordinates so we always take the first set (0).
            vec.x = mesh->mTextureCoords[0][i].x; 
            vec.y = mesh->mTextureCoords[0][i].y;
            vertex.TexCoord = vec;
        } else {
            vertex.TexCoord = glm::vec2(0.0f, 0.0f);
        }
        
        // if (mesh->HasTangentsAndBitangents())
        // {
        //     glm::vec3 tan;
        //     glm::vec3 biTan;
        //     tan.x = mesh->mTangents[i].x;
        //     tan.y = mesh->mTangents[i].y;
        //     tan.z = mesh->mTangents[i].z;
        //     biTan.x = mesh->mBitangents[i].x;
        //     biTan.y = mesh->mBitangents[i].y;
        //     biTan.z = mesh->mBitangents[i].z;
        //     vertex.Tangent = tan;
        //     vertex.BiTangent = biTan;
        // }
        // else
        // {
        //     vertex.Tangent = glm::vec3(0.0f, 0.0f, 1.0f);
        //     vertex.BiTangent = vertex.Tangent * vertex.Normal;
        // }

        vertices.push_back(vertex);
    }
    // process indices
    for(unsigned int i = 0; i < mesh->mNumFaces; ++i)
    {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; ++j) {
            indices.push_back(face.mIndices[j]);
        }
    }
        
    // process material

    /* TODO: don't assume PBRMaterial, I'm doing this only because it's the only valid assumption right now */
    PBRMaterial *material = new PBRMaterial();

    /* TODO: read and support more and more AI_MATKEYs */
    {
        aiMaterial *aiMaterial = scene->mMaterials[mesh->mMaterialIndex];
        float roughness;
        float metallic;
        // material->Get(AI_MATKEY_SHININESS, shininess);
        aiMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
        aiMaterial->Get(AI_MATKEY_METALLIC_FACTOR, metallic);

        size_t diffuseTextureCount = aiMaterial->GetTextureCount(aiTextureType_DIFFUSE);
        if (diffuseTextureCount >= 1) {
            aiString path;
            aiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &path);
            diffuseMap = std::string(std::filesystem::absolute(path.C_Str()));
        }

        aiColor4D aiDiffuseColor;
        aiGetMaterialColor(aiMaterial, AI_MATKEY_COLOR_DIFFUSE, &aiDiffuseColor);
        diffuse = {aiDiffuseColor.r, aiDiffuseColor.g, aiDiffuseColor.b};

        /* TODO: diffuse maps */
        material->SetColor(diffuse);
        material->SetMetallicFactor(metallic);
        material->SetRoughnessFactor(roughness);
        // vector<Texture> specularMaps = loadMaterialTextures(material, 
        //                                     aiTextureType_SPECULAR, "texture_specular");
        // textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
        // // assimp for some reason loads some normal maps as heightmaps
        // vector<Texture> normalMaps = loadMaterialTextures(material, 
        //                                     aiTextureType_HEIGHT, "texture_normal");
        // textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
    }

    m_Meshes.push_back(Mesh3D(vertices, indices, *material));
}
