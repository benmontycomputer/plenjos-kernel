#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <limits.h>

char *fb;
int scanline;
int width,height;

// TODO: move the PSF header stuff into a header file

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

// Set the base revision to 3, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent, _and_ they should be accessed at least
// once or marked as used with the "used" attribute as done here.

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

// GCC and Clang reserve the right to generate calls to the following
// 4 functions even if they are not directly called.
// Implement them as the C specification mandates.
// DO NOT remove or rename these functions, or stuff will eventually break!
// They CAN be moved to a different .c file.

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;

    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }

    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

// Halt and catch fire function.
static void hcf(void) {
    for (;;) {
        asm ("hlt");
    }
}

/* import our font that's in the object file we've created above */
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
 
void putchar(
    /* note that this is int, not char as it's a unicode character */
    unsigned short int c,
    /* cursor position on screen, in characters not in pixels */
    int cx, int cy,
    /* foreground and background colors, say 0xFFFFFF and 0x000000 */
    uint32_t fg, uint32_t bg)
{
    /* cast the address to PSF header struct */
    PSF_font *font = (PSF_font*)&_binary_src_fonts_ter_v24b_psf_start;

    if (font->width<=0 | font->height<=0 | font->bytesperglyph<=0 | font->numglyph<=0) return;

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
        (cy * font->height * scanline) +
        (cx * (font->width + 1) * sizeof(PIXEL));
    /* finally display pixels according to the bitmap */
    int x,y, line, mask;
    for(y=0;y<font->height;y++){
        if(offs>=height*scanline) break;

        /* save the starting position of the line */
        line=offs;
        mask=1<<(font->width-1);
        /* display a row */
        for(x=0;x<font->width;x++){
            if(line-offs>=scanline) break;
            *((PIXEL*)(fb + line)) = *((unsigned int*)glyph) & mask<<(-3*bytesperline+(x/8)*16) ? fg : bg;
            /* adjust to the next pixel */
            mask >>= 1;
            line += sizeof(PIXEL);
        }
        /* adjust to the next line */
        glyph += bytesperline;
        offs  += scanline;
    }
}

// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.
void kmain(void) {
    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        hcf();
    }

    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    // Fetch the first framebuffer.
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    fb = framebuffer->address;
    scanline = (int)framebuffer->pitch;
    width = (int)framebuffer->width;
    height = (int)framebuffer->height;

    // Note: we assume the framebuffer model is RGB with 32-bit pixels.
    //for (size_t i = 0; i < 100; i++) {
    //    volatile uint32_t *fb_ptr = framebuffer->address;
    //    fb_ptr[i * (framebuffer->pitch / 4) + i] = 0xffffff;
    //}

    putchar('H', 0, 0, (uint32_t)0xffffff, (uint32_t)0x000000);
    putchar('e', 1, 0, (uint32_t)0xffffff, (uint32_t)0x000000);
    putchar('l', 2, 0, (uint32_t)0xffffff, (uint32_t)0x000000);
    putchar('l', 3, 0, (uint32_t)0xffffff, (uint32_t)0x000000);
    putchar('o', 4, 0, (uint32_t)0xffffff, (uint32_t)0x000000);

    putchar('W', 6, 0, (uint32_t)0xffffff, (uint32_t)0x000000);
    putchar('o', 7, 0, (uint32_t)0xffffff, (uint32_t)0x000000);
    putchar('r', 8, 0, (uint32_t)0xffffff, (uint32_t)0x000000);
    putchar('l', 9, 0, (uint32_t)0xffffff, (uint32_t)0x000000);
    putchar('d', 10, 0, (uint32_t)0xffffff, (uint32_t)0x000000);
    putchar('!', 11, 0, (uint32_t)0xffffff, (uint32_t)0x000000);

    // We're done, just hang...
    hcf();
}