#include <stdbool.h>
#include <limits.h>

#include <stdint.h>

#include <stdarg.h>

#include "string.h"

#include "uconsole.h"

#define PSF1_FONT_MAGIC 0x0436

typedef struct {
    uint16_t magic; // Magic bytes for identification.
    uint8_t fontMode; // PSF font mode.
    uint8_t characterSize; // PSF character size.
} PSF1_Header;


#define PSF_FONT_MAGIC 0x864ab572

typedef struct {
    uint32_t magic;         /* magic bytes to identify PSF */
    uint32_t version;       /* zero */
    uint32_t headersize;    /* offset of bitmaps in file, 32 */
    uint32_t flags;         /* 0 if there's no unicode table */
    uint32_t numglyph;      /* number of glyphs */
    uint32_t bytesperglyph; /* size of each glyph */
    uint32_t height;        /* height in pixels */
    uint32_t width;         /* width in pixels */
} PSF_font;

/* import our font that's in the object file we've created above */
extern PSF_font _binary_src_fonts_ter_v14n_psf_start;
extern PSF_font _binary_src_fonts_ter_v14n_psf_end;

extern PSF_font _binary_src_fonts_ter_v16n_psf_start;
extern PSF_font _binary_src_fonts_ter_v16n_psf_end;

extern PSF_font _binary_src_fonts_ter_v16b_psf_start;
extern PSF_font _binary_src_fonts_ter_v16b_psf_end;

extern PSF_font _binary_src_fonts_ter_v24b_psf_start;
extern PSF_font _binary_src_fonts_ter_v24b_psf_end;

extern PSF_font _binary_src_fonts_ter_v32n_psf_start;
extern PSF_font _binary_src_fonts_ter_v32n_psf_end;

uint16_t *unicode;

#define PIXEL uint32_t   /* pixel pointer */
 
// Uses PSF2 format
void putcharbig(
    /* note that this is int, not char as it's a unicode character */
    unsigned short int c,
    /* cursor position on screen, in characters not in pixels */
    int cx, int cy,
    /* foreground and background colors, say 0xFFFFFF and 0x000000 */
    uint32_t fg, uint32_t bg)
{
    /* cast the address to PSF header struct */
    PSF_font *font = (PSF_font*)&_binary_src_fonts_ter_v24b_psf_start;

    /* we need to know how many bytes encode one row */
    int bytesperline=(font->width+7)/8;
    /* unicode translation */
    if(unicode != NULL) {
        c = unicode[c];
    }
    /* get the glyph for the character. If there's no
       glyph for a given character, we'll display the first glyph. */
    unsigned char *glyph =
     (unsigned char*)&_binary_src_fonts_ter_v24b_psf_start +
     font->headersize +
     (c>0&&c<font->numglyph?c:0)*font->bytesperglyph;
    /* calculate the upper left corner on screen where we want to display.
       we only do this once, and adjust the offset later. This is faster. */
    int offs =
        (cy * font->height * fb_scanline) +
        (cx * (font->width + 1) * sizeof(PIXEL));
    /* finally display pixels according to the bitmap */
    int x,y, line,mask;
    for(y=0;y<(int)font->height;y++){
        if(offs>=fb_height*fb_scanline) break;

        /* save the starting position of the line */
        line=offs;
        mask=1<<(8*bytesperline-1);
        /* display a row */
        for(x=0;x<(int)font->width;x++){
            if(line-offs>=fb_scanline) break;
            int shift = ((x/8)*16)-(8*(bytesperline-1));
            *((PIXEL*)(fb + line)) = *((unsigned int*)glyph) & (shift>=0?mask<<shift:mask>>-shift) ? fg : bg;
            /* adjust to the next pixel */
            mask >>= 1;
            line += sizeof(PIXEL);
        }
        /* adjust to the next line */
        glyph += bytesperline;
        offs  += fb_scanline;
    }
}

// Uses PSF1 format
void kputchar(
    /* note that this is int, not char as it's a unicode character */
    unsigned short int c,
    /* cursor position on screen, in characters not in pixels */
    int cx, int cy,
    /* foreground and background colors, say 0xFFFFFF and 0x000000 */
    uint32_t fg, uint32_t bg)
{
    if((cx+1)>=(fb_width/FONT_W)) return;

    /* cast the address to PSF header struct */
    PSF1_Header *font = (PSF1_Header*)&_binary_src_fonts_ter_v16b_psf_start;

    /* we need to know how many bytes encode one row */
    int bytesperline=(8+7)/8;
    /* unicode translation */
    if(unicode != NULL) {
        c = unicode[c];
    }
    /* get the glyph for the character. If there's no
       glyph for a given character, we'll display the first glyph. */
    unsigned char *glyph =
     (unsigned char*)&_binary_src_fonts_ter_v16b_psf_start +
     4 +
     (c)*font->characterSize;
    /* calculate the upper left corner on screen where we want to display.
       we only do this once, and adjust the offset later. This is faster. */
    int offs =
        (cy * font->characterSize * fb_scanline) +
        (cx * (8 + 1) * sizeof(PIXEL));
    /* finally display pixels according to the bitmap */
    int x,y, line,mask;
    for(y=0;y<font->characterSize;y++){
        if(offs>=fb_height*fb_scanline) break;

        /* save the starting position of the line */
        line=offs;
        mask=1<<(8-1);
        /* display a row */
        for(x=0;x<8;x++){
            if(line-offs>=fb_scanline) break;
            *((PIXEL*)(fb + line)) = *((unsigned int*)glyph) & mask ? fg : bg;
            /* adjust to the next pixel */
            mask >>= 1;
            line += sizeof(PIXEL);
        }
        /* adjust to the next line */
        glyph += bytesperline;
        offs  += fb_scanline;
    }
}

void kputs(const char *str, int cx, int cy){
    char *ch = (char *)str;

    for(;*ch;cx++){
        if(cx>=(fb_width/FONT_W)){
            cx-=(fb_width/FONT_W);
            cy++;
        }

        kputchar(*ch,cx,cy,0xffffff,0x000000);

        ch+=sizeof(char);
    }
}

char indices[] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};

void kputhex(uint64_t hex, int cx, int cy) {
    uint64_t adj = hex;

    kputs("0x", cx, cy);

    for(int i = 16; i > 0; i--) {
        kputchar(indices[adj & 0xf], cx + i + 1, cy, 0xffffff, 0x000000);

        adj = (adj >> 4);
    }
}