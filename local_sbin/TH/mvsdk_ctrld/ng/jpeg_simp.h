#ifndef JPEG_SIMP_H
#define JPEG_SIMP_H
/*----
 * libjpeg simple iface for
 * compress and decompress from mem to mem
 * only support 3channels-RGB888 and 1channels-MONO,
 * other format will lead to crash
 *----*/

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
    /*
     * @brief read jpegfile and decompress it to memory
     * @param dstMem an output-param to point to the image-buffer,
     *      the size of the buffer is height*width*channels,
     *      see below.
     * @param filename an input-param to specify the input jpegfile
     * @param height an output-param to indicate the height
     * @param width an output-param to indicate the width
     * @param channels an output-param to indicate the channels,
     *      support only 1 and 3
     * @return return 0 or -1 when error occurs
     *      and all output-param will be set to 0
     */
    int read_JPEG_file2mem(unsigned char **dstMem,
            const char * filename,
            long unsigned int *height,
            long unsigned int *width,
            long unsigned int *channels);

    /*
     * @brief compress the jpeg from memory to memory
     * @param dstMem an output-param to point to the image-buffer
     *      the size of the buffer is dstimagesizecprs,
     *      see below.
     * @param srcMem an input-param to specify the in-memory-jpeg-buffer
     * @param height an input-param to specify the height
     * @param width an input-param to specify the width
     * @param channels an input-param specify the channels,
     *      support only 1 and 3
     * @param quality an input-param specify the compressed quality
     *      `70' is suggested
     * @param dstImageSizeCprs an output-param
     *      to indicate the size of dstMem
     * @return return 0 or -1 when error occurs
     *      and all output-param will be set to 0
     */
    int encompress_JPEG_m2m(unsigned char **dstMem,
            unsigned char *srcMem,
            const long unsigned int height,
            const long unsigned int width,
            const long unsigned int channels,
            const long unsigned int quality,
            long unsigned int *dstImageSizeCprs);

    /*
     * @brief decompress jpeg from memory to memory
     * @param dstMem an output-param points to image-buffer
     *      size of the buffer is height*width*channels
     *      see below.
     * @param srcMem an input-param to specify the in-memory-jpeg-buffer
     * @param height an output-param to indicate the height
     * @param width an output-param to indicate the width
     * @param channels an output-param indicate the channels,
     *      support only 1 and 3
     * @return return 0 or -1 when error occurs
     *      and all output-param will be set to 0
     */
    int decompress_JPEG_m2m(unsigned char **dstMem,
            unsigned char *srcMem,
            const long unsigned int srcImageSizeCprs,
            long unsigned int *height,
            long unsigned int *width,
            long unsigned int *channels);

    /*
     * @brief compress the jpeg from memory to file
     * @param filename an input-param to specify the output jpegfile
     * @param srcMem an input-param to specify the in-memory-jpeg-buffer
     * @param height an input-param to specify the height
     * @param width an input-param to specify the width
     * @param channels an input-param specify the channels,
     *      support only 1 and 3
     * @param quality an input-param specify the compressed quality
     *      `70' is suggested
     * @return return 0 or -1 when error occurs
     */
    int write_JPEG_mem2file(const char *filename,
            unsigned char *srcMem,
            const long int height,
            const long int width,
            const long int channels,
            const long int quality);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // JPEG_SIMP_H
