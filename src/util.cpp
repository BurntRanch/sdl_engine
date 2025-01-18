#include "util.hpp"
#include "BulletCollision/CollisionDispatch/btCollisionObject.h"
#include "BulletCollision/CollisionShapes/btBoxShape.h"
#include "BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h"
#include "BulletCollision/CollisionShapes/btCollisionShape.h"
#include "BulletCollision/CollisionShapes/btConcaveShape.h"
#include "BulletCollision/CollisionShapes/btStridingMeshInterface.h"
#include "BulletCollision/CollisionShapes/btTriangleIndexVertexArray.h"
#include "LinearMath/btVector3.h"
#include "camera.hpp"
#include "engine.hpp"
#include "isteamnetworkingsockets.h"
#include "rapidxml.hpp"
#include "ui/label.hpp"
#include "common.hpp"
#include <algorithm>
#include <assimp/metadata.h>
#include <sstream>
#include <stdexcept>
#include "switch_fnv1a.h"

bool endsWith(const std::string_view fullString, const std::string_view ending) {
    if (ending.length() > fullString.length())
        return false;
    return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
}

std::vector<std::string> split(const std::string_view text, const char delim) {
    std::string                 line;
    std::vector<std::string>    vec;
    std::stringstream ss(text.data());
    while (std::getline(ss, line, delim)) {
        vec.push_back(line);
    }
    return vec;
}

Object *DeepSearchObjectTree(Object *obj, std::function<bool(Object *)> pred) {
    for (Object *child : obj->GetChildren()) {
        if (pred(child)) {
            return child;
        }

        Object *deepSearchResult = DeepSearchObjectTree(child, pred);
        if (deepSearchResult != nullptr) {
            return deepSearchResult;
        }
    }

    return nullptr;
}

std::vector<int> FilterRelatedNetworkingObjects(std::vector<Networking_Object> &candidates, Networking_Object *object) {
    std::vector<int> relatedObjects;

    for (size_t i = 0; i < candidates.size(); i++) {
        if (std::find(object->children.begin(), object->children.end(), candidates[i].ObjectID) != object->children.end()) {
            relatedObjects.push_back(i);

            /* Go over any candidates that we may have missed. */
            std::vector<Networking_Object> previousCandidates{candidates.begin(), candidates.begin() + i};

            std::vector<int> relatedPreviousCandidates = FilterRelatedNetworkingObjects(previousCandidates, &candidates[i]);

            if (!relatedPreviousCandidates.empty()) {
                relatedObjects.insert(relatedObjects.end(), relatedPreviousCandidates.begin(), relatedPreviousCandidates.end());
            }

            continue;
        }
    }

    return relatedObjects;
}

bool intersects(const glm::vec3 &origin, const glm::vec3 &front, const std::array<glm::vec3, 2> &boundingBox) {
    const glm::vec3 inverse_front = 1.0f / front;

    const glm::vec3 &box_max = boundingBox[0];
    const glm::vec3 &box_min = boundingBox[1];

    const float t1 = (box_min.x - origin.x) * inverse_front.x;
    const float t2 = (box_max.x - origin.x) * inverse_front.x;

    float t_near = std::max(CAMERA_NEAR, glm::min(t1, t2));
    float t_far  = std::min(CAMERA_FAR,  glm::max(t1, t2));

    const float t3 = (box_min.y - origin.y) * inverse_front.y;
    const float t4 = (box_max.y - origin.y) * inverse_front.y;

    t_near = glm::max(t_near, glm::min(t3, t4));
    t_far  = glm::min(t_far,  glm::max(t3, t4));

    return t_near <= t_far && t_far >= 0;
}

glm::vec2 adjustScaleToFitType(UI::Scalable *self, glm::vec2 scale, UI::FitType fitType) {
    if (fitType == UI::UNSET)
        fitType = self->fitType;
    
    UI::Scalable *element = self;   // Used in UNSET handling
    switch (fitType) {
        case UI::UNSET:
            while (element->GetParent() && element->GetParent()->genericType == UI::SCALABLE) {
                element = reinterpret_cast<UI::Scalable *>(element->GetParent());

                if (element->fitType == UI::UNSET) {
                    continue;
                }

                scale = adjustScaleToFitType(self, scale, element->fitType);
            }
        case UI::NONE:
            break;
        case UI::FIT_CHILDREN:
            for (UI::GenericElement *child : self->GetChildren()) {
                if (child->genericType != UI::SCALABLE && child->genericType != UI::LABEL) {
                    continue;
                }

                if (child->genericType == UI::LABEL) {
                    scale = glm::max(scale, reinterpret_cast<UI::Label *>(child)->CalculateMinimumScaleToFit());
                    continue;
                }

                scale = glm::max(scale, reinterpret_cast<UI::Scalable *>(child)->GetScale());
            }
            
            break;
    }

    return scale;
}

rapidxml::xml_node<char>* getPropertiesNode(rapidxml::xml_node<char> *uiObjectNode) {
    using rapidxml::xml_node;

    xml_node<char> *propertiesNode = uiObjectNode->first_node("Properties");
    UTILASSERT(propertiesNode);

    return propertiesNode;
};

std::string getID(rapidxml::xml_node<char> *propertiesNode) {
    using rapidxml::xml_node;

    xml_node<char> *idNode = propertiesNode->first_node("ID");
    UTILASSERT(idNode);
    std::string id = idNode->value();

    return id;
}

glm::vec3 getColor(rapidxml::xml_node<char> *propertiesNode) {
    using rapidxml::xml_node;

    xml_node<char> *colorNode = propertiesNode->first_node("Color");
    UTILASSERT(colorNode);

    xml_node<char> *colorRNode = colorNode->first_node("R");
    UTILASSERT(colorRNode);
    float colorR = std::stof(colorRNode->value());

    xml_node<char> *colorGNode = colorNode->first_node("G");
    UTILASSERT(colorGNode);
    float colorG = std::stof(colorGNode->value());

    xml_node<char> *colorBNode = colorNode->first_node("B");
    UTILASSERT(colorBNode);
    float colorB = std::stof(colorBNode->value());

    return glm::vec3(colorR, colorG, colorB);
}

glm::vec2 getPosition(rapidxml::xml_node<char> *propertiesNode) {
    using rapidxml::xml_node;

    xml_node<char> *positionNode = propertiesNode->first_node("Position");
    UTILASSERT(positionNode);

    xml_node<char> *positionXNode = positionNode->first_node("X");
    UTILASSERT(positionXNode);
    float positionX = std::stof(positionXNode->value());

    xml_node<char> *positionYNode = positionNode->first_node("Y");
    UTILASSERT(positionYNode);
    float positionY = std::stof(positionYNode->value());

    return glm::vec2(positionX, positionY);
}

glm::vec2 getScale(rapidxml::xml_node<char> *propertiesNode) {
    using rapidxml::xml_node;

    xml_node<char> *scaleNode = propertiesNode->first_node("Scale");
    UTILASSERT(scaleNode);

    xml_node<char> *scaleXNode = scaleNode->first_node("X");
    UTILASSERT(scaleXNode);
    float scaleX = std::stof(scaleXNode->value());

    xml_node<char> *scaleYNode = scaleNode->first_node("Y");
    UTILASSERT(scaleYNode);
    float scaleY = std::stof(scaleYNode->value());

    return glm::vec2(scaleX, scaleY);
}

float getZDepth(rapidxml::xml_node<char> *propertiesNode, float depthDefault) {
    using rapidxml::xml_node;

    xml_node<char> *zDepthNode = propertiesNode->first_node("ZDepth");
    float zDepth = depthDefault;

    if (zDepthNode) {
        zDepth = std::stof(zDepthNode->value());
    }

    return zDepth;
}

bool getVisible(rapidxml::xml_node<char> *propertiesNode) {
    using rapidxml::xml_node;

    xml_node<char> *visibleNode = propertiesNode->first_node("Visible");

    if (!visibleNode) {
        return true;
    }

    std::string visibleString = std::string(visibleNode->value());

    std::transform(visibleString.begin(), visibleString.end(), visibleString.begin(), ::tolower);

    if (visibleString == "true") {
        return true;
    }

    return false;
}

btCollisionShape *createBoxShape(glm::vec3 size) {
    btCollisionShape *shapePtr = new btBoxShape(btVector3(size.x, size.y, size.z));

    return shapePtr;
}

struct glTFRigidBody getColliderInfoFromNode(const aiNode *node, const aiScene *scene) {
    struct glTFRigidBody rigidBody{};

    UTILASSERT(node->mMetaData && node->mMetaData->HasKey("extensions"));

    aiMetadata extensionsData;

    node->mMetaData->Get("extensions", extensionsData);

    UTILASSERT(extensionsData.HasKey("KHR_physics_rigid_bodies") && scene->mMetaData && scene->mMetaData->HasKey("extensions"));

    aiMetadata subMetaData;
    extensionsData.Get("KHR_physics_rigid_bodies", subMetaData);

    aiMetadata colliderData;
    subMetaData.Get("collider", colliderData);

    aiMetadata geometryParams;
    colliderData.Get("geometry", geometryParams);

    aiMetadata sceneExtensionsData;
    scene->mMetaData->Get("extensions", sceneExtensionsData);

    if (geometryParams.HasKey("shape")) {
        int shapeIndex = 0;
        geometryParams.Get("shape", shapeIndex);

        UTILASSERT(sceneExtensionsData.HasKey("KHR_implicit_shapes"));

        aiMetadata implicitShapesData;
        sceneExtensionsData.Get("KHR_implicit_shapes", implicitShapesData);

        aiMetadata implicitShapesArray;
        implicitShapesData.Get("shapes", implicitShapesArray);

        aiMetadata shapeData;
        implicitShapesArray.Get(shapeIndex, shapeData);

        aiString shapeType;
        shapeData.Get("type", shapeType);

        switch (fnv1a32::hash(shapeType.C_Str())) {
            case "box"_fnv1a32:
                glm::vec3 boxSize{};

                aiMetadata shapeSize;
                geometryParams.Get("size", shapeSize);

                shapeSize.Get(0, boxSize.x);
                shapeSize.Get(1, boxSize.y);
                shapeSize.Get(2, boxSize.z);

                rigidBody.colliderInfo.shape = createBoxShape(boxSize);
            }

        fmt::println("Shape: {}", shapeType.C_Str());
    } else {
        bool isConvexHull = false;
        geometryParams.Get("convexHull", isConvexHull);

        if (isConvexHull) {
            throw std::runtime_error("Convex Hull rigid bodies are not supported!");
        }

        int targetNodeIdx = 0;
        geometryParams.Get("node", targetNodeIdx);

        std::vector<aiNode *> checkList(scene->mRootNode->mChildren, scene->mRootNode->mChildren + scene->mRootNode->mNumChildren);
        bool stopSearch = false;

        while (!checkList.empty() && !stopSearch) {
            aiNode *node = checkList[0];
            int nodeIdx = 0;

            if (node->mMetaData && node->mMetaData->HasKey("glTF_Index")) {
                node->mMetaData->Get("glTF_Index", nodeIdx);

                if (targetNodeIdx == nodeIdx) {
                    stopSearch = true;
                    continue;
                }
            }

            checkList.erase(checkList.begin());

            for (Uint32 i = 0; i < node->mNumChildren; i++) {
                checkList.push_back(node->mChildren[i]);
            }
        }

        UTILASSERT(stopSearch);
        aiNode *targetNode = checkList[0];

        UTILASSERT(targetNode->mNumMeshes > 0);
        aiMesh *mesh = scene->mMeshes[targetNode->mMeshes[0]];

        btTriangleIndexVertexArray *va = new btTriangleIndexVertexArray();

        Uint32 *indices = new Uint32[mesh->mNumFaces * 3];
        glm::vec3 *vertices = new glm::vec3[mesh->mNumVertices];

        // process indices
        for(unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                indices[i * 3 + j] = face.mIndices[j];
            }
        }

        // process vertices
        for(unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            vertices[i] = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
        }
        
        btIndexedMesh indexedMesh;
        indexedMesh.m_indexType = PHY_INTEGER;
        indexedMesh.m_numTriangles = mesh->mNumFaces;
        indexedMesh.m_triangleIndexBase = reinterpret_cast<Uint8 *>(indices);
        indexedMesh.m_triangleIndexStride = sizeof(int)*3;

        indexedMesh.m_vertexType = PHY_FLOAT;
        indexedMesh.m_numVertices = mesh->mNumVertices;
        indexedMesh.m_vertexBase = reinterpret_cast<Uint8 *>(vertices);
        indexedMesh.m_vertexStride = sizeof(float) * 3;

        va->addIndexedMesh(indexedMesh);

        rigidBody.colliderInfo.shape = new btBvhTriangleMeshShape(va, true);
    }

    /* Physics Material */
    int materialIndex = 0;
    colliderData.Get("physicsMaterial", materialIndex);

    aiMetadata rigidBodiesData;
    sceneExtensionsData.Get("KHR_physics_rigid_bodies", rigidBodiesData);

    aiMetadata physicsMaterials;
    rigidBodiesData.Get("physicsMaterials", physicsMaterials);

    aiMetadata physicsMaterial;
    physicsMaterials.Get(materialIndex, physicsMaterial);

    physicsMaterial.Get("staticFriction", rigidBody.colliderInfo.physicsMaterial.staticFriction);
    physicsMaterial.Get("dynamicFriction", rigidBody.colliderInfo.physicsMaterial.dynamicFriction);
    physicsMaterial.Get("restitution", rigidBody.colliderInfo.physicsMaterial.restitution);

    /* Mass */
    if (subMetaData.HasKey("motion")) {
        aiMetadata motionData;
        subMetaData.Get("motion", motionData);

        motionData.Get("mass", rigidBody.mass);
    }

    return rigidBody;
}
