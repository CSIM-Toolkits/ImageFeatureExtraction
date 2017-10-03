import os
import sys
import platform
import unittest
import vtk, qt, ctk, slicer
from slicer.ScriptedLoadableModule import *
import logging

#
# FeatureExtractor
#

class FeatureExtractor(ScriptedLoadableModule):
  """Uses ScriptedLoadableModule base class, available at:
  https://github.com/Slicer/Slicer/blob/master/Base/Python/slicer/ScriptedLoadableModule.py
  """

  def __init__(self, parent):
    ScriptedLoadableModule.__init__(self, parent)
    self.parent.title = "Feature Extractor"
    self.parent.categories = ["Features"]
    self.parent.dependencies = []
    self.parent.contributors = ["Antonio Carlos da Silva Senra Filho, Fabricio Henrique Simozo (University of São Paulo)"] # replace with "Firstname Lastname (Organization)"
    self.parent.helpText = """
This is a scripted module that applies the Z-Score Mapping CLI module using pre-defined templates. It offers templates for T1, T2, PD, DTI-FA, DTI-ADC, DTI-RA and DTI-RD images built from healthy subjects.
"""
    self.parent.helpText += self.getDefaultModuleDocumentationLink()
    self.parent.acknowledgementText = """
This work was partially funded by CAPES and CNPq, Brazillian Agencies.
""" # replace with organization, grant and thanks.

#
# FeatureExtractorWidget
#

class FeatureExtractorWidget(ScriptedLoadableModuleWidget):
  """Uses ScriptedLoadableModuleWidget base class, available at:
  https://github.com/Slicer/Slicer/blob/master/Base/Python/slicer/ScriptedLoadableModule.py
  """

  def setup(self):
    ScriptedLoadableModuleWidget.setup(self)

    # Instantiate and connect widgets ...

    #
    # Parameters Area
    #
    parametersCollapsibleButton = ctk.ctkCollapsibleButton()
    parametersCollapsibleButton.text = "Z-Score Mapping"
    # parametersCollapsibleButton.collapsed = True
    self.layout.addWidget(parametersCollapsibleButton)

    # Layout within the dummy collapsible button
    parametersZScoreLayout = qt.QFormLayout(parametersCollapsibleButton)

    #
    # input volume selector
    #
    self.inputSelector = slicer.qMRMLNodeComboBox()
    self.inputSelector.nodeTypes = ["vtkMRMLScalarVolumeNode"]
    self.inputSelector.selectNodeUponCreation = True
    self.inputSelector.addEnabled = False
    self.inputSelector.removeEnabled = False
    self.inputSelector.noneEnabled = False
    self.inputSelector.showHidden = False
    self.inputSelector.showChildNodeTypes = False
    self.inputSelector.setMRMLScene( slicer.mrmlScene )
    self.inputSelector.setToolTip( "Pick the input to the algorithm. NOTE: This image MUST be in ICBM space (as MNI-152)." )
    parametersZScoreLayout.addRow("Input Volume: ", self.inputSelector)


    #
    # output volume selector
    #
    self.outputSelector = slicer.qMRMLNodeComboBox()
    self.outputSelector.nodeTypes = ["vtkMRMLScalarVolumeNode"]
    self.outputSelector.selectNodeUponCreation = True
    self.outputSelector.addEnabled = True
    self.outputSelector.removeEnabled = True
    self.outputSelector.noneEnabled = False
    self.outputSelector.showHidden = False
    self.outputSelector.showChildNodeTypes = False
    self.outputSelector.setMRMLScene( slicer.mrmlScene )
    self.outputSelector.setToolTip( "Pick the output to the algorithm." )
    parametersZScoreLayout.addRow("Output Volume: ", self.outputSelector)

    #
    # region mask selector
    #
    self.regionMaskSelector = slicer.qMRMLNodeComboBox()
    self.regionMaskSelector.nodeTypes = ["vtkMRMLLabelMapVolumeNode"]
    self.regionMaskSelector.selectNodeUponCreation = True
    self.regionMaskSelector.addEnabled = False
    self.regionMaskSelector.removeEnabled = False
    self.regionMaskSelector.noneEnabled = True
    self.regionMaskSelector.showHidden = False
    self.regionMaskSelector.showChildNodeTypes = False
    self.regionMaskSelector.setMRMLScene( slicer.mrmlScene )
    self.regionMaskSelector.setToolTip( "Pick the label image that defines the region in which the Z-Score will be computed. If a mask is not selected, the operation will be performed for all voxels that contains a value bigger than zero in the selected pre-defined template." )
    parametersZScoreLayout.addRow("Region Mask: ", self.regionMaskSelector)

    #
    # Image Modality
    #
    self.setImageModalityComboBoxWidget = ctk.ctkComboBox()
    self.setImageModalityComboBoxWidget.addItem("T1")
    self.setImageModalityComboBoxWidget.addItem("T2")
    self.setImageModalityComboBoxWidget.addItem("PD")
    self.setImageModalityComboBoxWidget.addItem("DTI-FA")
    self.setImageModalityComboBoxWidget.addItem("DTI-ADC")
    self.setImageModalityComboBoxWidget.addItem("DTI-RA")
    self.setImageModalityComboBoxWidget.addItem("DTI-RD")
    self.setImageModalityComboBoxWidget.setToolTip(
      "Select the image modality of the template to be used. This should match the image modality of the input volume.")
    parametersZScoreLayout.addRow("Image Modality ", self.setImageModalityComboBoxWidget)

    #
    # Image Resolution
    #
    self.setImageResolutionComboBoxWidget = ctk.ctkComboBox()
    self.setImageResolutionComboBoxWidget.addItem("1mm")
    self.setImageResolutionComboBoxWidget.addItem("2mm")
    self.setImageResolutionComboBoxWidget.setToolTip(
      "Select the resolution of the template to be used. This should match the resolution of the input volume.")
    parametersZScoreLayout.addRow("Image Resolution ", self.setImageResolutionComboBoxWidget)

    #
    # Apply Histogram Matching?
    #
    self.setDoHistogramMatchingBooleanWidget = ctk.ctkCheckBox()
    self.setDoHistogramMatchingBooleanWidget.setChecked(False)
    self.setDoHistogramMatchingBooleanWidget.setToolTip(
      "If this box is checked, a histogram match operation will be performed between the input volume and the selected template")
    parametersZScoreLayout.addRow("Apply Histogram Matching",
                                      self.setDoHistogramMatchingBooleanWidget)

    #
    # Apply Button
    #
    self.applyButton = qt.QPushButton("Apply")
    self.applyButton.toolTip = "Run the algorithm."
    self.applyButton.enabled = False
    parametersZScoreLayout.addRow(self.applyButton)

    # connections
    self.applyButton.connect('clicked(bool)', self.onApplyButton)
    self.inputSelector.connect("currentNodeChanged(vtkMRMLNode*)", self.onSelect)
    self.outputSelector.connect("currentNodeChanged(vtkMRMLNode*)", self.onSelect)

    # Add vertical spacer
    self.layout.addStretch(1)

    # Refresh Apply button state
    self.onSelect()

  def cleanup(self):
    pass

  def onSelect(self):
    self.applyButton.enabled = self.inputSelector.currentNode() and self.outputSelector.currentNode()

  def onApplyButton(self):
    logic = FeatureExtractorLogic()
    modality=self.setImageModalityComboBoxWidget.currentText
    resolution=self.setImageResolutionComboBoxWidget.currentText
    doHistMatch=self.setDoHistogramMatchingBooleanWidget.isChecked()
    logic.run(self.inputSelector.currentNode(), self.outputSelector.currentNode(),self.regionMaskSelector.currentNode(),modality,resolution,doHistMatch)

#
# FeatureExtractorLogic
#

class FeatureExtractorLogic(ScriptedLoadableModuleLogic):
  """This class should implement all the actual
  computation done by your module.  The interface
  should be such that other python code can import
  this class and make use of the functionality without
  requiring an instance of the Widget.
  Uses ScriptedLoadableModuleLogic base class, available at:
  https://github.com/Slicer/Slicer/blob/master/Base/Python/slicer/ScriptedLoadableModule.py
  """

  def hasImageData(self,volumeNode):
    """This is an example logic method that
    returns true if the passed in volume
    node has valid image data
    """
    if not volumeNode:
      logging.debug('hasImageData failed: no volume node')
      return False
    if volumeNode.GetImageData() is None:
      logging.debug('hasImageData failed: no image data in volume node')
      return False
    return True

  def isValidInputOutputData(self, inputVolumeNode, outputVolumeNode):
    """Validates if the output is not the same as input
    """
    if not inputVolumeNode:
      logging.debug('isValidInputOutputData failed: no input volume node defined')
      return False
    if not outputVolumeNode:
      logging.debug('isValidInputOutputData failed: no output volume node defined')
      return False
    if inputVolumeNode.GetID()==outputVolumeNode.GetID():
      logging.debug('isValidInputOutputData failed: input and output volume is the same. Create a new volume for output to avoid this error.')
      return False
    return True

  def run(self, inputVolume, outputVolume, regionMask, modality, resolution, doHistMatch):
    """
    Run the actual algorithm
    """

    if not self.isValidInputOutputData(inputVolume, outputVolume):
      slicer.util.errorDisplay('Input volume is the same as output volume. Choose a different output volume.')
      return False

    logging.info('Processing started')
    slicer.util.showStatusMessage("Processing started")

    # Get the path to BrainTemplates files
    path2files = os.path.dirname(slicer.modules.featureextractor.path)

    templateMeanFileName=""
    templateSTDFileName=""
    if "DTI" not in modality:
      templateMeanFileName="IXI-ICBM-"+modality+"mean-"+resolution+".nii.gz"
      templateSTDFileName = "IXI-ICBM-" + modality + "std-" + resolution + ".nii.gz"
    else:
      templateMeanFileName="USP-ICBM-"+modality+"mean-131-"+resolution+".nii.gz"
      templateSTDFileName = "USP-ICBM-" + modality + "std-131-" + resolution + ".nii.gz"

    if platform.system() is "Windows":
      (read, MNITemplateMeanNode) = slicer.util.loadVolume(
        path2files + '\\Resources\\BrainTemplates\\'+templateMeanFileName,
        {}, True)
      (read, MNITemplateSTDNode) = slicer.util.loadVolume(
        path2files + '\\Resources\\BrainTemplates\\' + templateSTDFileName,
        {}, True)
    else:
      (read, MNITemplateMeanNode) = slicer.util.loadVolume(
      path2files + '/Resources/BrainTemplates/'+templateMeanFileName, {}, True)
      (read, MNITemplateSTDNode) = slicer.util.loadVolume(
        path2files + '/Resources/BrainTemplates/' + templateSTDFileName, {}, True)

    # Run Z-Score Mapping
    slicer.util.showStatusMessage("Calculating Z-Score map")
    params={}
    params['inputVolume']=inputVolume.GetID()
    params['inputTemplateMean']=MNITemplateMeanNode.GetID()
    params['inputTemplateStd']=MNITemplateSTDNode.GetID()
    params['outputVolume']=outputVolume.GetID()
    if regionMask is not None:
      params['regionMask']=regionMask.GetID()
    params['doHistogramMatching']=doHistMatch

    slicer.cli.run(slicer.modules.zscoremapping, None, params, wait_for_completion=True)

    # Removing unnecessary nodes
    slicer.mrmlScene.RemoveNode(MNITemplateMeanNode)
    slicer.mrmlScene.RemoveNode(MNITemplateSTDNode)

    logging.info('Processing completed')
    slicer.util.showStatusMessage("Processing completed")

    return True


class FeatureExtractorTest(ScriptedLoadableModuleTest):
  """
  This is the test case for your scripted module.
  Uses ScriptedLoadableModuleTest base class, available at:
  https://github.com/Slicer/Slicer/blob/master/Base/Python/slicer/ScriptedLoadableModule.py
  """

  def setUp(self):
    """ Do whatever is needed to reset the state - typically a scene clear will be enough.
    """
    slicer.mrmlScene.Clear(0)

  def runTest(self):
    """Run as few or as many tests as needed here.
    """
    self.setUp()
    self.test_FeatureExtractor1()

  def test_FeatureExtractor1(self):
    """ Ideally you should have several levels of tests.  At the lowest level
    tests should exercise the functionality of the logic with different inputs
    (both valid and invalid).  At higher levels your tests should emulate the
    way the user would interact with your code and confirm that it still works
    the way you intended.
    One of the most important features of the tests is that it should alert other
    developers when their changes will have an impact on the behavior of your
    module.  For example, if a developer removes a feature that you depend on,
    your test should break so they know that the feature is needed.
    """

    self.delayDisplay("Starting the test")
    #
    # first, get some data
    #
    import urllib
    downloads = (
        ('http://slicer.kitware.com/midas3/download?items=5767', 'FA.nrrd', slicer.util.loadVolume),
        )

    for url,name,loader in downloads:
      filePath = slicer.app.temporaryPath + '/' + name
      if not os.path.exists(filePath) or os.stat(filePath).st_size == 0:
        logging.info('Requesting download %s from %s...\n' % (name, url))
        urllib.urlretrieve(url, filePath)
      if loader:
        logging.info('Loading %s...' % (name,))
        loader(filePath)
    self.delayDisplay('Finished with download and loading')

    volumeNode = slicer.util.getNode(pattern="FA")
    logic = FeatureExtractorLogic()
    self.assertIsNotNone( logic.hasImageData(volumeNode) )
    self.delayDisplay('Test passed!')
