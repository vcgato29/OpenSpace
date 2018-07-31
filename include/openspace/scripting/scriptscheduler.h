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

#ifndef __OPENSPACE_CORE___SCRIPTSCHEDULER___H__
#define __OPENSPACE_CORE___SCRIPTSCHEDULER___H__

#include <openspace/scripting/lualibrary.h>
#include <openspace/interaction/keyframenavigator.h>


#include <queue>
#include <vector>

namespace {
    const char* KeyTime = "Time";
    const char* KeyForwardScript = "ForwardScript";
    const char* KeyBackwardScript = "BackwardScript";
    const char* KeyUniversalScript = "Script";
} // namespace

namespace ghoul { class Dictionary; }

namespace openspace::documentation { struct Documentation; }

namespace openspace::scripting {

/**
 * Maintains an ordered list of <code>ScheduledScript</code>s and provides a simple
 * interface for retrieveing scheduled scripts
 */
class ScriptScheduler {
public:
    struct ScheduledScript {
        ScheduledScript() = default;
        ScheduledScript(const ghoul::Dictionary& dict);

        double time;
        std::string forwardScript;
        std::string backwardScript;
    };

    /**
    * Load a schedule from a ghoul::Dictionary \p dictionary and adds the
     * ScheduledScript%s to the list of stored scripts.
     * \param dictionary Dictionary to read
     * \throw SpecificationError If the dictionary does not adhere to the Documentation as
     * specified in the openspace::Documentation function
    */
    void loadScripts(const ghoul::Dictionary& dictionary);


    /**
    * Rewinds the script scheduler to the first scheduled script.
    */
    void rewind();

    /**
    * Removes all scripts for the schedule.
    */
    void clearSchedule();

    /**
    * Progresses the script schedulers time and returns all scripts that has been
    * scheduled to run between \param newTime and the time provided in the last invocation
    * of this method.
    *
    * \param newTime_simulation A j2000 time value specifying the new time stamp that
    * the script scheduler should progress to.
    * \param newTime_application The seconds elapsed since the application started
    *
    * \returns the ordered queue of scripts .
    */

    /**
    * See <code>progressTo(double newTime)</code>.
    *
    * \param timeStr A string specifying the a new time stamp that the
    * scripts scheduler should progress to.
    */

    std::pair<
        std::vector<std::string>::const_iterator, std::vector<std::string>::const_iterator
    > progressTo(double newTime);

    /**
    * Returns the the j2000 time value that the script scheduler is currently at
    */
    double currentTime() const;

    /**
    * \returns a vector of all scripts that has been loaded
    */
    std::vector<ScheduledScript> allScripts() const;

    /**
    * Sets the mode for how each scheduled script's timestamp will be interpreted.
    * \param refType reference mode (for exact syntax, see definition of
    * openspace::interaction::KeyframeTimeRef) which is either relative to the
    * application start time, relative to the recorded session playback start time,
    * or according to the absolute simulation time in seconds from J2000 epoch.
    * \param playbackReferenceTimestamp the timestamp in seconds (double) when the
    * recorded session file playback was initiated. For modes other than recorded
    * session, any value is acceptable.
    */
    void setTimeReferenceMode(openspace::interaction::KeyframeTimeRef refType,
                              double playbackReferenceTimestamp);

    static LuaLibrary luaLibrary();
    void setModeApplicationTime();
    void setModeRecordedTime();
    void setModeSimulationTime();

    static documentation::Documentation Documentation();

private:
    std::vector<double> _timings;
    std::vector<std::string> _forwardScripts;
    std::vector<std::string> _backwardScripts;

    int _currentIndex = 0;
    double _currentTime = 0;

    openspace::interaction::KeyframeTimeRef _timeframeMode
        = openspace::interaction::KeyframeTimeRef::absolute_simTimeJ2000;
    double _playbackReferenceTimestamp = 0.0;
};

} // namespace openspace::scripting

#endif // __OPENSPACE_CORE___SCRIPTSCHEDULER___H__
