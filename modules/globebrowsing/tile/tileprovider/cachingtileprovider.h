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

#ifndef __CACHING_TILE_PROVIDER_H__
#define __CACHING_TILE_PROVIDER_H__

#include <gdal_priv.h>

#include <ghoul/logging/logmanager.h>
#include <ghoul/filesystem/filesystem.h> // absPath
#include <ghoul/opengl/texture.h>
#include <ghoul/io/texture/texturereader.h>

#include <modules/globebrowsing/tile/tileprovider/tileprovider.h>
#include <modules/globebrowsing/tile/asynctilereader.h>
#include <modules/globebrowsing/other/lrucache.h>


//////////////////////////////////////////////////////////////////////////////////////////
//                                    TILE PROVIDER                                        //
//////////////////////////////////////////////////////////////////////////////////////////


namespace openspace {

    /**
        Provides tiles through GDAL datasets which can be defined with xml files
        for example for wms.
    */
    class CachingTileProvider : public TileProvider {
    public:


        CachingTileProvider(
            std::shared_ptr<AsyncTileDataProvider> tileReader, 
            std::shared_ptr<TileCache> tileCache,
            int framesUntilFlushRequestQueue);

        virtual ~CachingTileProvider();
        
        virtual Tile getTile(const ChunkIndex& chunkIndex);
        virtual Tile getDefaultTile();
        virtual Tile::Status getTileStatus(const ChunkIndex& index);
        virtual TileDepthTransform depthTransform();
        virtual void update();
        virtual void reset();
        virtual int maxLevel();


    private:


        //////////////////////////////////////////////////////////////////////////////////
        //                                Helper functions                              //
        //////////////////////////////////////////////////////////////////////////////////
        
        Tile getOrStartFetchingTile(ChunkIndex chunkIndex);


        
        /**
            Creates an OpenGL texture and pushes the data to the GPU.
        */

        Tile createTile(std::shared_ptr<TileIOResult> res);

        void clearRequestQueue();

        void initTexturesFromLoadedData();



        //////////////////////////////////////////////////////////////////////////////////
        //                                Member variables                              //
        //////////////////////////////////////////////////////////////////////////////////

        std::shared_ptr<TileCache> _tileCache;
        Tile _defaultTile;

        int _framesSinceLastRequestFlush;
        int _framesUntilRequestFlush;


        std::shared_ptr<AsyncTileDataProvider> _asyncTextureDataProvider;
    };

}  // namespace openspace




#endif  // __CACHING_TILE_PROVIDER_H__