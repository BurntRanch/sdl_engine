#include "error.hpp"
#include "fmt/format.h"
#include "object.hpp"

#include <algorithm>
#include <assimp/material.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <model.hpp>
#include <stdexcept>
#include <vector>

Mesh Model::processMesh(aiMesh *mesh, const aiScene *scene)
{
    vector<Vertex> vertices;
    vector<Uint32> indices;
    string diffuseMap;

    glm::vec3 diffuse;
    //float shininess = 0.0;
    //float roughness = 0.1;
    //float metallic = 0.0;

    for(size_t i = 0; i < mesh->mNumVertices; i++)
    {
        Vertex vertex;
        // process vertex positions, normals and texture coordinates
        glm::vec3 vector; // we declare a placeholder vector since assimp uses its own vector class that doesn't directly convert to glm's vec3 class.
        // positions
        vector.x = mesh->mVertices[i].x;
        vector.y = mesh->mVertices[i].y;
        vector.z = mesh->mVertices[i].z;
        vertex.Position = vector;

        if (vector.x > m_BoundingBox[0].x)
            m_BoundingBox[0].x = vector.x;
        if (vector.x < m_BoundingBox[1].x)
            m_BoundingBox[1].x = vector.x;

        if (vector.y > m_BoundingBox[0].y)
            m_BoundingBox[0].y = vector.y;
        if (vector.y < m_BoundingBox[1].y)
            m_BoundingBox[1].y = vector.y;

        if (vector.z > m_BoundingBox[0].z)
            m_BoundingBox[0].z = vector.z;
        if (vector.z < m_BoundingBox[1].z)
            m_BoundingBox[1].z = vector.z;

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
    for(unsigned int i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }
        
    // process material
    {
        aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
        // material->Get(AI_MATKEY_SHININESS, shininess);
        // material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
        // material->Get(AI_MATKEY_METALLIC_FACTOR, metallic);

        size_t diffuseTextureCount = material->GetTextureCount(aiTextureType_DIFFUSE);
        if (diffuseTextureCount >= 1) {
            aiString path;
            material->GetTexture(aiTextureType_DIFFUSE, 0, &path);
            diffuseMap = string(std::filesystem::absolute(path.C_Str()));
        }

        aiColor4D aiDiffuseColor;
        aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &aiDiffuseColor);
        diffuse = {aiDiffuseColor.r, aiDiffuseColor.g, aiDiffuseColor.b};
        // vector<Texture> specularMaps = loadMaterialTextures(material, 
        //                                     aiTextureType_SPECULAR, "texture_specular");
        // textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
        // // assimp for some reason loads some normal maps as heightmaps
        // vector<Texture> normalMaps = loadMaterialTextures(material, 
        //                                     aiTextureType_HEIGHT, "texture_normal");
        // textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
    }

    return Mesh(*this, vertices, indices, diffuseMap/*, shininess, roughness, metallic*/, diffuse);
}

glm::mat4 Model::GetModelMatrix() {
    /* This used to exist before, but now we're not sure when the object attachment moves. */
    // if (!m_NeedsUpdate)
    //     return m_ModelMatrix;

    // fmt::println("Updating model matrix for object {}!", fmt::ptr(this));

    // Update the model matrix with the position/rotation.
    if (m_ObjectAttachment) {
        m_ModelMatrix = glm::translate(glm::mat4(1.0f), m_ObjectAttachment->GetPosition());
        m_ModelMatrix *= glm::mat4_cast(m_ObjectAttachment->GetRotation());
        m_ModelMatrix *= glm::scale(glm::mat4(1.0f), m_ObjectAttachment->GetScale());
    }

    return m_ModelMatrix;
}

void Model::SetObjectAttachment(Object *object) {
    m_ObjectAttachment = object;
}

Object *Model::GetObjectAttachment() {
    return m_ObjectAttachment;
}

int Model::GetModelID() {
    return m_ModelID;
}

void Model::SetModelID(int modelID) {
    m_ModelID = modelID;
}

//Texture Model::loadDefaultTexture(string typeName) {
//    Texture tex;
//    string str = "default_" + typeName + ".png";
//    bool skip = false;
//    for(unsigned int j = 0; j < textures_loaded.size(); j++)
//    {
//        if(std::strcmp(textures_loaded[j].path.data(), str.c_str()) == 0)
//        {
//            tex = textures_loaded[j];
//            skip = true; 
//            break;
//        }
//    }
//    if(!skip)
//    {   // if texture hasn't been loaded already, load it
//        Texture texture;
//        texture.id = TextureFromFile(str.c_str(), directory);
//        texture.type = typeName;
//        texture.path = str.c_str();
//        tex = texture;
//        textures_loaded.push_back(texture); // add to loaded textures
//    }
//    return tex;
//}
//
//vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, string typeName)
//{
//    vector<Texture> textures;
//    if (mat->GetTextureCount(type) == 0) {
//        textures.push_back(loadDefaultTexture(typeName));
//    }
//    for(unsigned int i = 0; i < mat->GetTextureCount(type); i++)
//    {
//        aiString str;
//        mat->GetTexture(type, i, &str);
//        bool skip = false;
//        for(unsigned int j = 0; j < textures_loaded.size(); j++)
//        {
//            if(std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0)
//            {
//                textures.push_back(textures_loaded[j]);
//                skip = true; 
//                break;
//            }
//        }
//        if(!skip)
//        {   // if texture hasn't been loaded already, load it
//            Texture texture;
//            texture.id = TextureFromFile(str.C_Str(), directory);
//            texture.type = typeName;
//            texture.path = str.C_Str();
//            textures.push_back(texture);
//            textures_loaded.push_back(texture); // add to loaded textures
//        }
//    }
//    return textures;
//}
