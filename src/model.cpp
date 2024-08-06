#include "error.hpp"
#include <assimp/material.h>
#include <model.hpp>
#include <stdexcept>


Model::Model(const string &path, glm::vec3 position, glm::vec3 rotation)
{
    loadModel(path);
    
    SetPosition(position);
    SetRotation(rotation);
}

void Model::loadModel(string path)
{
    Assimp::Importer import;
    const aiScene *scene = import.ReadFile(path, aiProcess_Triangulate | /*aiProcess_ForceGenNormals | aiProcess_GenSmoothNormals |*/ aiProcess_FlipUVs/* | aiProcess_CalcTangentSpace*/);	
    
    if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) 
        throw std::runtime_error(fmt::format("Couldn't load models from assimp: {}", import.GetErrorString()));
    
    m_Directory = path.substr(0, path.find_last_of('/'));

    processNode(scene->mRootNode, scene);
}

void Model::processNode(aiNode *node, const aiScene *scene)
{
    // process all the node's meshes (if any)
    for(unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh *mesh = scene->mMeshes[node->mMeshes[i]]; 
        meshes.push_back(processMesh(mesh, scene));
    }
    // then do the same for each of its children
    for(unsigned int i = 0; i < node->mNumChildren; i++)
    {
        processNode(node->mChildren[i], scene);
    }
}

Mesh Model::processMesh(aiMesh *mesh, const aiScene *scene)
{
    vector<Vertex> vertices;
    vector<unsigned int> indices;
    string diffuseMap;

    glm::vec3 diffuse;
    //float shininess = 0.0;
    //float roughness = 0.1;
    //float metallic = 0.0;

    for(unsigned int i = 0; i < mesh->mNumVertices; i++)
    {
        Vertex vertex;
        // process vertex positions, normals and texture coordinates
        glm::vec3 vector; // we declare a placeholder vector since assimp uses its own vector class that doesn't directly convert to glm's vec3 class.
        // positions
        vector.x = mesh->mVertices[i].x;
        vector.y = mesh->mVertices[i].y;
        vector.z = mesh->mVertices[i].z;
        vertex.Position = vector;

        if (vector.x > boundingBox[0].x)
            boundingBox[0].x = vector.x;
        if (vector.x < boundingBox[1].x)
            boundingBox[1].x = vector.x;

        if (vector.y > boundingBox[0].y)
            boundingBox[0].y = vector.y;
        if (vector.y < boundingBox[1].y)
            boundingBox[1].y = vector.y;

        if (vector.z > boundingBox[0].z)
            boundingBox[0].z = vector.z;
        if (vector.z < boundingBox[1].z)
            boundingBox[1].z = vector.z;

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
    if(mesh->mMaterialIndex >= 0)
    {
        aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
        // material->Get(AI_MATKEY_SHININESS, shininess);
        // material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
        // material->Get(AI_MATKEY_METALLIC_FACTOR, metallic);

        size_t diffuseTextureCount = material->GetTextureCount(aiTextureType_DIFFUSE);
        if (diffuseTextureCount >= 1) {
            aiString path;
            material->GetTexture(aiTextureType_DIFFUSE, 0, &path);
            diffuseMap = string(path.C_Str());
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
    } else {
        throw std::runtime_error(engineError::NO_MATERIALS);
    }

    return Mesh(vertices, indices, diffuseMap/*, shininess, roughness, metallic*/, diffuse);
}

glm::mat4 Model::GetModelMatrix() {
    if (!m_NeedsUpdate)
        return m_ModelMatrix;

    // Update the model matrix with the position/rotation.
    m_ModelMatrix = glm::translate(glm::mat4(1.0f), m_Position);
    m_ModelMatrix *= glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    m_ModelMatrix *= glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    m_ModelMatrix *= glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

    m_NeedsUpdate = false;

    return m_ModelMatrix;
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