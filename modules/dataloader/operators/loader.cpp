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

#include <iostream>
#include <ghoul/lua/lua_helper.h>
#include <string>
#include <modules/dataloader/operators/loader.h>
#include <modules/dataloader/dataloadermodule.h>
#include <ghoul/logging/logmanager.h>
#include <openspace/properties/triggerproperty.h>
#include <modules/dataloader/helpers.cpp>
#include <ghoul/filesystem/file.h>
#include <ghoul/misc/dictionary.h>


#ifdef _WIN32
#include <windows.h>
#endif

namespace {
  constexpr const char* _loggerCat = "Loader";
} 

namespace {
  static const openspace::properties::Property::PropertyInfo SelectedFilesInfo = {
      "SelectedFiles",
      "List of selected files and ready to load",
      "This list contains names of selected files in char format"
  };

  static const openspace::properties::Property::PropertyInfo UploadDataTriggerInfo = {
      "UploadDataTrigger",
      "Trigger load data files",
      "If this property is triggered it will call the function to load data"
  };
}

namespace openspace::dataloader {

Loader::Loader() 
    : PropertyOwner({ "Loader" })
    , _filePaths(SelectedFilesInfo)
    , _uploadDataTrigger(UploadDataTriggerInfo)
{
    _uploadDataTrigger.onChange([this](){
        uploadData();
    });

    addProperty(_filePaths);
    addProperty(_uploadDataTrigger);
}

void Loader::uploadData() {

  // Linux
  #ifdef _linux
  system("thunar /home/mberg");

  // Windows 
  #elif _WIN32

  char filepath[ MAX_PATH ];

  OPENFILENAME ofn;
    ZeroMemory( &filepath, sizeof( filepath ) );
    ZeroMemory( &ofn,      sizeof( ofn ) );
    ofn.lStructSize  = sizeof( ofn );
    ofn.hwndOwner    = NULL;  // If you have a window to center over, put its HANDLE here
    ofn.lpstrFilter  = "Text Files\0*.txt\0Any File\0*.*\0";
    ofn.lpstrFile    = filepath;
    ofn.nMaxFile     = MAX_PATH;
    ofn.lpstrTitle   = "Upload Data";
    ofn.Flags        = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST;
  
  if (GetOpenFileNameA( &ofn ))
  {
	// ghoul::filesystem::Directory fileDir(filepath);    
    // _filePaths = fileDir.readDirectories(
    //   ghoul::filesystem::Directory::Recursive::No,
    //   ghoul::filesystem::Directory::Sort::Yes
    // );
    _filePaths = filepath;
  }
  else
  {
    // All the below is to print incorrect user input. 
    switch (CommDlgExtendedError())
    {
      case CDERR_DIALOGFAILURE   : std::cout << "CDERR_DIALOGFAILURE\n";   break;
      case CDERR_FINDRESFAILURE  : std::cout << "CDERR_FINDRESFAILURE\n";  break;
      case CDERR_INITIALIZATION  : std::cout << "CDERR_INITIALIZATION\n";  break;
      case CDERR_LOADRESFAILURE  : std::cout << "CDERR_LOADRESFAILURE\n";  break;
      case CDERR_LOADSTRFAILURE  : std::cout << "CDERR_LOADSTRFAILURE\n";  break;
      case CDERR_LOCKRESFAILURE  : std::cout << "CDERR_LOCKRESFAILURE\n";  break;
      case CDERR_MEMALLOCFAILURE : std::cout << "CDERR_MEMALLOCFAILURE\n"; break;
      case CDERR_MEMLOCKFAILURE  : std::cout << "CDERR_MEMLOCKFAILURE\n";  break;
      case CDERR_NOHINSTANCE     : std::cout << "CDERR_NOHINSTANCE\n";     break;
      case CDERR_NOHOOK          : std::cout << "CDERR_NOHOOK\n";          break;
      case CDERR_NOTEMPLATE      : std::cout << "CDERR_NOTEMPLATE\n";      break;
      case CDERR_STRUCTSIZE      : std::cout << "CDERR_STRUCTSIZE\n";      break;
      case FNERR_BUFFERTOOSMALL  : std::cout << "FNERR_BUFFERTOOSMALL\n";  break;
      case FNERR_INVALIDFILENAME : std::cout << "FNERR_INVALIDFILENAME\n"; break;
      case FNERR_SUBCLASSFAILURE : std::cout << "FNERR_SUBCLASSFAILURE\n"; break;
      default                    : std::cout << "You cancelled.\n";
    }
  }
  // MAC
  #elif __APPLE__
  // Still to do
  #endif

;
}

void Loader::createInternalDataItemProperties() {
    module()->validateDataDirectory();
    std::vector<std::string> volumeItems = module()->volumeDataItems();

    LDEBUG("volume items vec size " + std::to_string(volumeItems.size()));

    for (auto item : volumeItems) {
        const std::string dirLeaf = openspace::dataloader::helpers::getDirLeaf(item);
        const openspace::properties::Property::PropertyInfo info = {
            "ItemTrigger_" + dirLeaf,
            dirLeaf,
            ""
        };

        // Initialize trigger property with data item name (are they unique?)
        auto volumeItemTrigger = properties::TriggerProperty(info);

        // Set onChange method to call loadDataItem with the path as argument
        volumeItemTrigger.onChange([this](){
            // loadDataItem(item);
        });

        // addProperty(volumeItemTrigger);
        // LDEBUG("Added property " + dirLeaf);
    }
}

// addDataItemProperty();
// removeDataItemProperties();

// loadDataItem(std::string absPathToItem);

// void Loader::createVolumeDataItem(std::string absPath) {}

void Loader::loadDataItem(std::string absPathToItem) {
    LINFO("Load item " + absPathToItem);

    std::string stateFile = openspace::dataloader::helpers::findStateFile(absPathToItem);
    
    // extract as dictionary
    ghoul::Dictionary stateDict = ghoul::lua::loadDictionaryFromFile(stateFile);

    // Renderable
    // Set Type = RenderableTimeVaryingVolume
    // Set transferfunction directory
    // Set source directory to absPathToItem
    // ?? that's it

    // Make renderable
        // timevaryingvolume instance?
        // is the renderable displayed at this point?

    // create rest of, more complete, dictionary for scene graph node
    const std::string dirLeaf = openspace::dataloader::helpers::getDirLeaf(absPathToItem);
    const std::string identifier = dirLeaf;
    const std::string parent = "";

    // create node
        // SceneGraphNode node = createFromDictionary(?)

    // get scene, add node to scene graph
        // getScene()->

    // great success
}

// void Loader::createVolumeDataItem(std::string absPath);

// }

// Will require dataItem rather than just filePath
ghoul::Dictionary Loader::createTaskDictionary(std::string filePath) {

  const int dimensions[3] = {100, 100, 128};
  const int lowerDomainBound[3] = {1, -90, 0};
  const int upperDomainBound[3] = {15, 90, 360};

  std::string filename = ghoul::filesystem::File(filePath).filename();
  std::string RawVolumeOutput = "${DATA}/dataloader/" + filename;
  std::string DictionaryOutput = "${DATA}/dataloader/" + filename + ".dictionary";

  std::initializer_list<std::pair<std::string, ghoul::any>> list = {
    std::make_pair( "Type", "KameleonVolumeToRawTask" ),
    std::make_pair( "Input", filePath ),
    std::make_pair( "Dimensions", dimensions ),
    std::make_pair( "Variable", "rho"),
    std::make_pair( "FactorRSquared", "true" ),
    std::make_pair( "LowerDomainBound", lowerDomainBound ),
    std::make_pair( "UpperDomainBound", upperDomainBound ),
    std::make_pair( "RawVolumeOutput", RawVolumeOutput ),
    std::make_pair( "DictionaryOutput", DictionaryOutput)
  };

  return ghoul::Dictionary(list);
}

// ghoul::Dictionary Loader::createVolumeItemDictionary(std::string dataDictionaryPath, std::string dataStatePath) {

// }

}
