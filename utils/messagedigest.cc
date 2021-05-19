#include "messagedigest.h"
#include <stdio.h>


#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

/* blk0() and blk() perform the initial expand. */
/* I got the idea of expanding during the round function from SSLeay */
#if BYTE_ORDER == LITTLE_ENDIAN
#define blk0(i) (block->l[i] = (rol(block->l[i],24)&0xFF00FF00) \
    |(rol(block->l[i],8)&0x00FF00FF))
#elif BYTE_ORDER == BIG_ENDIAN
#define blk0(i) block->l[i]
#else
#error "Endianness not defined!"
#endif
#define blk(i) (block->l[i&15] = rol(block->l[(i+13)&15]^block->l[(i+8)&15] \
    ^block->l[(i+2)&15]^block->l[i&15],1))

/* (R0+R1), R2, R3, R4 are the different operations used in SHA1 */
#define R0(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk0(i)+0x5A827999+rol(v,5); w=rol(w,30)
#define R1(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk(i)+0x5A827999+rol(v,5); w=rol(w,30)
#define R2(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0x6ED9EBA1+rol(v,5); w=rol(w,30)
#define R3(v,w,x,y,z,i) z+=(((w|x)&y)|(w&x))+blk(i)+0x8F1BBCDC+rol(v,5); w=rol(w,30)
#define R4(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0xCA62C1D6+rol(v,5); w=rol(w,30)


namespace md {


void Sha1::Digest(const unsigned char *_msg, size_t _msg_len, unsigned char *_res) {
    Sha1Context context{};
    Sha1ContextInit(context);
    
    size_t last_block_len = _msg_len & 63;
    
    size_t curr = 0;
    while (curr + 64 <= _msg_len) {
        Sha1Transform(context.states, _msg + curr);
        curr += 64;
    }
    
    unsigned char last_block[64] = {0, };
    memcpy(last_block, _msg + _msg_len - last_block_len, last_block_len);
    _msg_len *= 8;
    
    last_block[last_block_len] = 0x80;
    if (last_block_len < 56) {
        for (int i = 0; i < 8; ++i) {
            last_block[56 + i] = (_msg_len >> ((7 - i) * 8)) & 0xff;
        }
    }
    Sha1Transform(context.states, last_block);
    
    if (last_block_len >= 56) {
        unsigned char new_block[64] = {0, };
        for (int i = 0; i < 8; ++i) {
            new_block[56 + i] = (_msg_len >> ((7 - i) * 8)) & 0xff;
        }
        Sha1Transform(context.states, new_block);
    }
    
    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 4; ++j) {
            _res[i * 4 + j] = (context.states[i] >> ((3 - j) * 8)) & 0xff;
        }
    }
    _res[20] = 0;
}


void Sha1::Sha1Transform(uint32_t _state[5], const unsigned char _buf[64]) {
    using Char64Long16 = union {
        unsigned char c[64];
        uint32_t l[16];
    };
    
    uint32_t a, b, c, d, e;

    auto *block = (Char64Long16 *) _buf;
    
    a = _state[0];
    b = _state[1];
    c = _state[2];
    d = _state[3];
    e = _state[4];
    
    R0(a, b, c, d, e, 0);
    R0(e, a, b, c, d, 1);
    R0(d, e, a, b, c, 2);
    R0(c, d, e, a, b, 3);
    R0(b, c, d, e, a, 4);
    R0(a, b, c, d, e, 5);
    R0(e, a, b, c, d, 6);
    R0(d, e, a, b, c, 7);
    R0(c, d, e, a, b, 8);
    R0(b, c, d, e, a, 9);
    R0(a, b, c, d, e, 10);
    R0(e, a, b, c, d, 11);
    R0(d, e, a, b, c, 12);
    R0(c, d, e, a, b, 13);
    R0(b, c, d, e, a, 14);
    R0(a, b, c, d, e, 15);
    R1(e, a, b, c, d, 16);
    R1(d, e, a, b, c, 17);
    R1(c, d, e, a, b, 18);
    R1(b, c, d, e, a, 19);
    R2(a, b, c, d, e, 20);
    R2(e, a, b, c, d, 21);
    R2(d, e, a, b, c, 22);
    R2(c, d, e, a, b, 23);
    R2(b, c, d, e, a, 24);
    R2(a, b, c, d, e, 25);
    R2(e, a, b, c, d, 26);
    R2(d, e, a, b, c, 27);
    R2(c, d, e, a, b, 28);
    R2(b, c, d, e, a, 29);
    R2(a, b, c, d, e, 30);
    R2(e, a, b, c, d, 31);
    R2(d, e, a, b, c, 32);
    R2(c, d, e, a, b, 33);
    R2(b, c, d, e, a, 34);
    R2(a, b, c, d, e, 35);
    R2(e, a, b, c, d, 36);
    R2(d, e, a, b, c, 37);
    R2(c, d, e, a, b, 38);
    R2(b, c, d, e, a, 39);
    R3(a, b, c, d, e, 40);
    R3(e, a, b, c, d, 41);
    R3(d, e, a, b, c, 42);
    R3(c, d, e, a, b, 43);
    R3(b, c, d, e, a, 44);
    R3(a, b, c, d, e, 45);
    R3(e, a, b, c, d, 46);
    R3(d, e, a, b, c, 47);
    R3(c, d, e, a, b, 48);
    R3(b, c, d, e, a, 49);
    R3(a, b, c, d, e, 50);
    R3(e, a, b, c, d, 51);
    R3(d, e, a, b, c, 52);
    R3(c, d, e, a, b, 53);
    R3(b, c, d, e, a, 54);
    R3(a, b, c, d, e, 55);
    R3(e, a, b, c, d, 56);
    R3(d, e, a, b, c, 57);
    R3(c, d, e, a, b, 58);
    R3(b, c, d, e, a, 59);
    R4(a, b, c, d, e, 60);
    R4(e, a, b, c, d, 61);
    R4(d, e, a, b, c, 62);
    R4(c, d, e, a, b, 63);
    R4(b, c, d, e, a, 64);
    R4(a, b, c, d, e, 65);
    R4(e, a, b, c, d, 66);
    R4(d, e, a, b, c, 67);
    R4(c, d, e, a, b, 68);
    R4(b, c, d, e, a, 69);
    R4(a, b, c, d, e, 70);
    R4(e, a, b, c, d, 71);
    R4(d, e, a, b, c, 72);
    R4(c, d, e, a, b, 73);
    R4(b, c, d, e, a, 74);
    R4(a, b, c, d, e, 75);
    R4(e, a, b, c, d, 76);
    R4(d, e, a, b, c, 77);
    R4(c, d, e, a, b, 78);
    R4(b, c, d, e, a, 79);
    _state[0] += a;
    _state[1] += b;
    _state[2] += c;
    _state[3] += d;
    _state[4] += e;
}


void Sha1::Sha1ContextInit(Sha1::Sha1Context &_ctx) {
    _ctx.states[0] = 0x67452301;
    _ctx.states[1] = 0xEFCDAB89;
    _ctx.states[2] = 0x98BADCFE;
    _ctx.states[3] = 0x10325476;
    _ctx.states[4] = 0xC3D2E1F0;
}

void Sha1::UnitTest() {
    char s1[] = "abc";
    unsigned char answer1[] = "";
    unsigned char digest1[21];
    Digest((const unsigned char *) s1, strlen(s1), digest1);
    for (int i = 0; i < 20; ++i) {
        printf("%x", digest1[i]);
    }
    printf("\n");
    
    char s2[] = "abc";
    unsigned char answer2[] = "";
    unsigned char digest2[21];
    Digest((const unsigned char *) s2, strlen(s2), digest2);
    for (int i = 0; i < 20; ++i) {
        printf("%x", digest2[i]);
    }
    printf("\n");
}


}
