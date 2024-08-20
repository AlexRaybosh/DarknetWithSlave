#ifndef IMAGE_OPENCV_H
#define IMAGE_OPENCV_H
#define OPENCV4
#ifdef OPENCV4
#include <image.h>
#include <opencv2/core/core_c.h>
#include <opencv2/videoio/legacy/constants_c.h>
#include <opencv2/highgui/highgui_c.h>
#include <opencv2/opencv.hpp>

using namespace cv;
extern "C" {
	IplImage *image_to_ipl(image im);
	image ipl_to_image(IplImage* src);
	Mat image_to_mat(image im);

	image mat_to_image(Mat m);
	void *open_video_stream(const char *f, int c, int w, int h, int fps);
	image get_image_from_stream(void *p);
	image load_image_cv(char *filename, int channels);
	int show_image_cv(image im, const char* name, int ms);
	void make_window(char *name, int w, int h, int fullscreen);

	void save_image_to_jpeg_file(image im, const char* name);

}
#endif

#ifdef __cplusplus
#include <vector>
std::vector<uchar> image_to_jpeg_blob(image im);
#endif

#endif
