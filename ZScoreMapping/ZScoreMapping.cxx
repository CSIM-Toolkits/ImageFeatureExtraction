#include "itkImageFileWriter.h"

#include "itkImageRegionIterator.h"
#include "itkHistogramMatchingImageFilter.h"
#include <math.h>
#include "itkPluginUtilities.h"

#include "ZScoreMappingCLP.h"

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

    typedef TPixel        InputPixelType;
    typedef float         OutputPixelType;
    typedef unsigned char LabelPixelType;

    const unsigned int Dimension = 3;

    typedef itk::Image<InputPixelType,  Dimension> InputImageType;
    typedef itk::Image<LabelPixelType,  Dimension> LabelImageType;
    typedef itk::Image<OutputPixelType, Dimension> OutputImageType;

    typedef itk::ImageFileReader<InputImageType>  ReaderType;
    typedef itk::ImageFileReader<LabelImageType>  LabelReaderType;

    typename ReaderType::Pointer readerInput = ReaderType::New();
    typename ReaderType::Pointer readerTemplateMean = ReaderType::New();
    typename ReaderType::Pointer readerTemplateSTD = ReaderType::New();
    typename LabelReaderType::Pointer readerRegionMask = LabelReaderType::New();

    readerInput->SetFileName( inputVolume.c_str() );
    readerTemplateMean->SetFileName( inputTemplateMean.c_str() );
    readerTemplateSTD->SetFileName( inputTemplateStd.c_str() );
    readerInput->Update();
    readerTemplateMean->Update();
    readerTemplateSTD->Update();

    //Apply histogram matching
    typedef itk::HistogramMatchingImageFilter<InputImageType,InputImageType>        HistogramMatchingType;
    typename HistogramMatchingType::Pointer hMatch = HistogramMatchingType::New();
    if (doHistogramMatching) {
        std::cout<<"Performing histogram matching"<<std::endl;

        hMatch->SetReferenceImage( readerTemplateMean->GetOutput() );
        hMatch->SetInput( readerInput->GetOutput() );
        hMatch->SetNumberOfHistogramLevels( 255 );
        hMatch->SetNumberOfMatchPoints( 64 );
        hMatch->Update();
    }

    //Calculate Z-Score map
    typename OutputImageType::Pointer zScoreMap = OutputImageType::New();
    zScoreMap->CopyInformation(readerInput->GetOutput());
    zScoreMap->SetRegions(readerInput->GetOutput()->GetRequestedRegion());
    zScoreMap->Allocate();
    zScoreMap->FillBuffer(0);

    typedef itk::ImageRegionIterator<InputImageType>      RegionIteratorType;
    typedef itk::ImageRegionIterator<OutputImageType>      OutputRegionIteratorType;

    RegionIteratorType imgIt(doHistogramMatching?hMatch->GetOutput():readerInput->GetOutput(),
                             doHistogramMatching?hMatch->GetOutput()->GetRequestedRegion():readerInput->GetOutput()->GetRequestedRegion());
    RegionIteratorType tempMeanIt(readerTemplateMean->GetOutput(), readerTemplateMean->GetOutput()->GetRequestedRegion());
    RegionIteratorType tempStdIt(readerTemplateSTD->GetOutput(), readerTemplateSTD->GetOutput()->GetRequestedRegion());
    OutputRegionIteratorType zScoreIt(zScoreMap, zScoreMap->GetRequestedRegion());

    if (regionMask == "") {
        std::cout<<"Calculating Z-Score mapping - full brain coverage"<<std::endl;

        imgIt.GoToBegin();
        tempMeanIt.GoToBegin();
        tempStdIt.GoToBegin();
        zScoreIt.GoToBegin();
        float temp_zscore=0.0;
        while(!imgIt.IsAtEnd()){
            if (tempMeanIt.Get()>static_cast<InputPixelType>(0)) {
                temp_zscore = (static_cast<OutputPixelType>(imgIt.Get()) - static_cast<OutputPixelType>(tempMeanIt.Get()))
                        / static_cast<OutputPixelType>(tempStdIt.Get());

                //Preventing to add outlier in the z-score map
                if (temp_zscore> -10.0 && temp_zscore<10.0) {
                    zScoreIt.Set(temp_zscore);
                }
            }
            ++imgIt;
            ++tempMeanIt;
            ++tempStdIt;
            ++zScoreIt;
        }
    }else{
        std::cout<<"Calculating Z-Score mapping - region defined in label map"<<std::endl;

        readerRegionMask->SetFileName( regionMask.c_str() );
        readerRegionMask->Update();

        typedef itk::ImageRegionIterator<LabelImageType>      LabelIteratorType;
        LabelIteratorType regionIt(readerRegionMask->GetOutput(), readerRegionMask->GetOutput()->GetRequestedRegion());

        imgIt.GoToBegin();
        tempMeanIt.GoToBegin();
        tempStdIt.GoToBegin();
        regionIt.GoToBegin();
        zScoreIt.GoToBegin();
        float temp_zscore=0.0;
        while(!imgIt.IsAtEnd()){
            if (regionIt.Get()>static_cast<LabelPixelType>(0)) {
                temp_zscore = (static_cast<OutputPixelType>(imgIt.Get()) - static_cast<OutputPixelType>(tempMeanIt.Get()))
                        / static_cast<OutputPixelType>(tempStdIt.Get());
                //Preventing to add outlier in the z-score map
                if (temp_zscore > -10.0 && temp_zscore < 10.0) {
                    zScoreIt.Set(temp_zscore);
                }
            }
            ++imgIt;
            ++tempMeanIt;
            ++tempStdIt;
            ++regionIt;
            ++zScoreIt;
        }
    }


    typedef itk::ImageFileWriter<OutputImageType> WriterType;
    typename WriterType::Pointer writer = WriterType::New();
    writer->SetFileName( outputVolume.c_str() );
    writer->SetInput( zScoreMap );
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
        itk::GetImageType(inputVolume, pixelType, componentType);

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
