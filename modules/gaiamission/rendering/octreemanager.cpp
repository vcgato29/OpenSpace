/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2018                                                               *
 *                                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
 * software and associated documentation files (the "Software"), to deal in the Software *
 * without restriction, including without limitation the rights to use, copy, modify,    *
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to the following   *
 * conditions:                                                                           *
 *                                                                                       *
 * The above copyright notice and this permission notice shall be included in all copies *
 * or substantial portions of the Software.                                              *
 *                                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
 ****************************************************************************************/

#include <modules/gaiamission/rendering/octreemanager.h>

#include <modules/gaiamission/rendering/octreeculler.h>
#include <modules/gaiamission/rendering/renderoption.h>
#include <openspace/util/distanceconstants.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/glm.h>
#include <ghoul/fmt.h>
#include <fstream>

namespace {
    constexpr const char* _loggerCat = "OctreeManager";
} // namespace

namespace openspace {

OctreeManager::OctreeManager()
    : _totalNodes(0)
    , _numNodesPerFile(0)
    , _totalDepth(0)
    , _numLeafNodes(0)
    , _numInnerNodes(0)
    , _biggestChunkIndexInUse(0)
    , _rebuildVBO(false)
{   }

OctreeManager::~OctreeManager() {   }

// Initialize a one layer Octree with root and 8 children. Covers all stars.
void OctreeManager::initOctree() {

    LDEBUG("Initializing Octree");
    _root = std::make_unique<OctreeNode>();

    // Initialize the culler. The NDC.z of the comparing corners are always -1 or 1.
    _culler = std::make_unique<OctreeCuller>(
        globebrowsing::AABB3(glm::vec3(-1, -1, 0), glm::vec3(1, 1, 1e2)) 
    );
    _removedKeysInPrevCall = std::set<int>();

    // Reset default values when rebuilding the Octree during runtime.
    _numInnerNodes = 0;
    _numLeafNodes = 0;
    _totalDepth = 0;
    _numNodesPerFile = 0;
    _totalNodes = 0;

    for (size_t i = 0; i < 8; ++i) {
        _numLeafNodes++;
        _root->Children[i] = std::make_unique<OctreeNode>();
        _root->Children[i]->posData = std::vector<float>();
        _root->Children[i]->colData = std::vector<float>();
        _root->Children[i]->velData = std::vector<float>();
        _root->Children[i]->isLeaf = true;
        _root->Children[i]->vboIndex = DEFAULT_INDEX;
        _root->Children[i]->lodInUse = 0;
        _root->Children[i]->numStars = 0;
        _root->Children[i]->halfDimension = MAX_DIST / 2.f;
        _root->Children[i]->originX = (i % 2 == 0) ? _root->Children[i]->halfDimension
            : -_root->Children[i]->halfDimension;
        _root->Children[i]->originY = (i % 4 < 2) ? _root->Children[i]->halfDimension
            : -_root->Children[i]->halfDimension;
        _root->Children[i]->originZ = (i < 4) ? _root->Children[i]->halfDimension
            : -_root->Children[i]->halfDimension;
    }

    return;
}

// Initialize a stack that keeps track of all free spot in VBO stream.
void OctreeManager::initVBOIndexStack(int maxIndex) {
    
    // Clear stack if we've used it before.
    _biggestChunkIndexInUse = 0;
    _freeSpotsInVBO = std::stack<int>();

    // Build stack back-to-front.
    for (int idx = maxIndex - 1; idx >= 0; --idx) {
        _freeSpotsInVBO.push(idx);
    }
    _maxStackSize = static_cast<int>(_freeSpotsInVBO.size());
    LINFO("StackSize: " + std::to_string(_maxStackSize) );
}

// Inserts star values in correct position in Octree.
void OctreeManager::insert(std::vector<float> starValues) {

    size_t index = getChildIndex(starValues[0], starValues[1], starValues[2]);

    insertInNode(_root->Children[index], starValues);
}

// Prints the whole tree structure, including number of stars per node.
void OctreeManager::printStarsPerNode() const {

    auto accumulatedString = std::string();

    for (int i = 0; i < 8; ++i) {
        auto prefix = "{" + std::to_string(i);
        accumulatedString += printStarsPerNode(_root->Children[i], prefix);
    }
    LINFO(fmt::format("Number of stars per node: \n{}", accumulatedString));
    LINFO(fmt::format("Number of leaf nodes: {}", std::to_string(_numLeafNodes)));
    LINFO(fmt::format("Number of inner nodes: {}", std::to_string(_numInnerNodes)));
    LINFO(fmt::format("Depth of tree: {}", std::to_string(_totalDepth)));
}

// Builds render data structure by traversing the Octree and checking for intersection 
// with view frustum. Every vector in map contains data for one node.  
std::unordered_map<int, std::vector<float>> OctreeManager::traverseData(const glm::mat4 mvp, 
    const glm::vec2 screenSize, int& deltaStars, gaiamission::RenderOption option) {

    auto renderData = std::unordered_map<int, std::vector<float>>();

    // Reclaim indices from previous render call. 
    for (auto removedKey = _removedKeysInPrevCall.rbegin();
        removedKey != _removedKeysInPrevCall.rend(); ++removedKey) {

        // Uses a reverse loop to try to decrease the biggest chunk.
        if (*removedKey == _biggestChunkIndexInUse - 1) {
            _biggestChunkIndexInUse = *removedKey;
            LINFO("Decreased size to: " + std::to_string(_biggestChunkIndexInUse) +
                " FreeSpotsInVBO: " + std::to_string(_freeSpotsInVBO.size()));
        }
        _freeSpotsInVBO.push(*removedKey);
    }
    // Clear cache of removed keys before next render call.
    _removedKeysInPrevCall.clear();

    // Rebuild VBO from scratch if we're not using most of it but have a high max index.
    if (_biggestChunkIndexInUse > _maxStackSize * 4 / 5 &&
        _freeSpotsInVBO.size() > _maxStackSize * 5 / 6) {
        LINFO("Rebuilding VBO! - Biggest Chunk: " + std::to_string(_biggestChunkIndexInUse) +
            " 4/5: " + std::to_string(_maxStackSize * 4 / 5) +
            " FreeSpotsInVBO: " + std::to_string(_freeSpotsInVBO.size()) +
            " 5/6: " + std::to_string(_maxStackSize * 5 / 6));
        initVBOIndexStack(_maxStackSize);
        _rebuildVBO = true;
    }

    for (size_t i = 0; i < 8; ++i) {
        auto tmpData = checkNodeIntersection(_root->Children[i], mvp, screenSize, deltaStars, 
            option);
        // Observe that if there exists identical keys in renderData then those values in 
        // tmpData will be ignored! Thus we store the removed keys until next render call!
        renderData.insert(tmpData.begin(), tmpData.end());
    }

    if (_rebuildVBO) {
        // We need to overwrite bigger indices that had data before!
        auto idxToRemove = std::unordered_map<int, std::vector<float>>();
        for (size_t idx : _removedKeysInPrevCall) {
            idxToRemove[idx] = std::vector<float>();
        }
        _removedKeysInPrevCall.clear();

        // This will only insert indices that doesn't already exist in map (i.e. > biggestIdx).
        renderData.insert(idxToRemove.begin(), idxToRemove.end());
        deltaStars = 0;
        _rebuildVBO = false;
        LINFO("After rebuild - Biggest Chunk: " + std::to_string(_biggestChunkIndexInUse) +
            " _freeSpotsInVBO.size(): " + std::to_string(_freeSpotsInVBO.size()));
    }

    return renderData;
}

// Builds full render data structure by traversing all leaves in the Octree. 
std::vector<float> OctreeManager::getAllData(gaiamission::RenderOption option) {
    auto fullData = std::vector<float>();

    for (size_t i = 0; i < 8; ++i) {
        auto tmpData = getNodeData(_root->Children[i], option);
        fullData.insert(fullData.end(), tmpData.begin(), tmpData.end());
    }
    return fullData;
}

// Write entire Octree to a file.
void OctreeManager::writeToFile(std::ofstream& outFileStream) {
    
    outFileStream.write(reinterpret_cast<const char*>(&_valuesPerStar), sizeof(int32_t));
    outFileStream.write(reinterpret_cast<const char*>(&MAX_STARS_PER_NODE), sizeof(int32_t));

    // Use pre-traversal (Morton code / Z-order).
    for (size_t i = 0; i < 8; ++i) {
        writeNodeToFile(outFileStream, _root->Children[i]);
    }
}

// Write a node to outStream.
void OctreeManager::writeNodeToFile(std::ofstream& outFileStream, 
    std::shared_ptr<OctreeNode> node) {

    // Write node data.
    bool isLeaf = node->isLeaf;
    int32_t numStars = node->numStars;
    std::vector<float> nodeData = node->posData;
    nodeData.insert(nodeData.end(), node->colData.begin(), node->colData.end());
    nodeData.insert(nodeData.end(), node->velData.begin(), node->velData.end());
    int32_t nDataSize = nodeData.size();
    size_t nBytes = nDataSize * sizeof(nodeData[0]);
    outFileStream.write(reinterpret_cast<const char*>(&isLeaf), sizeof(bool));
    outFileStream.write(reinterpret_cast<const char*>(&numStars), sizeof(int32_t));
    outFileStream.write(reinterpret_cast<const char*>(&nDataSize), sizeof(int32_t));
    outFileStream.write(reinterpret_cast<const char*>(nodeData.data()), nBytes);

    // Write children to file (in Morton order) if we're in an inner node.
    if (!node->isLeaf) {
        for (size_t i = 0; i < 8; ++i) {
            writeNodeToFile(outFileStream, node->Children[i]);
        }
    }
}

// Read a constructed Octree from a file. 
void OctreeManager::readFromFile(std::ifstream& inFileStream) {

    _valuesPerStar = 0;
    inFileStream.read(reinterpret_cast<char*>(&_valuesPerStar), sizeof(int32_t));
    inFileStream.read(reinterpret_cast<char*>(&MAX_STARS_PER_NODE), sizeof(int32_t));

    if (_valuesPerStar != (_posSize + _colSize + _velSize)) {
        LERROR("Read file doesn't have the same structure of render parameters!");
    }

    // Use the same technique to construct octree from file. 
    for (size_t i = 0; i < 8; ++i) {
        readNodeFromFile(inFileStream, _root->Children[i]);
    }
}

// Read a node from file and its potential children.
void OctreeManager::readNodeFromFile(std::ifstream& inFileStream, 
    std::shared_ptr<OctreeNode> node) {

    // Read node data.
    bool isLeaf;
    int32_t numStars = 0;
    int32_t nDataSize = 0;

    inFileStream.read(reinterpret_cast<char*>(&isLeaf), sizeof(bool));
    inFileStream.read(reinterpret_cast<char*>(&numStars), sizeof(int32_t));
    inFileStream.read(reinterpret_cast<char*>(&nDataSize), sizeof(int32_t));

    auto readData = std::vector<float>(nDataSize, 0.0f);
    size_t nBytes = nDataSize * sizeof(readData[0]);
    inFileStream.read(reinterpret_cast<char*>(&readData[0]), nBytes);

    node->isLeaf = isLeaf;
    node->numStars = numStars;
    int starsInNode = nDataSize / _valuesPerStar;
    auto posEnd = readData.begin() + (starsInNode * _posSize);
    auto colEnd = posEnd + (starsInNode * _colSize);
    auto velEnd = colEnd + (starsInNode * _velSize);
    node->posData = std::vector<float>(readData.begin(), posEnd);
    node->colData = std::vector<float>(posEnd, colEnd);
    node->velData = std::vector<float>(colEnd, velEnd);

    // Create children if we're in an inner node and read from the corresponding nodes.
    if (!node->isLeaf) {
        createNodeChildren(node);
        for (size_t i = 0; i < 8; ++i) {
            readNodeFromFile(inFileStream, node->Children[i]);
        }
    }
}

// Return number of leaf nodes in Octree.
size_t OctreeManager::numLeafNodes() const {
    return _numLeafNodes;
}

// Return the set constant of max stars per node in Octree.
size_t OctreeManager::maxStarsPerNode() const {
    return MAX_STARS_PER_NODE;
}

// Return the largest index that the stack has given out thus far. 
size_t OctreeManager::biggestChunkIndexInUse() const {
    return _biggestChunkIndexInUse;
}

// Return the total number of nodes in Octree. 
size_t OctreeManager::totalNodes() const {
    return _numLeafNodes + _numInnerNodes;
}

// Returns the correct index of child node. Maps [1,1,1] to 0 and [-1,-1,-1] to 7.
size_t OctreeManager::getChildIndex(float posX, float posY, float posZ, float origX,
    float origY, float origZ) {

    size_t index = 0;
    if (posX < origX) index += 1;
    if (posY < origY) index += 2;
    if (posZ < origZ) index += 4;
    return index;
}

// Private help function for insert(). Recursively checks nodes if they have any
// data that should be rendered this frame, depending on camera.
bool OctreeManager::insertInNode(std::shared_ptr<OctreeNode> node,
    std::vector<float> starValues, int depth) {

    if (node->isLeaf && node->numStars < MAX_STARS_PER_NODE) {
        // Node is a leaf and it's not yet full -> insert star.
        node->numStars++;
        auto posEnd = starValues.begin() + _posSize;
        auto colEnd = posEnd + _colSize;
        node->posData.insert(node->posData.end(), starValues.begin(), posEnd);
        node->colData.insert(node->colData.end(), posEnd, colEnd);
        node->velData.insert(node->velData.end(), colEnd, starValues.end());
        if (depth > _totalDepth) {
            _totalDepth = depth;
        }
        return true;
    }
    else if (node->isLeaf) {
        // Too many stars in leaf node, subdivide into 8 new nodes.
        _valuesPerStar = starValues.size();

        // Create children and clean up parent.
        createNodeChildren(node);

        // Construct an initial LOD cache in inner node for faster traversals during render.
        auto tmpLodNode = std::make_shared<OctreeNode>();
        constructLodCache(tmpLodNode);

        // Distribute stars from parent node into children. 
        for (size_t n = 0; n < node->numStars; ++n) {
            // Position data.
            auto posBegin = node->posData.begin() + n * _posSize;
            auto posEnd = posBegin + _posSize;
            std::vector<float> tmpValues(posBegin, posEnd);
            // Color data.
            auto colBegin = node->colData.begin() + n * _colSize;
            auto colEnd = colBegin + _colSize;
            tmpValues.insert(tmpValues.end(), colBegin, colEnd);
            // Velocity data.
            auto velBegin = node->velData.begin() + n * _velSize;
            auto velEnd = velBegin + _velSize;
            tmpValues.insert(tmpValues.end(), velBegin, velEnd);

            size_t index = getChildIndex(tmpValues[0], tmpValues[1], tmpValues[2],
                node->originX, node->originY, node->originZ);
            insertInNode(node->Children[index], tmpValues, depth);
            
            // Check if we should keep this star in LOD cache. 
            insertStarInLodCache(tmpLodNode, starValues);
        }
        
        // Copy LOD cache data from the first MAX_STARS_PER_NODE stars.
        // Don't use LOD cache for our more shallow layers.
        if (depth > FIRST_LOD_DEPTH) {
            node->posData = tmpLodNode->posData;
            node->colData = tmpLodNode->colData;
            node->velData = tmpLodNode->velData;
        }
        else {
            node->posData = std::vector<float>();
            node->colData = std::vector<float>();
            node->velData = std::vector<float>();
        }
        
    }

    // Node is an inner node, keep recursion going.
    // This will also take care of the new star when a subdivision has taken place.
    size_t index = getChildIndex(starValues[0], starValues[1], starValues[2],
        node->originX, node->originY, node->originZ);

    // Determine if new star should be kept in our LOD cache. Don't add if chunk is full.
    // Don't use LOD cache for our more shallow layers.
    if (node->posData.size() / _posSize < MAX_STARS_PER_NODE && depth > FIRST_LOD_DEPTH) {
        insertStarInLodCache(node, starValues);
    }

    // Increase counter for inner node to keep track of total stars in all children as well.
    node->numStars++;
    return insertInNode(node->Children[index], starValues, ++depth);
}

// Private help function for insertInNode(). Constructs our LOD cache with 1 virtual star.
void OctreeManager::constructLodCache(std::shared_ptr<OctreeNode> node) {

    // Add this node's origin as the only value. 
    // This will be used initially for comparisons in insertStarInLodCache().
    std::vector<float> insertData(_posSize, 0.f);
    insertData[0] = node->originX;
    insertData[1] = node->originY;
    insertData[2] = node->originZ;
    node->posData = std::vector<float>(insertData.begin(), insertData.end());
    node->colData = std::vector<float>(_colSize, 0.f);
    node->velData = std::vector<float>(_velSize, 0.f);
}

// Private help function for insertInNode(). Determines if star should be stored in LOD.
void OctreeManager::insertStarInLodCache(std::shared_ptr<OctreeNode> node,
    std::vector<float> starValues) {
    
    // Add star if it is further away from last inserted star with a set threshold.
    std::vector<float> cachePosData(node->posData.end() - _posSize, node->posData.end());
    glm::vec3 lastCachePos(cachePosData[0], cachePosData[1], cachePosData[2]);
    glm::vec3 starPos(starValues[0], starValues[1], starValues[2]);
    float dist = glm::distance(lastCachePos, starPos);
    
    // Add star if it's more than a quarter of node's size away.
    if (dist > node->halfDimension / 2.0) {
        auto posEnd = starValues.begin() + _posSize;
        auto colEnd = posEnd + _colSize;
        node->posData.insert(node->posData.end(), starValues.begin(), posEnd);
        node->colData.insert(node->colData.end(), posEnd, colEnd);
        node->velData.insert(node->velData.end(), colEnd, starValues.end());
    }
}

// Private help function for printStarsPerNode(). Recursively adds all nodes to the string.
std::string OctreeManager::printStarsPerNode(std::shared_ptr<OctreeNode> node,
    std::string prefix) const {

    // Print both inner and leaf nodes. 
    auto str = prefix + "} : " + std::to_string(node->numStars);

    if (node->isLeaf) {
        return str + " - [Leaf] \n";
    }
    else {
        str += " LOD: " + std::to_string(node->posData.size() / _posSize) + " - [Parent] \n";
        for (int i = 0; i < 8; ++i) {
            auto pref = prefix + "->" + std::to_string(i);
            str += printStarsPerNode(node->Children[i], pref);
        }
        return str;
    }
}

// Private help function for traverseData(). Recursively checks which nodes intersects with
// the view frustum (interpreted as an AABB) and decides if data should be optimized away.
std::unordered_map<int, std::vector<float>> OctreeManager::checkNodeIntersection(
    std::shared_ptr<OctreeNode> node, const glm::mat4 mvp, const glm::vec2 screenSize,
    int& deltaStars, gaiamission::RenderOption option) {

    auto fetchedData = std::unordered_map<int, std::vector<float>>();
    glm::vec3 debugPos;
    int depth  = static_cast<int>(log2( MAX_DIST / node->halfDimension ));

    // Calculate the corners of the node. 
    std::vector<glm::dvec4> corners(8);
    for (int i = 0; i < 8; ++i) {
        float x = (i % 2 == 0) ? node->originX + node->halfDimension
            : node->originX - node->halfDimension;
        float y = (i % 4 < 2) ? node->originY + node->halfDimension
            : node->originY - node->halfDimension;
        float z = (i < 4) ? node->originZ + node->halfDimension
            : node->originZ - node->halfDimension;
        glm::dvec3 pos = glm::dvec3(x, y, z) * 1000.0 * distanceconstants::Parsec;
        corners[i] = glm::dvec4(pos, 1.0);
        debugPos = glm::vec3(x, y, z);
    }

    // Check if node is visible from camera. If not then return early.
    if (!(_culler->isVisible(corners, mvp))) {
        // Check if this node or any of its children existed in cache previously. 
        // If so, then remove them from cache and add those indices to stack.
        fetchedData = removeNodeFromCache(node, deltaStars);
        return fetchedData;
    }

    // Take care of inner nodes.
    if (!(node->isLeaf)) {
        glm::vec2 nodeSize = _culler->getNodeSizeInPixels(screenSize);
        float totalPixels = nodeSize.x * nodeSize.y;
        auto lodData = std::vector<float>();

        // Use full data in LOD cache. Multiply MinPixels with depth for smoother culling. 
        if (totalPixels < MIN_TOTAL_PIXELS_LOD * depth) {
            lodData = constructInsertData(node, option);
        }

        // Return LOD data if we've triggered any check. 
        if (!lodData.empty()) {
            // Get correct insert index from stack if node didn't exist already. 
            // Otherwise we will overwrite the old data. Key merging is not a problem here. 
            if (node->vboIndex == DEFAULT_INDEX || _rebuildVBO) {
                if (node->vboIndex != DEFAULT_INDEX) {
                    // If we're rebuilding VBO cache then store indices to overwrite later. 
                    _removedKeysInPrevCall.insert(node->vboIndex);
                }
                node->vboIndex = _freeSpotsInVBO.top();
                _freeSpotsInVBO.pop();

                // Keep track of how many chunks are in use (ceiling).
                if (_freeSpotsInVBO.top() > _biggestChunkIndexInUse) {
                    _biggestChunkIndexInUse = _freeSpotsInVBO.top();
                }

                // We're in an inner node, remove indices from potential children in cache!
                for (int i = 0; i < 8; ++i) {
                    auto tmpData = removeNodeFromCache(node->Children[i], deltaStars);
                    fetchedData.insert(tmpData.begin(), tmpData.end());
                }

                // Insert data and adjust stars added in this frame.
                fetchedData[node->vboIndex] = lodData;
                deltaStars += static_cast<int>(lodData.size());
                node->lodInUse = lodData.size();
            }
            else {
                // This node existed in cache before. Check if it was for the same level.
                if (lodData.size() != node->lodInUse) {
                    // Insert data and adjust stars added in this frame.
                    fetchedData[node->vboIndex] = lodData;
                    deltaStars += static_cast<int>(lodData.size()) 
                        - static_cast<int>(node->lodInUse);
                    node->lodInUse = lodData.size();
                }
            }
            return fetchedData;
        }
    }
    // Return node data if node is a leaf.
    else {
        // If node already is in cache then skip it, otherwise store it.
        if (node->vboIndex == DEFAULT_INDEX || _rebuildVBO) {
            if (node->vboIndex != DEFAULT_INDEX) {
                // If we're rebuilding VBO cache then store indices to overwrite later. 
                _removedKeysInPrevCall.insert(node->vboIndex);
            }
            // Get correct insert index from stack.
            node->vboIndex = _freeSpotsInVBO.top();
            _freeSpotsInVBO.pop();

            // Keep track of how many chunks are in use (ceiling).
            if (_freeSpotsInVBO.top() > _biggestChunkIndexInUse) {
                _biggestChunkIndexInUse = _freeSpotsInVBO.top();
            }

            // Insert data and adjust stars added in this frame.
            auto insertData = constructInsertData(node, option);
            fetchedData[node->vboIndex] = insertData;
            deltaStars += static_cast<int>(insertData.size());
        }
        return fetchedData;
    }

    // We're in a big, visible inner node -> remove it from cache if it existed.
    // But not its children -> set recursive check to false.
    fetchedData = removeNodeFromCache(node, deltaStars, false);

    // Recursively check if children should be rendered.
    for (size_t i = 0; i < 8; ++i) {
        // Observe that if there exists identical keys in fetchedData then those values in 
        // tmpData will be ignored! Thus we store the removed keys until next render call!
        auto tmpData = checkNodeIntersection(node->Children[i], mvp, screenSize, deltaStars, 
            option);
        fetchedData.insert(tmpData.begin(), tmpData.end());
    }
    return fetchedData;
}

// Checks if specified node existed in cache, and removes it if that's the case. 
// If node is an inner node then all children will be checked recursively as well.
std::unordered_map<int, std::vector<float>> OctreeManager::removeNodeFromCache(
    std::shared_ptr<OctreeNode> node, int& deltaStars, bool recursive) {
    
    auto keysToRemove = std::unordered_map<int, std::vector<float>>();

    // If we're in rebuilding mode then there is no need to remove any nodes.
    if(_rebuildVBO) return keysToRemove;

    // Check if this node was rendered == had a specified index.
    if (node->vboIndex != DEFAULT_INDEX) {

        // Reclaim that index. However we need to wait until next render call to use it again!
        _removedKeysInPrevCall.insert(node->vboIndex);

        // Insert dummy node at offset index that should be removed from render.
        keysToRemove[node->vboIndex] = std::vector<float>();

        // Reset index and adjust stars removed this frame. 
        node->vboIndex = DEFAULT_INDEX;
        if (node->lodInUse > 0) {
            // If we're removing an inner node from cache then only decrease correct amount. 
            deltaStars -= static_cast<int>(node->lodInUse);
            node->lodInUse = 0;
        }
        else {
            int totalSize = node->posData.size() + node->colData.size() + node->velData.size();
            deltaStars -= static_cast<int>(totalSize);
        }
    }

    // Check children recursively if we're in an inner node. 
    if (!(node->isLeaf) && recursive) {
        for (int i = 0; i < 8; ++i) {
            auto tmpData = removeNodeFromCache(node->Children[i], deltaStars);
            keysToRemove.insert(tmpData.begin(), tmpData.end());
        }
    }
    return keysToRemove;
}

// Get data in all leaves regardless if visible or not.
std::vector<float> OctreeManager::getNodeData(std::shared_ptr<OctreeNode> node, 
    gaiamission::RenderOption option) {
    
    // Return node data if node is a leaf.
    if (node->isLeaf) {
        return constructInsertData(node, option);
    }

    // If we're not in a leaf, get data from all children recursively.
    auto nodeData = std::vector<float>();
    for (size_t i = 0; i < 8; ++i) {
        auto tmpData = getNodeData(node->Children[i], option);
        nodeData.insert(nodeData.end(), tmpData.begin(), tmpData.end());
    }
    return nodeData;
}

// Contruct children for specified node.
void OctreeManager::createNodeChildren(std::shared_ptr<OctreeNode> node) {
    
    for (size_t i = 0; i < 8; ++i) {
        _numLeafNodes++;
        node->Children[i] = std::make_unique<OctreeNode>();
        node->Children[i]->isLeaf = true;
        node->Children[i]->vboIndex = DEFAULT_INDEX;
        node->Children[i]->lodInUse = 0;
        node->Children[i]->numStars = 0;
        node->Children[i]->posData = std::vector<float>();
        node->Children[i]->colData = std::vector<float>();
        node->Children[i]->velData = std::vector<float>();
        node->Children[i]->halfDimension = node->halfDimension / 2.f;

        // Calculate new origin.
        node->Children[i]->originX = node->originX;
        node->Children[i]->originX += (i % 2 == 0) ? node->Children[i]->halfDimension
            : -node->Children[i]->halfDimension;
        node->Children[i]->originY = node->originY;
        node->Children[i]->originY += (i % 4 < 2) ? node->Children[i]->halfDimension
            : -node->Children[i]->halfDimension;
        node->Children[i]->originZ = node->originZ;
        node->Children[i]->originZ += (i < 4) ? node->Children[i]->halfDimension
            : -node->Children[i]->halfDimension;
    }

    // Clean up parent.
    node->isLeaf = false;
    _numLeafNodes--;
    _numInnerNodes++;
}

std::vector<float> OctreeManager::constructInsertData(std::shared_ptr<OctreeNode> node, 
    gaiamission::RenderOption option) {
    // Fill chunk by appending zeroes to data so we overwrite possible earlier values.
    // And more importantly so our attribute pointers knows where to read!
    auto insertData = std::vector<float>(node->posData.begin(), node->posData.end());
    insertData.resize(_posSize * MAX_STARS_PER_NODE, 0.f);

    if (option != gaiamission::RenderOption::Static) {
        insertData.insert(insertData.end(), node->colData.begin(), node->colData.end());
        insertData.resize((_posSize + _colSize) * MAX_STARS_PER_NODE, 0.f);

        if (option == gaiamission::RenderOption::Motion) {
            insertData.insert(insertData.end(), node->velData.begin(), node->velData.end());
            insertData.resize((_posSize + _colSize + _velSize) * MAX_STARS_PER_NODE, 0.f);
        }
    }
    return insertData;
}

}