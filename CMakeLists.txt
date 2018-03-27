cmake_minimum_required(VERSION 3.5)

project(ImageFeatureExtraction)

#-----------------------------------------------------------------------------
# Extension meta-information
set(EXTENSION_HOMEPAGE "http://slicer.org/slicerWiki/index.php/Documentation/Nightly/Extensions/ImageFeatureExtraction")
set(EXTENSION_CATEGORY "Features")
set(EXTENSION_CONTRIBUTORS "Antonio Carlos da Silva Senra Filho, Fabricio Henrique Simozo (University of São Paulo)")
set(EXTENSION_DESCRIPTION "This extension includes modules for MRI voxel-wise comparison with signal distributions obtained from healthy subjects.")
set(EXTENSION_ICONURL "http://www.example.com/Slicer/Extensions/ImageFeatureExtraction.png")
set(EXTENSION_SCREENSHOTURLS "http://www.example.com/Slicer/Extensions/ImageFeatureExtraction/Screenshots/1.png")
set(EXTENSION_DEPENDS "NA") # Specified as a space separated string, a list or 'NA' if any

#-----------------------------------------------------------------------------
# Extension dependencies
find_package(Slicer REQUIRED)
include(${Slicer_USE_FILE})

#-----------------------------------------------------------------------------
# Extension modules
add_subdirectory(ZScoreMapping)
add_subdirectory(FeatureExtractor)
add_subdirectory(JointFeatureInformation)
## NEXT_MODULE

#-----------------------------------------------------------------------------
include(${Slicer_EXTENSION_GENERATE_CONFIG})
include(${Slicer_EXTENSION_CPACK})
