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

#ifndef __OPENSPACE_CORE___GLOBALS___H__
#define __OPENSPACE_CORE___GLOBALS___H__

namespace openspace {

struct Configuration;
class DownloadManager;
class ModuleEngine;
class NetworkEngine;
class ParallelPeer;
class RenderEngine;
class TimeManager;
namespace interaction { class KeybindingManager; }
namespace scripting { class ScriptEngine; }

namespace detail {

Configuration& gConfiguration();
DownloadManager& gDownloadManager();
ModuleEngine& gModuleEngine();
NetworkEngine& gNetworkEngine();
ParallelPeer& gParallelPeer();
RenderEngine& gRenderEngine();
TimeManager& gTimeManager();
interaction::KeybindingManager& gKeybindingManager();
scripting::ScriptEngine& gScriptEngine();

} // namespace detail

namespace global {

static Configuration& configuration = detail::gConfiguration();
static DownloadManager& downloadManager = detail::gDownloadManager();
static ModuleEngine& moduleEngine = detail::gModuleEngine();
static NetworkEngine& networkEngine = detail::gNetworkEngine();
static ParallelPeer& parallelPeer = detail::gParallelPeer();
static RenderEngine& renderEngine = detail::gRenderEngine();
static TimeManager& timeManager = detail::gTimeManager();
static interaction::KeybindingManager& keybindingManager = detail::gKeybindingManager();
static scripting::ScriptEngine& scriptEngine = detail::gScriptEngine();

} // namespace global



} // namespace openspace

#endif // __OPENSPACE_CORE___GLOBALS___H__
