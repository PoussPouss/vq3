#include <vq3.hpp>
#include <vq3demo.hpp>
#include <opencv2/opencv.hpp>

#include <iostream>
#include <iomanip>
#include <chrono>

// This example shows how to perform multi-threaded competitive
// Hebbian learning.



// Graph definition
//
///////////////

using vertex = vq3::demo2d::Point;
using edge   = vq3::decorator::tagged<void>; // We need tags on the edges.
using graph  = vq3::graph<vertex, edge>; 

// Distance
//
////////////////


// This is the distance used by closest-like algorithms. We need to
// compare actual vertex values with points.
double d2(const vertex& v, const vq3::demo2d::Point& p) {return vq3::demo2d::d2(v, p);}



// Main
//
////////////////

#define NB_VERTICES_PER_M2    750
#define NB_SAMPLES_PER_M2   10000


int main(int argc, char* argv[]) {
  if(argc != 2) {
    std::cout << "Usage : " << argv[0] << " nb_threads" << std::endl;
    return 0;
  }

  unsigned int nb_threads = std::atoi(argv[1]);
  
  std::random_device rd;  
  std::mt19937 random_device(rd());

  graph g;


  // Image settings
  //
  ///////////////////

  
  cv::namedWindow("image", CV_WINDOW_AUTOSIZE);
  auto image       = cv::Mat(768, 1024, CV_8UC3, cv::Scalar(255,255,255));
  auto frame       = vq3::demo2d::opencv::direct_orthonormal_frame(image.size(), .2*image.size().width, true);
  auto dd          = vq3::demo2d::opencv::dot_drawer<vq3::demo2d::Point>(image, frame,
									 [](const vq3::demo2d::Point& pt) {return                      true;},
									 [](const vq3::demo2d::Point& pt) {return                        pt;},
									 [](const vq3::demo2d::Point& pt) {return                         1;},
									 [](const vq3::demo2d::Point& pt) {return cv::Scalar(200, 200, 200);},
									 [](const vq3::demo2d::Point& pt) {return                        -1;});
  auto draw_edge   = vq3::demo2d::opencv::edge_drawer<graph::ref_edge>(image, frame,
								       [](const vertex& v1, const vertex& v2, const edge& e) {return true;},  // always draw
								       [](const vertex& v)                  {return                     v;},  // position
								       [](const edge&   e)                  {return cv::Scalar(255, 0, 0);},  // color
								       [](const edge&   e)                  {return                     3;}); // thickness
  auto draw_vertex = vq3::demo2d::opencv::vertex_drawer<graph::ref_vertex>(image, frame,
									   [](const vertex& v) {return                true;},  // always draw
									   [](const vertex& v) {return                   v;},  // position
									   [](const vertex& v) {return                   3;},  // radius
									   [](const vertex& v) {return cv::Scalar(0, 0, 0);},  // color
									   [](const vertex& v) {return                  -1;}); // thickness
  
  // Distribution settings
  //
  ///////////////////
  
  auto theta = vq3::demo::dyn::linear<vq3::demo::dyn::bound::wrap>(0, 0, 360, .5);

  double thickness = .2;
  double sep       =  1;
  double i         =  1;

  double w              = 1;
  double h              = 1;
  vq3::demo2d::Point p1 = {-sep, 0};
  auto rect             = vq3::demo2d::sample::rectangle(w, h, i) + p1;

  double R              = .5;
  double r              = R - thickness;
  vq3::demo2d::Point p2 = {sep, 0};
  auto crown            = (vq3::demo2d::sample::disk(R, i) - vq3::demo2d::sample::disk(r, i)) + p2;

  vq3::demo2d::Point A = {-sep + w/2, -thickness/2};
  vq3::demo2d::Point B = { sep - r,    thickness/2};
  auto bar             = vq3::demo2d::sample::rectangle(A, B, i);

  auto density = (rect || bar || crown) % theta();
  
  // Some initializations
  //
  ///////////////////

  // Let us generate the graph as random unconnected vertices.
  double g_intensity  = .1;
  double g_width      = 4;
  double g_height     = 2.2;
  auto vertex_distrib = vq3::demo2d::sample::rectangle(g_width, g_height, g_intensity);
  for(auto& v : vq3::demo2d::sample::sample_set(random_device, vertex_distrib, NB_VERTICES_PER_M2)) g += v;

  std::cout << std::endl
	    << std::endl 
	    << std::endl
	    << "##################" << std::endl
	    << std::endl
	    << "press ESC to quit." << std::endl
	    << std::endl
	    << "##################" << std::endl
	    << std::endl;
  int keycode = 0;
 
  // We need to register the input samples in a vector since we want
  // to both use and display them.
  std::vector<vq3::demo2d::Point> S;

  auto processor = vq3::epoch::chl::processor(g);
  
  // This is the loop
  //
  //////////////////

  while(keycode != 27) {
    
    // Get the samples
    
    auto S_ = vq3::demo2d::sample::sample_set(random_device, density, NB_SAMPLES_PER_M2);
    S.clear();
    auto out = std::back_inserter(S);
    std::copy(S_.begin(), S_.end(), out);

    // Edge update
    
    auto t_start = std::chrono::high_resolution_clock::now();
    processor.update_edges(nb_threads,
			   S.begin(), S.end(),
			   [](const vq3::demo2d::Point& s) {return s;}, // Gets the sample from *it.
			   [](const vertex& v) {return v;},             // Gets the prototype from the vertex value.
			   d2,                                          // d2(prototype, sample).
			   edge());                                     // New edge initialization value.
    auto t_end = std::chrono::high_resolution_clock::now();

    // Edge count (for display)
    
    unsigned int nb_edges = 0;
    g.foreach_edge([&nb_edges](graph::ref_edge& ref_e) {
	auto extr = ref_e->extremities();
	if(invalid_extremities(extr))
	  ref_e->kill();
	else
	  ++nb_edges;
      });
    
    int duration =  std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
    std::cout << "Step duration (" << nb_threads << " threads, " << std::setw(3) << nb_edges << " edges) : " << std::setw(6) << duration << " ms.    \r" << std::flush;
				  
    
    // Display
    
    image = cv::Scalar(255, 255, 255);
    std::copy(S.begin(), S.end(), dd);
    g.foreach_edge(draw_edge); 
    g.foreach_vertex(draw_vertex);
    cv::imshow("image", image);
    keycode = cv::waitKey(1) & 0xFF;

    ++theta;
  }

  std::cout << std::endl;

  return 0;
}