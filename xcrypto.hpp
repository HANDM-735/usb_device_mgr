#ifndef XCRYO_H
#define XCRYO_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include "xbasic.hpp"

#define HIGHFIRST

#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))
#define MD5STEP(f, w, x, y, z, data, s) ( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )

class xcryto //加解密类
{
public:
    typedef struct md5_context
    {
        unsigned int buf[4];
        unsigned int bits[2];
        unsigned char in[64];
    }MD5_CTX;

public:
    xcryto() {}
    ~xcryto() {}

public:
    static std::string get_md5(std::string input) //获得MD5
    {
        struct md5_context md5Sc;
        unsigned char md5nature[16] = {0};
        md5_init(&md5Sc);
        md5_update(&md5Sc, (unsigned char *)input.data(), (unsigned int)input.length());
        md5_final(md5nature, &md5Sc);
        char cal_md5[36] = {0};
        xbasic::hex_to_str(md5nature,cal_md5,16);
        cal_md5[32] = '\0';
        for(unsigned int i=0;i<strlen(cal_md5);i++) cal_md5[i] = tolower(cal_md5[i]);
        return std::string(cal_md5);
    }

    static std::string get_file_md5(std::string file_path,int *ret_file_len) //获得文件的MD5
    {
        FILE *fd_file =fopen(file_path.c_str(),"rb");
        if(!fd_file) {if(ret_file_len) *ret_file_len=0; return "";}
        struct md5_context md5Sc;
        unsigned char md5nature[16] ={0};
        md5_init(&md5Sc);
        if(ret_file_len) //获得文件长度
        {
            fseek(fd_file,0L,SEEK_END); //定位到文件末尾
            *ret_file_len =ftell(fd_file); //得到文件大小
            fseek(fd_file,0L,SEEK_SET); //定位到文件开头
        }
        int read_len =0;
        char read_buff[1024] ={0};
        do
        {
            read_len =fread(read_buff,1,1024,fd_file);
            if(read_len>0) md5_update(&md5Sc,(unsigned char *)read_buff,(unsigned int)read_len);
        }while(read_len>=1024);
        fclose(fd_file);
        md5_final(md5nature, &md5Sc);
        char cal_md5[36] = {0};
        xbasic::hex_to_str(md5nature,cal_md5,16);
        cal_md5[32] = '\0';
        for(unsigned int i=0;i<strlen(cal_md5);i++) cal_md5[i] =tolower(cal_md5[i]);
        return std::string(cal_md5);
    }

protected:

#ifdef HIGHFIRST
#define byte_reverse(buf, len)
#else
static void byte_reverse(unsigned char *buf, unsigned long len)
{
    unsigned int t;
    do
    {
        t =(unsigned int) ((unsigned char)buf[3] << 8 | buf[2]) << 16 |
        ((unsigned char)buf[1] << 8 | buf[0]);
        *(unsigned int *) buf = t;
        buf += 4;
    }while (--len);
}
#endif

static void md5_init(struct md5_context *ctx) //Start MD5 accumulation. Set bit count to 0 and buffer to mysterious initialization constants.
{
    ctx->buf[0] = 0x67452301;
    ctx->buf[1] = 0xefcdab89;
    ctx->buf[2] = 0x98badcfe;
    ctx->buf[3] = 0x10325476;
    ctx->bits[0] = 0;
    ctx->bits[1] = 0;
}

static void md5_update(struct md5_context *ctx, unsigned char *buf, unsigned int len) //Update context to reflect the concatenation of another buffer full of bytes.
{
    unsigned int t = ctx->bits[0];
    if((ctx->bits[0] += (unsigned int) len <<3)) t < ctx->bits[1]++; //Carry from low to high
    t = (t >> 3) &0x3F; //Bytes already in shsInfo->data
    if(t)
    {
        unsigned char *p = (unsigned char *) ctx->in +t;
        t = 64 -t;
        if(len < t) {memcpy(p, buf, len); return;}
        memcpy(p, buf, t);
        byte_reverse(ctx->in, 16);
        md5_transform(ctx->buf, (unsigned int *) ctx->in);
        buf += t;
        len -= t;
    }
    while(len >=64) //Process data in 64-byte chunks
    {
        memcpy(ctx->in, buf, 64);
        byte_reverse(ctx->in, 16);
        md5_transform(ctx->buf,(unsigned int *) ctx->in);
        buf += 64;
        len -= 64;
    }
    memcpy(ctx->in, buf, len); //Handle any remaining bytes of data
}

static void md5_final(unsigned char digest[16], struct md5_context *ctx) //Final wrapup - pad to 64-byte boundary with the bit pattern 1 0* (64-bit count of bits processed, MSB-first)
{
    unsigned count = ctx->bits[0] >>3 &0x3F; //Compute number of bytes mod 64
    unsigned char *p = ctx->in +count;
    *p++ =0x80; //Set the first char of padding to 0x80. This is safe since there is always at least one byte free
    count =64 -1 -count; //Bytes of padding needed to make 64 bytes
    if(count <8) //Pad out to 56 mod 64
    {
        memset(p, 0, count);
        byte_reverse(ctx->in, 16);
        md5_transform(ctx->buf, (unsigned int *)ctx->in); //Two lots of padding: Pad the first block to 64 bytes
        memset(p,0, 56); //Now fill the next block with 56 bytes
    }
    else
    {
        memset(p, 0, count -8); //Pad block to 56 bytes
    }
    byte_reverse(ctx->in, 14);
    ((unsigned int *) ctx->in)[14] = ctx->bits[0]; //Append length in bits and transform
    ((unsigned int *) ctx->in)[15] = ctx->bits[1];
    md5_transform(ctx->buf, (unsigned int *)ctx->in);
    byte_reverse((unsigned char *) ctx->buf, 4);
    memcpy(digest, ctx->buf, 16);
    memset(ctx, 0, (size_t)sizeof(md5_context)); //In case it's sensitive
}

static void md5_transform(unsigned int buf[4], unsigned int in[16]) //The core of the MD5 algorithm
{
    register unsigned int a, b, c, d;
    a = buf[0];
    b = buf[1];
    c = buf[2];
    d = buf[3];

    MD5STEP(F1, a, b, c, d, in[0] + 0xd76aa478, 7);
    MD5STEP(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
    MD5STEP(F1, c, d, a, b, in[2] + 0x242070db, 17);
    MD5STEP(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
    MD5STEP(F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
    MD5STEP(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
    MD5STEP(F1, c, d, a, b, in[6] + 0xa8304613, 17);
    MD5STEP(F1, b, c, d, a, in[7] + 0xfd469501, 22);
    MD5STEP(F1, a, b, c, d, in[8] + 0x698098d8, 7);
    MD5STEP(F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
    MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
    MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
    MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122, 7);
    MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193, 12);
    MD5STEP(F1, c, d, a, b, in[14] + 0xa679438e, 17);
    MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821, 22);

    MD5STEP(F2, a, b, c, d, in[1] + 0xf61e2562, 5);
    MD5STEP(F2, d, a, b, c, in[6] + 0xc040b340, 9);
    MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
    MD5STEP(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
    MD5STEP(F2, a, b, c, d, in[5] + 0xd62f105d, 5);
    MD5STEP(F2, d, a, b, c, in[10] + 0x02441453, 9);
    MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
    MD5STEP(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
    MD5STEP(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
    MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
    MD5STEP(F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
    MD5STEP(F2, b, c, d, a, in[8] + 0x455a14ed, 20);
    MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
    MD5STEP(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
    MD5STEP(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
    MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

    MD5STEP(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
    MD5STEP(F3, d, a, b, c, in[8] + 0x8771f681, 11);
    MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
    MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
    MD5STEP(F3, a, b, c, d, in[1] + 0xa4beea44, 4);
    MD5STEP(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
    MD5STEP(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
    MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
    MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
    MD5STEP(F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
    MD5STEP(F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
    MD5STEP(F3, b, c, d, a, in[6] + 0x04881d05, 23);
    MD5STEP(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
    MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
    MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
    MD5STEP(F3, b, c, d, a, in[2] + 0xc4ac5665, 23);

    MD5STEP(F4, a, b, c, d, in[0] + 0xf4292244, 6);
    MD5STEP(F4, d, a, b, c, in[7] + 0x432aff97, 10);
    MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
    MD5STEP(F4, b, c, d, a, in[5] + 0xfc93a039, 21);
    MD5STEP(F4, a, b, c, d, in[3] + 0x8f0ccc92, 10);
    MD5STEP(F4, d, a, b, c, in[10] + 0xffeff47d, 15);
    MD5STEP(F4, c, d, a, b, in[1] + 0x85845dd1, 21);
    MD5STEP(F4, b, c, d, a, in[8] + 0x6fa87e4f, 6);
    MD5STEP(F4, a, b, c, d, in[15] + 0xfe2ce6e0, 10);
    MD5STEP(F4, d, a, b, c, in[6] + 0xa3014314, 15);
    MD5STEP(F4, c, d, a, b, in[13] + 0x4e0811a1, 21);
    MD5STEP(F4, b, c, d, a, in[4] + 0xf7537e82, 6);
    MD5STEP(F4, a, b, c, d, in[11] + 0xbd3af235, 10);
    MD5STEP(F4, d, a, b, c, in[2] + 0x2ad7d2bb, 15);
    MD5STEP(F4, c, d, a, b, in[9] + 0xeb86d391, 21);

    buf[0] += a;
    buf[1] += b;
    buf[2] += c;
    buf[3] += d;
};

#endif
