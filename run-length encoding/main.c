#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#pragma pack(push, 1)
typedef struct {
    unsigned short fileTypeIdentifier;      //file type, for BMP should be 0x4D42
    unsigned int sizeOfFile;                //size of full .bmp file
    unsigned int reserved;                  //reserved; is not important for us
    unsigned int offset;                    //start address of pixel array (bitmap image data)
} BITMAPFILEHEADER;
#pragma pack(pop) //14 bytes

#pragma pack(push, 1)
typedef struct {
    unsigned int headerSize;                //size of this struct (header)
    unsigned int bitmapWidth;               //width [px]
    unsigned int bitmapHeight;              //height [px]
    unsigned short bitmapColorPlanes;       //color planes, should be 1
    unsigned short bitsPerPixel;            //color depth (bits per pixel), should be 8
    unsigned int typeOfCompression;         //type of compression, can be only BI_RGB(0), BI_RLE8(1)
    unsigned int imageSize;                 //total image size in bytes
    unsigned int horizontalResolution;      //number of pixels per meter in x axis
    unsigned int verticalResolution;        //number of pixels per meter in y axis
    unsigned int numberOfUsedColors;        //number of colors used
    unsigned int numberOfImportantColors;   //number of colors that are important
} BITMAPINFOHEADER; //40 bytes
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    unsigned char flags;
} COLORTABLE; //16 bytes per slot
#pragma pack(pop)

// step-by-step linkage: https://www.devdungeon.com/content/how-mix-c-and-assembly#
// tech specs: x86-64, all SSE's (up to 4.1) are allowed, no MMX & FPU tricks

unsigned char *loadBitmapFile(char *filename, BITMAPINFOHEADER *bitmapInfoHeader,
                              BITMAPFILEHEADER *bitmapFileHeader, COLORTABLE *colorTable, char *extraInfo) {

    FILE *filePtr;                  //pointer to a given .bmp file
    unsigned char *pixelArray;      //array of pixels (image data)

    //open given address in read binary mode
    filePtr = fopen(filename, "rb");

    if (filePtr == NULL) {
        printf("ERROR: can't open given .bmp file.\n");
        exit(1);
    }

    //read the bitmap file header
    fread(bitmapFileHeader, sizeof(BITMAPFILEHEADER), 1, filePtr);

    //verify file ID (.bmp -> 0x4d42)
    if (bitmapFileHeader->fileTypeIdentifier != 0x4D42) {
        printf("ERROR: File loading is failed, file ID isn't .bmp\n");
        fclose(filePtr);
        exit(1);
    }

    //read the bitmap info header
    fread(bitmapInfoHeader, sizeof(BITMAPINFOHEADER), 1, filePtr);

    if (bitmapInfoHeader->headerSize > 40) {
        extraInfo = malloc(bitmapInfoHeader->headerSize - sizeof(BITMAPINFOHEADER));
        fread(extraInfo, bitmapInfoHeader->headerSize - sizeof(BITMAPINFOHEADER), 1, filePtr);
    }
    //read the bitmap colorTable
    fread(colorTable, sizeof(COLORTABLE), 256, filePtr);

    //move file pointer to the beginning of the pixel array
    fseek(filePtr, (long) bitmapFileHeader->offset, SEEK_SET);

    //allocate enough memory for the pixel array
    pixelArray = (unsigned char *) malloc(bitmapInfoHeader->imageSize);

    //verify memory allocation
    if (!pixelArray) {
        printf("ERROR: failed to allocate enough memory for pixel array.\n");
        free(pixelArray);
        fclose(filePtr);
        exit(1);
    }

    //read in the bitmap image data (pixel array)
    fread(pixelArray, bitmapInfoHeader->imageSize, 1, filePtr);

    //close file and return the pixel array
    fclose(filePtr);

    return pixelArray;
}

unsigned int bmp_rle_simulation(const unsigned char *bitmapData, unsigned char *bitmapDataDest,
                                unsigned int width, unsigned int size) {

    unsigned int newWidth = width % 4 == 0 ? width : width + (4 - width % 4);

    //index of current memory slot in the destination array
    unsigned int currentIndex = 0;

    //outer loop - "first" to "last" pixel; (reading bottom-up left to right)
    for (unsigned int heightCounter = 0; heightCounter < size; heightCounter += newWidth) {

        //middle loop - processes a line of pixels;
        //widthCounter - index of the current pixel in the line
        for (unsigned int widthCounter = 0; widthCounter < newWidth; ++widthCounter) {

            //'color' defines a value of color of the current position in the line;
            // having a possibility to expect more pixels of the same color as the current 'color'
            // we define the size of our newly-formed (current) tuple as 1.*/

            unsigned char color = *(bitmapData + heightCounter + widthCounter);

            unsigned int tupleSize = 1;

            //collects (min. 3) consequent pixels which are to be put into tuples (chunks)
            for (int sequenceCollector = 1; sequenceCollector <= newWidth - widthCounter; ++sequenceCollector) {

                //next byte of the sequence
                unsigned char nextByte = *(bitmapData + heightCounter + widthCounter + sequenceCollector);

                //if colors of the firs byte in the current sequence and
                // further following (not only succeeding) byte match
                // then the tuple grows (increment)*/

                if (nextByte == color) {
                    tupleSize++;
                } else {

                    //if colors don't match,
                    // we check whether the current sequence is distinctly a tuple - has a size of min. 3 pixels
                    // if so, we write the tuple (Encoded mode) and start a new sequence [cont]*/

                    if (tupleSize > 2) {

                        //write the tuple (size, color) and increment the index of the current memory slot
                        unsigned int wholeChunks = tupleSize / 255;
                        unsigned int rest = tupleSize - (wholeChunks * 255);

                        for (int i = 0; i < wholeChunks; ++i) {
                            *(bitmapDataDest + currentIndex++) = 255;
                            *(bitmapDataDest + currentIndex++) = color;
                        }
                        if (rest != 0) {
                            *(bitmapDataDest + currentIndex++) = rest;
                            *(bitmapDataDest + currentIndex++) = color;
                            widthCounter += rest - 1;
                        } //else widthCounter--;


                        //switching to 'absolute mode'
                    } else {

                        //[cont] if not, we proceed with further pixels, but now in 'absolute mode'.

                        //n defines a number of elements to be stored in 'absolute mode'
                        unsigned int n = tupleSize + 1;

                        //counter works same way as tuple size - tracks the number of consequent pixels of the same color
                        unsigned char counter = 1;

                        unsigned char penultimateByte;

                        for (unsigned int absoluteCollector = 1; absoluteCollector < newWidth; ++absoluteCollector) {
                            //incrementing n, since moving on to the next pixel
                            n++;

                            //last fitting iteration
                            if (widthCounter + n > newWidth + 2) {
                                //trim to appropriate size
                                n = newWidth - widthCounter;

                                //set color
                                nextByte = *(bitmapData + heightCounter + widthCounter + sequenceCollector +
                                             absoluteCollector - 3);

                                //break the iteration and write sequence of adjusted length
                                break;
                            }

                            //checking the color, if the same => increment
                            if (*(bitmapData + heightCounter + widthCounter + sequenceCollector + absoluteCollector) ==
                                nextByte) {
                                counter++;

                                //if counter is greater than 2 => (3) we have a tuple (not necessarily complete),
                                // which we must not include in 'Absolute mode'
                                if (counter > 2) {

                                    //setting nextByte to the last color before the 'new tuple' started
                                    nextByte = *(bitmapData + heightCounter + widthCounter + sequenceCollector +
                                                 absoluteCollector - counter);

                                    penultimateByte = *(bitmapData + heightCounter + widthCounter + sequenceCollector +
                                                        absoluteCollector - counter - 1);

                                    //exclude 'new tuple' from the size of the 'absolute' sequence
                                    n -= counter;
                                    break;
                                }
                            } else {
                                //else switch to the next color and reset its counter
                                ////// @Max : ETALON GOVNOCODA
                                nextByte = *(bitmapData + heightCounter + widthCounter + sequenceCollector +
                                             absoluteCollector + (width > 1024));

                                counter = 1;
                            }
                        }

                        //if the 'absolute' sequence has a size less than 3 (2 or 1),
                        // we must write it differently, to avoid confusion with delta (0, 2)
                        if (n < 3) {
                            *(bitmapDataDest + currentIndex++) = 1;
                            *(bitmapDataDest + currentIndex++) = color;
                            if (n == 2) {
                                *(bitmapDataDest + currentIndex++) = 1;
                                *(bitmapDataDest + currentIndex++) = nextByte;
                                widthCounter += 1;
                            }
                        } else {
                            //rounding in to size of char
                            unsigned char nIn255 =  (n > 255) ? 255 : n;
                            *(bitmapDataDest + currentIndex++) = 0;
                            *(bitmapDataDest + currentIndex++) = nIn255;

                            memset(bitmapDataDest + currentIndex, color, tupleSize);
                            memcpy(bitmapDataDest + currentIndex + tupleSize,
                                   bitmapData + heightCounter + widthCounter + sequenceCollector,
                                   nIn255 - tupleSize);

                            if (n > 255) {

                                *(bitmapDataDest + currentIndex++) = 0;
                                widthCounter += 255;
                                currentIndex += 255;
                                n-=255;
                                unsigned int wholeChunks = n / 255;
                                unsigned int rest = n - (wholeChunks * 255);


                                for (int i = 0; i < wholeChunks; ++i) {

                                    *(bitmapDataDest + currentIndex++) = 0;
                                    *(bitmapDataDest + currentIndex++) = 255;

                                    memcpy(bitmapDataDest + currentIndex,
                                           bitmapData + heightCounter + widthCounter + sequenceCollector, 255);

                                    //increment the indices
                                    // because the decrement will be carried out in the last part of writing
                                    widthCounter += 255;
                                    currentIndex += 255;

                                    //255 mod 2 != 0, so we add 0 by default
                                    *(bitmapDataDest + currentIndex++) = 0;
                                }

                                if (rest != 0) {
                                    if (rest < 3) {
                                        *(bitmapDataDest + currentIndex++) = 1;
                                        *(bitmapDataDest + currentIndex++) = penultimateByte;
                                        if (rest == 2) {
                                            *(bitmapDataDest + currentIndex++) = 1;
                                            *(bitmapDataDest + currentIndex++) = nextByte;
                                            widthCounter += 1;
                                        }
                                    } else {
                                        *(bitmapDataDest + currentIndex++) = 0;
                                        *(bitmapDataDest + currentIndex++) = rest;

                                        memcpy(bitmapDataDest + currentIndex,
                                               bitmapData + heightCounter + widthCounter + sequenceCollector, rest);

                                        widthCounter += rest - 1;
                                        currentIndex += rest;

                                        //adding 0 depending on size of the sequence
                                        if (rest % 2 != 0) {
                                            *(bitmapDataDest + currentIndex++) = 0;
                                        }
                                    }
                                }

                                //standard case (size of 'char' is sufficient)
                            } else {
                                widthCounter += n - 1;
                                currentIndex += n;

                                if (n % 2 != 0 && n > 2) {
                                    *(bitmapDataDest + currentIndex++) = 0;
                                }
                            }

                        }
                    }
                    break;
                }
            }
        }

        //write end of line: 0x0000
        *(bitmapDataDest + currentIndex++) = 0;
        *(bitmapDataDest + currentIndex++) = 0;

    }
    //write end of file: 0x0001
    *(bitmapDataDest + currentIndex - 2) = 0;
    *(bitmapDataDest + currentIndex - 1) = 1;

    return currentIndex;
}

//for all if's here: https://en.wikipedia.org/wiki/BMP_file_format
void valuesCheck(BITMAPINFOHEADER infoHeader) {
    if (infoHeader.bitmapColorPlanes != 1) {
        printf("current # of planes: %hu\n", infoHeader.bitmapColorPlanes);
        printf("ERROR: number of planes in .bmp isn't 1.\n");
        exit(1);
    }
    if (infoHeader.bitsPerPixel != 8) {
        printf("current bpp: %d\n", infoHeader.bitsPerPixel);
        printf("ERROR: number of bits per pixe in .bmp isn't 8.\n");
        exit(1);
    }
    if (infoHeader.typeOfCompression > 0) {
        printf("current type of compression: %d\n", infoHeader.typeOfCompression);
        printf("ERROR: Compression type isn't 0 (BI_RGB/not compressed).\n");
        exit(1);
    }
}

int main(int argc, char *argv[]) {     //argv[1] - input image

    if (argc < 2) {
        printf("ERROR: Number of arguments is too low.\n");
        return -1;
    }

    BITMAPFILEHEADER fileHeader;
    BITMAPINFOHEADER infoHeader;

    //extra char array in case if header is more than 40 bytes
    char *extraData = NULL;

    //number of colors in a color table of 8 bit bitmap
    COLORTABLE *colorTable = malloc(sizeof(COLORTABLE) * 256);

    //bitmapData - pointer to array of pixels
    unsigned char *bitmapData = loadBitmapFile(argv[1], &infoHeader, &fileHeader, colorTable, extraData);

    //check if the values in infoHeader are valid
    valuesCheck(infoHeader);

    //pointer to a new pixel array
    //the size of 2 * sizeImage isn't random: in worst case (if all pixels are different) the size will be increased by 2
    unsigned char *newBitmapData = malloc(infoHeader.imageSize * 2);

    //bmp_rle modifies the array and writes it to newBitmapData, returns a new length
    unsigned int sizeOfCompressedPixelArray = bmp_rle_simulation(bitmapData,
                                                                 newBitmapData,
                                                                 infoHeader.bitmapWidth,
                                                                 infoHeader.imageSize);

    if (sizeOfCompressedPixelArray < 2) {
        printf("ERROR: Size of compressed pixel array is too small.\n");
        return -1;
    }

    //adjust the size of newBitmapData
    fileHeader.sizeOfFile =
            sizeof(BITMAPFILEHEADER) + infoHeader.headerSize + 256 * sizeof(COLORTABLE) +
            sizeOfCompressedPixelArray;

    //replace the imageSize in infoHeader
    infoHeader.imageSize = sizeOfCompressedPixelArray;

    //from uncompressed BI_RGB (0) to BI_RLE8 (1)
    infoHeader.typeOfCompression = 1;

    //save the new file to the location written in argv[1]
    //taken from https://stackoverflow.com/questions/2654480/writing-bmp-image-in-pure-c-c-without-other-libraries

    FILE *resultFile;
    resultFile = fopen("res.bmp", "wb");

    fwrite(&fileHeader, sizeof(BITMAPFILEHEADER), 1, resultFile);
    fwrite(&infoHeader, sizeof(BITMAPINFOHEADER), 1, resultFile);
    if (infoHeader.headerSize > 40) {
        //if the size of infoHeader is more than 40 bytes, we save the other part
        printf("-- writing extra part of infoHeader ---\n");
        fwrite(&extraData, infoHeader.headerSize - sizeof(BITMAPINFOHEADER), 1, resultFile);
    }
    fwrite(colorTable, sizeof(COLORTABLE), 256, resultFile);

    fseek(resultFile, fileHeader.offset, SEEK_SET);
    fwrite(newBitmapData, sizeOfCompressedPixelArray, 1, resultFile);

    free(extraData);
    free(newBitmapData);
    free(bitmapData);
    fclose(resultFile);

    return 0;
}