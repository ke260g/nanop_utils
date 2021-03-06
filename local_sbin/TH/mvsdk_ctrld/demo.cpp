#include<opencv2/core/core.hpp>
#include<opencv2/highgui/highgui.hpp>
#include<cstring>
#include<unistd.h>

#include"mvsdk_iface.h"

using namespace cv;
using std::memcpy;

static int imgCount = 0;

int main(int argc, char **argv) {
	int ret;
	int ImgHeight, ImgWidth;
	float exp_time;
	void *imgbuf;


	// 1: init
	ret = mvsdk_init();
	if(ret) {
		printf("mvsdk_init() error occurs\n");
		return -1;
	}

	// 2: get product name
	char product_name[12];
	ret = mvsdk_get_product_name(product_name, 12);
	if(ret < 0)
		printf("mvsdk_get_product_name() failed\n");
	else
		printf("%s\n", product_name);

	// 3: get resolution
	mvsdk_get_resolution(&ImgHeight, &ImgWidth);
	printf("resolution: %d * %d\n", ImgHeight, ImgWidth);

	// 4: get exposure time
	mvsdk_get_exposure_time(&exp_time);
	printf("exposure time: %f ms\n", exp_time/1000);

	// 5: mono or bayer
	mvsdk_get_mono_bayer(&ret);
	if(ret)
		printf("color camera\n");
	else
		printf("mono camera\n");


	int g_exposure = 1;
	namedWindow("img");
	createTrackbar("exposure(ms): ", "img",
	&g_exposure, 100, NULL, NULL);

	int j = 1;
	do {

		/* 6: set the exposure_time */
#if 0
		if(j == 40)
			j = 1;
		else
			j++;
		exp_time = j * 1000;
#endif

		exp_time = g_exposure * 1000;
		mvsdk_set_exposure_time(exp_time);

		/* 7: get image */
		ret = mvsdk_get_image(&imgbuf);
		if(ret)
			break;
		Mat img(ImgHeight, ImgWidth, CV_8UC1);
		int wr;
		memcpy(img.data, imgbuf, ImgHeight * ImgWidth);
		// flip(img, img, 0);
		imshow("img", img);
		wr = waitKey(500);
		if(wr == 'q')
			break;
		if(wr == 'w') {
			time_t tt_r;
			time(&tt_r);
			struct tm tNow;
			gmtime_r(&tt_r, &tNow);
#define IMG_NAME_LENGTH (40)
			char img_name[IMG_NAME_LENGTH];
			memset(img_name, '\0', IMG_NAME_LENGTH);
			snprintf(img_name, (IMG_NAME_LENGTH-1), "img_%d%d_%d%d_%d%d.bmp",
					tNow.tm_mon, tNow.tm_mday, tNow.tm_hour, tNow.tm_min,
					(int)getpid(), (int)imgCount);
			imwrite(img_name, img);
			printf("save image file: %s.\n", img_name);
			imgCount++;
		}
	} while(1);

	return 0;
}
