#ifndef MVSDK_IFACE_H
#define MVSDK_IFACE_H

/* 
 * Note: it's not safe to
 * using both .h and .hpp in a single process
 */

#ifdef __cplusplus
extern "C" {
#endif

/* all these interface return 0 or -1 if error occurs */
/*
 * @breif interface initialize
 * 		other interface can be used only after initialize successfully
 */
int mvsdk_init(void);
/*
 * @breif get product name
 * @param str point to a buffer user-manipulated
 * @param size size of the str pointed buffer,
 * 		suggestion is 12,
 * 		for example char str[12];
 */
int mvsdk_get_product_name(char *str, const int size);
/*
 * @breif get resolution
 * @param height output-paramter, unit is pixel
 *      will be set to -1 if error occurs
 * @param width output-parameter, unit is pixel
 *      will be set to -1 if error occurs
 */
int mvsdk_get_resolution(int *height, int *width);
/*
 * @breif get exposure time
 * @param exposure_time output-parameter, unit is microsecond(us)
 *      will be set to -1 if error occurs.
 */
int mvsdk_get_exposure_time(float *exposure_time);
/*
 * @breif set exposure time
 * @param exposure_time input-parameter, unit is microsecond(us)
 */
int mvsdk_set_exposure_time(const float exposure_time);
/*
 * @brief soft trigger and get an image
 * @param bufpt output-parameter, pointing to the raw image buffer
 * 		size is define by resolution, height * width
 * 		will be set to NULL if error occurs
 */
int mvsdk_get_image(void **bufpt);
/*
 * @brief get the color mode, mono or bayer
 * @param val output-parameter,
 * 		0 means mono, other means bayer
 *      can not be trusted if error occurs
 */
int mvsdk_get_mono_bayer(int *val);

#ifdef __cplusplus
}
#endif

#endif /* MVSDK_IFACE_H */
