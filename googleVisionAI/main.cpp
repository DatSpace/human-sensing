/*
 * Copyright (C) 2018 iCub Facility - Istituto Italiano di Tecnologia
 * Author: Ilaria Carlini Laura Cavaliere Vadim Tikhanoff 
 * email:  ilaria.carlini@iit.it laura.cavaliere@iit.it vadim.tikhanoff@iit.it 
 * Permission is granted to copy, distribute, and/or modify this program
 * under the terms of the GNU General Public License, version 2 or any
 * later version published by the Free Software Foundation.
 *
 * A copy of the license can be found at
 * http://www.robotcub.org/icub/license/gpl.txt
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details
 */

#include <vector>
#include <iostream>
#include <deque>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <iterator>
#include <string>
#include <yarp/os/BufferedPort.h>
#include <yarp/os/ResourceFinder.h>
#include <yarp/os/RFModule.h>
#include <yarp/os/Network.h>
#include <yarp/os/Time.h>
#include <yarp/os/Log.h>
#include <yarp/os/LogStream.h>
#include <yarp/os/Semaphore.h>
#include <yarp/sig/SoundFile.h>
#include <yarp/sig/Image.h>
#include <yarp/dev/PolyDriver.h>
#include <regex>
#include <yarp/cv/Cv.h>

#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>


#include <grpc++/grpc++.h>

#include "google/cloud/vision/v1/image_annotator.grpc.pb.h"
#include "google/cloud/vision/v1/geometry.grpc.pb.h"
#include "google/cloud/vision/v1/web_detection.grpc.pb.h"
#include "google/cloud/vision/v1/text_annotation.grpc.pb.h"
#include "google/cloud/vision/v1/product_search_service.grpc.pb.h"
#include "google/cloud/vision/v1/product_search.grpc.pb.h"


static const std::string base64_chars =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";



#include "googleVisionAI_IDL.h"

using namespace google::cloud::vision::v1;

using namespace std;

string SCOPE = "vision.googleapis.com";

/********************************************************/
class Processing : public yarp::os::BufferedPort<yarp::sig::ImageOf<yarp::sig::PixelRgb> >
{
    std::string moduleName;  
    
    yarp::os::RpcServer handlerPort;
    yarp::os::BufferedPort<yarp::sig::ImageOf<yarp::sig::PixelRgb> > outPort;

    yarp::os::BufferedPort<yarp::os::Bottle> targetPort;

    yarp::sig::ImageOf<yarp::sig::PixelRgb> annotate_img;

    std::mutex mtx;

public:
    /********************************************************/

    Processing( const std::string &moduleName )
    {
        this->moduleName = moduleName;

    }

    /********************************************************/
    ~Processing()
    {

    };

    /********************************************************/
    bool open()
    {
        this->useCallback();
        BufferedPort<yarp::sig::ImageOf<yarp::sig::PixelRgb> >::open( "/" + moduleName + "/image:i" );
        outPort.open("/" + moduleName + "/image:o");
        targetPort.open("/"+ moduleName + "/result:o");
        yarp::os::Network::connect(outPort.getName().c_str(), "/view");
        yarp::os::Network::connect("/icub/camcalib/left/out", BufferedPort<yarp::sig::ImageOf<yarp::sig::PixelRgb> >::getName().c_str());

        return true;
    }

    /********************************************************/
    void close()
    {
        BufferedPort<yarp::sig::ImageOf<yarp::sig::PixelRgb> >::close();
        outPort.close();
        targetPort.close();
    }

    /********************************************************/
    void interrupt()
    {
        BufferedPort<yarp::sig::ImageOf<yarp::sig::PixelRgb> >::interrupt();
    }
    
    /********************************************************/
    void onRead( yarp::sig::ImageOf<yarp::sig::PixelRgb> &img )
    {
        yarp::sig::ImageOf<yarp::sig::PixelRgb> &outImage  = outPort.prepare();
        outImage.resize(img.width(), img.height());
        outImage = img;

        std::lock_guard<std::mutex> lg(mtx);
        annotate_img = img;
        
        //do something on image with results

        outPort.write(); 

    }

    /********************************************************/
     static inline bool is_base64( unsigned char c )
    {
        return (isalnum(c) || (c == '+') || (c == '/'));
    }

    /********************************************************/
    std::string base64_encode(uchar const* bytes_to_encode, unsigned int in_len)
    {
        std::string ret;

        int i = 0;
        int j = 0;
        unsigned char char_array_3[3];
        unsigned char char_array_4[4];

        while (in_len--) 
        {
            char_array_3[i++] = *(bytes_to_encode++);
            if (i == 3)
            {
                char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                char_array_4[3] = char_array_3[2] & 0x3f;

                for (i = 0; (i <4); i++) 
                {
                    ret += base64_chars[char_array_4[i]];
                }
                i = 0;
            }
        }

        if (i) 
        {
            for (j = i; j < 3; j++) 
            {
                char_array_3[j] = '\0';
            }

            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (j = 0; (j < i + 1); j++) 
            {
                ret += base64_chars[char_array_4[j]];
            }
            
            while ((i++ < 3)) 
            {
                ret += '=';
            }
        }

        return ret;
    }
    
    /********************************************************/
    std::string base64_decode(std::string const& encoded_string)
    {
        int in_len = encoded_string.size();
        int i = 0;
        int j = 0;
        int in_ = 0;
        unsigned char char_array_4[4], char_array_3[3];
        std::string ret;

        while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_]))
        {
            char_array_4[i++] = encoded_string[in_]; in_++;

            if (i == 4)
            {
                for (i = 0; i < 4; i++)
                {
                    char_array_4[i] = base64_chars.find(char_array_4[i]);
                }

                char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
                char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
                char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

                for (i = 0; (i < 3); i++)
                {
                    ret += char_array_3[i];
                }

                i = 0;
            }
        }

        if (i)
        {
            for (j = i; j < 4; j++)
            {
                char_array_4[j] = 0;
            }
            
            for (j = 0; j < 4; j++)
            {
                char_array_4[j] = base64_chars.find(char_array_4[j]);
            }

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (j = 0; (j < i - 1); j++)
            {
                ret += char_array_3[j];
            }
        }

        return ret;
    }


/********************************************************/
    yarp::os::Bottle queryGoogleVisionAI( yarp::sig::ImageOf<yarp::sig::PixelRgb> &img )
    {
        BatchAnnotateImagesRequest requests; // Consists of multiple AnnotateImage requests // 
        BatchAnnotateImagesResponse responses;
        AnnotateImageResponse response;

        cv::Mat input_cv = yarp::cv::toCvMat(img);

        cv::imwrite("original.jpg", input_cv);

        // Encode data
        int params[3] = {0};
        params[0] = CV_IMWRITE_JPEG_QUALITY;
        params[1] = 100;
        std::vector<uchar> buf;
	    bool code = cv::imencode(".jpg", input_cv, buf, std::vector<int>(params, params+2));
	    uchar* result = reinterpret_cast<uchar*> (&buf[0]);
        
        std::string encoded = base64_encode(result, buf.size());
        
        
        // Decode data to verify consistency
        std::string decoded_string = base64_decode(encoded);
        std::vector<uchar> data(decoded_string.begin(), decoded_string.end());

        cv::Mat cv_out = imdecode(data, cv::IMREAD_UNCHANGED);
        
        cv::imwrite("decoded.jpg", cv_out);
         
        //std::cout << encoded <<std::endl;

        //----------------------//
        // Set up Configuration //
        //----------------------//

        // Image Source //
        requests.add_requests();

        requests.mutable_requests( 0 )->mutable_image()->set_content(result, buf.size());
        //requests.mutable_requests( 0 )->mutable_image()->set_content( encoded ); // base64 of local image
        
        //requests.mutable_requests( 0 )->mutable_image()->mutable_source()->set_image_uri(encoded);

        //requests.mutable_requests( 0 )->mutable_image()->mutable_source()->set_image_uri( "https://i.ibb.co/Ws3SCs8/test.jpg" ); // TODO [GCS_URL] // 
        //requests.mutable_requests( 0 )->mutable_image()->mutable_source()->set_gcs_image_uri( "gs://personal_projects/photo_korea.jpg" ); // TODO [GCS_URL] // 
        //requests.mutable_requests( 0 )->mutable_image_context(); // optional??
        
        // Features //
        requests.mutable_requests( 0 )->add_features();
        requests.mutable_requests( 0 )->add_features();
        requests.mutable_requests( 0 )->add_features();
        requests.mutable_requests( 0 )->add_features();
        requests.mutable_requests( 0 )->add_features();
        requests.mutable_requests( 0 )->add_features();
        requests.mutable_requests( 0 )->add_features();
        requests.mutable_requests( 0 )->add_features();
        requests.mutable_requests( 0 )->add_features();

        requests.mutable_requests( 0 )->mutable_features( 0 )->set_type( Feature_Type_FACE_DETECTION );
        requests.mutable_requests( 0 )->mutable_features( 1 )->set_type( Feature_Type_LANDMARK_DETECTION );
        requests.mutable_requests( 0 )->mutable_features( 2 )->set_type( Feature_Type_LOGO_DETECTION );
        requests.mutable_requests( 0 )->mutable_features( 3 )->set_type( Feature_Type_LABEL_DETECTION );
        requests.mutable_requests( 0 )->mutable_features( 4 )->set_type( Feature_Type_TEXT_DETECTION );
        requests.mutable_requests( 0 )->mutable_features( 5 )->set_type( Feature_Type_SAFE_SEARCH_DETECTION );
        requests.mutable_requests( 0 )->mutable_features( 6 )->set_type( Feature_Type_IMAGE_PROPERTIES );
        requests.mutable_requests( 0 )->mutable_features( 7 )->set_type( Feature_Type_CROP_HINTS );
        requests.mutable_requests( 0 )->mutable_features( 8 )->set_type( Feature_Type_WEB_DETECTION );
        
        // Print Configuration //
        std::cout << "\n\n---- Checking Request ----" << std::endl;
        std::cout << "Features size: " << requests.mutable_requests( 0 )->features_size() << std::endl;
        for ( int i = 0; i < requests.mutable_requests( 0 )->features_size(); i++ ) {
            //requests.mutable_requests( 0 ).features( int ); // Feature
            //requests.mutable_requests( 0 ).features( i ).type(); // Feature_Type
            std::cout << "Feature " << i << " name: " << Feature_Type_Name( requests.mutable_requests( 0 )->features( i ).type() ) << std::endl;
            std::cout << "max results: " << requests.mutable_requests( 0 )->features( i ).max_results() << std::endl;
        }

        std::cout << "Image Source: ";
        requests.mutable_requests( 0 )->mutable_image()->has_source() ? std::cout << "OK" << std::endl  :  std::cout << "FALSE" << std::endl; 

        std::cout << "Request has Image: ";
        requests.mutable_requests( 0 )->has_image() ? std::cout << "OK" << std::endl  :  std::cout << "FALSE" << std::endl;
        
        std::cout << "Request has Image Context: ";
        requests.mutable_requests( 0 )->has_image_context() ? std::cout << "OK" << std::endl  :  std::cout << "FALSE" << std::endl;

        std::cout << "BatchRequests size: " << requests.requests_size() << std::endl;

        //--------------//
        // Send Request //
        //--------------//
        std::cout << "\n---- Sending Request ----" << std::endl;
        grpc::Status status;
        grpc::ClientContext context;

        std::cout << "Getting GoogleDefaultCredentials...";
        auto creds = grpc::GoogleDefaultCredentials();
        std::cout << "DONE!\nCreating Channel...";
        auto channel = ::grpc::CreateChannel( SCOPE, creds );
        std::cout << "DONE!\nGetting Status...";
        std::unique_ptr< ImageAnnotator::Stub> stub( ImageAnnotator::NewStub( channel ) );
        status = stub->BatchAnnotateImages( &context, requests, &responses );
        std::cout << "DONE!" << std::endl;

        //---------------//
        // Read Response //
        //---------------//
        std::cout << "\n\n------ Responses ------" << std::endl;
        if ( status.ok() ) {
            std::cout << "Status returned OK\nResponses size: " << responses.responses_size() << std::endl;
            for ( int h = 0; h < responses.responses_size(); h++ ) {
                response = responses.responses( h );

                // Response Error //
                std::cout << "Response has Error: " << response.has_error() << std::endl;
                if ( response.has_error() ) {
                    std::cout
                        << "Error Code: "
                        << response.error().code()
                        << "\nError Message: "
                        << response.error().message()
                        << std::endl;

                    std::cout << "Do you wish to continue?[y/n]...";
                    string input;
                    std::cin >> input;
                    if ( std::regex_match( input, std::regex( "y(?:es)?|1" ) ) ) {
                        std::cout << "Continuing... " << std::endl;
                    } else {
                        std::cout << "Breaking... " << std::endl;
                        break;
                    
                    }
                }

                std::cout << "\n\n----Face Annotations----" << std::endl;
                std::cout << "Size: " << response.face_annotations_size() << std::endl;
                for ( int i = 0; i <  response.face_annotations_size(); i++ ) {

                    if ( response.face_annotations( i ).has_bounding_poly() ) {
                        //response.face_annotation( i ).bounding_poly();
                        std::cout << "Has Bounding Poly: "
                            << response.face_annotations( i ).has_bounding_poly()
                            << std::endl;

                        for ( int j = 0; j < response.face_annotations( i ).bounding_poly().vertices_size(); j++ ) {
                            std::cout
                                << "\tvert: "
                                << response.face_annotations( i ).bounding_poly().vertices( j ).x() // vertex
                                << ","
                                << response.face_annotations( i ).bounding_poly().vertices( j ).y() // vertex
                                << std::endl;
                        }
                    }

                    if ( response.face_annotations( i ).has_fd_bounding_poly() ) {
                        //response.face_annotations( 0 ).fd_bounding_poly();
                        for ( int j = 0; j < response.face_annotations( i ).fd_bounding_poly().vertices_size(); j++ ) {
                            std::cout
                                << "FD Bounding Poly: "
                                << response.face_annotations( i ).fd_bounding_poly().vertices( j ).x() // vertex
                                << ","
                                << response.face_annotations( i ).fd_bounding_poly().vertices( j ).y() // vertex
                                << std::endl;
                        }
                    }

                    // TODO [FA_LANDMARK] //
                    for ( int j = 0; j < response.face_annotations( i ).landmarks_size(); j++ ) {
                        //response.face_annotations( i ).landmarks( j ). // FaceAnnotation_Landmarks
                        std::cout << "FaceAnnotationLandmark: "
                            << "\n\tType: "
                            << FaceAnnotation_Landmark_Type_Name( response.face_annotations( i ).landmarks( j ).type() )
                            << "\n\tValue: "
                            << response.face_annotations( i ).landmarks( j ).type()
                            << "\n\ti: " << i << "  j: " << j;
                        
                        if ( response.face_annotations( i ).landmarks( j ).has_position() ) {
                            std::cout
                            << "X: "
                            << response.face_annotations( i ).landmarks( j ).position().x()
                            << "  Y: "
                            << response.face_annotations( i ).landmarks( j ).position().y()
                            << "  Z: "
                            << response.face_annotations( i ).landmarks( j ).position().z();
                        
                        } else {
                            std::cout << "\n\tNO POSITION";
                        }
                        std::cout
                        << std::endl;
                    }
                    
                    std::cout << "roll angle: "
                        << response.face_annotations( i ).roll_angle() // float
                        << "\npan angle: "
                        << response.face_annotations( i ).pan_angle() // float
                        << "\ntilt angle: "
                        << response.face_annotations( i ).tilt_angle() // float
                        << "\ndetection confidence: "
                        << response.face_annotations( i ).detection_confidence() // float
                        << "\nlandmarking confidence: "
                        << response.face_annotations( i ).landmarking_confidence() // float
                        << std::endl;

                    std::cout << "Alt Info:"
                        << "\n\tJoy: "
                        << Likelihood_Name( response.face_annotations( i ).joy_likelihood() )
                        << "\n\tSorrow: "
                        << Likelihood_Name( response.face_annotations( i ).sorrow_likelihood() )
                        << "\n\tAnger: "
                        << Likelihood_Name( response.face_annotations( i ).anger_likelihood() )
                        << "\n\tSurprise: "
                        << Likelihood_Name( response.face_annotations( i ).surprise_likelihood() )
                        << "\n\tUnder Exposed: "
                        << Likelihood_Name( response.face_annotations( i ).under_exposed_likelihood() )
                        << "\n\tBlured: "
                        << Likelihood_Name( response.face_annotations( i ).blurred_likelihood() )
                        << "\n\tHeadwear: "
                        << Likelihood_Name( response.face_annotations( i ).headwear_likelihood() )
                        << std::endl;
                }


                std::cout << "\n\n----Landmark Annotations----" << std::endl;
                std::cout << "Size: " << response.landmark_annotations_size() << std::endl;

                for ( int i = 0; i < response.landmark_annotations_size(); i++ ) {
                    //response.landmark_annotations( i ); // EntityAnnotation
                    // 4977
                    response.landmark_annotations( i ).mid(); // string
                    response.landmark_annotations( i ).locale(); //string
                    response.landmark_annotations( i ).description(); // string
                    response.landmark_annotations( i ).score(); // float 
                    response.landmark_annotations( i ).confidence(); // float
                    response.landmark_annotations( i ).topicality(); // float

                    /*
                    response.landmark_annotations( int )// ---- BoundingPoly ---- //
                    response.landmark_annotations( int ).has_bounding_poly(); //bool
                    response.landmark_annotations( int ).bounding_poly(); // BoundingPoly
                    response.landmark_annotations( int ).bounding_poly().verticies_size(); // int
                    response.landmark_annotations( int ).bounding_poly().verticies( int ); // Vertex
                    response.landmark_annotations( int ).bounding_poly().verticies( int ).x(); // google::protobuf::int32
                    response.landmark_annotations( int ).bounding_poly().verticies( int ).y(); // google::protobuf::int32
                    response.landmark_annotations( int )// ---- LocationInfo ---- //
                    response.landmark_annotations( int ).locations_size(); // int
                    response.landmark_annotations( int ).locations( int ); // LocationInfo
                    response.landmark_annotations( int ).locations( int ).has_lat_lng(); // bool
                    response.landmark_annotations( int ).locations( int ).lat_lng(); // LatLng
                    response.landmark_annotations( int ).locations( int ).lat_lng().latitude(); // double
                    response.landmark_annotations( int ).locations( int ).lat_lng().longtude(); // double
                    response.landmark_annotations( int )// ---- Propert ---- //
                    response.landmark_annotations( int ).properties_size(); // int
                    response.landmark_annotations( int ).properties( int ); // Property
                    response.landmark_annotations( int ).properties( int ).name(); //string
                    response.landmark_annotations( int ).properties( int ).value(); // string
                    response.landmark_annotations( int ).properties( int ).unit64_value(); // google::protobuf
                    */
                    //
                    std::cout
                        << "Label "
                        << i 
                        << "\n\tmid: "
                        << response.landmark_annotations( i ).mid() // string
                        << "\n\tlocale: "
                        << response.landmark_annotations( i ).locale() // string
                        << "\n\tdescription: "
                        << response.landmark_annotations( i ).description() // string
                        << "\n\tscore: "
                        << response.landmark_annotations( i ).score() // float
                        << "\n\tconfidence: "
                        << response.landmark_annotations( i ).confidence() // float
                        << "\n\ttopicality: "
                        << response.landmark_annotations( i ).topicality() // float
                        << std::endl;

                    std::cout
                        << "\tHas Bounding Poly: "
                        << response.landmark_annotations(i ).has_bounding_poly()
                        << std::endl;

                    if ( response.landmark_annotations( i ).has_bounding_poly() ) {
                        std::cout
                            << "\t\tVerticies: "
                            << response.landmark_annotations( i ).bounding_poly().vertices_size()
                            << std::endl;

                        for ( int j = 0; j < response.landmark_annotations( i ).bounding_poly().vertices_size(); j++ ) {
                            std::cout
                                << "\t\tvert " << j << ": "
                                << response.landmark_annotations( i ).bounding_poly().vertices( j ).x()
                                << ", "
                                << response.landmark_annotations( i ).bounding_poly().vertices( j ).y()
                                << std::endl;
                        }
                    }

                    std::cout
                        << "\tLocations: "
                        << response.landmark_annotations( i ).locations_size()
                        << std::endl;

                    for ( int j = 0; j < response.landmark_annotations( i ).locations_size(); j++ ) {
                        if ( response.landmark_annotations( i ).locations( j ).has_lat_lng() ) {
                            std::cout
                                << j
                                << ": "
                                << response.landmark_annotations( i ).locations( j ).lat_lng().latitude() // double
                                << " , "
                                << response.landmark_annotations( i ).locations( j ).lat_lng().longitude() // double
                                << std::endl;
                        }
                    }

                    std::cout
                        << "\tProperties: "
                        << response.landmark_annotations( i ).properties_size()
                        << std::endl;

                    for ( int j = 0; j < response.landmark_annotations( i ).properties_size(); j++ ) {
                        std::cout
                            << "\tname: "
                            << response.landmark_annotations( i ).properties( j ).name() // string
                            << "\t\nvalue: "
                            << response.landmark_annotations( i ).properties( j ).value() // string
                            << "\t\nuint64 value: "
                            << response.landmark_annotations( i ).properties( j ).uint64_value() // uint64
                            << std::endl;
                    }
                }

                std::cout << "\n\n----Logo Annotations----" << std::endl;
                std::cout << "Size: " << response.logo_annotations_size() << std::endl;
                for ( int i = 0; i <  response.logo_annotations_size(); i++ ) {
                    //response.logo_annotations( i ); // EntityAnnotation
                    std::cout
                        << "Label "
                        << i 
                        << "\n\tmid: "
                        << response.logo_annotations( i ).mid() // string
                        << "\n\tlocale: "
                        << response.logo_annotations( i ).locale() // string
                        << "\n\tdescription: "
                        << response.logo_annotations( i ).description() // string
                        << "\n\tscore: "
                        << response.logo_annotations( i ).score() // float
                        << "\n\tconfidence: "
                        << response.logo_annotations( i ).confidence() // float
                        << "\n\ttopicality: "
                        << response.logo_annotations( i ).topicality() // float
                        << std::endl;

                    std::cout
                        << "\tHas Bounding Poly: "
                        << response.logo_annotations(i ).has_bounding_poly()
                        << std::endl;

                    if ( response.logo_annotations( i ).has_bounding_poly() ) {
                        std::cout
                            << "\tVerticies: "
                            << response.logo_annotations( i ).bounding_poly().vertices_size()
                            << std::endl;

                        for ( int j = 0; j < response.logo_annotations( i ).bounding_poly().vertices_size(); j++ ) {
                            std::cout
                                << "\t\tvert " << j << ": "
                                << response.logo_annotations( i ).bounding_poly().vertices( j ).x()
                                << ", "
                                << response.logo_annotations( i ).bounding_poly().vertices( j ).y()
                                << std::endl;
                        }
                    }

                    std::cout
                        << "\tLocations: "
                        << response.logo_annotations( i ).locations_size()
                        << std::endl;

                    for ( int j = 0; j < response.logo_annotations( i ).locations_size(); j++ ) {
                        if ( response.logo_annotations( i ).locations( j ).has_lat_lng() ) {
                            std::cout
                                << j
                                << ": "
                                << response.logo_annotations( i ).locations( j ).lat_lng().latitude() // double
                                << " , "
                                << response.logo_annotations( i ).locations( j ).lat_lng().longitude() // double
                                << std::endl;
                        }
                    }

                    std::cout
                        << "\tProperties: "
                        << response.logo_annotations( i ).properties_size()
                        << std::endl;

                    for ( int j = 0; j < response.logo_annotations( i ).properties_size(); j++ ) {
                        std::cout
                            << "\tname: "
                            << response.logo_annotations( i ).properties( j ).name() // string
                            << "\t\nvalue: "
                            << response.logo_annotations( i ).properties( j ).value() // string
                            << "\t\nuint64 value: "
                            << response.logo_annotations( i ).properties( j ).uint64_value() // uint64
                            << std::endl;
                    }
                }

                std::cout << "\n\n----Label Annotations----" << std::endl;
                std::cout << "Size: " << response.label_annotations_size() << std::endl;
                for ( int i = 0; i < response.label_annotations_size(); i++ ) {
                    //response.label_annotations( i ); // EntityAnnotation
                    std::cout
                        << "Label "
                        << i 
                        << "\n\tmid: "
                        << response.label_annotations( i ).mid() // string
                        << "\n\tlocale: "
                        << response.label_annotations( i ).locale() // string
                        << "\n\tdescription: "
                        << response.label_annotations( i ).description() // string
                        << "\n\tscore: "
                        << response.label_annotations( i ).score() // float
                        << "\n\tconfidence: "
                        << response.label_annotations( i ).confidence() // float
                        << "\n\ttopicality: "
                        << response.label_annotations( i ).topicality() // float
                        << std::endl;

                    std::cout
                        << "\tHas Bounding Poly: "
                        << response.label_annotations(i ).has_bounding_poly()
                        << std::endl;

                    if ( response.label_annotations( i ).has_bounding_poly() ) {
                        std::cout
                            << "\tVerticies: "
                            << response.label_annotations( i ).bounding_poly().vertices_size()
                            << std::endl;

                        for ( int j = 0; j < response.label_annotations( i ).bounding_poly().vertices_size(); j++ ) {
                            std::cout
                                << "\t\tvert " << j << ": "
                                << response.label_annotations( i ).bounding_poly().vertices( j ).x()
                                << ", "
                                << response.label_annotations( i ).bounding_poly().vertices( j ).y()
                                << std::endl;
                        }
                    }

                    std::cout
                        << "\tLocations: "
                        << response.label_annotations( i ).locations_size()
                        << std::endl;

                    for ( int j = 0; j < response.label_annotations( i ).locations_size(); j++ ) {
                        if ( response.label_annotations( i ).locations( j ).has_lat_lng() ) {
                            std::cout
                                << j
                                << ": "
                                << response.label_annotations( i ).locations( j ).lat_lng().latitude() // double
                                << " , "
                                << response.label_annotations( i ).locations( j ).lat_lng().longitude() // double
                                << std::endl;
                        }
                    }

                    std::cout
                        << "\tProperties: "
                        << response.label_annotations( i ).properties_size()
                        << std::endl;

                    for ( int j = 0; j < response.label_annotations( i ).properties_size(); j++ ) {
                        std::cout
                            << "\tname: "
                            << response.label_annotations( i ).properties( j ).name() // string
                            << "\t\nvalue: "
                            << response.label_annotations( i ).properties( j ).value() // string
                            << "\t\nuint64 value: "
                            << response.label_annotations( i ).properties( j ).uint64_value() // uint64
                            << std::endl;
                    }
                }

                std::cout << "\n\n----Text Annotations----" << std::endl;
                std::cout << "Size: " << response.text_annotations_size() << std::endl;
                for ( int i = 0; i < response.text_annotations_size(); i++ ) {
                    //response.text_annotations( i ); // EntityAnnotation
                    std::cout
                        << "Label "
                        << i 
                        << "\n\tmid: "
                        << response.text_annotations( i ).mid() // string
                        << "\n\tlocale: "
                        << response.text_annotations( i ).locale() // string
                        << "\n\tdescription: "
                        << response.text_annotations( i ).description() // string
                        << "\n\tscore: "
                        << response.text_annotations( i ).score() // float
                        << "\n\tconfidence: "
                        << response.text_annotations( i ).confidence() // float
                        << "\n\ttopicality: "
                        << response.text_annotations( i ).topicality() // float
                        << std::endl;

                    std::cout
                        << "\tHas Bounding Poly: "
                        << response.text_annotations(i ).has_bounding_poly()
                        << std::endl;

                    if ( response.text_annotations( i ).has_bounding_poly() ) {
                        std::cout
                            << "\tVerticies: "
                            << response.text_annotations( i ).bounding_poly().vertices_size()
                            << std::endl;

                        for ( int j = 0; j < response.text_annotations( i ).bounding_poly().vertices_size(); j++ ) {
                            std::cout
                                << "\t\tvert " << j << ": "
                                << response.text_annotations( i ).bounding_poly().vertices( j ).x()
                                << ", "
                                << response.text_annotations( i ).bounding_poly().vertices( j ).y()
                                << std::endl;
                        }
                    }

                    std::cout
                        << "\tLocations: "
                        << response.text_annotations( i ).locations_size()
                        << std::endl;

                    for ( int j = 0; j < response.text_annotations( i ).locations_size(); j++ ) {
                        if ( response.text_annotations( i ).locations( j ).has_lat_lng() ) {
                            std::cout
                                << j
                                << ": "
                                << response.text_annotations( i ).locations( j ).lat_lng().latitude() // double
                                << " , "
                                << response.text_annotations( i ).locations( j ).lat_lng().longitude() // double
                                << std::endl;
                        }
                    }

                    std::cout
                        << "\tProperties: "
                        << response.text_annotations( i ).properties_size()
                        << std::endl;

                    for ( int j = 0; j < response.text_annotations( i ).properties_size(); j++ ) {
                        std::cout
                            << "\tname: "
                            << response.text_annotations( i ).properties( j ).name() // string
                            << "\t\nvalue: "
                            << response.text_annotations( i ).properties( j ).value() // string
                            << "\t\nuint64 value: "
                            << response.text_annotations( i ).properties( j ).uint64_value() // uint64
                            << std::endl;
                    }
                }

                std::cout << "\n\n----Full Text Annotation----" << std::endl;
                if ( response.has_full_text_annotation() ) {
                    std::cout << response.has_full_text_annotation() << std::endl;
                    response.full_text_annotation(); // TextAnnotation
                
                } else { std::cout << "NONE" << std::endl; }


                std::cout << "\n\n----Safe Search Annotation----" << std::endl;
                if ( response.has_safe_search_annotation() ) {
                    response.safe_search_annotation(); // SafeSearchAnnotation 

                } else { std::cout << "NONE" << std::endl; }


                std::cout << "\n\n----Image Properties Annotations----" << std::endl;
                if ( response.has_image_properties_annotation()  ) {
                    std::cout << response.has_image_properties_annotation() << std::endl; // ImageProperties
                    std::cout << "Dominant Colors: " << response.image_properties_annotation().has_dominant_colors() << std::endl;
                    std::cout << "\tSize: " << response.image_properties_annotation().dominant_colors().colors_size() << std::endl;

                    for ( int i = 0; i <  response.image_properties_annotation().dominant_colors().colors_size(); i++ ) {
                        std::cout << "Has Color: " << response.image_properties_annotation().dominant_colors().colors( i ).has_color() << std::endl;
                        if ( response.image_properties_annotation().dominant_colors().colors( i ).has_color() ) {
                            std::cout
                                << "\trgb: "
                                << response.image_properties_annotation().dominant_colors().colors( i ).color().red() // float
                                << ","
                                << response.image_properties_annotation().dominant_colors().colors( i ).color().green() // float
                                << ","
                                << response.image_properties_annotation().dominant_colors().colors( i ).color().blue() // float
                                << std::endl;

                                if ( response.image_properties_annotation().dominant_colors().colors( i ).color().has_alpha() ) {
                                    std::cout
                                        << "\talpha: "
                                        << response.image_properties_annotation().dominant_colors().colors( i ).color().alpha().value() // float
                                        << std::endl;
                                }
                        }
                    }

                } else { std::cout << "NONE" << std::endl; }


                std::cout << "\n\n----Crop Hints Annotations----" << std::endl;

                if ( response.has_crop_hints_annotation() ) {
                    std::cout << "Size: " << response.crop_hints_annotation().crop_hints_size() << std::endl; 
                    for ( int i = 0; i < response.crop_hints_annotation().crop_hints_size(); i++ ) {
                        std::cout << "BoundingPoly: " << response.crop_hints_annotation().crop_hints( i ).has_bounding_poly() << std::endl;
                        std::cout << "\tVertSize: " << response.crop_hints_annotation().crop_hints( i ).bounding_poly().vertices_size() << std::endl;
                        for ( int j = 0; j < response.crop_hints_annotation().crop_hints( i ).bounding_poly().vertices_size(); j++ ) {
                            std::cout << "\tVert: X: " << response.crop_hints_annotation().crop_hints( i ).bounding_poly().vertices( j ).x() << " Y: " << response.crop_hints_annotation().crop_hints( i ).bounding_poly().vertices( j ).y() << std::endl;
                        }
                    }
                } else { std::cout << "NONE" << std::endl; }


                std::cout << "\n\n----Web Detection----" << std::endl;
                if ( response.has_web_detection()  ) {
                    
                    std::cout << "WebEntities: " << response.web_detection().web_entities_size() << std::endl;
                    for ( int i = 0; i < response.web_detection().web_entities_size(); i++ ) {
                        std::cout
                            << "\tid: "
                            << response.web_detection().web_entities( i ).entity_id() // string
                            << "\n\tscore: "
                            << response.web_detection().web_entities( i ).score() // float
                            << "\n\tdescription: "
                            << response.web_detection().web_entities( i ).description() // string
                            << std::endl;
                    }

                    std::cout << "Full Matching Images: " << response.web_detection().full_matching_images_size() << std::endl;
                    for ( int i = 0; i < response.web_detection().full_matching_images_size(); i++ ) {
                        std::cout << "\tScore: "
                            << response.web_detection().full_matching_images( i ).score() // float
                            << "    URL: "
                            << response.web_detection().full_matching_images( i ).url() // string
                            << std::endl;
                    }
                    
                    std::cout << "Partially Matching Images: " << response.web_detection().partial_matching_images_size() << std::endl;
                    for ( int i = 0; i < response.web_detection().partial_matching_images_size(); i++ ) {
                        std::cout << "\tScore: "
                            << response.web_detection().partial_matching_images( i ).score() // float
                            << "    URL: "
                            << response.web_detection().partial_matching_images( i ).url() // string
                            << std::endl;
                    }

                    std::cout << "Pages With Matching Images: " << response.web_detection().pages_with_matching_images_size() << std::endl;
                    for ( int i = 0; i < response.web_detection().pages_with_matching_images_size(); i ++ ) {
                        std::cout << "\tScore: "
                            << response.web_detection().pages_with_matching_images( i ).score() // float
                            << "    URL: "
                            << response.web_detection().pages_with_matching_images( i ).url() // string
                            << std::endl;
                    }
                
                } else { std::cout << "NONE" << std::endl; }

            }

        } else if (  status.ok() ){
            std::cout << "Status Returned Canceled" << std::endl;

            //sleep(0.5);
        }else {
            std::cerr << "RPC failed" << std::endl;
            //sleep(0.5);
            std::cout << status.error_code() << ": " << status.error_message() << status.ok() << std::endl;
        }

        google::protobuf::ShutdownProtobufLibrary();
        
        requests.release_parent();
        response.release_error();
        response.release_context();
        response.release_web_detection();
        response.release_full_text_annotation();
        response.release_crop_hints_annotation();
        response.release_product_search_results();
        response.release_safe_search_annotation();
        response.release_image_properties_annotation();
        
        std::cout << "\nAll Finished!" << std::endl;
       

        yarp::os::Bottle bottle_result;
        bottle_result.clear();
       
        bottle_result.addString("Got it");
        
        /*if ( dialog_status.ok() ) {

           yInfo() << "Status returned OK";
           yInfo() << "\n------Response------\n";

           result.addString(response.query_result().response_messages().Get(0).text().text().Get(0).c_str());
           yDebug() << "result bottle" << result.toString();

      } else if ( !dialog_status.ok() ) {
            yError() << "Status Returned Canceled";
      }
      request.release_query_input();
      query_input.release_text();*/
      
      return bottle_result;
   }

    /********************************************************/
    bool annotate()
    {
        yarp::os::Bottle &outTargets = targetPort.prepare();
        outTargets.clear();
        std::lock_guard<std::mutex> lg(mtx);
        outTargets = queryGoogleVisionAI(annotate_img);
        targetPort.write();
        
        return true;
    }

    /********************************************************/
    bool stop_acquisition()
    {
        return true;
    }
};

/********************************************************/
class Module : public yarp::os::RFModule, public googleVisionAI_IDL
{
    yarp::os::ResourceFinder    *rf;
    yarp::os::RpcServer         rpcPort;

    Processing                  *processing;
    friend class                processing;

    bool                        closing;

    /********************************************************/
    bool attach(yarp::os::RpcServer &source)
    {
        return this->yarp().attachAsServer(source);
    }

public:

    /********************************************************/
    bool configure(yarp::os::ResourceFinder &rf)
    {
        this->rf=&rf;
        std::string moduleName = rf.check("name", yarp::os::Value("googleVisionAI"), "module name (string)").asString();
        std::string project_id = rf.check("project", yarp::os::Value("1"), "id of the project").asString();
        std::string language_code = rf.check("language", yarp::os::Value("en-US"), "language of the dialogflow").asString();
        
        yDebug() << "Module name" << moduleName;
        setName(moduleName.c_str());

        rpcPort.open(("/"+getName("/rpc")).c_str());

        closing = false;

        processing = new Processing( moduleName );

        /* now start the thread to do the work */
        processing->open();

        attach(rpcPort);

        return true;
    }

    /**********************************************************/
    bool close()
    {
        processing->close();
        delete processing;
        return true;
    }

    /********************************************************/
    double getPeriod()
    {
        return 0.1;
    }

    /********************************************************/
    bool quit()
    {
        closing=true;
        return true;
    }

    /********************************************************/
    bool annotate()
    {
        processing->annotate();
        return true;
    }

    /********************************************************/
    bool updateModule()
    {
        return !closing;
    }
};

/********************************************************/
int main(int argc, char *argv[])
{
    yarp::os::Network::init();

    yarp::os::Network yarp;
    if (!yarp.checkNetwork())
    {
        yError("YARP server not available!");
        return 1;
    }

    Module module;
    yarp::os::ResourceFinder rf;

    rf.setVerbose( true );
    rf.setDefaultContext( "googleVisionAI" );
    rf.setDefaultConfigFile( "config.ini" );
    rf.setDefault("name","googleVisionAI");
    rf.configure(argc,argv);

    return module.runModule(rf);
}