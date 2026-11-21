#include "boot_delta.h"
#include <string.h>
static uint16_t u16(const uint8_t *p) { return p[0] | ((uint16_t)p[1]<<8); }
static uint32_t u32(const uint8_t *p) { return u16(p) | ((uint32_t)u16(p+2)<<16); }
bool boot_delta_open(boot_delta_t *d, const uint8_t *data, size_t size)
{
    if (!d || !data || size<528 || memcmp(data,"VPB1",4) ||
        u16(data+4)!=360 || u16(data+6)!=360 || !u16(data+8) ||
        !u16(data+10) || u32(data+12)) return false;
    memset(d,0,sizeof(*d));
    d->frames=u16(data+8); d->period_ms=u16(data+10);
    for (unsigned i=0;i<256;i++) d->palette[i]=u16(data+16+i*2);
    d->cursor=data+528; d->end=data+size;
    return true;
}
static void pixel(uint16_t *dst, uint32_t at, uint16_t value)
{
    unsigned y=at/360, x=at%360;
    /* Destination is 720 x 720 RGB565; each source pixel becomes 2 x 2. */
    uint16_t *p=dst+y*720+x*2;
    p[0]=p[1]=p[720]=p[721]=value;
}
bool boot_delta_step(boot_delta_t *d, uint16_t *pixels, boot_rect_t *dirty)
{
    if (!d || !pixels || !dirty || d->frame>=d->frames ||
        (size_t)(d->end-d->cursor)<4) return false;
    const uint8_t *p=d->cursor;
    uint32_t bytes=u32(p); p+=4;
    if (bytes>(size_t)(d->end-p)) return false;
    const uint8_t *end=p+bytes; uint32_t at=0;
    *dirty=(boot_rect_t){.x1=719,.y1=719,.x2=0,.y2=0,.changed=false};
    while (p<end) {
        if (end-p<6) return false;
        unsigned skip=u16(p), count=u16(p+2), encoded=u16(p+4); p+=6;
        if (at>129600 || skip>129600-at) return false;
        at+=skip;
        if (count>129600-at || encoded>(size_t)(end-p)) return false;
        const uint8_t *stop=p+encoded;
        unsigned left=count;
        if (count) {
            if (at/360!=(at+count-1)/360) return false;
            unsigned x=(at%360)*2, y=(at/360)*2;
            if (x<dirty->x1) dirty->x1=x;
            if (y<dirty->y1) dirty->y1=y;
            if (x+count*2-1>dirty->x2) dirty->x2=x+count*2-1;
            if (y+1>dirty->y2) dirty->y2=y+1;
            dirty->changed=true;
        }
        while (left) {
            if (p==stop) return false;
            unsigned op=*p++, n=(op&127)+1;
            if (n>left) return false;
            if (op&128) {
                if (p==stop) return false;
                uint16_t c=d->palette[*p++];
                for (unsigned i=0;i<n;i++) pixel(pixels,at++,c);
            } else {
                if (n>(size_t)(stop-p)) return false;
                for (unsigned i=0;i<n;i++) pixel(pixels,at++,d->palette[*p++]);
            }
            left-=n;
        }
        if (p!=stop) return false;
    }
    d->cursor=end; d->frame++;
    return true;
}
