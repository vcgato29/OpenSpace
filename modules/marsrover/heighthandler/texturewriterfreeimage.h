/*****************************************************************************************
 *                                                                                       *
 * GHOUL                                                                                 *
 * General Helpful Open Utility Library                                                  *
 *                                                                                       *
 * Copyright (c) 2012-2018                                                               *
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

#ifndef __GHOUL___TEXTUREWRITERFREEIMAGE___H__
#define __GHOUL___TEXTUREWRITERFREEIMAGE___H__

#include <modules/marsrover/rendering/renderableheightmap.h>
#include <memory>
#include <string>


struct FIBITMAP;

namespace ghoul::io {

#ifdef GHOUL_USE_FREEIMAGE

class TextureWriterFreeImage {
public:
   
    static void writeTexture(std::vector<GLubyte> texturePixels, int w, int h); 
    static bool success(int ok);

   

private:
    int bitsPerPixels = 24; // 8 bits is grayscale?
    int WIDTH = 1920;
    int HEIGHT = 1080;
};

#endif // GHOUL_USE_FREEIMAGE

} // namespace ghoul::io

#endif // __GHOUL___TEXTUREREADERFREEIMAGE___H__
