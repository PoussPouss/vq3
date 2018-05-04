#include <vq3demo.hpp>
#include <random>

#define GRID_WIDTH             15
#define GRID_HEIGHT             8

#define SOM_H_RADIUS            5.1
#define SOM_MAX_DIST            (unsigned int)(SOM_H_RADIUS-1e-3)

//                                                                               ## Node properties :
using layer_0 = vq3::demo2d::Point;                                              // prototypes are 2D points (this is the "user defined" value).
using layer_1 = vq3::decorator::tagged<layer_0>;                                 // we add a tag for topology computation.
using layer_2 = vq3::decorator::smoother<layer_1, vq3::demo2d::Point, 1, 21, 2>; // smoothing of prototypes.
using vertex  = layer_2;

using graph  = vq3::graph<vertex, void>;

// This is the distance used by closest-like algorithms. We need to
// compare actual vertex values with points.
double d2(const vertex& v, const vq3::demo2d::Point& p) {return vq3::demo2d::d2(v.vq3_value, p);}

using epoch_data_0 = vq3::epoch::data::none<vq3::demo2d::Point>; // This is the root of the stack, the sample type has to be provided.
using epoch_data_1 = vq3::epoch::data::wtm<epoch_data_0>;        // This gathers computation for batch winner-take-most.
using epoch_data   = epoch_data_1;

#define SPEED_TO_METER .5

int main(int argc, char* argv[]) {
  
  unsigned int nb_threads = std::thread::hardware_concurrency();
  if(nb_threads == 0) {
    nb_threads = 1;
    std::cout << "The harware multi-threading capabilities cannot be queried. I use a single thread." << std::endl;
  }
  else
    std::cout << "I use " << nb_threads << " thread(s)." << std::endl;
  std::cout << std::endl;

  std::random_device rd;  
  std::mt19937 random_device(rd());

  
  // Distribution and display
  //
  ///////////////////
  
  int N_slider =  5000;
  int T_slider =   127;
  int I_slider =     0; // I use a slider for a boolean value since I do not require QT support for opencv.

  auto video_data = vq3::demo2d::opencv::sample::video_data(0, [&T_slider, &I_slider](const unsigned char* rgb_pixel) {
      unsigned char threshold = (unsigned char)(T_slider);
      double res;
      if(rgb_pixel[0] < threshold
	 || rgb_pixel[1] < threshold
	 || rgb_pixel[2] < threshold)
	res = 1.;
      else
	res = 0.;
      if(I_slider == 1)
	return 1.0 - res;
      else
	return res;
    });

  auto density = vq3::demo2d::opencv::sample::webcam(video_data);

  auto input_size  = video_data.image.size();
  video_data.frame = vq3::demo2d::opencv::direct_orthonormal_frame(input_size, .5*input_size.width, true);

  
  cv::namedWindow("video", CV_WINDOW_AUTOSIZE);
  cv::namedWindow("image", CV_WINDOW_AUTOSIZE);
  cv::createTrackbar("nb/m^2",           "image", &N_slider, 10000, nullptr);
  cv::createTrackbar("threshold",        "image", &T_slider,   255, nullptr);
  cv::createTrackbar("invert selection", "image", &I_slider,     1, nullptr);

  
  auto image = cv::Mat(600, 800, CV_8UC3, cv::Scalar(255,255,255));
  auto frame = vq3::demo2d::opencv::direct_orthonormal_frame(image.size(), .4*image.size().width, true);
  
  auto dd    = vq3::demo2d::opencv::dot_drawer<vq3::demo2d::Point>(image, frame,
								   [](const vq3::demo2d::Point& pt) {return                      true;},
								   [](const vq3::demo2d::Point& pt) {return                        pt;},
								   [](const vq3::demo2d::Point& pt) {return                         1;},
								   [](const vq3::demo2d::Point& pt) {return cv::Scalar(255, 120, 120);},
								   [](const vq3::demo2d::Point& pt) {return                        -1;});

  auto draw_edge   = vq3::demo2d::opencv::edge_drawer<graph::ref_edge>(image, frame,
								       [](const vertex& v1, const vertex& v2) {return   true;}, // always drawing
  								       [](const vertex& v) {return               v.vq3_value;}, // position
  								       []()                {return cv::Scalar(127, 127, 127);}, // color
  								       []()                {return                        1;}); // thickness
  
  auto draw_vertex = vq3::demo2d::opencv::vertex_drawer<graph::ref_vertex>(image, frame,
									   [](const vertex& v) {return                      true;},  // always drawing
  									   [](const vertex& v) {return               v.vq3_value;},  // position
  									   [](const vertex& v) {return                         3;},  // radius
  									   [](const vertex& v) {return cv::Scalar(127, 127, 127);},  // color
  									   [](const vertex& v) {return                        -1;}); // thickness

  auto smooth_edge   = vq3::demo2d::opencv::edge_drawer<graph::ref_edge>(image, frame,
									 [](const vertex& v1, const vertex& v2) {
									   return v1.vq3_smoother.get<0>() && v2.vq3_smoother.get<0>();}, // draw anly if smoothed vertices are available.
									 [](const vertex& v) {return   v.vq3_smoother.get<0>().value();}, // position
									 []()                {return         cv::Scalar(  0,   0, 200);}, // color
									 []()                {return                                1;}); // thickness
  
  auto smooth_vertex = vq3::demo2d::opencv::vertex_drawer<graph::ref_vertex>(image, frame,
									     [](const vertex& v) {return (bool)(v.vq3_smoother.get<0>());},  // draw only is the smoothed vertex is available.
									     [](const vertex& v) {return v.vq3_smoother.get<0>().value();},  // position
									     [](const vertex& v) {return                               3;},  // radius
									     [](const vertex& v) {return       cv::Scalar(  0,   0, 200);},  // color
									     [](const vertex& v) {return                              -1;}); // thickness
  
  auto smooth_speed = vq3::demo2d::opencv::segment_at_vertex_drawer<graph::ref_vertex>(image, frame,
										       [](const vertex& v) {return (bool)(v.vq3_smoother.get<0>());},  // draw only is the smoothed vertex is available.
										       [](const vertex& v) {return v.vq3_smoother.get<0>().value();},  // position
										       [](const vertex& v) {return v.vq3_smoother.get<1>().value()
													    *                      -SPEED_TO_METER;},  // speed
										       [](const vertex& v) {return       cv::Scalar(  0, 200,   0);},  // color
										       [](const vertex& v) {return                               3;}); // thickness

  // Data for computation
  //
  ///////////////////

  graph g;
  
  vq3::utils::make_grid(g, GRID_WIDTH, GRID_HEIGHT,
			[&random_device](unsigned int w, unsigned int h) {
			  graph::vertex_value_type value(vq3::demo2d::uniform(random_device, {-.5, -.5}, {.5, .5}));
			  return value;
			});
  

  // We need to register the input samples in a vector since we want
  // to both use and display them.
  std::vector<vq3::demo2d::Point> S;
  
  // First, we need a structure for handling SOM-like computation. The
  // template argument is the type of input samples. For the sake of
  // function notation homogeneity, let us use the alias algo::som
  // indtead of epoch::wtm.
  auto vertices = vq3::utils::vertices(g);
  auto som      = vq3::algo::som::processor(g, vertices);

  // We need to inform the processor about the graph topology. This
  // can be done once here, since the topology do not change.
  vertices.update_topology(g);
  som.update_topology([](unsigned int edge_distance) {return std::max(0., 1 - edge_distance/double(SOM_H_RADIUS));},
		      SOM_MAX_DIST,
		      1e-3);
  
  // This is the loop
  //
  //////////////////

  std::cout << std::endl
	    << std::endl
	    << std::endl
	    << "##################" << std::endl
	    << std::endl
	    << "click in the image to restart, press ESC to quit." << std::endl
	    << std::endl
	    << "##################" << std::endl
	    << std::endl;

  vq3::temporal::dt_averager frame_delay(.05);
  
  int keycode = 0;
  while(keycode != 27) {
    ++video_data; // get next frame.
    
    // Get the samples
    
    auto S_ = vq3::demo2d::sample::sample_set(random_device, density, N_slider);
    S.clear();
    std::copy(S_.begin(), S_.end(), std::back_inserter(S));

    // SOM update
    
    som.update_prototypes<epoch_data>(nb_threads,
				      S.begin(), S.end(),
				      [](const vq3::demo2d::Point& p) -> const vq3::demo2d::Point& {return p;},
				      [](vertex& vertex_value) -> vq3::demo2d::Point& {return vertex_value.vq3_value;},
				      d2);

    // Temporal update
    
    frame_delay.tick();
    double delay = frame_delay().value_or(1.);
    
    g.foreach_vertex([delay](graph::ref_vertex ref_v) {
	auto& value = (*ref_v)();
	value.vq3_smoother += value.vq3_value;
	value.vq3_smoother.set_timestep(delay);
      });
    
    // Display
    
    image = cv::Scalar(255, 255, 255);
    
    std::copy(S.begin(), S.end(), dd);
    g.foreach_edge(draw_edge); 
    g.foreach_vertex(draw_vertex);
    g.foreach_edge(smooth_edge); 
    g.foreach_vertex(smooth_speed);
    g.foreach_vertex(smooth_vertex);
    
    cv::imshow("image", image);
    cv::imshow("video", video_data.image);
    keycode = cv::waitKey(1) & 0xFF;
  }
  
  return 0;
}