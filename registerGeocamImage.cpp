

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>

#include "opencv2/core.hpp"
#include "opencv2/features2d.hpp"
#include "opencv2/calib3d.hpp"
#include "opencv2/imgproc.hpp"

#include "opencv2/xfeatures2d.hpp"

//#include <Common.h>
#include "processingFunctions.h"

enum DetectorType {DETECTOR_TYPE_BRISK = 0, 
                   DETECTOR_TYPE_ORB   = 1,
                   DETECTOR_TYPE_SIFT  = 2,
                   DETECTOR_TYPE_AKAZE = 3};


/// Write the transform parameters and the confidence to a file on disk
bool writeOutput(const std::string &outputPath, const cv::Mat &transform,
                 std::string confidenceString, bool print=false)
{
  std::ofstream file(outputPath.c_str());
  file << confidenceString << std::endl;
  for (size_t r=0; r<transform.rows; ++r)
  {
    for (size_t c=0; c<transform.cols-1; ++c)
    {
      file << transform.at<double>(r,c) << ", ";
      if (print)
        printf("%lf    ", transform.at<double>(r,c));
    }
    file << transform.at<double>(r,transform.cols-1) << std::endl;
    if (print)
      printf("\n");
  }
  file.close();
  
  return (!file.fail());
}

void preprocess(const cv::Mat &inputImage, cv::Mat &outputImage)
{
  // No preprocessing, just operate on the grayscale images.
  //outputImage = inputImage;
  
  // TODO: Utilize color information
  // Convert from color to grayscale
  cv::Mat grayImage;
  cvtColor(inputImage, grayImage, CV_BGR2GRAY);
  
  // Intensity Stretching
  cv::Mat normImage;
  intensityStretch(grayImage, normImage);
  
  
  outputImage = normImage;
  return; // TODO: Experiment with other preprocessing
  
  
  int kernelSize = 9;
  cv::Mat temp;
  
  
  
  // Simple Edge detection
  const int scale = 1;
  const int delta = 0;
  cv::Laplacian( normImage, temp, CV_32S, kernelSize, scale, delta, cv::BORDER_DEFAULT );
  cv::convertScaleAbs( temp, outputImage, 0.001);
  return;
  
  
  // Canny edge detection
  cv::Mat small;
  double kScaleFactor = 1.0/1.0;
  cv::resize(inputImage, small, cvSize(0, 0), kScaleFactor, kScaleFactor);
  
  const int cannyLow  = 200;
  const int cannyHigh = 300;
  cv::blur(small, temp, cv::Size(kernelSize, kernelSize));
  cv::Canny(temp, outputImage, cannyLow, cannyHigh, kernelSize);
  
}



/// Returns the number of inliers
int computeImageTransform(const cv::Mat &refImageIn, const cv::Mat &matchImageIn,
                          cv::Mat &transform,
                          const std::string debugFolder,
                          const int          kernelSize  =5, 
                          const DetectorType detectorType=DETECTOR_TYPE_ORB,
                          bool debug=true)
{
  
  // Preprocess the images to improve feature detection
  cv::Mat refImage, matchImage;
  preprocess(refImageIn,   refImage);
  preprocess(matchImageIn, matchImage);
  
  if (debug)
  {
    printf("Writing preprocessed images...\n");
    cv::imwrite( debugFolder+"basemapProcessed.jpeg", refImage );
    cv::imwrite( debugFolder+"geocamProcessed.jpeg", matchImage );
  }
    
  std::vector<cv::KeyPoint> keypointsA, keypointsB;
  cv::Mat descriptorsA, descriptorsB;  

  int nfeatures = 2000;
  
  cv::Ptr<cv::FeatureDetector    > detector;
  cv::Ptr<cv::DescriptorExtractor> extractor;
  if (detectorType == DETECTOR_TYPE_BRISK)
  {
    detector  = cv::BRISK::create();
    extractor = cv::BRISK::create();
  }
  if (detectorType == DETECTOR_TYPE_ORB)
  {
    detector  = cv::ORB::create(nfeatures);
    extractor = cv::ORB::create(nfeatures);
  }
  if (detectorType == DETECTOR_TYPE_SIFT)
  {
    int nOctaveLayers        = 6; // Output seems very sensitive to this value!
    double contrastThreshold = 0.04;
    double edgeThreshold     = 15;
    double sigma             = 1.2;
    detector  = cv::xfeatures2d::SIFT::create(nfeatures, nOctaveLayers, contrastThreshold, edgeThreshold, sigma);
    extractor = cv::xfeatures2d::SIFT::create(nfeatures, nOctaveLayers, contrastThreshold, edgeThreshold, sigma);
    printf("Using the SIFT feature detector\n");
  }
  if (detectorType == DETECTOR_TYPE_AKAZE)
  {
    int   descriptorType     = cv::AKAZE::DESCRIPTOR_MLDB;
    //int   descriptorType     = cv::AKAZE::DESCRIPTOR_KAZE;
    int   descriptorSize     = 0; // Max
    int   descriptorChannels = 3;
    float threshold          = 0.003f; // Controls number of points found
    int   numOctaves         = 8;
    int   numOctaveLayers    = 5; // Num sublevels per octave
    detector  = cv::AKAZE::create(descriptorType, descriptorSize, descriptorChannels, threshold, numOctaves, numOctaveLayers);
    extractor = cv::AKAZE::create(descriptorType, descriptorSize, descriptorChannels, threshold, numOctaves, numOctaveLayers);
  }
  
  detector->detect(  refImage, keypointsA); // Basemap
  extractor->compute(refImage, keypointsA, descriptorsA);

  detector->detect(  matchImage, keypointsB); // HRSC
  extractor->compute(matchImage, keypointsB, descriptorsB);

  if ( (keypointsA.size() == 0) || (keypointsB.size() == 0) )
  {
    std::cout << "Failed to find any features in an image!\n";
    return 0;
  }

  // TODO: Does not seem to make a difference...
  //if ( (detectorType == DETECTOR_TYPE_SIFT) || (detectorType == DETECTOR_TYPE_AKAZE))
  if (detectorType == DETECTOR_TYPE_SIFT)
  {
    applyRootSift(descriptorsA);
    applyRootSift(descriptorsB);
  }
  
  if (debug)
  {
    cv::Mat keypointImageA, keypointImageB;
    cv::drawKeypoints(refImageIn, keypointsA, keypointImageA,
                      cv::Scalar::all(-1), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
    cv::drawKeypoints(matchImageIn, keypointsB, keypointImageB,
                      cv::Scalar::all(-1), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
    cv::imwrite( debugFolder+"refKeypoints.tif", keypointImageA);
    cv::imwrite( debugFolder+"matchKeypoints.tif", keypointImageB);
  }
  
  // Find the closest match for each feature
  //cv::FlannBasedMatcher matcher;
  cv::Ptr<cv::DescriptorMatcher> matcher;
  if (detectorType == DETECTOR_TYPE_SIFT)
    matcher = cv::DescriptorMatcher::create("BruteForce");
  else // Hamming distance is used for binary descriptors
    matcher = cv::DescriptorMatcher::create("BruteForce-Hamming");
  std::vector<std::vector<cv::DMatch> > matches;
  matcher->knnMatch(descriptorsA, descriptorsB, matches, 2);
  printf("Initial matching finds %d matches.\n", matches.size());
  
  const float SEPERATION_RATIO = 0.8; // Min seperation between top two matches
  std::vector<cv::DMatch> seperatedMatches;
  seperatedMatches.reserve(matches.size());
  for (int i = 0; i < matches.size(); ++i)
  {
    if (matches[i][0].distance < SEPERATION_RATIO * matches[i][1].distance)
    {
      seperatedMatches.push_back(matches[i][0]);
    }
  }
  printf("After match seperation have %d out of %d points remaining\n",
         seperatedMatches.size(), matches.size());
  const size_t MIN_LEGAL_MATCHES = 3;
  if (seperatedMatches.size() < MIN_LEGAL_MATCHES)
    return 0;

  // TODO: If this ever works, try to use it!
  printf("Attempting to compute aligning image rotation...\n");
  double calcRotation=0;
  if (!estimateImageRotation(keypointsA, keypointsB, seperatedMatches, calcRotation))
    printf("Failed to compute a rotation alignment between the images!\n"); 

  
  //-- Quick calculation of max and min distances between keypoints
  double max_dist = 0; double min_dist = 9999999;
  for (size_t i=0; i<seperatedMatches.size(); i++)
  { 
    if ((seperatedMatches[i].queryIdx < 0) || (seperatedMatches[i].trainIdx < 0))
      continue;
    double dist = seperatedMatches[i].distance;
    //std::cout << matches[i].queryIdx <<", "<< matches[i].trainIdx << ", " << dist <<  std::endl;
    if (dist < min_dist) 
      min_dist = dist;
    if (dist > max_dist) 
      max_dist = dist;
  }
  //printf("-- Max dist : %f \n", max_dist );
  //printf("-- Min dist : %f \n", min_dist );
  
  if (debug)
  {
    cv::Mat matches_image1;
    cv::drawMatches(refImageIn, keypointsA, matchImageIn, keypointsB,
                    seperatedMatches, matches_image1, cv::Scalar::all(-1), cv::Scalar::all(-1),
                    std::vector<char>(),cv::DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS);
    cv::imwrite(debugFolder+"seperated_matches.tif", matches_image1);
  }
  
  
  //-- Pick out "good" matches
  float goodDist = max_dist;//(min_dist + max_dist) / 2.0;
  //if (argc > 3)
  //  goodDist = atof(argv[3]);
  const size_t DUPLICATE_CUTOFF = 2;
  std::vector< cv::DMatch > good_matches;
  for (int i=0; i<seperatedMatches.size(); i++)
  { 
    // First verify that the match is valid
    if ( (seperatedMatches[i].queryIdx < 0) ||
         (seperatedMatches[i].trainIdx < 0) ||  
         (seperatedMatches[i].queryIdx >= keypointsA.size()) || 
         (seperatedMatches[i].trainIdx >= keypointsB.size()) )
      continue;
    
    // Throw out matches that match to the same point as other matches
    size_t duplicateCount = 0;
    for (int j=0; j<seperatedMatches.size(); j++)
    {
      if (i == j) continue;
      if ( (seperatedMatches[i].queryIdx == seperatedMatches[j].queryIdx) ||
           (seperatedMatches[i].trainIdx == seperatedMatches[j].trainIdx)  )
      ++duplicateCount;
    }
    //printf("Count = %d\n", duplicateCount);
    if (duplicateCount >= DUPLICATE_CUTOFF)
      continue;
    
    // Now check the distance
    if (seperatedMatches[i].distance <= goodDist)
      good_matches.push_back( seperatedMatches[i]);
  }
  printf("After score filtering have %u out of %u points remaining\n",
         good_matches.size(), seperatedMatches.size());
  if (good_matches.size() < MIN_LEGAL_MATCHES)
    return 0;

  if (debug)
  {
    cv::Mat matches_image2;
    cv::drawMatches(refImageIn, keypointsA, matchImageIn, keypointsB,
                    good_matches, matches_image2, cv::Scalar::all(-1), cv::Scalar::all(-1),
                    std::vector<char>(),cv::DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS);
    cv::imwrite(debugFolder+"good_matches.tif", matches_image2);
  }
  
  // Get the coordinates from the remaining good matches  
  std::vector<cv::Point2f> refPts;
  std::vector<cv::Point2f> matchPts;
  for(size_t i = 0; i < good_matches.size(); i++ )
  {
    refPts.push_back  (keypointsA[good_matches[i].queryIdx].pt);
    matchPts.push_back(keypointsB[good_matches[i].trainIdx].pt);
  }
  printf("Computing homography...\n");
  
  // Compute a transform between the images using RANSAC
  const double MAX_INLIER_DIST_PIXELS = 30;
  cv::Mat inlierMask;
  transform = cv::findHomography( matchPts, refPts, cv::RHO, MAX_INLIER_DIST_PIXELS, inlierMask );
  printf("Finished computing homography.\n");
  
  if (inlierMask.rows == 0)
  {
    printf("Failed to find any inliers!\n");
    return 0;
  }
  
  // Convert from OpenCV inlier mask to vector of inlier indices
  std::vector<size_t    > inlierIndices;
  std::vector<cv::DMatch> inlierMatches;
  inlierMatches.reserve(refPts.size());
  inlierIndices.reserve(refPts.size());
  for (size_t i=0; i<refPts.size(); ++i) {
    if (inlierMask.at<unsigned char>(i, 0) > 0)
    {
      inlierIndices.push_back(i);
      inlierMatches.push_back(good_matches[i]);
    }
  }
  printf("Obtained %d inliers.\n", inlierIndices.size());

  
  std::vector<cv::Point2f> usedPtsRef, usedPtsMatch;
  for(size_t i = 0; i < inlierIndices.size(); i++ )
  {
    // Get the keypoints from the used matches
    usedPtsRef.push_back  (refPts  [inlierIndices[i]]);
    usedPtsMatch.push_back(matchPts[inlierIndices[i]]);
  }

  if (debug)
  {
    cv::Mat matches_image3;
    cv::drawMatches(refImageIn, keypointsA, matchImageIn, keypointsB,
                    inlierMatches, matches_image3, cv::Scalar::all(-1), cv::Scalar::all(-1),
                    std::vector<char>(),cv::DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS);
                       
    cv::imwrite(debugFolder+"match_debug_image.tif", matches_image3);
  }

  // Return the number of inliers found
  return static_cast<int>(inlierIndices.size());
}

/// Calls computImageTransform with multiple parameters until one succeeds
int computeImageTransformRobust(const cv::Mat &refImageIn, const cv::Mat &matchImageIn,
                                const std::string &debugFolder,
                                cv::Mat &transform,
                                bool debug)
{
  // Try not to accept solutions with fewer outliers
  const int DESIRED_NUM_INLIERS  = 20;
  const int REQUIRED_NUM_INLIERS = 10;
  cv::Mat bestTransform;
  int bestNumInliers = 0;
  int numInliers;
  
  // Keep trying transform parameter combinations until we get a good
  //   match as determined by the inlier count
  for (int kernelSize=5; kernelSize<6; kernelSize += 20)
  {
    for (int detectorType=3; detectorType<4; detectorType+=10)
    {
      printf("Attempting transform with kernel size = %d and detector type = %d\n",
             kernelSize, detectorType);
      numInliers = computeImageTransform(refImageIn, matchImageIn, transform, debugFolder,
                                         kernelSize, static_cast<DetectorType>(detectorType), debug);
      
      
      return numInliers; // DEBUG!!!!!!!!!
      
      if (numInliers >= DESIRED_NUM_INLIERS)
        return numInliers; // This transform is good enough, return it.

      if (numInliers > bestNumInliers)
      {
        // This is the best transform yet.
        bestTransform  = transform;
        bestNumInliers = numInliers;
      }
    } // End detector type loop
  } // End kernel size loop

  if (bestNumInliers < REQUIRED_NUM_INLIERS)
    return 0; // Did not get an acceptable transform!

  // Use the best transform we got
  transform = bestTransform;
  return bestNumInliers;
}

/// Try to estimate the accuracy of the computed registration
std::string evaluateRegistrationAccuracy(int numInliers, const cv::Mat &transform)
{
  // Make some simple decisions based on the inlier count
  if (numInliers < 5)
    return "CONFIDENCE_NONE";
  if (numInliers > 25)
    return "CONFIDENCE_HIGH";

  return "CONFIDENCE_LOW";
}


//=============================================================




int main(int argc, char** argv )
{
  
  if (argc < 4)
  {
    printf("usage: registerGeocamImage <Base map path> <New image path> <Output path> [debug]\n");
    return -1;
  }
  std::string refImagePath   = argv[1];
  std::string matchImagePath = argv[2];
  std::string outputPath     = argv[3];
  bool debug = false;
  if (argc > 4) // Set debug option
    debug = true;
  
  // TODO: Experiment with color processing
  const int LOAD_GRAY = 0;
  const int LOAD_RGB  = 1;
  
  // Load the input image
  cv::Mat refImageIn = cv::imread(refImagePath, LOAD_RGB);
  if (!refImageIn.data)
  {
    printf("Failed to load reference image\n");
    return -1;
  }

  cv::Mat matchImageIn = cv::imread(matchImagePath, LOAD_RGB);
  if (!matchImageIn.data)
  {
    printf("Failed to load match image\n");
    return -1;
  }

  // Write any debug files to this folder
  size_t stop = outputPath.rfind("/");
  std::string debugFolder = outputPath.substr(0,stop+1);

  // First compute the transform between the two images
  cv::Mat transform(3, 3, CV_32FC1);
  int numInliers = computeImageTransformRobust(refImageIn, matchImageIn, debugFolder, transform, debug);
  if (!numInliers)
  {
    printf("Failed to compute image transform!\n");
    return -1;
  }
   
  std::string confString = evaluateRegistrationAccuracy(numInliers, transform);
  printf("Computed %s transform with %d inliers.\n", confString.c_str(), numInliers);

  // Write the output to a file
  writeOutput(outputPath, transform, confString, debug);
  
  if (!debug) // Only debug stuff beyond this point
    return 0;
  
  // DEBUG - Paste the match image on top of the reference image
  writeOverlayImage(refImageIn, matchImageIn, transform, debugFolder+"warped.tif");
  
  
  return 0;
}







