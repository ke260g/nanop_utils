#if 0 /* core test interface */
/* Data source and destination managers: memory buffers. */
EXTERN(void)jpeg_mem_dest(j_compress_ptr cinfo, unsigned char ** outbuffer,
        unsigned long * outsize);
EXTERN(void)jpeg_mem_src(j_decompress_ptr cinfo, unsigned char * inbuffer,
        unsigned long insize);

typedef JSAMPLE *JSAMPROW;      /* ptr to one image row of pixel samples. */
typedef JSAMPROW *JSAMPARRAY;   /* ptr to some rows (a 2-D sample array) */
typedef JSAMPARRAY *JSAMPIMAGE; /* a 3-D sample array: top index is color */

/* reading the header, before decompress */
printf("image_width = %d\n", cinfo.image_width);
printf("image_height = %d\n", cinfo.image_height);
printf("num_components = %d\n", cinfo.num_components);
/* after decompress */
printf("output_width = %d\n", cinfo.output_width);
printf("output_height = %d\n", cinfo.output_height);
printf("output_components = %d\n", cinfo.output_components);
#endif

#include<stddef.h>
#include<setjmp.h>
#include<stdlib.h>
#include<string.h>
#include<stdio.h>
#include<jpeglib.h>
#include<errno.h>

#define ERROR_PROMPT "JPEG_SIMP_ERROR: "
#if 1 /* recommand handler interface */
struct my_error_mgr {
    struct jpeg_error_mgr pub;    /* "public" fields */

    jmp_buf setjmp_buffer;        /* for return to caller */
};
typedef struct my_error_mgr * my_error_ptr;
METHODDEF(void) my_error_exit (j_common_ptr cinfo)
{
    /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
    my_error_ptr myerr = (my_error_ptr) cinfo->err;

    /* Always display the message. */
    /* We could postpone this until after returning, if we chose. */
    (*cinfo->err->output_message) (cinfo);

    /* Return control to the setjmp point */
    longjmp(myerr->setjmp_buffer, 1);
}
#endif

int read_JPEG_file2mem(unsigned char **dstMem,
        const char * filename,
        long unsigned int *height,
        long unsigned int *width,
        long unsigned int *channels) {
    /* read in a jpeg file */
    struct jpeg_decompress_struct cinfo; /* decompress object */
    struct my_error_mgr jerr;
    FILE * infile;          /* source file */
    int row_stride;         /* physical row width in output buffer */
    int errno_r;
    int counter;

    *dstMem = NULL;
    *width = 0;
    *height = 0;
    *channels = 0;

    if ((infile = fopen(filename, "rb")) == NULL) {
        errno_r = errno;
        fprintf(stderr, ERROR_PROMPT"fopen() %s error: %s\n", filename, strerror(errno_r));
        errno = errno_r;
        return -1;
    }

    // jpegfile error, it may be not a valid jpegfile
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;
    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return 0;
    }

    jpeg_create_decompress(&cinfo);

    jpeg_stdio_src(&cinfo, infile);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    row_stride = cinfo.output_width * cinfo.output_components;

    *dstMem =
        (unsigned char *)malloc(cinfo.output_height * row_stride);
    if(dstMem == NULL) {
        errno_r = errno;
        fprintf(stderr, ERROR_PROMPT"failed to malloc(): %s", strerror(errno_r));
        errno = errno_r;
        return -1;
    }

    JSAMPROW buffer = (JSAMPLE *)malloc(sizeof(JSAMPLE) * row_stride);
    if(buffer == NULL) {
        errno_r = errno;
        fprintf(stderr, ERROR_PROMPT"failed to malloc(): %s", strerror(errno_r));
        errno = errno_r;
        return -1;
    }

    counter = 0;
    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, &buffer, 1);
        memcpy(*dstMem + counter, buffer, row_stride);
        counter += row_stride;
    }

    // pass out
    *height = cinfo.output_height;
    *width = cinfo.output_width;
    *channels = cinfo.output_components;

    // release temp buffer
    (void)jpeg_finish_decompress(&cinfo);
    (void)jpeg_destroy_decompress(&cinfo);
    fclose(infile);
    free(buffer);
    return 0;
}

int encompress_JPEG_m2m(unsigned char **dstMem,
        unsigned char *srcMem,
        const long unsigned int height,
        const long unsigned int width,
        const long unsigned int channels,
        const long unsigned int quality,
        long unsigned int *dstImageSizeCprs) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    int row_stride;     /* physical row width in image buffer */
    unsigned char *dstImageBuffer;
    int errno_r;

    if(channels != 1 && channels != 3) {
        fprintf(stderr, ERROR_PROMPT"invalid channels: %ld, only 1 or 3", channels);
        return -1;
    }

    // allocate a large buffer for dstImageBuffer
    dstImageBuffer = (unsigned char *)malloc(height * width * channels);
    if(dstImageBuffer == NULL) {
        errno_r = errno;
        fprintf(stderr, ERROR_PROMPT"failed to malloc(): %s", strerror(errno_r));
        errno = errno_r;
        return -1;
    }

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_mem_dest(&cinfo, &dstImageBuffer, dstImageSizeCprs);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = channels;

    // handle mono or rgb, JCS_YCbCr or JCS_RGB
    switch(channels) {
        case 3:
            cinfo.in_color_space = JCS_RGB;
            break;
        case 1:
            cinfo.in_color_space = JCS_GRAYSCALE;
            break;
    }

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    row_stride = width * channels;
    JSAMPROW row_pointer;
    while (cinfo.next_scanline < cinfo.image_height) {
        int line = cinfo.next_scanline;
        row_pointer = srcMem + (line * row_stride);
        jpeg_write_scanlines(&cinfo, &row_pointer, 1);
    }

    // *dstMem = dstImageBuffer;
    (void)jpeg_finish_compress(&cinfo);
    (void)jpeg_destroy_compress(&cinfo);

    // shrink the original large buffer
    *dstMem = realloc(dstImageBuffer, *dstImageSizeCprs);
    return 0;
}

int decompress_JPEG_m2m(unsigned char **dstMem,
        unsigned char *srcMem,
        const long unsigned int srcImageSizeCprs,
        long unsigned int *height,
        long unsigned int *width,
        long unsigned int *channels) {
    struct jpeg_decompress_struct cinfo; /* decompress object */
    struct my_error_mgr jerr;
    int row_stride;         /* physical row width in output buffer */
    int counter;
    unsigned char *ImageBufferNew;
    int errno_r;

    *dstMem = NULL;
    *height = 0;
    *width = 0;
    *channels = 0;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;
    if (setjmp(jerr.setjmp_buffer)) {
        fprintf(stderr, ERROR_PROMPT"unexpected error occurs\n");
        goto err1;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, srcMem, srcImageSizeCprs);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);
    row_stride = cinfo.output_width * cinfo.output_components;
    ImageBufferNew = (unsigned char *)malloc(cinfo.output_height * row_stride);
    if(ImageBufferNew == NULL) {
        errno_r = errno;
        fprintf(stderr, ERROR_PROMPT"failed to malloc(): %s", strerror(errno_r));
        goto err1;
    }

    counter = 0;
    JSAMPROW buffer = (JSAMPLE *)malloc(sizeof(JSAMPLE) * row_stride);
    if(buffer == NULL) {
        errno_r = errno;
        fprintf(stderr, ERROR_PROMPT"failed to malloc(): %s", strerror(errno_r));
        goto err2;
    }
    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, &buffer, 1);
        memcpy(ImageBufferNew + counter, buffer, row_stride);
        counter += row_stride;
    }

    // pass out
    *dstMem = ImageBufferNew;
    *height = cinfo.output_height;
    *width = cinfo.output_width;
    *channels = cinfo.output_components;

    // release temp resources
    (void)jpeg_finish_decompress(&cinfo);
    (void)jpeg_destroy_decompress(&cinfo);
    free(buffer);

    return 0;

err2:
    free(ImageBufferNew);
err1:
    jpeg_destroy_decompress(&cinfo);
    return -1;
}

int write_JPEG_mem2file(const char *filename,
        unsigned char *srcMem,
        const long int height,
        const long int width,
        const long int channels,
        const long int quality) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE * outfile;     /* target file */
    int row_stride;     /* physical row width in image buffer */
    int errno_r;

    if(channels != 1 && channels != 3) {
        fprintf(stderr, ERROR_PROMPT"invalid channels: %ld, only 1 or 3", channels);
        return -1;
    }

    if ((outfile = fopen(filename, "wb")) == NULL) {
        errno_r = errno;
        fprintf(stderr, ERROR_PROMPT"fopen() %s error: %s\n", filename, strerror(errno_r));
        return -1;
    }

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    jpeg_stdio_dest(&cinfo, outfile);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = channels;
    // handle mono or rgb, JCS_YCbCr or JCS_RGB
    switch(channels) {
        case 3:
            cinfo.in_color_space = JCS_RGB;
            break;
        case 1:
            cinfo.in_color_space = JCS_GRAYSCALE;
            break;
    }
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    row_stride = width * channels; //cinfo.input_components;
    JSAMPROW row_pointer;
    while (cinfo.next_scanline < cinfo.image_height) {
        int line = cinfo.next_scanline;
        row_pointer = srcMem + (line * row_stride);
        jpeg_write_scanlines(&cinfo, &row_pointer, 1);
    }

    // release temp resources
    (void)jpeg_finish_compress(&cinfo);
    (void)jpeg_destroy_compress(&cinfo);
    fclose(outfile);
    return 0;
}
