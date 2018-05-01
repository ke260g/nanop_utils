#ifndef MVSDK_IFACE_H
#define MVSDK_IFACE_H

/* 
 * Note: it's not safe to
 * using both .h and .hpp in a single process
 */

#define MVSDK_PRODUCT_NAME_LENGTH (16)
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
 * 		suggestion is 16, MVSDK_PRODUCT_NAME_LENGTH
 * 		for example char str[MVSDK_PRODUCT_NAME_LENGTH];
 */
int mvsdk_get_product_name(char *str, const int size);
/*
 * @breif get resolution
 * @param height output-paramter, unit is pixel
 *      will be set to -1 if error occurs
 * @param width output-parameter, unit is pixel
 *      will be set to -1 if error occurs
 */
int mvsdk_get_resolution(unsigned int *height, unsigned int *width);
/*
 * @breif get expouse time
 * @param expouse_time output-parameter, unit is microsecond(us)
 *      will be set to -1 if error occurs.
 */
int mvsdk_get_exposure_time(unsigned long *expouse_time);
/*
 * @breif set expouse time
 * @param expouse_time input-parameter, unit is microsecond(us)
 */
int mvsdk_set_exposure_time(const unsigned long exposure_time);
/*
 * @brief soft trigger and get an image
 * @param bufpt output-parameter, pointing to the raw image buffer
 * 		size is define by resolution, height * width
 * 		will be set to NULL if error occurs
 */
int mvsdk_get_image(unsigned char **bufpt);
/*
 * @brief get the color mode, mono or bayer
 * @param val output-parameter,
 * 		0 means mono,
 * 		1 means bayer,
 * 		other means error,
 *      can not be trusted if error occurs
 */
int mvsdk_get_mono_bayer(int *val);

#ifdef __cplusplus
}
#endif

#endif /* MVSDK_IFACE_H */
