#include <errno.h>
#include <ctype.h>
#include <wctype.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <curses.h>
#include <limits.h>

#include "utils.h"
#include "macros.h"
#include "xmalloc.h"

const char *channel_name_from_enum(enum spa_audio_channel chan) {
    switch (chan) {
    case SPA_AUDIO_CHANNEL_UNKNOWN: return "UNK";
    case SPA_AUDIO_CHANNEL_NA:      return "NA";

    case SPA_AUDIO_CHANNEL_MONO: return "MONO";

    case SPA_AUDIO_CHANNEL_FL:   return "FL";
    case SPA_AUDIO_CHANNEL_FR:   return "FR";
    case SPA_AUDIO_CHANNEL_FC:   return "FC";
    case SPA_AUDIO_CHANNEL_LFE:  return "LFE";
    case SPA_AUDIO_CHANNEL_SL:   return "SL";
    case SPA_AUDIO_CHANNEL_SR:   return "SR";
    case SPA_AUDIO_CHANNEL_FLC:  return "FLC";
    case SPA_AUDIO_CHANNEL_FRC:  return "FRC";
    case SPA_AUDIO_CHANNEL_RC:   return "RC";
    case SPA_AUDIO_CHANNEL_RL:   return "RL";
    case SPA_AUDIO_CHANNEL_RR:   return "RR";
    case SPA_AUDIO_CHANNEL_TC:   return "TC";
    case SPA_AUDIO_CHANNEL_TFL:  return "TFL";
    case SPA_AUDIO_CHANNEL_TFC:  return "TFC";
    case SPA_AUDIO_CHANNEL_TFR:  return "TFR";
    case SPA_AUDIO_CHANNEL_TRL:  return "TRL";
    case SPA_AUDIO_CHANNEL_TRC:  return "TRC";
    case SPA_AUDIO_CHANNEL_TRR:  return "TRR";
    case SPA_AUDIO_CHANNEL_RLC:  return "RLC";
    case SPA_AUDIO_CHANNEL_RRC:  return "RRC";
    case SPA_AUDIO_CHANNEL_FLW:  return "FLW";
    case SPA_AUDIO_CHANNEL_FRW:  return "FRW";
    case SPA_AUDIO_CHANNEL_LFE2: return "LFE2";
    case SPA_AUDIO_CHANNEL_FLH:  return "FLH";
    case SPA_AUDIO_CHANNEL_FCH:  return "FCH";
    case SPA_AUDIO_CHANNEL_FRH:  return "FRH";
    case SPA_AUDIO_CHANNEL_TFLC: return "TFLC";
    case SPA_AUDIO_CHANNEL_TFRC: return "TFRC";
    case SPA_AUDIO_CHANNEL_TSL:  return "TSL";
    case SPA_AUDIO_CHANNEL_TSR:  return "TSR";
    case SPA_AUDIO_CHANNEL_LLFE: return "LLFR";
    case SPA_AUDIO_CHANNEL_RLFE: return "RLFE";
    case SPA_AUDIO_CHANNEL_BC:   return "BC";
    case SPA_AUDIO_CHANNEL_BLC:  return "BLC";
    case SPA_AUDIO_CHANNEL_BRC:  return "BRC";

    case SPA_AUDIO_CHANNEL_AUX0:  return "AUX0";
    case SPA_AUDIO_CHANNEL_AUX1:  return "AUX1";
    case SPA_AUDIO_CHANNEL_AUX2:  return "AUX2";
    case SPA_AUDIO_CHANNEL_AUX3:  return "AUX3";
    case SPA_AUDIO_CHANNEL_AUX4:  return "AUX4";
    case SPA_AUDIO_CHANNEL_AUX5:  return "AUX5";
    case SPA_AUDIO_CHANNEL_AUX6:  return "AUX6";
    case SPA_AUDIO_CHANNEL_AUX7:  return "AUX7";
    case SPA_AUDIO_CHANNEL_AUX8:  return "AUX8";
    case SPA_AUDIO_CHANNEL_AUX9:  return "AUX9";
    case SPA_AUDIO_CHANNEL_AUX10: return "AUX10";
    case SPA_AUDIO_CHANNEL_AUX11: return "AUX11";
    case SPA_AUDIO_CHANNEL_AUX12: return "AUX12";
    case SPA_AUDIO_CHANNEL_AUX13: return "AUX13";
    case SPA_AUDIO_CHANNEL_AUX14: return "AUX14";
    case SPA_AUDIO_CHANNEL_AUX15: return "AUX15";
    case SPA_AUDIO_CHANNEL_AUX16: return "AUX16";
    case SPA_AUDIO_CHANNEL_AUX17: return "AUX17";
    case SPA_AUDIO_CHANNEL_AUX18: return "AUX18";
    case SPA_AUDIO_CHANNEL_AUX19: return "AUX19";
    case SPA_AUDIO_CHANNEL_AUX20: return "AUX20";
    case SPA_AUDIO_CHANNEL_AUX21: return "AUX21";
    case SPA_AUDIO_CHANNEL_AUX22: return "AUX22";
    case SPA_AUDIO_CHANNEL_AUX23: return "AUX23";
    case SPA_AUDIO_CHANNEL_AUX24: return "AUX24";
    case SPA_AUDIO_CHANNEL_AUX25: return "AUX25";
    case SPA_AUDIO_CHANNEL_AUX26: return "AUX26";
    case SPA_AUDIO_CHANNEL_AUX27: return "AUX27";
    case SPA_AUDIO_CHANNEL_AUX28: return "AUX28";
    case SPA_AUDIO_CHANNEL_AUX29: return "AUX29";
    case SPA_AUDIO_CHANNEL_AUX30: return "AUX30";
    case SPA_AUDIO_CHANNEL_AUX31: return "AUX31";
    case SPA_AUDIO_CHANNEL_AUX32: return "AUX32";
    case SPA_AUDIO_CHANNEL_AUX33: return "AUX33";
    case SPA_AUDIO_CHANNEL_AUX34: return "AUX34";
    case SPA_AUDIO_CHANNEL_AUX35: return "AUX35";
    case SPA_AUDIO_CHANNEL_AUX36: return "AUX36";
    case SPA_AUDIO_CHANNEL_AUX37: return "AUX37";
    case SPA_AUDIO_CHANNEL_AUX38: return "AUX38";
    case SPA_AUDIO_CHANNEL_AUX39: return "AUX39";
    case SPA_AUDIO_CHANNEL_AUX40: return "AUX40";
    case SPA_AUDIO_CHANNEL_AUX41: return "AUX41";
    case SPA_AUDIO_CHANNEL_AUX42: return "AUX42";
    case SPA_AUDIO_CHANNEL_AUX43: return "AUX43";
    case SPA_AUDIO_CHANNEL_AUX44: return "AUX44";
    case SPA_AUDIO_CHANNEL_AUX45: return "AUX45";
    case SPA_AUDIO_CHANNEL_AUX46: return "AUX46";
    case SPA_AUDIO_CHANNEL_AUX47: return "AUX47";
    case SPA_AUDIO_CHANNEL_AUX48: return "AUX48";
    case SPA_AUDIO_CHANNEL_AUX49: return "AUX49";
    case SPA_AUDIO_CHANNEL_AUX50: return "AUX50";
    case SPA_AUDIO_CHANNEL_AUX51: return "AUX51";
    case SPA_AUDIO_CHANNEL_AUX52: return "AUX52";
    case SPA_AUDIO_CHANNEL_AUX53: return "AUX53";
    case SPA_AUDIO_CHANNEL_AUX54: return "AUX54";
    case SPA_AUDIO_CHANNEL_AUX55: return "AUX55";
    case SPA_AUDIO_CHANNEL_AUX56: return "AUX56";
    case SPA_AUDIO_CHANNEL_AUX57: return "AUX57";
    case SPA_AUDIO_CHANNEL_AUX58: return "AUX58";
    case SPA_AUDIO_CHANNEL_AUX59: return "AUX59";
    case SPA_AUDIO_CHANNEL_AUX60: return "AUX60";
    case SPA_AUDIO_CHANNEL_AUX61: return "AUX61";
    case SPA_AUDIO_CHANNEL_AUX62: return "AUX62";
    case SPA_AUDIO_CHANNEL_AUX63: return "AUX63";

    default: return "?????";
    }
}

const char *key_name_from_key_code(wint_t code) {
    static char codepoint[5];
    if (iswgraph(code)) {
        snprintf(codepoint, sizeof(codepoint), "%lc", code);
        return codepoint;
    }

    switch (code) {
    case 0: return "NUL";
    case 1: return "SOH";
    case 2: return "STX";
    case 3: return "ETX";
    case 4: return "EOT";
    case 5: return "ENQ";
    case 6: return "ACK";
    case 7: return "BEL";
    case 8: return "BS";
    case 9: return "HT";
    case 10: return "LF";
    case 11: return "VT";
    case 12: return "FF";
    case 13: return "CR";
    case 14: return "SO";
    case 15: return "SI";
    case 16: return "DLE";
    case 17: return "DC1";
    case 18: return "DC2";
    case 19: return "DC3";
    case 20: return "DC4";
    case 21: return "NAK";
    case 22: return "SYN";
    case 23: return "ETB";
    case 24: return "CAN";
    case 25: return "EM";
    case 26: return "SUB";
    case 27: return "ESC";
    case 28: return "FS";
    case 29: return "GS";
    case 30: return "RS";
    case 31: return "US";
    case 32: return "SPACE";
    /* Fallback */
    default: return "?????";
    }
}

/* TODO: this function is probably wrong in some way */
bool key_code_from_key_name(const char *name, wint_t *keycode) {
    if (name == NULL || name[0] == '\0') {
        return false;
    }

    if (name[1] == '\0' && isgraph(name[0])) {
        /* printable ascii */
        *keycode = name[0];
        return true;
    } else if (name[2] == '\0' || name[3] == '\0' || name[4] == '\0') {
        /* a single utf-8 char */
        mbtowc(NULL, NULL, 0); /* reset state */
        wchar_t res;
        int ret = mbtowc(&res, name, 4);
        if (ret > 0 && name[ret] == '\0' && iswgraph(res)) {
            *keycode = res;
            return true;
        }
    }

    if (STREQ(name, "up")) { *keycode = KEY_UP; return true; };
    if (STREQ(name, "down")) { *keycode = KEY_DOWN; return true; };
    if (STREQ(name, "left")) { *keycode = KEY_LEFT; return true; };
    if (STREQ(name, "right")) { *keycode = KEY_RIGHT; return true; };
    if (STREQ(name, "enter")) { *keycode = '\n'; return true; };
    if (STREQ(name, "tab")) { *keycode = '\t'; return true; };
    if (STREQ(name, "backtab")) { *keycode = KEY_BTAB; return true; };
    if (STREQ(name, "space")) { *keycode = ' '; return true; };
    if (STREQ(name, "backspace")) { *keycode = KEY_BACKSPACE; return true; };
    if (STREQ(name, "escape")) { *keycode = '\e'; return true; };

    const char *prefix;
    if (prefix = "code:", STRSTARTSWITH(name, prefix)) {
        const char *num = name + strlen(prefix);
        unsigned long code;
        if (str_to_ulong(num, &code) && code < INT_MAX) {
            *keycode = code;
            return true;
        }
    }

    return false;
}

bool str_to_long(const char *str, long *res) {
    char *endptr = NULL;
    int base = 10;

    if (str[0] == '0' && str[1] == 'x') {
        base = 16;
    }

    errno = 0;
    long res_tmp = strtol(str, &endptr, base);
    if (errno == 0 && *endptr == '\0') {
        *res = res_tmp;
        return true;
    }

    return false;
}


bool str_to_ulong(const char *str, unsigned long *res) {
    char *endptr = NULL;
    int base = 10;

    if (str[0] == '0' && str[1] == 'x') {
        base = 16;
    }

    errno = 0;
    unsigned long res_tmp = strtoul(str, &endptr, base);
    if (errno == 0 && *endptr == '\0') {
        *res = res_tmp;
        return true;
    }

    return false;
}

bool str_to_u32(const char *str, uint32_t *res) {
    unsigned long res_tmp;
    if (!str_to_ulong(str, &res_tmp) || res_tmp > UINT32_MAX) {
        return false;
    } else {
        *res = res_tmp;
        return true;
    }
}

bool str_to_i32(const char *str, int32_t *res) {
    long res_tmp;
    if (!str_to_long(str, &res_tmp) || res_tmp > INT32_MAX || res_tmp < INT32_MIN) {
        return false;
    } else {
        *res = res_tmp;
        return true;
    }
}

size_t wcstrimcols(wchar_t *str, size_t col) {
    size_t width = 0;
    size_t n_chars = 0;

    wchar_t *p = str;
    wchar_t c;
    while ((c = *p) != L'\0') {
        if ((width += wcwidth(c)) > col) {
            *p = L'\0';
            break;
        }
        n_chars += 1;
        p += 1;
    }

    return n_chars;
}

char *read_string_from_fd(int fd, size_t *len) {
    const size_t chunk_size = 1024;
    size_t capacity = chunk_size;
    size_t length = 0;
    char *buffer = xmalloc(capacity + 1 /* terminator */);

    while (1) {
        if (length + chunk_size > capacity) {
            capacity *= 2;
            buffer = xrealloc(buffer, capacity + 1 /* terminator */);
        }

        ssize_t bytes_read = read(fd, buffer + length, chunk_size);
        if (bytes_read < 0) {
            goto err;
        } else if (bytes_read == 0) {
            /* EOF */
            break;
        } else {
            length += bytes_read;
        }
    }

    buffer[length] = '\0';

    if (len != NULL) {
        *len = length;
    }
    return buffer;

err:
    if (len != NULL) {
        *len = 0;
    }
    return NULL;
}

