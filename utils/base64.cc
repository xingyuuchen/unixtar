#include "base64.h"
#include "log.h"
#include <cassert>


namespace base64 {


static const char padding = '=';

static const char encode_table[] = {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
        'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
        'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
        'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
        'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
        'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
        'w', 'x', 'y', 'z', '0', '1', '2', '3',
        '4', '5', '6', '7', '8', '9', '+', '/',
};


/**
 * 255 indicates error.
 */
static const unsigned char decode_table[] = {
        /* nul, soh, stx, etx, eot, enq, ack, bel, */
        255, 255, 255, 255, 255, 255, 255, 255,
        /*  bs,  ht,  nl,  vt,  np,  cr,  so,  si, */
        255, 255, 255, 255, 255, 255, 255, 255,
        /* dle, dc1, dc2, dc3, dc4, nak, syn, etb, */
        255, 255, 255, 255, 255, 255, 255, 255,
        /* can,  em, sub, esc,  fs,  gs,  rs,  us, */
        255, 255, 255, 255, 255, 255, 255, 255,
        /*  sp, '!', '"', '#', '$', '%', '&', ''', */
        255, 255, 255, 255, 255, 255, 255, 255,
        /* '(', ')', '*', '+', ',', '-', '.', '/', */
        255, 255, 255,  62, 255, 255, 255,  63,
        /* '0', '1', '2', '3', '4', '5', '6', '7', */
        52,  53,  54,  55,  56,  57,  58,  59,
        /* '8', '9', ':', ';', '<', '=', '>', '?', */
        60,  61, 255, 255, 255, 255, 255, 255,
        /* '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', */
        255,   0,   1,  2,   3,   4,   5,    6,
        /* 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', */
        7,   8,   9,  10,  11,  12,  13,  14,
        /* 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', */
        15,  16,  17,  18,  19,  20,  21,  22,
        /* 'X', 'Y', 'Z', '[', '\', ']', '^', '_', */
        23,  24,  25, 255, 255, 255, 255, 255,
        /* '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', */
        255,  26,  27,  28,  29,  30,  31,  32,
        /* 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', */
        33,  34,  35,  36,  37,  38,  39,  40,
        /* 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', */
        41,  42,  43,  44,  45,  46,  47,  48,
        /* 'x', 'y', 'z', '{', '|', '}', '~', del, */
        49,  50,  51, 255, 255, 255, 255, 255,
};

size_t Encode(const unsigned char *_in, size_t _in_len, char *_res) {
    size_t curr_encode = 0;
    unsigned char last = 0;
    
    int where = 0;
    for (int i = 0; i < _in_len; ++i) {
        unsigned char curr = _in[i];
    
        if (where == 0) {
            _res[curr_encode++] = encode_table[(curr >> 2) & 0x3f];
            where = 1;
        } else if (where == 1) {
            _res[curr_encode++] = encode_table[((last & 0x3) << 4) | ((curr >> 4) & 0xf)];
            where = 2;
        } else {
            _res[curr_encode++] = encode_table[((last & 0xf)) << 2 | ((curr >> 6) & 0x3)];
            _res[curr_encode++] = encode_table[curr & 0x3f];
            where = 0;
        }
        last = curr;
    }
    
    if (where == 1) {
        _res[curr_encode++] = encode_table[(last & 0x3) << 4];
        _res[curr_encode++] = padding;
        _res[curr_encode++] = padding;
    } else if (where == 2) {
        _res[curr_encode++] = encode_table[(last & 0xf) << 2];
        _res[curr_encode++] = padding;
    }
    _res[curr_encode] = 0;
    
    if ((strlen(_res) & 3) != 0) {
        LogE("len of encoded: %ld", strlen(_res))
        assert(false);
    }
    return curr_encode;
}


size_t Decode(const char *_in, size_t _in_len, unsigned char *_res) {
    if (_in_len & 3) {
        throw IllegalBase64LengthException(_in_len);
    }
    size_t curr_decode = 0;
    unsigned char last = 255;
    int where = 0;
    
    for (size_t i = 0; i < _in_len; ++i) {
        if (_in[i] == padding) {
            break;
        }
        
        unsigned char curr = decode_table[_in[i]];
        
        if (curr == 255) {
            throw Base64DecodeException();
        }
        
        if (where == 0) {
            where = 1;
        } else if (where == 1) {
            _res[curr_decode++] = ((last << 2) & 0xff) + ((curr >> 4) & 0x3);
            where = 2;
        } else if (where == 2) {
            _res[curr_decode++] = ((last & 0xf) << 4) + ((curr >> 2) & 0xf);
            where = 3;
        } else {
            _res[curr_decode++] = ((last & 0x3) << 6) + (curr & 0x3f);
            where = 0;
        }
        last = curr;
    }
    
    _res[curr_decode] = 0;
    return curr_decode;
}


IllegalBase64LengthException::IllegalBase64LengthException(size_t _len)
        : Exception("Length of base64: " + std::to_string(_len) + ", Not divisible by 3.") {
}
Base64DecodeException::Base64DecodeException()
        : Exception("Illegal base64 string given.") {
}



void UnitTest() {
    try {
        char s[] = "this is a test for base64";
        char encode[1024];
        Encode((const unsigned char *) s, strlen(s), encode);
        printf("%s\n", encode);
    
        unsigned char decode[1024];
        Decode(encode, strlen(encode), decode);
        if (0 != strcmp(s, (const char *)decode)) {
            printf("error, not identical\n");
        }
        
    } catch (std::exception &e) {
        printf("base64 exception: %s\n", e.what());
    }
}

}

