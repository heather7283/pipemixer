#include <errno.h>
#include <ctype.h>
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

const char *key_name_from_key_code(int code) {
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
    case 33: return "!";
    case 34: return "\"";
    case 35: return "#";
    case 36: return "$";
    case 37: return "%";
    case 38: return "&";
    case 39: return "'";
    case 40: return "(";
    case 41: return ")";
    case 42: return "*";
    case 43: return "+";
    case 44: return ",";
    case 45: return "-";
    case 46: return ".";
    case 47: return "/";
    case 48: return "0";
    case 49: return "1";
    case 50: return "2";
    case 51: return "3";
    case 52: return "4";
    case 53: return "5";
    case 54: return "6";
    case 55: return "7";
    case 56: return "8";
    case 57: return "9";
    case 58: return ":";
    case 59: return ";";
    case 60: return "<";
    case 61: return "=";
    case 62: return ">";
    case 63: return "?";
    case 64: return "@";
    case 65: return "A";
    case 66: return "B";
    case 67: return "C";
    case 68: return "D";
    case 69: return "E";
    case 70: return "F";
    case 71: return "G";
    case 72: return "H";
    case 73: return "I";
    case 74: return "J";
    case 75: return "K";
    case 76: return "L";
    case 77: return "M";
    case 78: return "N";
    case 79: return "O";
    case 80: return "P";
    case 81: return "Q";
    case 82: return "R";
    case 83: return "S";
    case 84: return "T";
    case 85: return "U";
    case 86: return "V";
    case 87: return "W";
    case 88: return "X";
    case 89: return "Y";
    case 90: return "Z";
    case 91: return "[";
    case 92: return "\\";
    case 93: return "]";
    case 94: return "^";
    case 95: return "_";
    case 96: return "`";
    case 97: return "a";
    case 98: return "b";
    case 99: return "c";
    case 100: return "d";
    case 101: return "e";
    case 102: return "f";
    case 103: return "g";
    case 104: return "h";
    case 105: return "i";
    case 106: return "j";
    case 107: return "k";
    case 108: return "l";
    case 109: return "m";
    case 110: return "n";
    case 111: return "o";
    case 112: return "p";
    case 113: return "q";
    case 114: return "r";
    case 115: return "s";
    case 116: return "t";
    case 117: return "u";
    case 118: return "v";
    case 119: return "w";
    case 120: return "x";
    case 121: return "y";
    case 122: return "z";
    case 123: return "{";
    case 124: return "|";
    case 125: return "}";
    case 126: return "~";
    case 127: return "DEL";
    /* Predefined key codes, see curs_getch(3x) */
    case KEY_BREAK: return "KEY_BREAK";
    case KEY_DOWN: return "KEY_DOWN";
    case KEY_UP: return "KEY_UP";
    case KEY_LEFT: return "KEY_LEFT";
    case KEY_RIGHT: return "KEY_RIGHT";
    case KEY_HOME: return "KEY_HOME";
    case KEY_BACKSPACE: return "KEY_BACKSPACE";
    case KEY_F0: return "KEY_F0";
    case KEY_F(1): return "KEY_F1";
    case KEY_F(2): return "KEY_F2";
    case KEY_F(3): return "KEY_F3";
    case KEY_F(4): return "KEY_F4";
    case KEY_F(5): return "KEY_F5";
    case KEY_F(6): return "KEY_F6";
    case KEY_F(7): return "KEY_F7";
    case KEY_F(8): return "KEY_F8";
    case KEY_F(9): return "KEY_F9";
    case KEY_F(10): return "KEY_F10";
    case KEY_F(11): return "KEY_F11";
    case KEY_F(12): return "KEY_F12";
    case KEY_F(13): return "KEY_F13";
    case KEY_F(14): return "KEY_F14";
    case KEY_F(15): return "KEY_F15";
    case KEY_F(16): return "KEY_F16";
    case KEY_F(17): return "KEY_F17";
    case KEY_F(18): return "KEY_F18";
    case KEY_F(19): return "KEY_F19";
    case KEY_F(20): return "KEY_F20";
    case KEY_F(21): return "KEY_F21";
    case KEY_F(22): return "KEY_F22";
    case KEY_F(23): return "KEY_F23";
    case KEY_F(24): return "KEY_F24";
    case KEY_F(25): return "KEY_F25";
    case KEY_F(26): return "KEY_F26";
    case KEY_F(27): return "KEY_F27";
    case KEY_F(28): return "KEY_F28";
    case KEY_F(29): return "KEY_F29";
    case KEY_F(30): return "KEY_F30";
    case KEY_F(31): return "KEY_F31";
    case KEY_F(32): return "KEY_F32";
    case KEY_F(33): return "KEY_F33";
    case KEY_F(34): return "KEY_F34";
    case KEY_F(35): return "KEY_F35";
    case KEY_F(36): return "KEY_F36";
    case KEY_F(37): return "KEY_F37";
    case KEY_F(38): return "KEY_F38";
    case KEY_F(39): return "KEY_F39";
    case KEY_F(40): return "KEY_F40";
    case KEY_F(41): return "KEY_F41";
    case KEY_F(42): return "KEY_F42";
    case KEY_F(43): return "KEY_F43";
    case KEY_F(44): return "KEY_F44";
    case KEY_F(45): return "KEY_F45";
    case KEY_F(46): return "KEY_F46";
    case KEY_F(47): return "KEY_F47";
    case KEY_F(48): return "KEY_F48";
    case KEY_F(49): return "KEY_F49";
    case KEY_F(50): return "KEY_F50";
    case KEY_F(51): return "KEY_F51";
    case KEY_F(52): return "KEY_F52";
    case KEY_F(53): return "KEY_F53";
    case KEY_F(54): return "KEY_F54";
    case KEY_F(55): return "KEY_F55";
    case KEY_F(56): return "KEY_F56";
    case KEY_F(57): return "KEY_F57";
    case KEY_F(58): return "KEY_F58";
    case KEY_F(59): return "KEY_F59";
    case KEY_F(60): return "KEY_F60";
    case KEY_F(61): return "KEY_F61";
    case KEY_F(62): return "KEY_F62";
    case KEY_F(63): return "KEY_F63";
    case KEY_DL: return "KEY_DL";
    case KEY_IL: return "KEY_IL";
    case KEY_DC: return "KEY_DC";
    case KEY_IC: return "KEY_IC";
    case KEY_EIC: return "KEY_EIC";
    case KEY_CLEAR: return "KEY_CLEAR";
    case KEY_EOS: return "KEY_EOS";
    case KEY_EOL: return "KEY_EOL";
    case KEY_SF: return "KEY_SF";
    case KEY_SR: return "KEY_SR";
    case KEY_NPAGE: return "KEY_NPAGE";
    case KEY_PPAGE: return "KEY_PPAGE";
    case KEY_STAB: return "KEY_STAB";
    case KEY_CTAB: return "KEY_CTAB";
    case KEY_CATAB: return "KEY_CATAB";
    case KEY_ENTER: return "KEY_ENTER";
    case KEY_SRESET: return "KEY_SRESET";
    case KEY_RESET: return "KEY_RESET";
    case KEY_PRINT: return "KEY_PRINT";
    case KEY_LL: return "KEY_LL";
    case KEY_A1: return "KEY_A1";
    case KEY_A3: return "KEY_A3";
    case KEY_B2: return "KEY_B2";
    case KEY_C1: return "KEY_C1";
    case KEY_C3: return "KEY_C3";
    case KEY_BTAB: return "KEY_BTAB";
    case KEY_BEG: return "KEY_BEG";
    case KEY_CANCEL: return "KEY_CANCEL";
    case KEY_CLOSE: return "KEY_CLOSE";
    case KEY_COMMAND: return "KEY_COMMAND";
    case KEY_COPY: return "KEY_COPY";
    case KEY_CREATE: return "KEY_CREATE";
    case KEY_END: return "KEY_END";
    case KEY_EXIT: return "KEY_EXIT";
    case KEY_FIND: return "KEY_FIND";
    case KEY_HELP: return "KEY_HELP";
    case KEY_MARK: return "KEY_MARK";
    case KEY_MESSAGE: return "KEY_MESSAGE";
    case KEY_MOUSE: return "KEY_MOUSE";
    case KEY_MOVE: return "KEY_MOVE";
    case KEY_NEXT: return "KEY_NEXT";
    case KEY_OPEN: return "KEY_OPEN";
    case KEY_OPTIONS: return "KEY_OPTIONS";
    case KEY_PREVIOUS: return "KEY_PREVIOUS";
    case KEY_REDO: return "KEY_REDO";
    case KEY_REFERENCE: return "KEY_REFERENCE";
    case KEY_REFRESH: return "KEY_REFRESH";
    case KEY_REPLACE: return "KEY_REPLACE";
    case KEY_RESIZE: return "KEY_RESIZE";
    case KEY_RESTART: return "KEY_RESTART";
    case KEY_RESUME: return "KEY_RESUME";
    case KEY_SAVE: return "KEY_SAVE";
    case KEY_SELECT: return "KEY_SELECT";
    case KEY_SUSPEND: return "KEY_SUSPEND";
    case KEY_UNDO: return "KEY_UNDO";
    case KEY_SBEG: return "KEY_SBEG";
    case KEY_SCANCEL: return "KEY_SCANCEL";
    case KEY_SCOMMAND: return "KEY_SCOMMAND";
    case KEY_SCOPY: return "KEY_SCOPY";
    case KEY_SCREATE: return "KEY_SCREATE";
    case KEY_SDC: return "KEY_SDC";
    case KEY_SDL: return "KEY_SDL";
    case KEY_SEND: return "KEY_SEND";
    case KEY_SEOL: return "KEY_SEOL";
    case KEY_SEXIT: return "KEY_SEXIT";
    case KEY_SFIND: return "KEY_SFIND";
    case KEY_SHELP: return "KEY_SHELP";
    case KEY_SHOME: return "KEY_SHOME";
    case KEY_SIC: return "KEY_SIC";
    case KEY_SLEFT: return "KEY_SLEFT";
    case KEY_SMESSAGE: return "KEY_SMESSAGE";
    case KEY_SMOVE: return "KEY_SMOVE";
    case KEY_SNEXT: return "KEY_SNEXT";
    case KEY_SOPTIONS: return "KEY_SOPTIONS";
    case KEY_SPREVIOUS: return "KEY_SPREVIOUS";
    case KEY_SPRINT: return "KEY_SPRINT";
    case KEY_SREDO: return "KEY_SREDO";
    case KEY_SREPLACE: return "KEY_SREPLACE";
    case KEY_SRIGHT: return "KEY_SRIGHT";
    case KEY_SRSUME: return "KEY_SRSUME";
    case KEY_SSAVE: return "KEY_SSAVE";
    case KEY_SSUSPEND: return "KEY_SSUSPEND";
    case KEY_SUNDO: return "KEY_SUNDO";
    /* getch() can also return ERR */
    case ERR: return "ERR";
    /* Fallback */
    default: return "KEY_????????";
    }
}

int key_code_from_key_name(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return ERR;
    }

    if (name[0] != '\0' && name[1] == '\0') {
        if (isprint(name[0])) {
            return name[0];
        } else {
            return ERR;
        }
    }

    /* TODO: add more common keys? */
    if (STREQ(name, "up")) return KEY_UP;
    if (STREQ(name, "down")) return KEY_DOWN;
    if (STREQ(name, "left")) return KEY_LEFT;
    if (STREQ(name, "right")) return KEY_RIGHT;
    if (STREQ(name, "enter")) return KEY_ENTER;
    if (STREQ(name, "tab")) return '\t';
    if (STREQ(name, "backtab")) return KEY_BTAB;
    if (STREQ(name, "space")) return ' ';
    if (STREQ(name, "backspace")) return KEY_BACKSPACE;

    const char *prefix;
    if (prefix = "code:", STRSTARTSWITH(name, prefix)) {
        const char *num = name + strlen(prefix);
        unsigned long code;
        if (str_to_ulong(num, &code) && code < INT_MAX) {
            return code;
        } else {
            return ERR;
        }
    }

    /* See curs_getch(3x) */
    if (STREQ(name, "BREAK")) return KEY_BREAK;
    if (STREQ(name, "DOWN")) return KEY_DOWN;
    if (STREQ(name, "UP")) return KEY_UP;
    if (STREQ(name, "LEFT")) return KEY_LEFT;
    if (STREQ(name, "RIGHT")) return KEY_RIGHT;
    if (STREQ(name, "HOME")) return KEY_HOME;
    if (STREQ(name, "BACKSPACE")) return KEY_BACKSPACE;
    if (STREQ(name, "F0")) return KEY_F0;
    if (STREQ(name, "F1")) return KEY_F(1);
    if (STREQ(name, "F2")) return KEY_F(2);
    if (STREQ(name, "F3")) return KEY_F(3);
    if (STREQ(name, "F4")) return KEY_F(4);
    if (STREQ(name, "F5")) return KEY_F(5);
    if (STREQ(name, "F6")) return KEY_F(6);
    if (STREQ(name, "F7")) return KEY_F(7);
    if (STREQ(name, "F8")) return KEY_F(8);
    if (STREQ(name, "F9")) return KEY_F(9);
    if (STREQ(name, "F10")) return KEY_F(10);
    if (STREQ(name, "F11")) return KEY_F(11);
    if (STREQ(name, "F12")) return KEY_F(12);
    if (STREQ(name, "F13")) return KEY_F(13);
    if (STREQ(name, "F14")) return KEY_F(14);
    if (STREQ(name, "F15")) return KEY_F(15);
    if (STREQ(name, "F16")) return KEY_F(16);
    if (STREQ(name, "F17")) return KEY_F(17);
    if (STREQ(name, "F18")) return KEY_F(18);
    if (STREQ(name, "F19")) return KEY_F(19);
    if (STREQ(name, "F20")) return KEY_F(20);
    if (STREQ(name, "F21")) return KEY_F(21);
    if (STREQ(name, "F22")) return KEY_F(22);
    if (STREQ(name, "F23")) return KEY_F(23);
    if (STREQ(name, "F24")) return KEY_F(24);
    if (STREQ(name, "F25")) return KEY_F(25);
    if (STREQ(name, "F26")) return KEY_F(26);
    if (STREQ(name, "F27")) return KEY_F(27);
    if (STREQ(name, "F28")) return KEY_F(28);
    if (STREQ(name, "F29")) return KEY_F(29);
    if (STREQ(name, "F30")) return KEY_F(30);
    if (STREQ(name, "F31")) return KEY_F(31);
    if (STREQ(name, "F32")) return KEY_F(32);
    if (STREQ(name, "F33")) return KEY_F(33);
    if (STREQ(name, "F34")) return KEY_F(34);
    if (STREQ(name, "F35")) return KEY_F(35);
    if (STREQ(name, "F36")) return KEY_F(36);
    if (STREQ(name, "F37")) return KEY_F(37);
    if (STREQ(name, "F38")) return KEY_F(38);
    if (STREQ(name, "F39")) return KEY_F(39);
    if (STREQ(name, "F40")) return KEY_F(40);
    if (STREQ(name, "F41")) return KEY_F(41);
    if (STREQ(name, "F42")) return KEY_F(42);
    if (STREQ(name, "F43")) return KEY_F(43);
    if (STREQ(name, "F44")) return KEY_F(44);
    if (STREQ(name, "F45")) return KEY_F(45);
    if (STREQ(name, "F46")) return KEY_F(46);
    if (STREQ(name, "F47")) return KEY_F(47);
    if (STREQ(name, "F48")) return KEY_F(48);
    if (STREQ(name, "F49")) return KEY_F(49);
    if (STREQ(name, "F50")) return KEY_F(50);
    if (STREQ(name, "F51")) return KEY_F(51);
    if (STREQ(name, "F52")) return KEY_F(52);
    if (STREQ(name, "F53")) return KEY_F(53);
    if (STREQ(name, "F54")) return KEY_F(54);
    if (STREQ(name, "F55")) return KEY_F(55);
    if (STREQ(name, "F56")) return KEY_F(56);
    if (STREQ(name, "F57")) return KEY_F(57);
    if (STREQ(name, "F58")) return KEY_F(58);
    if (STREQ(name, "F59")) return KEY_F(59);
    if (STREQ(name, "F60")) return KEY_F(60);
    if (STREQ(name, "F61")) return KEY_F(61);
    if (STREQ(name, "F62")) return KEY_F(62);
    if (STREQ(name, "F63")) return KEY_F(63);
    if (STREQ(name, "DL")) return KEY_DL;
    if (STREQ(name, "IL")) return KEY_IL;
    if (STREQ(name, "DC")) return KEY_DC;
    if (STREQ(name, "IC")) return KEY_IC;
    if (STREQ(name, "EIC")) return KEY_EIC;
    if (STREQ(name, "CLEAR")) return KEY_CLEAR;
    if (STREQ(name, "EOS")) return KEY_EOS;
    if (STREQ(name, "EOL")) return KEY_EOL;
    if (STREQ(name, "SF")) return KEY_SF;
    if (STREQ(name, "SR")) return KEY_SR;
    if (STREQ(name, "NPAGE")) return KEY_NPAGE;
    if (STREQ(name, "PPAGE")) return KEY_PPAGE;
    if (STREQ(name, "STAB")) return KEY_STAB;
    if (STREQ(name, "CTAB")) return KEY_CTAB;
    if (STREQ(name, "CATAB")) return KEY_CATAB;
    if (STREQ(name, "ENTER")) return KEY_ENTER;
    if (STREQ(name, "SRESET")) return KEY_SRESET;
    if (STREQ(name, "RESET")) return KEY_RESET;
    if (STREQ(name, "PRINT")) return KEY_PRINT;
    if (STREQ(name, "LL")) return KEY_LL;
    if (STREQ(name, "A1")) return KEY_A1;
    if (STREQ(name, "A3")) return KEY_A3;
    if (STREQ(name, "B2")) return KEY_B2;
    if (STREQ(name, "C1")) return KEY_C1;
    if (STREQ(name, "C3")) return KEY_C3;
    if (STREQ(name, "BTAB")) return KEY_BTAB;
    if (STREQ(name, "BEG")) return KEY_BEG;
    if (STREQ(name, "CANCEL")) return KEY_CANCEL;
    if (STREQ(name, "CLOSE")) return KEY_CLOSE;
    if (STREQ(name, "COMMAND")) return KEY_COMMAND;
    if (STREQ(name, "COPY")) return KEY_COPY;
    if (STREQ(name, "CREATE")) return KEY_CREATE;
    if (STREQ(name, "END")) return KEY_END;
    if (STREQ(name, "EXIT")) return KEY_EXIT;
    if (STREQ(name, "FIND")) return KEY_FIND;
    if (STREQ(name, "HELP")) return KEY_HELP;
    if (STREQ(name, "MARK")) return KEY_MARK;
    if (STREQ(name, "MESSAGE")) return KEY_MESSAGE;
    if (STREQ(name, "MOUSE")) return KEY_MOUSE;
    if (STREQ(name, "MOVE")) return KEY_MOVE;
    if (STREQ(name, "NEXT")) return KEY_NEXT;
    if (STREQ(name, "OPEN")) return KEY_OPEN;
    if (STREQ(name, "OPTIONS")) return KEY_OPTIONS;
    if (STREQ(name, "PREVIOUS")) return KEY_PREVIOUS;
    if (STREQ(name, "REDO")) return KEY_REDO;
    if (STREQ(name, "REFERENCE")) return KEY_REFERENCE;
    if (STREQ(name, "REFRESH")) return KEY_REFRESH;
    if (STREQ(name, "REPLACE")) return KEY_REPLACE;
    if (STREQ(name, "RESIZE")) return KEY_RESIZE;
    if (STREQ(name, "RESTART")) return KEY_RESTART;
    if (STREQ(name, "RESUME")) return KEY_RESUME;
    if (STREQ(name, "SAVE")) return KEY_SAVE;
    if (STREQ(name, "SELECT")) return KEY_SELECT;
    if (STREQ(name, "SUSPEND")) return KEY_SUSPEND;
    if (STREQ(name, "UNDO")) return KEY_UNDO;
    if (STREQ(name, "SBEG")) return KEY_SBEG;
    if (STREQ(name, "SCANCEL")) return KEY_SCANCEL;
    if (STREQ(name, "SCOMMAND")) return KEY_SCOMMAND;
    if (STREQ(name, "SCOPY")) return KEY_SCOPY;
    if (STREQ(name, "SCREATE")) return KEY_SCREATE;
    if (STREQ(name, "SDC")) return KEY_SDC;
    if (STREQ(name, "SDL")) return KEY_SDL;
    if (STREQ(name, "SEND")) return KEY_SEND;
    if (STREQ(name, "SEOL")) return KEY_SEOL;
    if (STREQ(name, "SEXIT")) return KEY_SEXIT;
    if (STREQ(name, "SFIND")) return KEY_SFIND;
    if (STREQ(name, "SHELP")) return KEY_SHELP;
    if (STREQ(name, "SHOME")) return KEY_SHOME;
    if (STREQ(name, "SIC")) return KEY_SIC;
    if (STREQ(name, "SLEFT")) return KEY_SLEFT;
    if (STREQ(name, "SMESSAGE")) return KEY_SMESSAGE;
    if (STREQ(name, "SMOVE")) return KEY_SMOVE;
    if (STREQ(name, "SNEXT")) return KEY_SNEXT;
    if (STREQ(name, "SOPTIONS")) return KEY_SOPTIONS;
    if (STREQ(name, "SPREVIOUS")) return KEY_SPREVIOUS;
    if (STREQ(name, "SPRINT")) return KEY_SPRINT;
    if (STREQ(name, "SREDO")) return KEY_SREDO;
    if (STREQ(name, "SREPLACE")) return KEY_SREPLACE;
    if (STREQ(name, "SRIGHT")) return KEY_SRIGHT;
    if (STREQ(name, "SRSUME")) return KEY_SRSUME;
    if (STREQ(name, "SSAVE")) return KEY_SSAVE;
    if (STREQ(name, "SSUSPEND")) return KEY_SSUSPEND;
    if (STREQ(name, "SUNDO")) return KEY_SUNDO;

    return ERR;
}

bool str_to_ulong(const char *str, unsigned long *res) {
    char *endptr = NULL;

    errno = 0;
    unsigned long res_tmp = strtoul(str, &endptr, 10);

    if (errno == 0 && *endptr == '\0') {
        *res = res_tmp;
        return true;
    } else {
        return false;
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

