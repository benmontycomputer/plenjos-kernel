#include "pint.h"

/*
 * pint: a tiny terminal text editor.
 *
 * Only POSIX basics are used: termios for raw mode, read()/write() for I/O and VT100/ANSI escape sequences for output
 * (cursor positioning, erase-to-end-of-line and inverse video). Screen size comes from $LINES/$COLUMNS (80x24 if unset).
 */

#define DEFAULT_FILENAME ".pinttmp"
#define TABLEN 8

#define CTRLHACCEL 8
#define CTRLVACCEL 8

#define TBARH 1
#define BBARH 2

#define DEFAULT_ROWS 24
#define DEFAULT_COLS 80
#define MIN_ROWS 5
#define MIN_COLS 20

#define TITLE " Pint Editor "
#define BBARM "^X: Quit    ^W: Write"
#define BBARI "^X          ^W       "

#define SAVEMSG "Do you want to save? (y)es/(n)o/(c)ancel"
#define FNPROMPT "Filename: "
#define SFAIL_EACCES "Failed to save: permission denied."
#define SFAIL_GENERIC "Failed to save."
#define SFAIL_INVALIDINPUT "Invalid input. Not closing."
#define WOSUCCES "Saved."

#define CTRL_KEY(k) ((k) & 0x1f)
#define PATH_BUF 1024
#define MSG_BUF 1100

#define QUIT 5

enum key {
    K_NONE = 0,
    K_BACKSPACE = 127,
    K_LEFT = 1000,
    K_RIGHT,
    K_UP,
    K_DOWN,
    K_CTRL_LEFT,
    K_CTRL_RIGHT,
    K_CTRL_UP,
    K_CTRL_DOWN,
};

typedef struct fileLines {
    pstring *lines;
    long lineCount;
} fileLines;

typedef struct display {
    long firstRow;
    long rowCap;
    long currentRow;
    long widthCap;
    long xIndex;
    long xOffset;
} display;

static struct fileLines lines;
static struct display currentDisplay;

static char *filename;

static int screenRows = DEFAULT_ROWS;
static int screenCols = DEFAULT_COLS;

/* Where layout() decided the cursor goes (0-based screen coordinates). */
static long cursorY, cursorX;

/* Transient message shown on the row above the bottom bar; cleared on the next key press. */
static char statusmsg[MSG_BUF];
static bool promptActive;

/* ---------------------------------------------------------------------------------------------------------------- */
/* Prototypes */

static long displayPos(pstring row, long arrayIndex);
static int arrayPos(pstring row, long index);
static int edit(void);
static int processChar(int ch);
static int savePrompt(void);
static void layout(void);
static void render(void);
static int save(char *path);
static int combineRows(long keep, long remove);
static int splitRow(long row, long index);
static int insertRow(pstring contents, long index);
static int removeRow(long index);
static int insertChar(long row, char c, long pos);
static int removeChar(long row, long pos);

/* ---------------------------------------------------------------------------------------------------------------- */
/* Terminal handling */

static struct termios orig_termios;
static bool raw_enabled = false;

static void write_all(const char *buf, size_t len) {
    while (len > 0) {
        ssize_t n = write(STDOUT_FILENO, buf, len);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }
        buf += n;
        len -= (size_t)n;
    }
}

static void disable_raw_mode(void) {
    if (raw_enabled) {
        /* Leave the terminal clean: reset attributes, show the cursor, clear the screen. */
        static const char reset[] = "\x1b[m\x1b[?25h\x1b[2J\x1b[H";
        write_all(reset, sizeof(reset) - 1);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_enabled = false;
    }
}

static int enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        return -1;
    }

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_cflag |= CS8;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* read() returns after 1/10 s even without input, so a lone ESC can be told apart from an escape sequence. */
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        return -1;
    }
    raw_enabled = true;
    return 0;
}

static void get_screen_size(void) {
    const char *l = getenv("LINES");
    const char *c = getenv("COLUMNS");

    int rows = l ? atoi(l) : 0;
    int cols = c ? atoi(c) : 0;

    screenRows = rows >= MIN_ROWS ? rows : DEFAULT_ROWS;
    screenCols = cols >= MIN_COLS ? cols : DEFAULT_COLS;
}

/* Reads one byte; returns 1 on success, 0 on timeout. */
static int read_byte(char *out) {
    for (;;) {
        ssize_t n = read(STDIN_FILENO, out, 1);
        if (n == 1) {
            return 1;
        }
        if (n == 0) {
            return 0;
        }
        if (errno == EINTR || errno == EAGAIN) {
            continue;
        }
        return 0;
    }
}

/* Blocks until a key is pressed and returns it. Escape sequences are decoded into K_* values. */
static int read_key(void) {
    char c;
    while (!read_byte(&c)) {
    }

    if (c != '\x1b') {
        return (unsigned char)c;
    }

    /* ESC [ <params> <final>   (e.g. ESC [ A, or ESC [ 1 ; 5 C for ctrl+right) */
    char b;
    if (!read_byte(&b) || (b != '[' && b != 'O')) {
        return K_NONE;
    }

    char params[8];
    int n = 0;
    char final = 0;
    while (read_byte(&b)) {
        if (b >= '@' && b <= '~') {
            final = b;
            break;
        }
        if (n < (int)sizeof(params)) {
            params[n++] = b;
        }
    }

    /* xterm-style modifier parameter: "...;5" means ctrl */
    bool ctrl = n > 0 && params[n - 1] == '5';

    switch (final) {
    case 'A': return ctrl ? K_CTRL_UP : K_UP;
    case 'B': return ctrl ? K_CTRL_DOWN : K_DOWN;
    case 'C': return ctrl ? K_CTRL_RIGHT : K_RIGHT;
    case 'D': return ctrl ? K_CTRL_LEFT : K_LEFT;
    default: return K_NONE;
    }
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* Output buffer: a frame is assembled in memory and written with a single write() to avoid flicker. */

struct abuf {
    char *b;
    size_t len;
    size_t cap;
};

static void ab_append(struct abuf *ab, const char *s, size_t n) {
    if (ab->len + n > ab->cap) {
        size_t cap = ab->cap ? ab->cap * 2 : 4096;
        while (cap < ab->len + n) {
            cap *= 2;
        }
        char *nb = realloc(ab->b, cap);
        if (!nb) {
            return;
        }
        ab->b   = nb;
        ab->cap = cap;
    }
    memcpy(ab->b + ab->len, s, n);
    ab->len += n;
}

static void ab_puts(struct abuf *ab, const char *s) {
    ab_append(ab, s, strlen(s));
}

static void ab_int(struct abuf *ab, long v) {
    char tmp[24];
    int i = 0;
    if (v <= 0) {
        ab_append(ab, "0", 1);
        return;
    }
    while (v > 0 && i < (int)sizeof(tmp)) {
        tmp[i++] = (char)('0' + v % 10);
        v /= 10;
    }
    while (i > 0) {
        ab_append(ab, &tmp[--i], 1);
    }
}

/* Moves to a 0-based row/column. */
static void ab_move(struct abuf *ab, long row, long col) {
    ab_puts(ab, "\x1b[");
    ab_int(ab, row + 1);
    ab_puts(ab, ";");
    ab_int(ab, col + 1);
    ab_puts(ab, "H");
}

#define INVERT_ON "\x1b[7m"
#define INVERT_OFF "\x1b[m"
#define ERASE_EOL "\x1b[K"

/* Appends the visible part of a row, starting at array index startIdx, expanding tabs and clipping to the screen. */
static void ab_row(struct abuf *ab, pstring row, long startIdx) {
    long col  = displayPos(row, startIdx);
    long base = col;

    for (long i = startIdx; i < row.len; i++) {
        unsigned char c = (unsigned char)row.chars[i];
        long w          = (c == '\t') ? TABLEN - col % TABLEN : 1;
        char out        = (c == '\t') ? ' ' : ((c >= 32 && c < 127) ? (char)c : '?');

        for (long k = 0; k < w && col - base < screenCols; k++, col++) {
            ab_append(ab, &out, 1);
        }
        if (col - base >= screenCols) {
            break;
        }
    }
}

static long messageRow(void) {
    return screenRows - BBARH - 1;
}

static void ab_message(struct abuf *ab, const char *msg, long *cursorColOut) {
    long len        = (long)strlen(msg);
    long buffersize = (screenCols - len) / 2;
    long row        = messageRow();

    ab_move(ab, row, 0);
    ab_puts(ab, ERASE_EOL);

    if (len + 1 < screenCols) {
        long col = buffersize - 1;
        ab_move(ab, row, col);
        ab_puts(ab, INVERT_ON "[");
        ab_append(ab, msg, (size_t)len);
        ab_puts(ab, "]" INVERT_OFF);
        *cursorColOut = col + 1 + len;
    } else {
        ab_move(ab, row, 0);
        ab_puts(ab, INVERT_ON);
        ab_append(ab, msg, (size_t)screenCols);
        ab_puts(ab, INVERT_OFF);
        *cursorColOut = screenCols - 1;
    }
}

static void render(void) {
    struct abuf ab = { 0 };

    ab_puts(&ab, "\x1b[?25l");

    /* Title bar */
    ab_move(&ab, 0, 0);
    ab_puts(&ab, INVERT_ON);
    for (int i = 0; i < screenCols; i++) {
        char c = (i < (int)sizeof(TITLE) - 1) ? TITLE[i] : ' ';
        ab_append(&ab, &c, 1);
    }
    ab_puts(&ab, INVERT_OFF);

    /* Text area */
    for (long y = 0; y < currentDisplay.rowCap; y++) {
        long i = currentDisplay.firstRow + y;
        ab_move(&ab, TBARH + y, 0);
        if (i < lines.lineCount) {
            ab_row(&ab, lines.lines[i], i == currentDisplay.currentRow ? currentDisplay.xOffset : 0);
        }
        ab_puts(&ab, ERASE_EOL);
    }

    /* Bottom bar: the shortcut letters are shown inverted */
    ab_move(&ab, screenRows - BBARH, 0);
    for (size_t i = 0; i < sizeof(BBARM) - 1; i++) {
        bool inv = i < sizeof(BBARI) - 1 && BBARI[i] != ' ';
        if (inv) {
            ab_puts(&ab, INVERT_ON);
        }
        ab_append(&ab, &BBARM[i], 1);
        if (inv) {
            ab_puts(&ab, INVERT_OFF);
        }
    }
    ab_puts(&ab, ERASE_EOL);

    /* Whatever is left below the bar */
    for (int r = screenRows - BBARH + 1; r < screenRows; r++) {
        ab_move(&ab, r, 0);
        ab_puts(&ab, ERASE_EOL);
    }

    /* Message / prompt row (drawn last so it sits on top of the text area) */
    long promptCol = 0;
    if (statusmsg[0]) {
        ab_message(&ab, statusmsg, &promptCol);
    }

    if (promptActive && statusmsg[0]) {
        ab_move(&ab, messageRow(), promptCol);
    } else {
        ab_move(&ab, cursorY, cursorX);
    }
    ab_puts(&ab, "\x1b[?25h");

    write_all(ab.b, ab.len);
    free(ab.b);
}

static void setmsg(const char *msg) {
    strncpy(statusmsg, msg, sizeof(statusmsg) - 1);
    statusmsg[sizeof(statusmsg) - 1] = '\0';
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* Entry point */

int main(int argc, char **argv) {
    if (argc == 2) {
        if (strcmp(argv[1], "--help") == 0) {
            printf("Usage:\n%s [FILE]\n", argv[0]);
            return 0;
        }
        if (strcmp(argv[1], "--version") == 0) {
            printf("pint v0.0.1-alpha.2\n");
            return 0;
        }

        filename = malloc(strlen(argv[1]) + 1);
        if (!filename) {
            return 1;
        }
        strcpy(filename, argv[1]);
    } else {
        filename = malloc(sizeof(DEFAULT_FILENAME));
        if (!filename) {
            return 1;
        }
        strcpy(filename, DEFAULT_FILENAME);
    }

    if (enable_raw_mode() == -1) {
        printf("pint: standard input is not a terminal\n");
        return 2;
    }
    atexit(disable_raw_mode);

    get_screen_size();

    int returnVal = edit();

    disable_raw_mode();
    return returnVal;
}

static int edit(void) {
    currentDisplay.currentRow = 0;
    currentDisplay.firstRow   = 0;
    currentDisplay.xIndex     = 0;
    currentDisplay.xOffset    = 0;
    currentDisplay.widthCap   = screenCols;
    currentDisplay.rowCap     = screenRows - (TBARH + BBARH);

    lines.lines    = openFile(filename);
    long lineCount = countLines(filename);
    if (lineCount <= 0 || lines.lines == NULL) {
        lines.lines = malloc(sizeof(pstring));
        if (!lines.lines) {
            return 1;
        }
        lines.lineCount = 1;
        lines.lines[0]  = pstr_new();
    } else {
        lines.lineCount = lineCount;
    }

    layout();

    for (;;) {
        render();

        int ch = read_key();
        statusmsg[0] = '\0';

        if (processChar(ch) == QUIT) {
            return 0;
        }
    }
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* Editing logic */

static int processChar(int ch) {
    /* The cursor row on screen, as of the last layout() */
    long oldY = currentDisplay.currentRow - currentDisplay.firstRow + TBARH;

    if (ch == K_LEFT) {
        if (currentDisplay.xIndex == 0) {
            if (currentDisplay.currentRow == 0) {
                /* at beginning */
            } else {
                if (currentDisplay.currentRow == currentDisplay.firstRow) {
                    currentDisplay.firstRow--;
                }
                currentDisplay.currentRow--;
                currentDisplay.xIndex  = lines.lines[currentDisplay.currentRow].len;
                currentDisplay.xOffset = -1;
                layout();
            }
        } else {
            if (currentDisplay.xIndex - currentDisplay.xOffset <= 1) {
                currentDisplay.xOffset--;
                if (currentDisplay.xOffset <= 0) {
                    currentDisplay.xOffset = 0;
                }
            }
            currentDisplay.xIndex--;
            layout();
        }
    } else if (ch == K_RIGHT) {
        if (currentDisplay.xIndex >= lines.lines[currentDisplay.currentRow].len) {
            if (currentDisplay.currentRow + 1 >= lines.lineCount) {
                /* EOF */
            } else {
                if (oldY + 1 >= screenRows - BBARH) {
                    currentDisplay.firstRow++;
                }
                currentDisplay.currentRow++;
                currentDisplay.xIndex  = 0;
                currentDisplay.xOffset = 0;
                layout();
            }
        } else {
            if (displayPos(lines.lines[currentDisplay.currentRow], currentDisplay.xIndex)
                        - displayPos(lines.lines[currentDisplay.currentRow], currentDisplay.xOffset) + 1
                    >= screenCols - 1) {
                currentDisplay.xOffset++;
            }
            currentDisplay.xIndex++;
            layout();
        }
    } else if (ch == K_DOWN) {
        if (currentDisplay.currentRow + 1 >= lines.lineCount) {
            currentDisplay.xOffset = -1;
            currentDisplay.xIndex  = lines.lines[currentDisplay.currentRow].len;
            layout();
        } else {
            currentDisplay.currentRow++;

            if (TBARH + currentDisplay.rowCap <= oldY + 1) {
                currentDisplay.firstRow++;
            }
            currentDisplay.xOffset = -1;
            currentDisplay.xIndex  = arrayPos(lines.lines[currentDisplay.currentRow],
                                              displayPos(lines.lines[currentDisplay.currentRow - 1], currentDisplay.xIndex));
            layout();
        }
    } else if (ch == K_UP) {
        if (currentDisplay.currentRow <= 0) {
            currentDisplay.xOffset = 0;
            currentDisplay.xIndex  = 0;
            layout();
        } else {
            currentDisplay.currentRow--;
            if (oldY <= TBARH) {
                currentDisplay.firstRow--;
            }

            currentDisplay.xOffset = -1;
            currentDisplay.xIndex  = arrayPos(lines.lines[currentDisplay.currentRow],
                                              displayPos(lines.lines[currentDisplay.currentRow + 1], currentDisplay.xIndex));
            layout();
        }
    } else if ((ch >= 32 && ch < 127) || ch == '\t') {
        insertChar(currentDisplay.currentRow, (char)ch, currentDisplay.xIndex);
        currentDisplay.xIndex++;
        layout();
    } else if (ch == K_BACKSPACE || ch == CTRL_KEY('h')) {
        if (currentDisplay.xIndex == 0) {
            if (currentDisplay.currentRow != 0 && lines.lineCount != 1) {
                currentDisplay.xOffset = -1;
                currentDisplay.xIndex  = lines.lines[currentDisplay.currentRow - 1].len;
                combineRows(currentDisplay.currentRow - 1, currentDisplay.currentRow);
                currentDisplay.currentRow--;
            }
        } else {
            removeChar(currentDisplay.currentRow, currentDisplay.xIndex - 1);
            currentDisplay.xIndex--;
        }
        layout();
    } else if (ch == '\n' || ch == '\r') {
        splitRow(currentDisplay.currentRow, currentDisplay.xIndex);
        currentDisplay.xIndex  = 0;
        currentDisplay.xOffset = 0;
        currentDisplay.currentRow++;
        layout();
    } else if (ch == CTRL_KEY('w')) {
        savePrompt();
    } else if (ch == CTRL_KEY('x')) {
        setmsg(SAVEMSG);
        render();

        int ans = read_key();
        statusmsg[0] = '\0';

        if (ans == 'y' || ans == 'Y') {
            if (savePrompt()) {
                return QUIT;
            }
        } else if (ans == 'n' || ans == 'N') {
            return QUIT;
        } else if (ans == 'c' || ans == 'C') {
            /* cancel: just go back to editing */
        } else {
            setmsg(SFAIL_INVALIDINPUT);
        }
    } else if (ch == K_CTRL_LEFT) {
        for (int i = 0; i < CTRLHACCEL; i++) {
            processChar(K_LEFT);
        }
    } else if (ch == K_CTRL_RIGHT) {
        for (int i = 0; i < CTRLHACCEL; i++) {
            processChar(K_RIGHT);
        }
    } else if (ch == K_CTRL_DOWN) {
        for (int i = 0; i < CTRLVACCEL; i++) {
            processChar(K_DOWN);
        }
    } else if (ch == K_CTRL_UP) {
        for (int i = 0; i < CTRLVACCEL; i++) {
            processChar(K_UP);
        }
    } else if (ch == CTRL_KEY('f')) {
        /* Insert search here */
    }

    return 0;
}

/* Asks for a filename and writes the buffer to it. Returns 1 if the file was saved, 0 otherwise. */
static int savePrompt(void) {
    char fname[PATH_BUF];
    size_t n = 0;

    fname[0] = '\0';
    if (filename[0] != '/' && getcwd(fname, sizeof(fname)) != NULL) {
        n = strlen(fname);
        if (n > 0 && fname[n - 1] != '/' && n + 1 < sizeof(fname)) {
            fname[n++] = '/';
        }
        fname[n] = '\0';
    } else {
        fname[0] = '\0';
    }

    size_t flen = strlen(filename);
    if (n + flen >= sizeof(fname)) {
        flen = sizeof(fname) - 1 - n;
    }
    memcpy(fname + n, filename, flen);
    n += flen;
    fname[n] = '\0';

    bool confirmed = false;
    promptActive   = true;
    for (;;) {
        char msg[MSG_BUF];
        size_t plen = sizeof(FNPROMPT) - 1;
        memcpy(msg, FNPROMPT, plen);
        memcpy(msg + plen, fname, n + 1);
        setmsg(msg);
        render();

        int ch = read_key();
        if (ch == CTRL_KEY('x')) {
            break;
        } else if (ch == '\n' || ch == '\r') {
            confirmed = true;
            break;
        } else if (ch == K_BACKSPACE || ch == CTRL_KEY('h')) {
            if (n > 0) {
                fname[--n] = '\0';
            }
        } else if (ch >= 32 && ch < 127) {
            if (n + 1 < sizeof(fname)) {
                fname[n++] = (char)ch;
                fname[n]   = '\0';
            }
        }
    }
    promptActive = false;
    statusmsg[0] = '\0';

    if (!confirmed || n == 0) {
        return 0;
    }

    int result = save(fname);
    if (result != 0) {
        setmsg(result == EACCES ? SFAIL_EACCES : SFAIL_GENERIC);
        return 0;
    }

    setmsg(WOSUCCES);

    char *newname = malloc(n + 1);
    if (newname) {
        memcpy(newname, fname, n + 1);
        free(filename);
        filename = newname;
    }
    return 1;
}

static int save(char *path) {
    return writeFile(path, lines.lines, lines.lineCount);
}

static int combineRows(long keep, long remove) {
    pstring tmp = pstr_combine(lines.lines[keep], lines.lines[remove]);
    free(lines.lines[keep].chars);
    lines.lines[keep].chars = tmp.chars;
    lines.lines[keep].len   = tmp.len;

    return removeRow(remove);
}

static int splitRow(long row, long index) {
    int res = insertRow(pstr_start_at(lines.lines[row], index), row + 1);

    pstring tmp = pstr_first_n(lines.lines[row], index);
    free(lines.lines[row].chars);
    lines.lines[row].chars = tmp.chars;
    lines.lines[row].len   = tmp.len;

    return res;
}

static int insertRow(pstring contents, long index) {
    pstring *grown = realloc(lines.lines, (lines.lineCount + 1) * sizeof(pstring));
    if (!grown) {
        return -1;
    }
    lines.lines = grown;

    memmove(&lines.lines[index + 1], &lines.lines[index], (lines.lineCount - index) * sizeof(pstring));
    lines.lines[index] = contents;
    lines.lineCount++;

    return 0;
}

static int removeRow(long index) {
    free(lines.lines[index].chars);

    memmove(&lines.lines[index], &lines.lines[index + 1], (lines.lineCount - index - 1) * sizeof(pstring));
    lines.lineCount--;

    pstring *shrunk = realloc(lines.lines, lines.lineCount * sizeof(pstring));
    if (shrunk) {
        lines.lines = shrunk;
    }

    return 0;
}

static int insertChar(long row, char c, long pos) {
    pstring tmp = pstr_insert_char(lines.lines[row], c, pos);
    free(lines.lines[row].chars);
    lines.lines[row].chars = tmp.chars;
    lines.lines[row].len   = tmp.len;

    return 0;
}

static int removeChar(long row, long pos) {
    pstring tmp = pstr_remove_char(lines.lines[row], pos);
    free(lines.lines[row].chars);
    lines.lines[row].chars = tmp.chars;
    lines.lines[row].len   = tmp.len;

    return 0;
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* Scrolling and cursor placement (no output) */

static void layout(void) {
    long newY      = currentDisplay.currentRow - currentDisplay.firstRow + TBARH;
    long xIndexPos = displayPos(lines.lines[currentDisplay.currentRow], currentDisplay.xIndex);

    if (currentDisplay.xOffset == -1) {
        if (xIndexPos >= screenCols - 1) {
            currentDisplay.xOffset = currentDisplay.xIndex - screenCols + 8;
        } else {
            currentDisplay.xOffset = 0;
        }
    }
    if (currentDisplay.xOffset < 0) {
        currentDisplay.xOffset = 0;
    }

    while (xIndexPos - displayPos(lines.lines[currentDisplay.currentRow], currentDisplay.xOffset) >= screenCols - 1) {
        currentDisplay.xOffset++;
    }

    cursorY = newY;
    cursorX = xIndexPos - displayPos(lines.lines[currentDisplay.currentRow], currentDisplay.xOffset);
}

static long displayPos(pstring row, long arrayIndex) {
    long i   = 0;
    long pos = 0;
    while (i < arrayIndex) {
        if (i >= row.len) {
            return pos;
        }
        if (row.chars[i] == '\t') {
            pos += TABLEN - pos % TABLEN;
        } else {
            pos++;
        }
        i++;
    }
    return pos;
}

static int arrayPos(pstring row, long index) {
    long i      = 0;
    long newlen = 0;
    while (i < row.len) {
        if (index == 0) {
            i = 0;
            break;
        }
        if (row.chars[i] == '\t') {
            newlen += TABLEN - newlen % TABLEN;
        } else {
            newlen++;
        }

        i++;
        if (newlen >= index) {
            break;
        }
    }

    return (int)i;
}
