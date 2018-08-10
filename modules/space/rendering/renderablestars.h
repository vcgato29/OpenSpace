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

#ifndef __OPENSPACE_MODULE_SPACE___RENDERABLESTARS___H__
#define __OPENSPACE_MODULE_SPACE___RENDERABLESTARS___H__

#include <openspace/rendering/renderable.h>

#include <openspace/properties/stringproperty.h>
#include <openspace/properties/optionproperty.h>
#include <openspace/properties/scalar/floatproperty.h>
#include <openspace/properties/optionproperty.h>
#include <openspace/properties/propertyowner.h>

#include <ghoul/opengl/ghoul_gl.h>
#include <ghoul/opengl/uniformcache.h>

namespace ghoul::filesystem { class File; }
namespace ghoul::opengl {
    class ProgramObject;
    class Texture;
} // namespace ghoul::opengl

namespace openspace {

    namespace documentation { struct Documentation; }

    class RenderableStars : public Renderable {
    public:
        explicit RenderableStars(const ghoul::Dictionary& dictionary);
        ~RenderableStars();

        void initializeGL() override;
        void deinitializeGL() override;

        bool isReady() const override;

        void render(const RenderData& data, RendererTasks& rendererTask) override;
        void update(const UpdateData& data) override;

        static documentation::Documentation Documentation();

    private:
        enum ColorOption {
            Color = 0,
            Velocity = 1,
            Speed = 2
        };

        void createDataSlice(ColorOption option);

        bool loadData();
        bool readSpeckFile();
        bool loadCachedFile(const std::string& file);
        bool saveCachedFile(const std::string& file) const;

        properties::StringProperty _colorTexturePath;
        std::unique_ptr<ghoul::opengl::Texture> _colorTexture;
        std::unique_ptr<ghoul::filesystem::File> _colorTextureFile;
        bool _colorTextureIsDirty;

        properties::OptionProperty _colorOption;
        bool _dataIsDirty;


        // Old Method
        properties::StringProperty _pointSpreadFunctionTexturePath;
        std::unique_ptr<ghoul::opengl::Texture> _pointSpreadFunctionTexture;
        std::unique_ptr<ghoul::filesystem::File> _pointSpreadFunctionFile;
        bool _pointSpreadFunctionTextureIsDirty;

        properties::FloatProperty _alphaValue;
        properties::FloatProperty _scaleFactor;
        properties::FloatProperty _minBillboardSize;


        // PSF Based Method
        properties::OptionProperty _psfMethodOption;
        properties::OptionProperty _psfMultiplyOption;
        properties::FloatProperty _lumCent;
        properties::FloatProperty _radiusCent;
        properties::FloatProperty _brightnessCent;
        properties::FloatProperty _magnitudeExponent;
        properties::FloatProperty _colorContribution;
        properties::FloatProperty _billboardSize;
        properties::PropertyOwner _spencerPSFParamOwner;
        properties::FloatProperty _p0Param;
        properties::FloatProperty _p1Param;
        properties::FloatProperty _p2Param;
        properties::FloatProperty _spencerAlphaConst;
        properties::PropertyOwner _moffatPSFParamOwner;
        properties::FloatProperty _FWHMConst;
        properties::FloatProperty _moffatBetaConst;

        // Debug GUI for Carter's tests
        properties::OptionProperty _renderingMethodOption;
        properties::PropertyOwner _oldMethodOwner;
        properties::PropertyOwner _psfParamOwner;
        properties::PropertyOwner _moffatMethodOwner;

        std::unique_ptr<ghoul::opengl::ProgramObject> _program;
        UniformCache(
            renderingMethod, psfMethod,
            // Spencer Method
            modelMatrix, cameraUp, cameraViewProjectionMatrix,
            colorOption, magnitudeExponent, colorContribution,
            eyePosition,
            psfParamConf, lumCent, radiusCent, brightnessCent,
            p0Param, p1Param, p2Param, alphaConst
        ) _uniformCacheSpencer;

        UniformCache(
            FWHM, betaConstant
        ) _uniformCacheMoffat;

        UniformCache(
            colorTexture, alphaValue, 
            psfTexture
        ) _uniformCacheOld;

        std::string _speckFile;

        std::vector<float> _slicedData;
        std::vector<float> _fullData;
        int _nValuesPerStar;

        GLuint _vao;
        GLuint _vbo;
    };

} // namespace openspace

#endif // __OPENSPACE_MODULE_SPACE___RENDERABLESTARS___H__
