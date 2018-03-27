#include "itkImageFileWriter.h"

#include "itkMaskImageFilter.h"
#include "itkImageRegionIterator.h"
#include "itkImageRegionConstIterator.h"
#include "itkImageRegionIteratorWithIndex.h"
#include "itkStatisticsImageFilter.h"
#include "itkImageToHistogramFilter.h"
#include "itkMaskedImageToHistogramFilter.h"
#include "itkThresholdImageFilter.h"
#include "itkBinaryThresholdImageFilter.h"
#include "itkCastImageFilter.h"

#include "itkPluginUtilities.h"

#include "JointFeatureInformationCLP.h"

// Use an anonymous namespace to keep class types and function names
// from colliding when module is used as shared object module.  Every
// thing should be in an anonymous namespace except for the module
// entry point, e.g. main()
//
namespace
{

template <typename TPixel>
int DoIt( int argc, char * argv[], TPixel )
{
    PARSE_ARGS;

    typedef float InputPixelType;
    typedef float OutputPixelType;

    std::cout<<"Joint information from:"<<std::endl;
    for (unsigned int i = 0; i < inputs.size(); ++i) {
        std::cout<<inputs[i]<<std::endl;
    }

    const unsigned int Dimension = 3;

    typedef itk::Image<InputPixelType,  Dimension> InputImageType;
    typedef itk::Image<OutputPixelType, Dimension> OutputImageType;
    typedef itk::Image<unsigned int, Dimension>   LabelImageType;

    typedef itk::ImageFileReader<InputImageType>  ReaderType;
    typedef itk::ImageFileReader<LabelImageType>  LabelReaderType;
    typedef itk::MaskImageFilter<InputImageType,LabelImageType> MaskImageType;

    typename LabelReaderType::Pointer label = LabelReaderType::New();
    if (maskVolume!="") {
        label->SetFileName(maskVolume.c_str());
        label->Update();
    }

    //Read each feature to a vector. If the mask is provided, it is applied on the input images.
    std::vector<typename InputImageType::Pointer>  inputFeatures;
    for (unsigned int img = 0; img < inputs.size(); ++img) {
        typename ReaderType::Pointer reader = ReaderType::New();
        typename MaskImageType::Pointer masked = MaskImageType::New();
        reader->SetFileName(inputs[img].c_str());
        reader->Update();
        if (maskVolume!="") {
            masked->SetInput(reader->GetOutput());
            masked->SetMaskImage(label->GetOutput());
            //            masked->SetMaskingValue(); TODO COlocar um int para o valor do label que sera usado para a mascara XML
            masked->Update();

            inputFeatures.push_back(masked->GetOutput());
        }else{
            inputFeatures.push_back(reader->GetOutput());
        }
    }

    //Calculates the joint information from the input features
    typename OutputImageType::Pointer jointFeature = OutputImageType::New();
    jointFeature->CopyInformation(inputFeatures[0]);
    jointFeature->SetRegions(inputFeatures[0]->GetRequestedRegion());
    jointFeature->Allocate();
    jointFeature->FillBuffer(static_cast<OutputPixelType>(0));

    typedef itk::ImageRegionConstIterator<InputImageType>           RegionConstIteratorType;
    typedef itk::ImageRegionIteratorWithIndex<InputImageType>       RegionIteratorType;
    RegionIteratorType jointIt(jointFeature,jointFeature->GetRequestedRegion());

    jointIt.GoToBegin();
    while (!jointIt.IsAtEnd()) {
        double jointValue=0.0, sumWeigths=0.0;
        for (unsigned int f = 0; f < inputFeatures.size(); ++f) {
            RegionConstIteratorType featureIt(inputFeatures[f],inputFeatures[f]->GetRequestedRegion());
            featureIt.SetIndex(jointIt.GetIndex());
            //Applying the feature weights
            jointValue+=featureIt.Get()*weights[f];
            sumWeigths+=weights[f];
        }
        jointIt.Set(jointValue/sumWeigths);

        ++jointIt;
    }

    typedef itk::CastImageFilter<InputImageType, LabelImageType>    CastInputToLabelType;
    typedef itk::BinaryThresholdImageFilter<InputImageType, LabelImageType>   BinaryFilterType;
    typedef itk::ThresholdImageFilter<InputImageType>   ThresholdFilterType;
    typename ThresholdFilterType::Pointer thr = ThresholdFilterType::New();
    typename BinaryFilterType::Pointer createdMask = BinaryFilterType::New();
    if (doOutlierRemoval) {
        std::cout<<"INFO: Outlier removal requested"<<std::endl;
        //Image statistics
        typedef itk::StatisticsImageFilter<InputImageType> StatisticsImageFilterType;
        typename StatisticsImageFilterType::Pointer stat = StatisticsImageFilterType::New ();
        stat->SetInput(jointFeature);
        stat->Update();

        typedef itk::Statistics::MaskedImageToHistogramFilter< InputImageType, LabelImageType >   HistogramFilterType;
        typename HistogramFilterType::Pointer histogramFilter = HistogramFilterType::New();

        typedef typename HistogramFilterType::HistogramSizeType   SizeType;
        SizeType size( 1 );
        size[0] = std::sqrt(stat->GetMaximum() - stat->GetMinimum());

        histogramFilter->SetHistogramSize( size );
        histogramFilter->SetMarginalScale( 10.0 );

        typename HistogramFilterType::HistogramMeasurementVectorType lowerBound( 1 );
        typename HistogramFilterType::HistogramMeasurementVectorType upperBound( 1 );
        lowerBound[0] = stat->GetMinimum();
        upperBound[0] = stat->GetMaximum();
        histogramFilter->SetHistogramBinMinimum( lowerBound );
        histogramFilter->SetHistogramBinMaximum( upperBound );

        histogramFilter->SetInput(  jointFeature  );
        createdMask->SetInput(jointFeature);
        createdMask->SetLowerThreshold(1); // cut off the zeros from the statistics
        createdMask->SetUpperThreshold(itk::NumericTraits<InputPixelType>::max());
        createdMask->SetInsideValue(1);
        createdMask->SetOutsideValue(0);
        histogramFilter->SetMaskValue(1);
        histogramFilter->SetMaskImage( createdMask->GetOutput() );

        histogramFilter->Update();

        typedef typename HistogramFilterType::HistogramType  HistogramType;
        const HistogramType *histogram = histogramFilter->GetOutput();

        //Setting the image thresholds

//        for (int var = 0; var < histogram->Size(); ++var) {
//            std::cout<<histogram->GetFrequency(var,0)<<std::endl;
//        }

        double upper_percentile = histogram->Quantile(0,upperCut), lower_percentile = histogram->Quantile(0,lowerCut);
        std::cout<<"upperCut: "<<upper_percentile<<" - lowerCut: "<<lower_percentile<<std::endl;

        thr->SetInput(jointFeature);
        thr->ThresholdOutside(lower_percentile,upper_percentile);
    }

    if (transformWeighting) {
        std::cout<<"INFO: Transforming the joint information into a weighting map"<<std::endl;
        //Transforms the final joint information into a CT weigthing map. This is useful to CT signal remodeling driven by the joint features.
        double min=itk::NumericTraits<InputPixelType>::max(), max=itk::NumericTraits<InputPixelType>::min();

        jointIt.GoToBegin();
        while (!jointIt.IsAtEnd()) {
            //We will consider only non-zero values
            if (jointIt.Get()>static_cast<InputPixelType>(0)) {
                //Finding minima and maxima
                if (jointIt.Get()>max) {
                    max=jointIt.Get();
                }
                if (jointIt.Get()<min) {
                    min=jointIt.Get();
                }
            }
            ++jointIt;
        }

        std::cout<<"min: "<<min<<" - max:"<<max<<std::endl;
        jointIt.GoToBegin();
        while (!jointIt.IsAtEnd()) {
            //Transforming the joint information to a weighting map
            if (jointIt.Get()>static_cast<InputPixelType>(0)) {
                jointIt.Set((jointIt.Get()-min)/(max-min));
            }
            ++jointIt;
        }//TODO Terminar de verificar a conta...o mapa esta saindo zero!



    }


    typedef itk::ImageFileWriter<OutputImageType> WriterType;
    typename WriterType::Pointer writer = WriterType::New();
    //    for (int i = 0; i < inputFeatures.size(); ++i) {
    //        std::stringstream out;
    //        out<<"/home/antonio/Downloads/feature_";
    //        out<<i<<".nii.gz";

    //    }
    writer->SetFileName( outputJoint.c_str() );
    (doOutlierRemoval)?writer->SetInput( thr->GetOutput() ):writer->SetInput( jointFeature );

    writer->SetUseCompression(1);
    writer->Update();

    return EXIT_SUCCESS;
}

} // end of anonymous namespace

int main( int argc, char * argv[] )
{
    PARSE_ARGS;

    itk::ImageIOBase::IOPixelType     pixelType;
    itk::ImageIOBase::IOComponentType componentType;

    try
    {
        itk::GetImageType(inputs[0], pixelType, componentType);

        // This filter handles all types on input, but only produces
        // signed types
        switch( componentType )
        {
        case itk::ImageIOBase::UCHAR:
            return DoIt( argc, argv, static_cast<unsigned char>(0) );
            break;
        case itk::ImageIOBase::CHAR:
            return DoIt( argc, argv, static_cast<signed char>(0) );
            break;
        case itk::ImageIOBase::USHORT:
            return DoIt( argc, argv, static_cast<unsigned short>(0) );
            break;
        case itk::ImageIOBase::SHORT:
            return DoIt( argc, argv, static_cast<short>(0) );
            break;
        case itk::ImageIOBase::UINT:
            return DoIt( argc, argv, static_cast<unsigned int>(0) );
            break;
        case itk::ImageIOBase::INT:
            return DoIt( argc, argv, static_cast<int>(0) );
            break;
        case itk::ImageIOBase::ULONG:
            return DoIt( argc, argv, static_cast<unsigned long>(0) );
            break;
        case itk::ImageIOBase::LONG:
            return DoIt( argc, argv, static_cast<long>(0) );
            break;
        case itk::ImageIOBase::FLOAT:
            return DoIt( argc, argv, static_cast<float>(0) );
            break;
        case itk::ImageIOBase::DOUBLE:
            return DoIt( argc, argv, static_cast<double>(0) );
            break;
        case itk::ImageIOBase::UNKNOWNCOMPONENTTYPE:
        default:
            std::cerr << "Unknown input image pixel component type: ";
            std::cerr << itk::ImageIOBase::GetComponentTypeAsString( componentType );
            std::cerr << std::endl;
            return EXIT_FAILURE;
            break;
        }
    }

    catch( itk::ExceptionObject & excep )
    {
        std::cerr << argv[0] << ": exception caught !" << std::endl;
        std::cerr << excep << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
