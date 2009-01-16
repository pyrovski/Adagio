#include "SimpleGraphics.h"

typedef struct {
    //unsigned short sig; //0x4D42 (omitted to be multiple of 32bits in size)
    unsigned int size; //sizeof(this)+img
    unsigned short r1; //0
    unsigned short r2; //0
    unsigned int imgOffset; // sizeof(this)=54
    unsigned int headerSize; //40
    unsigned int width;
    unsigned int height;
    unsigned short planes; // 1
    unsigned short bpp; // {1,4,8,24}
    unsigned int compression; //0
    unsigned int sizeImageData; //width*height*bpp/8
    unsigned int hRes;
    unsigned int vRes;
    unsigned int colors; //0
    unsigned int importantColors; //0
} BitmapHeaderTail;

static BitmapHeaderTail genBMPHeader(int sx, int sy, int bpp) {
    BitmapHeaderTail result;
    //result.sig = 0x4D42;
    result.r1 = 0;
    result.r2 = 0;
    result.headerSize = 40;
    result.width = sx;
    result.height = sy;
    result.planes = 1;
    result.bpp =  bpp;
    result.compression = 0;
    result.sizeImageData = sx*sy*bpp/8;
    result.hRes = 72;
    result.vRes = 72;
    result.colors = 0;
    result.importantColors = 0;
    result.imgOffset = sizeof(BitmapHeaderTail)+2;
    result.size = result.imgOffset+result.sizeImageData;
    return result;

}

Color1b* createImage8bit(int sx, int sy) {
	int i;
    //Color1b* image = new Color1b[sx*sy];
	Color1b* image = (Color1b*)calloc(sx*sy,sizeof(Color1b));

    for (i=0; i<sx*sy; i++) {
        image[i] = 0;
    }

    return image;
}

Color3b* createImage24bit(int sx, int sy) {
	int i;
    //Color3b* image = new Color3b[sx*sy];
	Color3b* image = (Color3b*)calloc(sx*sy,sizeof(Color3b));

    for (i=0; i<sx*sy; i++) {
        image[i].r=image[i].g=image[i].b=0;
    }

    return image;
}

Color4b* createImage32bit(int sx, int sy) {
	int i;
    //Color4b* image = new Color4b[sx*sy];
	Color4b* image = (Color4b*)calloc(sx*sy,sizeof(Color4b));

    for (i=0; i<sx*sy; i++) {
        image[i].r=image[i].g=image[i].b=image[i].a=0;
    }

    return image;

}

static bool writeBMPHeader(FILE* fp, int sx, int sy, int bpp) {
    BitmapHeaderTail bmpHead = genBMPHeader(sx,sy,bpp);
    if (fputc(0x42,fp) == EOF) { fclose(fp); return FALSE; }
    if (fputc(0x4D,fp) == EOF) { fclose(fp); return FALSE; }
    if (fwrite(&bmpHead,sizeof(BitmapHeaderTail),1,fp) != 1) { fclose(fp); return FALSE; }
    return TRUE;
}

// ugly hack - only does well when a few colored pixels are on a black background
bool writeTGA(const char* filename, int sx, int sy, Color3b* image) {
    int pixels = sx*sy;
    
    FILE* fp = fopen(filename,"wb");
    if (fp==NULL) {
        return FALSE;
    }

    //http://astronomy.swin.edu.au/~pbourke/dataformats/tga/
    putc(0,fp);
    putc(0,fp);
    putc(10,fp);            /* 2=uncompressed RGB, 10=RGB RLE */
    putc(0,fp); putc(0,fp);
    putc(0,fp); putc(0,fp);
    putc(0,fp);
    putc(0,fp); putc(0,fp);           /* X origin */
    putc(0,fp); putc(0,fp);           /* y origin */
    putc((sx & 0x00FF),fp);
    putc((sx & 0xFF00) / 256,fp);
    putc((sy & 0x00FF),fp);
    putc((sy & 0xFF00) / 256,fp);
    putc(24,fp);                        /* 24 bit bitmap */
    putc(0,fp);    

    //writeBMPHeader(fp,sx,sy,24);

    int i=0,state=0,length=0;
    Color3b pxBG = {0,0,0};
    //while (i<pixels) {
    for (i=0; i<pixels; i++) {
        Color3b px = image[i];
        if (px.r==pxBG.r && px.g==pxBG.g && px.b==pxBG.b) { // stay in black run
            length++;
            if (length >= 128 || (i==pixels-1) || (i%sx==sx-1)) {
                /*putc(0xFF,fp);
                putc(0xFF,fp);
                putc(0xFE,fp);
                fprintf(fp,"L=(%d)",length);
                putc(0xFF,fp);
                putc(0xFF,fp);*/
                putc(0x80 | (length-1),fp);
                fwrite(&px,sizeof(Color3b),1,fp);
                length=0;
            }
        } else {
            if (length > 0) {
                /*putc(0xFF,fp);
                putc(0xFF,fp);
                putc(0xFA,fp);*/
                putc(0x80 | (length-1),fp);
                fwrite(&pxBG,sizeof(Color3b),1,fp);
                length=0;
            }
            // end run, output this run and this colored pixel
            putc(0,fp);
            fwrite(&px,sizeof(Color3b),1,fp);
        }
    }

    fclose(fp);
    return TRUE;
}

bool writeBMP3b(const char* filename, int sx, int sy, Color3b* image) {
    int pixels = sx*sy;

    FILE* fp = fopen(filename,"wb");
    if (fp==NULL) {
        return FALSE;
    }

    writeBMPHeader(fp,sx,sy,24);
    if (fwrite(image,sizeof(Color3b),pixels,fp) != pixels) { fclose(fp); return FALSE; }


    fclose(fp);
    return TRUE;

}

bool writeBMP1b(const char* filename, int sx, int sy, Color1b* image) {
    int pixels = sx*sy;

    FILE* fp = fopen(filename,"wb");
    if (fp==NULL) {
        return FALSE;
    }

    writeBMPHeader(fp,sx,sy,8);
    if (fwrite(image,sizeof(Color1b),pixels,fp) != pixels) { fclose(fp); return FALSE; }

    fclose(fp);
    return TRUE;

}

bool writeBMP4b(const char* filename, int sx, int sy, Color4b* image) {
    int pixels = sx*sy;

    FILE* fp = fopen(filename,"wb");
    if (fp==NULL) {
        return FALSE;
    }

    writeBMPHeader(fp,sx,sy,32);
    if (fwrite(image,sizeof(Color4b),pixels,fp) != pixels) { fclose(fp); return FALSE; }

    fclose(fp);
    return TRUE;

}

Color3b HSBtoRGB(float hue, float saturation, float brightness) {

    Color3b result;
    if (saturation == 0) {
        result.r = result.g = result.b = (unsigned char) (brightness * 255.0f + 0.5f);
    } else {
        float h = (hue - (float)floor(hue)) * 6.0f;
        float f = h - (float)floor(h);
        float p = brightness * (1.0f - saturation);
        float q = brightness * (1.0f - saturation * f);
        float t = brightness * (1.0f - (saturation * (1.0f - f)));
        switch ((int) h) {
        case 0:
            result.r = (unsigned char) (brightness * 255.0f + 0.5f);
            result.g = (unsigned char) (t * 255.0f + 0.5f);
            result.b = (unsigned char) (p * 255.0f + 0.5f);
            break;
        case 1:
            result.r = (unsigned char) (q * 255.0f + 0.5f);
            result.g = (unsigned char) (brightness * 255.0f + 0.5f);
            result.b = (unsigned char) (p * 255.0f + 0.5f);
            break;
        case 2:
            result.r = (unsigned char) (p * 255.0f + 0.5f);
            result.g = (unsigned char) (brightness * 255.0f + 0.5f);
            result.b = (unsigned char) (t * 255.0f + 0.5f);
            break;
        case 3:
            result.r = (unsigned char) (p * 255.0f + 0.5f);
            result.g = (unsigned char) (q * 255.0f + 0.5f);
            result.b = (unsigned char) (brightness * 255.0f + 0.5f);
            break;
        case 4:
            result.r = (unsigned char) (t * 255.0f + 0.5f);
            result.g = (unsigned char) (p * 255.0f + 0.5f);
            result.b = (unsigned char) (brightness * 255.0f + 0.5f);
            break;
        case 5:
            result.r = (unsigned char) (brightness * 255.0f + 0.5f);
            result.g = (unsigned char) (p * 255.0f + 0.5f);
            result.b = (unsigned char) (q * 255.0f + 0.5f);
            break;
        }
    }
    return result;
}


