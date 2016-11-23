/*****************************************************************************************
*                                                                                       *
* OpenSpace                                                                             *
*                                                                                       *
* Copyright (c) 2014-2016                                                               *
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

// globe browsing
#include <modules/globebrowsing/globes/renderableglobe.h>

#include <modules/globebrowsing/tile/tileselector.h>

#include <modules/globebrowsing/globes/pointglobe.h>

#include <modules/globebrowsing/globes/chunkedlodglobe.h>
#include <modules/globebrowsing/rendering/layermanager.h>

// open space includes
#include <openspace/engine/openspaceengine.h>
#include <openspace/rendering/renderengine.h>
#include <openspace/util/spicemanager.h>
#include <openspace/scene/scenegraphnode.h>

#include <modules/debugging/rendering/debugrenderer.h>

// ghoul includes
#include <ghoul/misc/assert.h>
#include <ghoul/misc/threadpool.h>

namespace {
    const std::string _loggerCat = "RenderableGlobe";

    // Keys for the dictionary
    const std::string keyFrame = "Frame";
    const std::string keyRadii = "Radii";
    const std::string keyInteractionDepthBelowEllipsoid =
        "InteractionDepthBelowEllipsoid";
    const std::string keyCameraMinHeight = "CameraMinHeight";
    const std::string keySegmentsPerPatch = "SegmentsPerPatch";
    const std::string keyLayers = "Layers";
}

namespace openspace {
namespace globebrowsing {
    
    RenderableGlobe::RenderableGlobe(const ghoul::Dictionary& dictionary)
    : _generalProperties({
        properties::BoolProperty("enabled", "Enabled", true),
        properties::BoolProperty("performShading", "perform shading", true),
        properties::BoolProperty("atmosphere", "atmosphere", false),
        properties::FloatProperty("lodScaleFactor", "lodScaleFactor",10.0f, 1.0f, 50.0f),
        properties::FloatProperty("cameraMinHeight", "cameraMinHeight", 100.0f, 0.0f, 1000.0f)
    })
    , _debugProperties({
        properties::BoolProperty("saveOrThrowCamera", "save or throw camera", false),
        properties::BoolProperty("showChunkEdges", "show chunk edges", false),
        properties::BoolProperty("showChunkBounds", "show chunk bounds", false),
        properties::BoolProperty("showChunkAABB", "show chunk AABB", false),
    	properties::BoolProperty("showHeightResolution", "show height resolution", false),
        properties::BoolProperty("showHeightIntensities", "show height intensities", false),
        properties::BoolProperty("performFrustumCulling", "perform frustum culling", true),
        properties::BoolProperty("performHorizonCulling", "perform horizon culling", true),
        properties::BoolProperty("levelByProjectedAreaElseDistance", "level by projected area (else distance)",false),
        properties::BoolProperty("resetTileProviders", "reset tile providers", false),
        properties::BoolProperty("toggleEnabledEveryFrame", "toggle enabled every frame", false),
        properties::BoolProperty("collectStats", "collect stats", false)
        })

    {
        setName("RenderableGlobe");
        
        dictionary.getValue(keyFrame, _frame);

        // Read the radii in to its own dictionary
        Vec3 radii;
        dictionary.getValue(keyRadii, radii);
        _ellipsoid = Ellipsoid(radii);
        setBoundingSphere(pss(_ellipsoid.averageRadius(), 0.0));

        // Ghoul can't read ints from lua dictionaries...
        double patchSegmentsd;
        dictionary.getValue(keySegmentsPerPatch, patchSegmentsd);
        int patchSegments = patchSegmentsd;
        
        dictionary.getValue(keyInteractionDepthBelowEllipsoid,
            _interactionDepthBelowEllipsoid);
        float cameraMinHeight;
        dictionary.getValue(keyCameraMinHeight, cameraMinHeight);
        _generalProperties.cameraMinHeight.set(cameraMinHeight);

        // Init layer manager
        ghoul::Dictionary layersDictionary;
        if(!dictionary.getValue(keyLayers, layersDictionary))
            throw ghoul::RuntimeError(keyLayers + " must be specified specified!");

        _layerManager = std::make_shared<LayerManager>(layersDictionary);

        _chunkedLodGlobe = std::make_shared<ChunkedLodGlobe>(
            *this, patchSegments, _layerManager);
        
        // This distance will be enough to render the globe as one pixel if the field of
        // view is 'fov' radians and the screen resolution is 'res' pixels.
        double fov = 2 * M_PI / 6; // 60 degrees
        int res = 2880;
        double distance = res * _ellipsoid.maximumRadius() / tan(fov / 2);
        _distanceSwitch.addSwitchValue(_chunkedLodGlobe, distance);
        
        _debugPropertyOwner.setName("Debug");
        _texturePropertyOwner.setName("Textures");

        addProperty(_generalProperties.isEnabled);
        addProperty(_generalProperties.atmosphereEnabled);
        addProperty(_generalProperties.performShading);
        addProperty(_generalProperties.lodScaleFactor);
        addProperty(_generalProperties.cameraMinHeight);
        
        _debugPropertyOwner.addProperty(_debugProperties.saveOrThrowCamera);
        _debugPropertyOwner.addProperty(_debugProperties.showChunkEdges);
        _debugPropertyOwner.addProperty(_debugProperties.showChunkBounds);
        _debugPropertyOwner.addProperty(_debugProperties.showChunkAABB);
        _debugPropertyOwner.addProperty(_debugProperties.showHeightResolution);
        _debugPropertyOwner.addProperty(_debugProperties.showHeightIntensities);
        _debugPropertyOwner.addProperty(_debugProperties.performFrustumCulling);
        _debugPropertyOwner.addProperty(_debugProperties.performHorizonCulling);
        _debugPropertyOwner.addProperty(
            _debugProperties.levelByProjectedAreaElseDistance);
        _debugPropertyOwner.addProperty(_debugProperties.resetTileProviders);
        _debugPropertyOwner.addProperty(_debugProperties.toggleEnabledEveryFrame);
        _debugPropertyOwner.addProperty(_debugProperties.collectStats);
        
        addPropertySubOwner(_debugPropertyOwner);
        addPropertySubOwner(_layerManager.get());
    }

    RenderableGlobe::~RenderableGlobe() {

    }

    bool RenderableGlobe::initialize() {
        return _distanceSwitch.initialize();
    }

    bool RenderableGlobe::deinitialize() {
        return _distanceSwitch.deinitialize();
    }

    bool RenderableGlobe::isReady() const {
        return _distanceSwitch.isReady();
    }

    void RenderableGlobe::render(const RenderData& data) {
        bool statsEnabled = _debugProperties.collectStats.value();
        _chunkedLodGlobe->stats.setEnabled(statsEnabled);

        if (_debugProperties.toggleEnabledEveryFrame.value()) {
            _generalProperties.isEnabled.setValue(
                !_generalProperties.isEnabled.value());
        }
        if (_generalProperties.isEnabled.value()) {
            if (_debugProperties.saveOrThrowCamera.value()) {
                _debugProperties.saveOrThrowCamera.setValue(false);

                if (savedCamera() == nullptr) { // save camera
                    LDEBUG("Saving snapshot of camera!");
                    setSaveCamera(std::make_shared<Camera>(data.camera));
                }
                else { // throw camera
                    LDEBUG("Throwing away saved camera!");
                    setSaveCamera(nullptr);
                }
            }
            _distanceSwitch.render(data);
        }
        if (_savedCamera != nullptr) {
            DebugRenderer::ref().renderCameraFrustum(data, *_savedCamera);
        }
    }

    void RenderableGlobe::update(const UpdateData& data) {
        _time = data.time;
        _distanceSwitch.update(data);

        glm::dmat4 translation =
            glm::translate(glm::dmat4(1.0), data.modelTransform.translation);
        glm::dmat4 rotation = glm::dmat4(data.modelTransform.rotation);
        glm::dmat4 scaling =
            glm::scale(glm::dmat4(1.0), glm::dvec3(data.modelTransform.scale,
                data.modelTransform.scale, data.modelTransform.scale));

        _cachedModelTransform = translation * rotation * scaling;
        _cachedInverseModelTransform = glm::inverse(_cachedModelTransform);

        if (_debugProperties.resetTileProviders) {
            _layerManager->reset();
            _debugProperties.resetTileProviders = false;
        }
        _layerManager->update();
        _chunkedLodGlobe->update(data);
    }

    glm::dvec3 RenderableGlobe::projectOnEllipsoid(glm::dvec3 position) {
        return _ellipsoid.geodeticSurfaceProjection(position);
    }

    float RenderableGlobe::getHeight(glm::dvec3 position) {
        if (_chunkedLodGlobe) {
            return _chunkedLodGlobe->getHeight(position);
        }
        else {
            return 0;
        }
    }

    std::shared_ptr<ChunkedLodGlobe> RenderableGlobe::chunkedLodGlobe() const{
        return _chunkedLodGlobe;
    }

    const Ellipsoid& RenderableGlobe::ellipsoid() const{
        return _ellipsoid;
    }

    const glm::dmat4& RenderableGlobe::modelTransform() const{
        return _cachedModelTransform;
    }

    const glm::dmat4& RenderableGlobe::inverseModelTransform() const{
        return _cachedInverseModelTransform;
    }

    const RenderableGlobe::DebugProperties&
        RenderableGlobe::debugProperties() const{
        return _debugProperties;
    }
    
    const RenderableGlobe::GeneralProperties&
        RenderableGlobe::generalProperties() const{
        return _generalProperties;
    }

    const std::shared_ptr<const Camera> RenderableGlobe::savedCamera() const {
        return _savedCamera;
    }

    double RenderableGlobe::interactionDepthBelowEllipsoid() {
        return _interactionDepthBelowEllipsoid;
    }

    void RenderableGlobe::setSaveCamera(std::shared_ptr<Camera> camera) { 
        _savedCamera = camera;
    }
} // namespace globebrowsing
} // namespace openspace

