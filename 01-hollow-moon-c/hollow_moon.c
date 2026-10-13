/* ============================================================================
   HOLLOW MOON — a movie trailer written from nothing.
   ----------------------------------------------------------------------------
   One C file. No images. No audio files. No video clips. No FFmpeg. No libs.
   Every pixel is placed by arithmetic. Every sound is summed from waveforms.
   The program hand-rolls a baseline JPEG encoder (MJPEG) and an AVI muxer,
   then interleaves 48 kHz stereo PCM it synthesized with the picture.

   Output: hollow_moon.avi  (512x288 @ 24 fps, ~74 s, MJPEG + PCM s16le)
           Play it with VLC, mpv, Windows Media Player, QuickTime, etc.

   Build:   cc -O2 -o hollow_moon hollow_moon.c -lm
   Credits: written, directed & scored by the program that printed this file.
   ============================================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

/* ------------------------------------------------------------------ format */
#define W        512          /* frame width   (divisible by 16 for 4:2:0)   */
#define H        288          /* frame height  (divisible by 16 for 4:2:0)   */
#define FPS      24
#define SR       48000        /* audio sample rate                           */
#define SPF      2000         /* audio samples per video frame (SR/FPS)      */
#define T_END    74.0         /* total seconds                               */
#define NFRAMES  ((int)(T_END * FPS))
#define LB_TOP   36           /* letterbox bar height (2.37:1 image)         */
#define LB_BOT   (H - 36)
#define NAUDIO   ((long)NFRAMES * SPF)

static const double PI = 3.14159265358979323846;

/* ------------------------------------------------------------- scene times */
static const double T_S0  =  0.0;   /* studio card                            */
static const double T_S1  =  6.0;   /* drowned city, the dead moon            */
static const double T_S2  = 14.0;   /* the bridge, the lamplighter            */
static const double T_S3  = 22.0;   /* lantern montage (3 cuts)               */
static const double T_S4  = 30.0;   /* the spirit                             */
static const double T_S5  = 38.0;   /* riser montage                          */
static const double T_S6  = 44.0;   /* silence. the price, typed.             */
static const double T_S7  = 48.5;   /* the release                            */
static const double T_S8  = 56.0;   /* title                                  */
static const double T_S9  = 64.0;   /* credits                                */
static const double T_S10 = 70.0;   /* last light                             */

/* ----------------------------------------------------------------- math ui */
static double dclamp(double v, double a, double b){ return v<a?a:(v>b?b:v); }
static double dmin(double a,double b){ return a<b?a:b; }
static double dmax(double a,double b){ return a>b?a:b; }
static double lerpd(double a, double b, double t){ return a + (b-a)*t; }
static double smooth(double t){ t=dclamp(t,0,1); return t*t*(3-2*t); }
static double ease_io(double t){ t=dclamp(t,0,1); return t<0.5 ? 2*t*t : 1-2*(1-t)*(1-t); }
/* segment ramp: 0 before a, 1 after b, smooth inside */
static double ramp(double t, double a, double b){ return smooth((t-a)/(b-a)); }
static double pulsd(double t, double a, double b){ return ramp(t,a,(a+b)/2) * (1-ramp(t,(a+b)/2,b)); }
static double frac1(double x){ return x-floor(x); }

/* ----------------------------------------------------------------- the rng */
static uint32_t rs = 0x9E3779B9u;
static uint32_t rnd32(void){
    rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs;
}
static double rndf(void){ return rnd32() * (1.0/4294967296.0); }
static uint32_t hashu(uint32_t x){
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x;
}
static double hashf(int x){ return hashu((uint32_t)x) * (1.0/4294967296.0); }

/* value noise 1D/2D — smooth, deterministic, no tables */
static double vnoise1(double x){
    int i = (int)floor(x); double f = x - i; f = f*f*(3-2*f);
    return lerpd(hashf(i), hashf(i+1), f);
}
static double vnoise2(double x, double y){
    int ix=(int)floor(x), iy=(int)floor(y); double fx=x-ix, fy=y-iy;
    fx = fx*fx*(3-2*fx); fy = fy*fy*(3-2*fy);
    double a = hashf(ix*374761393 + iy*668265263);
    double b = hashf((ix+1)*374761393 + iy*668265263);
    double c = hashf(ix*374761393 + (iy+1)*668265263);
    double d = hashf((ix+1)*374761393 + (iy+1)*668265263);
    return lerpd(lerpd(a,b,fx), lerpd(c,d,fx), fy);
}

/* --------------------------------------------------------- little endian io */
static void wu16(FILE*f, uint32_t v){ fputc(v&0xFF,f); fputc((v>>8)&0xFF,f); }
static void wu16be(FILE*f, uint32_t v){ fputc((v>>8)&0xFF,f); fputc(v&0xFF,f); }
static void wu32(FILE*f, uint32_t v){
    fputc(v&0xFF,f); fputc((v>>8)&0xFF,f); fputc((v>>16)&0xFF,f); fputc((v>>24)&0xFF,f);
}
static void wtag(FILE*f, const char*s){ fwrite(s,1,4,f); }

/* ================================================================== VIDEO ==
   Framebuffer, drawing primitives, the 5x7 ASCII-art font, post effects.   */

static uint8_t fb[W*H*3];              /* RGB, top-down                      */
static uint8_t blursmall[(W/4)*(H/4)*3];

static void fb_clear(int r,int g,int b){
    for(int i=0;i<W*H;i++){ fb[i*3]=(uint8_t)r; fb[i*3+1]=(uint8_t)g; fb[i*3+2]=(uint8_t)b; }
}
static inline int inb(int x,int y){ return x>=0 && x<W && y>=0 && y<H; }
static void px_a(int x,int y,int r,int g,int b,double a){
    if(!inb(x,y)) return; a=dclamp(a,0,1); if(a<=0) return;
    uint8_t*p=&fb[(y*W+x)*3];
    p[0]=(uint8_t)(p[0]+(r-p[0])*a); p[1]=(uint8_t)(p[1]+(g-p[1])*a); p[2]=(uint8_t)(p[2]+(b-p[2])*a);
}
static void px_add(int x,int y,int r,int g,int b,double a){
    if(!inb(x,y)) return; a=dclamp(a,0,1); if(a<=0) return;
    uint8_t*p=&fb[(y*W+x)*3];
    int nr=p[0]+(int)(r*a), ng=p[1]+(int)(g*a), nb=p[2]+(int)(b*a);
    p[0]=(uint8_t)(nr>255?255:nr); p[1]=(uint8_t)(ng>255?255:ng); p[2]=(uint8_t)(nb>255?255:nb);
}
static void fill_rect(int x0,int y0,int x1,int y1,int r,int g,int b,double a){
    for(int y=y0;y<y1;y++) for(int x=x0;x<x1;x++) px_a(x,y,r,g,b,a);
}
static void fill_circle(int cx,int cy,int rad,int r,int g,int b,double a){
    if(rad<=0) return;
    for(int y=cy-rad;y<=cy+rad;y++) for(int x=cx-rad;x<=cx+rad;x++){
        int dx=x-cx, dy=y-cy;
        if(dx*dx+dy*dy <= rad*rad) px_a(x,y,r,g,b,a);
    }
}
static void draw_line(int x0,int y0,int x1,int y1,int r,int g,int b,double a){
    int dx=abs(x1-x0), sx=x0<x1?1:-1;
    int dy=-abs(y1-y0), sy=y0<y1?1:-1;
    int err=dx+dy;
    for(;;){
        px_a(x0,y0,r,g,b,a);
        if(x0==x1 && y0==y1) break;
        int e2=2*err;
        if(e2>=dy){ err+=dy; x0+=sx; }
        if(e2<=dx){ err+=dx; y0+=sy; }
    }
}
/* additive radial glow — the soul of every lantern in this film */
static void glow_add(double cx,double cy,double rad,int r,int g,int b,double inten){
    int x0=(int)(cx-rad), x1=(int)(cx+rad), y0=(int)(cy-rad), y1=(int)(cy+rad);
    if(x0<0)x0=0; if(y0<0)y0=0; if(x1>W)x1=W; if(y1>H)y1=H;
    for(int y=y0;y<y1;y++) for(int x=x0;x<x1;x++){
        double dx=x-cx, dy=y-cy; double d=sqrt(dx*dx+dy*dy)/rad;
        if(d>=1) continue;
        double f=(1-d)*(1-d)*inten;
        px_add(x,y,r,g,b,f);
    }
}
static void fill_poly(const int*xs,const int*ys,int n,int r,int g,int b,double a){
    int y0=H,y1=0; for(int i=0;i<n;i++){ if(ys[i]<y0)y0=ys[i]; if(ys[i]>y1)y1=ys[i]; }
    if(y0<0)y0=0; if(y1>H)y1=H;
    for(int y=y0;y<=y1;y++){
        int xlist[32]; int nx=0;
        for(int i=0;i<n;i++){
            int j=(i+1)%n;
            int ya=ys[i], yb=ys[j];
            if((ya<=y&&yb>y)||(yb<=y&&ya>y)){
                double t=(double)(y-ya)/(yb-ya);
                int x=(int)(xs[i]+t*(xs[j]-xs[i]));
                if(nx<32) xlist[nx++]=x;
            }
        }
        for(int i=0;i<nx-1;i++) for(int j=i+1;j<nx;j++)
            if(xlist[j]<xlist[i]){ int t=xlist[i]; xlist[i]=xlist[j]; xlist[j]=t; }
        for(int i=0;i+1<nx;i+=2)
            for(int x=xlist[i];x<=xlist[i+1];x++) px_a(x,y,r,g,b,a);
    }
}

/* ---------------------------------------------------------- the 5x7 font --
   The typeface of this film, defined as ASCII art in the source itself.     */
typedef struct { uint8_t col[5]; } Glyph;
static Glyph FONT[128];
static int FONT_READY = 0;

static const char *GLYPHS[][8] = {
{"A","01110","10001","10001","11111","10001","10001","10001"},
{"B","11110","10001","10001","11110","10001","10001","11110"},
{"C","01110","10001","10000","10000","10000","10001","01110"},
{"D","11110","10001","10001","10001","10001","10001","11110"},
{"E","11111","10000","10000","11110","10000","10000","11111"},
{"F","11111","10000","10000","11110","10000","10000","10000"},
{"G","01110","10001","10000","10111","10001","10001","01111"},
{"H","10001","10001","10001","11111","10001","10001","10001"},
{"I","01110","00100","00100","00100","00100","00100","01110"},
{"J","00111","00010","00010","00010","00010","10010","01100"},
{"K","10001","10010","10100","11000","10100","10010","10001"},
{"L","10000","10000","10000","10000","10000","10000","11111"},
{"M","10001","11011","10101","10101","10001","10001","10001"},
{"N","10001","11001","10101","10011","10001","10001","10001"},
{"O","01110","10001","10001","10001","10001","10001","01110"},
{"P","11110","10001","10001","11110","10000","10000","10000"},
{"Q","01110","10001","10001","10001","10101","10010","01101"},
{"R","11110","10001","10001","11110","10100","10010","10001"},
{"S","01111","10000","10000","01110","00001","00001","11110"},
{"T","11111","00100","00100","00100","00100","00100","00100"},
{"U","10001","10001","10001","10001","10001","10001","01110"},
{"V","10001","10001","10001","10001","10001","01010","00100"},
{"W","10001","10001","10001","10101","10101","11011","10001"},
{"X","10001","10001","01010","00100","01010","10001","10001"},
{"Y","10001","10001","01010","00100","00100","00100","00100"},
{"Z","11111","00001","00010","00100","01000","10000","11111"},
{"0","01110","10001","10011","10101","11001","10001","01110"},
{"1","00100","01100","00100","00100","00100","00100","01110"},
{"2","01110","10001","00001","00010","00100","01000","11111"},
{"3","11111","00010","00100","00010","00001","10001","01110"},
{"4","00010","00110","01010","10010","11111","00010","00010"},
{"5","11111","10000","11110","00001","00001","10001","01110"},
{"6","00110","01000","10000","11110","10001","10001","01110"},
{"7","11111","00001","00010","00100","01000","01000","01000"},
{"8","01110","10001","10001","01110","10001","10001","01110"},
{"9","01110","10001","10001","01111","00001","00010","01100"},
{" ","00000","00000","00000","00000","00000","00000","00000"},
{".","00000","00000","00000","00000","00000","00100","00100"},
{",","00000","00000","00000","00000","00100","00100","01000"},
{"!","00100","00100","00100","00100","00100","00000","00100"},
{"'","00100","00100","00000","00000","00000","00000","00000"},
{"-","00000","00000","00000","11111","00000","00000","00000"},
{":","00000","00100","00100","00000","00100","00100","00000"},
{"?","01110","10001","00001","00010","00100","00000","00100"},
{"/","00001","00010","00010","00100","01000","01000","10000"},
{"(","00010","00100","01000","01000","01000","00100","00010"},
{")","01000","00100","00010","00010","00010","00100","01000"},
{0}
};

static void font_init(void){
    if(FONT_READY) return;
    for(int i=0; GLYPHS[i][0]; i++){
        int c = (unsigned char)GLYPHS[i][0][0];
        for(int col=0; col<5; col++){
            uint8_t v=0;
            for(int row=0; row<7; row++)
                if(GLYPHS[i][row+1][col]=='1') v |= (1u<<row);
            FONT[c].col[col]=v;
        }
    }
    FONT_READY=1;
}
static int text_w(const char*s,int scale,int ls){
    int n=(int)strlen(s);
    return n*(5*scale+ls)-ls;
}
static void draw_text_ex(const char*s,int x,int y,int scale,int ls,
                         int r,int g,int b,double a,int nmax){
    font_init();
    int n=(int)strlen(s); if(nmax>=0 && nmax<n) n=nmax;
    for(int i=0;i<n;i++){
        unsigned char c=(unsigned char)s[i];
        for(int col=0;col<5;col++){
            uint8_t bits=FONT[c].col[col];
            for(int row=0;row<7;row++) if(bits&(1u<<row)){
                for(int sy=0;sy<scale;sy++) for(int sx=0;sx<scale;sx++)
                    px_a(x+i*(5*scale+ls)+col*scale+sx, y+row*scale+sy, r,g,b,a);
            }
        }
    }
}
static void draw_text_c(const char*s,int cy,int scale,int ls,
                        int r,int g,int b,double a,int nmax){
    int x=(W-text_w(s,scale,ls))/2;
    draw_text_ex(s,x,cy,scale,ls,r,g,b,a,nmax);
}

/* --------------------------------------------------------------- art kit -- */
static void sky_gradient(int tr,int tg,int tb,int br,int bg,int bb){
    for(int y=0;y<H;y++){
        double t=(double)y/H;
        int r=(int)lerpd(tr,br,t), g=(int)lerpd(tg,bg,t), b=(int)lerpd(tb,bb,t);
        for(int x=0;x<W;x++){ uint8_t*p=&fb[(y*W+x)*3]; p[0]=(uint8_t)r; p[1]=(uint8_t)g; p[2]=(uint8_t)b; }
    }
}
static void stars(int n,double t,int ymax,double a){
    for(int i=0;i<n;i++){
        uint32_t h=hashu((uint32_t)i*7919+13);
        int x=(int)(h%W), y=(int)((h>>10)%(uint32_t)ymax);
        double tw=0.5+0.5*sin(t*(1.5+(h>>20)%10*0.4)+ (h>>8)%7);
        px_a(x,y,200,210,235,a*tw);
    }
}
/* the broken moon. bite = the chunk the world lost. */
static void dead_moon(double cx,double cy,double r,double biteA,double alpha){
    int x0=(int)(cx-r-2),x1=(int)(cx+r+2),y0=(int)(cy-r-2),y1=(int)(cy+r+2);
    if(x0<0)x0=0; if(y0<0)y0=0; if(x1>W)x1=W; if(y1>H)y1=H;
    double bx=cx+r*0.55, by=cy-r*0.62, br=r*0.52;   /* the bite */
    for(int y=y0;y<y1;y++) for(int x=x0;x<x1;x++){
        double dx=x-cx, dy=y-cy; double d=sqrt(dx*dx+dy*dy);
        if(d>r) continue;
        double dbx=x-bx, dby=y-by;
        if(sqrt(dbx*dbx+dby*dby)<br) continue;
        double n=vnoise2(x*0.045,y*0.045);
        double crater=(n<0.42)?(0.42-n)*0.9:0.0;
        double limb=1.0-0.38*pow(d/r,3.0);
        double v=dclamp((0.78+0.22*n)*limb-crater,0,1);
        int rr=(int)(205*v), gg=(int)(214*v), bb2=(int)(236*v);
        px_a(x,y,rr,gg,bb2,alpha);
    }
}
static void cloud_band(double y0,double hgt,double speed,double t,double a){
    for(int x=0;x<W;x++){
        double n=vnoise1(x*0.018+t*speed);
        int hh=(int)(n*hgt); if(hh<2) hh=2;
        for(int y=0;y<hh;y++){
            double u=(double)y/hh;
            double fade=1.0-(2.0*u-1.0)*(2.0*u-1.0);   /* soft both edges */
            px_a(x,(int)(y0+hh-y),10,13,26,a*fade);
        }
    }
}
static void skyline(int seed,int base_y,int amp,int r,int g,int b,double a,
                    double parallax,double windows,double t){
    for(int x=0;x<W;x++){
        double wx=x+parallax;
        double n=vnoise1(wx*0.012+seed*57.31)*0.65+vnoise1(wx*0.05+seed*13.7)*0.35;
        int top=base_y-(int)(n*amp);
        for(int y=top;y<LB_BOT;y++) px_a(x,y,r,g,b,a);
        if(windows>0){
            for(int wy=top+4; wy<base_y-4; wy+=9){
                uint32_t h=hashu((uint32_t)(x*131+wy*17+seed*997));
                if((h%100)<(uint32_t)(windows*100)){
                    double fl=0.55+0.45*vnoise1(x*0.9+t*2.0+h);
                    px_a(x+ (h>>4)%2, wy, 255*fl,190*fl,110*fl, a);
                }
            }
        }
    }
}
typedef struct { double x,y,spd,len; int layer; } Drop;
static Drop RAIN[420];
static void rain_init(void){
    for(int i=0;i<420;i++){
        RAIN[i].x=rndf()*W; RAIN[i].y=rndf()*H;
        RAIN[i].layer=(i<260)?0:1;
        RAIN[i].spd=RAIN[i].layer? 340+rndf()*180 : 150+rndf()*90;
        RAIN[i].len=RAIN[i].layer? 12+rndf()*7 : 6+rndf()*4;
    }
}
static void rain_draw(double dt,double a,double lx,double ly,double boost){
    for(int i=0;i<420;i++){
        Drop*d=&RAIN[i];
        d->y+=d->spd*dt; d->x-=d->spd*dt*0.16;
        if(d->y>H+16){ d->y=-16; d->x=rndf()*(W+90); }
        if(d->x<-16) d->x=W+14;
        double al=a*(d->layer?0.34:0.16);
        if(boost>0){
            double dx=d->x-lx, dy=d->y-ly;
            al += boost*exp(-(dx*dx+dy*dy)/2600.0);
        }
        draw_line((int)d->x,(int)d->y,(int)(d->x-d->len*0.18),(int)(d->y-d->len),190,205,235,dclamp(al,0,1));
    }
}
/* a floating lantern: warm core + breathing glow + rise + sway */
static void lantern(double x,double y,double s,double t,double a){
    double fl=0.82+0.18*vnoise1(t*7.0+x);
    glow_add(x,y,26*s,255,150,60,0.55*a*fl);
    glow_add(x,y,9*s,255,200,120,0.9*a);
    fill_circle((int)x,(int)y,(int)(3.2*s),(int)(255*fl),(int)(196*fl),(int)(120*fl),a);
}

/* --------------------------------------------------------------- post fx -- */
static void bloom(void){
    int bw=W/4, bh=H/4;
    memset(blursmall,0,sizeof(blursmall));
    for(int y=0;y<bh;y++) for(int x=0;x<bw;x++){
        int sy=y*4+2, sx=x*4+2;
        uint8_t*p=&fb[(sy*W+sx)*3];
        int v0=p[0]-118, v1=p[1]-118, v2=p[2]-118;
        blursmall[(y*bw+x)*3  ]=(uint8_t)(v0>0?v0:0);
        blursmall[(y*bw+x)*3+1]=(uint8_t)(v1>0?v1:0);
        blursmall[(y*bw+x)*3+2]=(uint8_t)(v2>0?v2:0);
    }
    /* two box passes, separable */
    static uint8_t tmp[(W/4)*(H/4)*3];
    for(int pass=0;pass<2;pass++){
        memcpy(tmp,blursmall,sizeof(tmp));
        for(int y=0;y<bh;y++) for(int x=0;x<bw;x++){
            for(int c=0;c<3;c++){
                int s=0;
                for(int k=-2;k<=2;k++){
                    int xx=x+k; if(xx<0)xx=0; if(xx>=bw)xx=bw-1;
                    s+=tmp[(y*bw+xx)*3+c];
                }
                blursmall[(y*bw+x)*3+c]=(uint8_t)(s/5);
            }
        }
        memcpy(tmp,blursmall,sizeof(tmp));
        for(int y=0;y<bh;y++) for(int x=0;x<bw;x++){
            for(int c=0;c<3;c++){
                int s=0;
                for(int k=-2;k<=2;k++){
                    int yy=y+k; if(yy<0)yy=0; if(yy>=bh)yy=bh-1;
                    s+=tmp[(yy*bw+x)*3+c];
                }
                blursmall[(y*bw+x)*3+c]=(uint8_t)(s/5);
            }
        }
    }
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        double fy=(double)y/H*(bh-1), fx=(double)x/W*(bw-1);
        int iy=(int)fy, ix=(int)fx; double ty=fy-iy, tx=fx-ix;
        int iy2=iy+1<bh?iy+1:bh-1, ix2=ix+1<bw?ix+1:bw-1;
        for(int c=0;c<3;c++){
            double a=(double)blursmall[(iy*bw+ix)*3+c], b=(double)blursmall[(iy*bw+ix2)*3+c];
            double cc=(double)blursmall[(iy2*bw+ix)*3+c], d=(double)blursmall[(iy2*bw+ix2)*3+c];
            double v=lerpd(lerpd(a,b,tx),lerpd(cc,d,tx),ty)*0.62;
            if(v>0.5) px_add(x,y,(int)v,(int)v,(int)v,1.0);
        }
    }
}
static double VIG[W*H];
static void vignette_init(void){
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        double nx=(x-W/2)/(W/2.0), ny=(y-H/2)/(H/2.0);
        double r=sqrt(nx*nx+ny*ny)/1.35;
        VIG[y*W+x]=dclamp(1.0-0.42*r*r,0,1);
    }
}
static void vignette(void){
    for(int i=0;i<W*H;i++){
        double v=VIG[i]; uint8_t*p=&fb[i*3];
        p[0]=(uint8_t)(p[0]*v); p[1]=(uint8_t)(p[1]*v); p[2]=(uint8_t)(p[2]*v);
    }
}
static void grain(int frame,double amt){
    uint32_t s=hashu((uint32_t)frame*2654435761u+7);
    for(int i=0;i<W*H;i++){
        s^=s<<13; s^=s>>17; s^=s<<5;
        int d=(int)(s%17)-8;
        uint8_t*p=&fb[i*3];
        int v=p[0]+d; p[0]=(uint8_t)(v<0?0:(v>255?255:v));
        v=p[1]+d;    p[1]=(uint8_t)(v<0?0:(v>255?255:v));
        v=p[2]+d;    p[2]=(uint8_t)(v<0?0:(v>255?255:v));
    }
}
static void letterbox(void){
    fill_rect(0,0,W,LB_TOP,0,0,0,1);
    fill_rect(0,LB_BOT,W,H-LB_BOT,0,0,0,1);
}
/* V-shaped dip: brightness 1 far away, 0 exactly at the seam (cut through black) */
static double dip(double t,double s,double w){
    double d=fabs(t-s);
    return smooth(dclamp((d-0.05)/w,0,1));
}
static double fade_mul(double t){
    double m=ramp(t,0.15,0.9);          /* open from black */
    m*=dip(t, 6.0,0.55);                /* card -> city        */
    m*=dip(t,14.0,0.55);                /* city -> bridge      */
    m*=dip(t,48.5,0.55);                /* silence -> release  */
    m*=dip(t,56.0,0.55);                /* release -> title    */
    m*=dip(t,64.0,0.55);                /* title -> credits    */
    m*=1.0-ramp(t,73.0,74.0);           /* final fade out      */
    return m;
}
static void apply_fade(double m){
    if(m>=0.999) return;
    for(int i=0;i<W*H;i++){
        uint8_t*p=&fb[i*3];
        p[0]=(uint8_t)(p[0]*m); p[1]=(uint8_t)(p[1]*m); p[2]=(uint8_t)(p[2]*m);
    }
}
static void white_flash(double t,double t0,double dur,double peak){
    double a=0;
    if(t>=t0 && t<t0+dur) a=peak*(1-(t-t0)/dur);
    if(a>0) for(int i=0;i<W*H;i++){
        uint8_t*p=&fb[i*3];
        int v;
        v=p[0]+(int)(255*a); p[0]=(uint8_t)(v>255?255:v);
        v=p[1]+(int)(255*a); p[1]=(uint8_t)(v>255?255:v);
        v=p[2]+(int)(255*a); p[2]=(uint8_t)(v>255?255:v);
    }
}

/* ================================================================== MJPEG ==
   A baseline JPEG encoder, written from the spec. MJPEG-in-AVI requires the
   tables inside every frame, which this does. Quality 72, 4:2:0.
   ========================================================================== */

static uint8_t  QYL[64], QCH[64];
static int      ZIG[64];
static uint16_t HCODE[4][256]; static uint8_t  HLEN[4][256]; /* dc0 ac0 dc1 ac1 */

static const uint8_t ZIGZAG[64]={
  0, 1, 8,16, 9, 2, 3,10,17,24,32,25,18,11, 4, 5,
 12,19,26,33,40,48,41,34,27,20,13, 6, 7,14,21,28,
 35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,
 58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63};

static const uint8_t BASE_Q_L[64]={
 16, 11, 10, 16, 24, 40, 51, 61,
 12, 12, 14, 19, 26, 58, 60, 55,
 14, 13, 16, 24, 40, 57, 69, 56,
 14, 17, 22, 29, 51, 87, 80, 62,
 18, 22, 37, 56, 68,109,103, 77,
 24, 35, 55, 64, 81,104,113, 92,
 49, 64, 78, 87,103,121,120,101,
 72, 92, 95, 98,112,100,103, 99};

static const uint8_t BASE_Q_C[64]={
 17, 18, 24, 47, 99, 99, 99, 99,
 18, 21, 26, 66, 99, 99, 99, 99,
 24, 26, 56, 99, 99, 99, 99, 99,
 47, 66, 99, 99, 99, 99, 99, 99,
 99, 99, 99, 99, 99, 99, 99, 99,
 99, 99, 99, 99, 99, 99, 99, 99,
 99, 99, 99, 99, 99, 99, 99, 99,
 99, 99, 99, 99, 99, 99, 99, 99};

static const uint8_t DC_L_BITS[17]={0,0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0};
static const uint8_t DC_L_VALS[12]={0,1,2,3,4,5,6,7,8,9,10,11};
static const uint8_t DC_C_BITS[17]={0,0,3,1,1,1,1,1,1,1,1,0,0,0,0,0,0};
static const uint8_t DC_C_VALS[12]={0,1,2,3,4,5,6,7,8,9,10,11};
static const uint8_t AC_L_BITS[17]={0,0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,0x7d};
static const uint8_t AC_L_VALS[162]={
 0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,
 0x22,0x71,0x14,0x32,0x81,0x91,0xA1,0x08,0x23,0x42,0xB1,0xC1,0x15,0x52,0xD1,0xF0,
 0x24,0x33,0x62,0x72,0x82,0x09,0x0A,0x16,0x17,0x18,0x19,0x1A,0x25,0x26,0x27,0x28,
 0x29,0x2A,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x43,0x44,0x45,0x46,0x47,0x48,0x49,
 0x4A,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x63,0x64,0x65,0x66,0x67,0x68,0x69,
 0x6A,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x83,0x84,0x85,0x86,0x87,0x88,0x89,
 0x8A,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,
 0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xC2,0xC3,0xC4,0xC5,
 0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xE1,0xE2,
 0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,
 0xF9,0xFA};
static const uint8_t AC_C_BITS[17]={0,0,2,1,2,4,4,3,4,7,5,4,4,0,1,2,0x77};
static const uint8_t AC_C_VALS[162]={
 0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,
 0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91,0xA1,0xB1,0xC1,0x09,0x23,0x33,0x52,0xF0,
 0x15,0x62,0x72,0xD1,0x0A,0x16,0x24,0x34,0xE1,0x25,0xF1,0x17,0x18,0x19,0x1A,0x26,
 0x27,0x28,0x29,0x2A,0x35,0x36,0x37,0x38,0x39,0x3A,0x43,0x44,0x45,0x46,0x47,0x48,
 0x49,0x4A,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x63,0x64,0x65,0x66,0x67,0x68,
 0x69,0x6A,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x82,0x83,0x84,0x85,0x86,0x87,
 0x88,0x89,0x8A,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0xA2,0xA3,0xA4,0xA5,
 0xA6,0xA7,0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xC2,0xC3,
 0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,
 0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,
 0xF9,0xFA};

typedef struct { FILE*f; uint32_t acc; int n; } BW;
static void bw_put(BW*b,uint32_t code,int len){
    for(int i=len-1;i>=0;i--){
        b->acc=(b->acc<<1)|((code>>i)&1); b->n++;
        if(b->n==8){
            uint8_t byte=(uint8_t)(b->acc&0xFF);
            fputc(byte,b->f);
            if(byte==0xFF) fputc(0x00,b->f);   /* byte stuffing */
            b->acc=0; b->n=0;
        }
    }
}
static void bw_flush(BW*b){
    while(b->n<8){ b->acc=(b->acc<<1)|1; b->n++; }
    fputc((uint8_t)(b->acc&0xFF),b->f);
    b->acc=0; b->n=0;
}

static void scale_qtab(const uint8_t*base,uint8_t*out,int quality){
    int scale = quality<50 ? 5000/quality : 200-2*quality;
    for(int i=0;i<64;i++){
        int v=(base[i]*scale+50)/100;
        if(v<1) v=1; if(v>255) v=255;
        out[i]=(uint8_t)v;
    }
}
/* canonical Huffman from BITS/HUFFVAL, JPEG spec Annex C */
static void build_huff(const uint8_t bits[17],const uint8_t*vals,int nvals,
                       uint16_t*code,uint8_t*len){
    uint16_t hcode[256]; uint8_t hlen[256];
    memset(hcode,0,sizeof(hcode)); memset(hlen,0,sizeof(hlen));
    int k=0; uint16_t c=0;
    for(int l=1;l<=16;l++){
        for(int i=0;i<bits[l];i++){
            if(k<256){ hcode[k]=c; hlen[k]=(uint8_t)l; }
            c++; k++;
        }
        c<<=1;
    }
    for(int i=0;i<nvals;i++){ code[vals[i]]=hcode[i]; len[vals[i]]=hlen[i]; }
}
static float CT[8][8];
static void jpeg_init(void){
    scale_qtab(BASE_Q_L,QYL,72);
    scale_qtab(BASE_Q_C,QCH,72);
    for(int i=0;i<64;i++) ZIG[ZIGZAG[i]]=i;
    build_huff(DC_L_BITS,DC_L_VALS,12,HCODE[0],HLEN[0]);
    build_huff(AC_L_BITS,AC_L_VALS,162,HCODE[1],HLEN[1]);
    build_huff(DC_C_BITS,DC_C_VALS,12,HCODE[2],HLEN[2]);
    build_huff(AC_C_BITS,AC_C_VALS,162,HCODE[3],HLEN[3]);
    for(int u=0;u<8;u++) for(int x=0;x<8;x++)
        CT[u][x]=(float)(cos((2*x+1)*u*PI/16.0)*(u==0?0.70710678:1.0));
}
static int cat_of(int v){
    int a=abs(v), c=0;
    while(a){ c++; a>>=1; }
    return c;
}
static void fdct8(float*b){
    float tmp[64];
    for(int y=0;y<8;y++) for(int u=0;u<8;u++){
        float s=0; for(int x=0;x<8;x++) s+=b[y*8+x]*CT[u][x];
        tmp[y*8+u]=s;
    }
    for(int x=0;x<8;x++) for(int v=0;v<8;v++){
        float s=0; for(int y=0;y<8;y++) s+=tmp[y*8+x]*CT[v][y];
        b[v*8+x]=s*0.25f;
    }
}
static int enc_blk(BW*bw,const float*blk,const uint8_t*qt,
                   int*prevdc,int dcT,int acT){
    float f[64]; int zz[64];
    memcpy(f,blk,sizeof(f));
    fdct8(f);
    for(int i=0;i<64;i++){
        int u=ZIGZAG[i];
        int q=(int)lrintf(f[u]/qt[u]);
        if(q<-1023) q=-1023; if(q>1023) q=1023;
        zz[i]=q;
    }
    int dc=zz[0]-*prevdc; *prevdc=zz[0];
    if(dc==0){ bw_put(bw,HCODE[dcT][0],HLEN[dcT][0]); }
    else{
        int c=cat_of(dc);
        bw_put(bw,HCODE[dcT][c],HLEN[dcT][c]);
        bw_put(bw,(uint32_t)(dc<0?dc-1:dc)&((1u<<c)-1),c);
    }
    int run=0;
    for(int i=1;i<64;i++){
        int v=zz[i];
        if(v==0){ run++; continue; }
        while(run>15){ bw_put(bw,HCODE[acT][0xF0],HLEN[acT][0xF0]); run-=16; }
        int c=cat_of(v);
        int sym=(run<<4)|c;
        bw_put(bw,HCODE[acT][sym],HLEN[acT][sym]);
        bw_put(bw,(uint32_t)(v<0?v-1:v)&((1u<<c)-1),c);
        run=0;
    }
    if(run>0) bw_put(bw,HCODE[acT][0x00],HLEN[acT][0x00]);
    return 0;
}
static float YPL[W*H], CBF[(W/2)*(H/2)], CRF[(W/2)*(H/2)];
static void encode_jpeg_frame(FILE*out){
    /* RGB -> YCbCr with 4:2:0 subsampling */
    memset(CBF,0,sizeof(CBF)); memset(CRF,0,sizeof(CRF));
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        uint8_t*p=&fb[(y*W+x)*3];
        float r=p[0],g=p[1],b=p[2];
        YPL[y*W+x] =  0.299f*r+0.587f*g+0.114f*b - 128.0f;   /* level shift! */
        CBF[(y/2)*(W/2)+(x/2)] += (-0.168736f*r-0.331264f*g+0.5f*b)*0.25f;
        CRF[(y/2)*(W/2)+(x/2)] += ( 0.5f*r-0.418688f*g-0.081312f*b)*0.25f;
    }
    BW bw={out,0,0};
    fputc(0xFF,out); fputc(0xD8,out);                       /* SOI  */
    fputc(0xFF,out); fputc(0xE0,out); wu16be(out,16);       /* APP0 JFIF */
    fwrite("JFIF\0",1,5,out); fputc(1,out); fputc(1,out); fputc(0,out);
    wu16be(out,1); wu16be(out,1); fputc(0,out); fputc(0,out);
    fputc(0xFF,out); fputc(0xDB,out); wu16be(out,132);      /* DQT  */
    fputc(0x00,out); fwrite(QYL,1,64,out);
    fputc(0x01,out); fwrite(QCH,1,64,out);
    fputc(0xFF,out); fputc(0xC0,out); wu16be(out,17);       /* SOF0 */
    fputc(8,out); wu16be(out,H); wu16be(out,W); fputc(3,out);
    fputc(1,out); fputc(0x22,out); fputc(0x00,out);         /* Y  2x2 */
    fputc(2,out); fputc(0x11,out); fputc(0x01,out);         /* Cb 1x1 */
    fputc(3,out); fputc(0x11,out); fputc(0x01,out);         /* Cr 1x1 */
    fputc(0xFF,out); fputc(0xC4,out); wu16be(out,2+(1+16+12)*2+(1+16+162)*2); /* DHT */
    fputc(0x00,out); for(int i=1;i<=16;i++) fputc(DC_L_BITS[i],out);
    fwrite(DC_L_VALS,1,12,out);
    fputc(0x10,out); for(int i=1;i<=16;i++) fputc(AC_L_BITS[i],out);
    fwrite(AC_L_VALS,1,162,out);
    fputc(0x01,out); for(int i=1;i<=16;i++) fputc(DC_C_BITS[i],out);
    fwrite(DC_C_VALS,1,12,out);
    fputc(0x11,out); for(int i=1;i<=16;i++) fputc(AC_C_BITS[i],out);
    fwrite(AC_C_VALS,1,162,out);
    fputc(0xFF,out); fputc(0xDA,out); wu16be(out,12);       /* SOS  */
    fputc(3,out);
    fputc(1,out); fputc(0x00,out);
    fputc(2,out); fputc(0x11,out);
    fputc(3,out); fputc(0x11,out);
    fputc(0,out); fputc(63,out); fputc(0,out);

    int dcy=0, dcb=0, dcr=0;
    float blk[64];
    for(int my=0;my<H/16;my++) for(int mx=0;mx<W/16;mx++){
        for(int s=0;s<4;s++){                                /* 4 luma blocks */
            int bx=(mx*16+(s%2)*8), by=(my*16+(s/2)*8);
            for(int y=0;y<8;y++) for(int x=0;x<8;x++) blk[y*8+x]=YPL[(by+y)*W+bx+x];
            enc_blk(&bw,blk,QYL,&dcy,0,1);
        }
        {                                                    /* Cb */
            int bx=mx*8, by=my*8;
            for(int y=0;y<8;y++) for(int x=0;x<8;x++) blk[y*8+x]=CBF[(by+y)*(W/2)+bx+x];
            enc_blk(&bw,blk,QCH,&dcb,2,3);
        }
        {                                                    /* Cr */
            int bx=mx*8, by=my*8;
            for(int y=0;y<8;y++) for(int x=0;x<8;x++) blk[y*8+x]=CRF[(by+y)*(W/2)+bx+x];
            enc_blk(&bw,blk,QCH,&dcr,2,3);
        }
    }
    bw_flush(&bw);
    fputc(0xFF,out); fputc(0xD9,out);                        /* EOI  */
}

/* ==================================================================== AVI ==
   RIFF AVI 1.0 muxer: one '00dc' MJPG chunk + one '01wb' PCM chunk per frame,
   interleaved, with an idx1 at the end. Hand-built, byte by byte.
   ========================================================================== */

static FILE  *AVI;
static long   MOVI_FOURCC_POS, MOVI_SIZE_POS, RIFF_SIZE_POS;
static long   AVIH_SBUF_POS, VID_STRH_SBUF_POS, VID_STRF_SIZEIMG_POS;
static long   MOVI_BYTES;
static long   MAX_VCHUNK, MAX_ACHUNK;
static uint32_t IDX_N;
typedef struct { uint32_t id, flags, offset, size; } IdxEnt;
static IdxEnt *IDX;

static void avi_begin(const char*path){
    AVI=fopen(path,"wb");
    RIFF_SIZE_POS=4;
    wtag(AVI,"RIFF"); wu32(AVI,0); wtag(AVI,"AVI ");
    /* ---------- LIST hdrl ----------
       hdrl LIST = 4 + (8+56)avih + (8+116)video-strl + (8+94)audio-strl = 294 */
    wtag(AVI,"LIST"); wu32(AVI,294); wtag(AVI,"hdrl");
    wtag(AVI,"avih"); wu32(AVI,56);
    wu32(AVI,1000000/FPS);            /* usec per frame          */
    wu32(AVI,1600000);                /* max bytes per sec (info)*/
    wu32(AVI,0);                      /* padding granularity     */
    wu32(AVI,0x10|0x100);             /* HASINDEX | ISINTERLEAVED*/
    wu32(AVI,(uint32_t)NFRAMES);      /* total frames            */
    wu32(AVI,0);                      /* initial frames          */
    wu32(AVI,2);                      /* streams                 */
    AVIH_SBUF_POS=ftell(AVI);
    wu32(AVI,300000);                 /* suggested buffer        */
    wu32(AVI,W); wu32(AVI,H);
    wu32(AVI,0); wu32(AVI,0); wu32(AVI,0); wu32(AVI,0);   /* reserved */
    /* video stream: strl LIST = 4 + (8+56) + (8+40) = 116 */
    wtag(AVI,"LIST"); wu32(AVI,116); wtag(AVI,"strl");
    wtag(AVI,"strh"); wu32(AVI,56);
    wtag(AVI,"vids"); wtag(AVI,"MJPG");
    wu32(AVI,0); wu16(AVI,0); wu16(AVI,0);
    wu32(AVI,0);                      /* initial frames */
    wu32(AVI,1); wu32(AVI,FPS);       /* scale, rate    */
    wu32(AVI,0); wu32(AVI,(uint32_t)NFRAMES); /* start, length */
    VID_STRH_SBUF_POS=ftell(AVI);
    wu32(AVI,300000);                 /* suggested buffer */
    wu32(AVI,0xFFFFFFFFu);            /* quality */
    wu32(AVI,0);                      /* sample size */
    wu16(AVI,0); wu16(AVI,0); wu16(AVI,W); wu16(AVI,H);   /* rcFrame */
    wtag(AVI,"strf"); wu32(AVI,40);   /* BITMAPINFOHEADER */
    wu32(AVI,40); wu32(AVI,W); wu32(AVI,H);
    wu16(AVI,1); wu16(AVI,24); wtag(AVI,"MJPG");
    VID_STRF_SIZEIMG_POS=ftell(AVI);
    wu32(AVI,W*H*3);
    wu32(AVI,0); wu32(AVI,0); wu32(AVI,0); wu32(AVI,0);
    /* audio stream: strl LIST = 4 + (8+56) + (8+18) = 94 */
    wtag(AVI,"LIST"); wu32(AVI,94); wtag(AVI,"strl");
    wtag(AVI,"strh"); wu32(AVI,56);
    wtag(AVI,"auds"); wu32(AVI,0);
    wu32(AVI,0); wu16(AVI,0); wu16(AVI,0);
    wu32(AVI,0);
    wu32(AVI,1); wu32(AVI,SR);        /* scale=1 rate=SR: units are samples */
    wu32(AVI,0); wu32(AVI,(uint32_t)NAUDIO);
    wu32(AVI,8192); wu32(AVI,0xFFFFFFFFu);
    wu32(AVI,4);                      /* sample size = 4 bytes (stereo 16-bit) */
    wu16(AVI,0); wu16(AVI,0); wu16(AVI,0); wu16(AVI,0);
    wtag(AVI,"strf"); wu32(AVI,18);   /* WAVEFORMATEX (PCM, cbSize=0) */
    wu16(AVI,1); wu16(AVI,2); wu32(AVI,SR); wu32(AVI,SR*4); wu16(AVI,4); wu16(AVI,16); wu16(AVI,0);
    /* ---------- LIST movi ---------- */
    wtag(AVI,"LIST"); MOVI_SIZE_POS=ftell(AVI); wu32(AVI,0); wtag(AVI,"movi");
    MOVI_FOURCC_POS=ftell(AVI)-4;
    MOVI_BYTES=4; MAX_VCHUNK=0; MAX_ACHUNK=0; IDX_N=0;
    IDX=malloc(sizeof(IdxEnt)*(size_t)(NFRAMES*2));
}
static void avi_video_frame(const uint8_t*jpg,long n){
    wtag(AVI,"00dc"); wu32(AVI,(uint32_t)n);
    fwrite(jpg,1,(size_t)n,AVI);
    if(n&1) fputc(0,AVI);
    long tot=n+(n&1);
    IDX[IDX_N].id=0x63643030u; /* "00dc" LE */ IDX[IDX_N].flags=0x10;
    IDX[IDX_N].offset=(uint32_t)(ftell(AVI)-8-tot-MOVI_FOURCC_POS);
    IDX[IDX_N].size=(uint32_t)n; IDX_N++;
    MOVI_BYTES+=8+tot;
    if(n>MAX_VCHUNK) MAX_VCHUNK=n;
}
static void avi_audio_chunk(const uint8_t*pcm){
    wtag(AVI,"01wb"); wu32(AVI,SPF*4);
    fwrite(pcm,1,(size_t)SPF*4,AVI);
    IDX[IDX_N].id=0x62773130u; /* "01wb" LE */ IDX[IDX_N].flags=0x10;
    IDX[IDX_N].offset=(uint32_t)(ftell(AVI)-8-SPF*4-MOVI_FOURCC_POS);
    IDX[IDX_N].size=(uint32_t)SPF*4; IDX_N++;
    MOVI_BYTES+=8+SPF*4;
    if(SPF*4>MAX_ACHUNK) MAX_ACHUNK=SPF*4;
}
static void avi_finish(void){
    wtag(AVI,"idx1"); wu32(AVI,IDX_N*16);
    for(uint32_t i=0;i<IDX_N;i++){
        wu32(AVI,IDX[i].id); wu32(AVI,IDX[i].flags);
        wu32(AVI,IDX[i].offset); wu32(AVI,IDX[i].size);
    }
    long eof=ftell(AVI);
    /* patch sizes at positions we saved while writing */
    fseek(AVI,RIFF_SIZE_POS,SEEK_SET); wu32(AVI,(uint32_t)(eof-8));
    fseek(AVI,MOVI_SIZE_POS,SEEK_SET); wu32(AVI,(uint32_t)(MOVI_BYTES+4));
    fseek(AVI,AVIH_SBUF_POS,SEEK_SET); wu32(AVI,(uint32_t)(MAX_VCHUNK+64));
    fseek(AVI,VID_STRH_SBUF_POS,SEEK_SET); wu32(AVI,(uint32_t)(MAX_VCHUNK+64));
    fseek(AVI,VID_STRF_SIZEIMG_POS,SEEK_SET); wu32(AVI,(uint32_t)(MAX_VCHUNK+64));
    fclose(AVI);
    free(IDX);
}

/* ================================================================== AUDIO ==
   Every sound in this film is grown from oscillators, noise and envelopes.
   Stereo float bus -> tiny Schroeder hall -> normalize -> tanh -> PCM.
   ========================================================================== */

static float *AL, *AR;                 /* the master bus */
static long   BUSN;

static void bus_add(long start,const float*mono,long n,double gain,double pan){
    double th=(pan+1.0)*PI*0.25;
    double gl=gain*cos(th), gr=gain*sin(th);
    for(long i=0;i<n;i++){
        long j=start+i; if(j<0||j>=BUSN) continue;
        AL[j]+=mono[i]*(float)gl; AR[j]+=mono[i]*(float)gr;
    }
}
static void bus_add_st(long start,const float*l,const float*r,long n,double gain){
    for(long i=0;i<n;i++){
        long j=start+i; if(j<0||j>=BUSN) continue;
        AL[j]+=l[i]*(float)gain; AR[j]+=r[i]*(float)gain;
    }
}
static double mtof(double m){ return 440.0*pow(2.0,(m-69.0)/12.0); }

/* one-pole lowpass in place */
static void lp1(float*b,long n,double cutoff){
    double a=1.0-exp(-2.0*PI*cutoff/SR), y=0;
    for(long i=0;i<n;i++){ y+=a*(b[i]-y); b[i]=(float)y; }
}
/* Chamberlin state-variable bandpass (resonant), in place, out->b */
static void svf_bp(float*b,long n,double f0,double f1,double Q){
    double f=2.0*sin(PI*f0/SR);
    double lo=0, band=0;
    for(long i=0;i<n;i++){
        double t=(double)i/n;
        double fc=lerpd(f0,f1,t); if(fc>12000) fc=12000;
        f=2.0*sin(PI*fc/SR);
        double in=b[i];
        lo+=f*band;          band+=f*(in-lo-Q*band);
        b[i]=(float)band;
    }
}
static void osc_sine(float*b,long n,double f0,double f1){
    double ph=0;
    for(long i=0;i<n;i++){
        double t=(double)i/n, f=exp(lerpd(log(f0),log(f1),t));
        ph+=2*PI*f/SR; if(ph>2*PI) ph-=2*PI;
        b[i]=(float)sin(ph);
    }
}
static void osc_saw(float*b,long n,double f0,double f1){
    double ph=0;
    for(long i=0;i<n;i++){
        double t=(double)i/n;
        double f=exp(lerpd(log(f0+1e-9),log(f1+1e-9),t));
        double inc=f/SR;
        ph+=inc; if(ph>=1) ph-=1;
        b[i]=(float)(2.0*ph-1.0);
    }
}
static void osc_noise(float*b,long n){
    for(long i=0;i<n;i++) b[i]=(float)(rndf()*2.0-1.0);
}
/* exponential env: attack, then decay with time-constant tau */
static void env_ad(float*b,long n,double att,double tau){
    for(long i=0;i<n;i++){
        double t=(double)i/SR;
        double a=1.0-exp(-t/att);
        double d=exp(-t/tau);
        b[i]*=(float)(a*d);
    }
}
static void gain_ramp(float*b,long n,double g0,double g1){
    for(long i=0;i<n;i++){
        double t=(double)i/n;
        b[i]*=(float)lerpd(g0,g1,t*t);
    }
}
#define TMPN (SR*8)
static float TMP[TMPN], TMP2[TMPN];

/* ----------------------------------------------------------- instruments -- */
static void ins_kick(double t,double g){
    long n=(long)(0.32*SR); if(n>TMPN) n=TMPN;
    osc_sine(TMP,n,105.0,42.0);
    env_ad(TMP,n,0.002,0.09);
    bus_add((long)(t*SR),TMP,n,g,0);
}
static void ins_taiko(double t,double g){
    long n=(long)(0.7*SR); if(n>TMPN) n=TMPN;
    osc_sine(TMP,n,72.0,38.0);  env_ad(TMP,n,0.003,0.16);
    osc_noise(TMP2,n); svf_bp(TMP2,n,110.0,80.0,0.8); env_ad(TMP2,n,0.001,0.05);
    for(long i=0;i<n;i++) TMP[i]+=TMP2[i]*0.9f;
    bus_add((long)(t*SR),TMP,n,g,0);
}
static void ins_braam(double t,double dur,double root,double g){
    long n=(long)(dur*SR); if(n>TMPN) n=TMPN;
    float *acc=TMP2; memset(acc,0,n*sizeof(float));
    double det[6]={1.0,1.007,0.996,2.0,1.503,2.996};
    for(int k=0;k<6;k++){
        osc_saw(TMP,n,root*det[k],root*det[k]);
        lp1(TMP,n,lerpd(220.0,900.0,0.5));
        for(long i=0;i<n;i++) acc[i]+=TMP[i]*(float)(0.5/(k<3?1.0:2.2));
    }
    env_ad(acc,n,0.35,dur*0.45);
    bus_add((long)(t*SR),acc,n,g,0.05);
}
static void ins_riser(double t,double dur,double g){
    long n=(long)(dur*SR); if(n>TMPN) n=TMPN;
    osc_noise(TMP,n);
    svf_bp(TMP,n,260.0,5400.0,4.0);
    gain_ramp(TMP,n,0.04,1.0);
    bus_add((long)(t*SR),TMP,n,g,0);
    osc_sine(TMP,n,180.0,850.0);
    for(long i=0;i<n;i++){
        double ph=i*(2*PI*(180+ (850-180)*(double)i/n)/SR);
        TMP[i]*=(float)(0.35+0.3*sin(ph*0.02));
    }
    gain_ramp(TMP,n,0.02,0.8);
    bus_add((long)(t*SR),TMP,n,g*0.5,0.2);
}
static void ins_piano(double t,double midi,double vel,double dur){
    long n=(long)(dur*SR); if(n>TMPN) n=TMPN;
    double f=mtof(midi);
    float *acc=TMP2; memset(acc,0,n*sizeof(float));
    double a5[5]={1.0,0.52,0.28,0.13,0.06};
    for(int k=0;k<5;k++){
        osc_sine(TMP,n,f*(k+1),f*(k+1));
        env_ad(TMP,n,0.004,dur/(1.0+1.7*k));
        for(long i=0;i<n;i++) acc[i]+=TMP[i]*(float)a5[k];
    }
    lp1(acc,n,5200.0);
    env_ad(acc,n,0.003,dur*0.55);
    double pan=dclamp((midi-60)/24.0,-0.5,0.5);
    bus_add((long)(t*SR),acc,n,vel,pan);
}
static void ins_musicbox(double t,double midi,double g){
    long n=(long)(1.4*SR); if(n>TMPN) n=TMPN;
    double f=mtof(midi);
    osc_sine(TMP,n,f,f); env_ad(TMP,n,0.002,0.45);
    osc_sine(TMP2,n,f*4,f*4); env_ad(TMP2,n,0.002,0.10);
    for(long i=0;i<n;i++) TMP[i]+=TMP2[i]*0.35f;
    osc_sine(TMP2,n,f*1.003,f*1.003); env_ad(TMP2,n,0.002,0.4);
    for(long i=0;i<n;i++) TMP[i]+=TMP2[i]*0.3f;
    env_ad(TMP,n,0.001,0.5);
    bus_add((long)(t*SR),TMP,n,g,-0.15);
}
static void ins_heartbeat(double t,double g){
    ins_kick(t,g*0.8); ins_kick(t+0.28,g*0.55);
}
static void ins_pad(double t,double dur,const int*midis,int nvoices,double g){
    long n=(long)(dur*SR); if(n>TMPN) n=TMPN;
    static float PL[TMPN], PR[TMPN];
    memset(PL,0,n*sizeof(float)); memset(PR,0,n*sizeof(float));
    for(int v=0;v<nvoices;v++){
        double f=mtof(midis[v]);
        osc_saw(TMP,n,f,f);      lp1(TMP,n,lerpd(320.0,760.0,0.5));
        osc_sine(TMP2,n,f*1.004,f*1.004);
        for(long i=0;i<n;i++) TMP[i]=(float)(TMP[i]*0.55+TMP2[i]*0.45);
        double pan=((v%2)?1:-1)*0.45;
        double vg=g/(0.6+nvoices*0.25);
        for(long i=0;i<n;i++){ PL[i]+=TMP[i]*(float)vg*(float)(1-pan*0.5); PR[i]+=TMP[i]*(float)vg*(float)(1+pan*0.5); }
    }
    double att=dmin(dur*0.35,2.2), rel=dmin(dur*0.4,2.0);
    for(long i=0;i<n;i++){
        double tt=(double)i/SR;
        double e=1.0-exp(-tt/att);
        double rem=dur-tt; if(rem<rel) e*=rem/rel;
        PL[i]*=(float)e; PR[i]*=(float)e;
    }
    bus_add_st((long)(t*SR),PL,PR,n,1.0);
}
static void ins_wind(double t,double dur,double g){
    long n=(long)(dur*SR); if(n>TMPN) n=TMPN;
    osc_noise(TMP,n);  svf_bp(TMP ,n,320.0,520.0,1.1);
    osc_noise(TMP2,n); svf_bp(TMP2,n,260.0,480.0,1.3);
    for(long i=0;i<n;i++){
        double tt=(double)i/SR;
        double lfo=0.7+0.3*sin(2*PI*0.06*tt+1.2)+0.12*sin(2*PI*0.023*tt);
        TMP [i]*=(float)(lfo*0.9);
        TMP2[i]*=(float)(lfo*0.9);
    }
    long st=(long)(t*SR);
    double att=dmin(dur*0.3,3.0), rel=dmin(dur*0.3,3.0);
    for(long i=0;i<n;i++){
        double tt=(double)i/SR; long j=st+i; if(j<0||j>=BUSN) continue;
        double e=1.0-exp(-tt/att);
        double rem=dur-tt; if(rem<rel) e*=dmax(rem,0)/rel;
        AL[j]+=TMP [i]*(float)(g*e);
        AR[j]+=TMP2[i]*(float)(g*e);
    }
}
static void ins_rain(double t,double dur,double g){
    long n=(long)(dur*SR); if(n>TMPN) n=TMPN;
    osc_noise(TMP,n);
    /* crude highpass: subtract lowpassed copy */
    lp1(TMP,n,1800.0);
    osc_noise(TMP2,n);
    for(long i=0;i<n;i++) TMP2[i]=(float)(TMP2[i]*0.6-TMP[i]*0.4);
    long st=(long)(t*SR);
    for(long i=0;i<n;i++){
        long j=st+i; if(j<0||j>=BUSN) continue;
        double tt=(double)i/SR;
        double e=ramp(tt,0,2.0)*(1-ramp(tt,dur-2.0,dur));
        AL[j]+=TMP2[i]*(float)(g*e);
        AR[j]+=TMP2[i]*(float)(g*e*0.92);
    }
}
static void ins_shimmer(double t,double dur,double g){
    int mid[6]={76,79,83,88,91,95};
    long n=(long)(dur*SR); if(n>TMPN) n=TMPN;
    static float SL[TMPN], SRb[TMPN];
    memset(SL,0,n*sizeof(float)); memset(SRb,0,n*sizeof(float));
    for(int v=0;v<6;v++){
        double f=mtof(mid[v]);
        osc_sine(TMP,n,f,f);
        for(long i=0;i<n;i++){
            double tt=(double)i/SR;
            double a=0.5+0.5*sin(2*PI*(0.11+0.03*v)*tt+v*2.1);
            double pan=(v%2)?-0.5:0.5;
            SL[i]+=(float)(TMP[i]*a*(1-pan*0.5)*0.16);
            SRb[i]+=(float)(TMP[i]*a*(1+pan*0.5)*0.16);
        }
    }
    for(long i=0;i<n;i++){
        double tt=(double)i/SR;
        double e=ramp(tt,0,dur*0.3)*(1-ramp(tt,dur*0.75,dur));
        SL[i]*=(float)e; SRb[i]*=(float)e;
    }
    bus_add_st((long)(t*SR),SL,SRb,n,g);
}
static void ins_subdrone(double t,double dur,double g){
    long n=(long)(dur*SR); if(n>TMPN) n=TMPN;
    osc_sine(TMP,n,41.2,41.2); osc_sine(TMP2,n,41.7,41.7);
    for(long i=0;i<n;i++) TMP[i]=(float)(TMP[i]+TMP2[i]*0.8);
    osc_sine(TMP2,n,82.4,82.4);
    for(long i=0;i<n;i++) TMP[i]=(float)(TMP[i]+TMP2[i]*0.25);
    long st=(long)(t*SR);
    for(long i=0;i<n;i++){
        long j=st+i; if(j<0||j>=BUSN) continue;
        double tt=(double)i/SR;
        double e=1.0-exp(-tt/2.0);
        double rem=dur-tt; if(rem<2.5) e*=dmax(rem,0)/2.5;
        AL[j]+=TMP[i]*(float)(g*e); AR[j]+=TMP[i]*(float)(g*e);
    }
}
static void ins_tick(double t,double g){
    long n=(long)(0.02*SR); if(n>TMPN) n=TMPN;
    osc_sine(TMP,n,1250.0,900.0); env_ad(TMP,n,0.001,0.008);
    bus_add((long)(t*SR),TMP,n,g,0);
}
/* the hall: three combs per channel, gentle, glue for the whole score */
static void apply_hall(double wet){
    static float *DRY_L=0,*DRY_R=0;
    long d1=(long)(0.031*SR), d2=(long)(0.047*SR), d3=(long)(0.061*SR);
    DRY_L=malloc(sizeof(float)*(size_t)BUSN);
    DRY_R=malloc(sizeof(float)*(size_t)BUSN);
    memcpy(DRY_L,AL,sizeof(float)*(size_t)BUSN);
    memcpy(DRY_R,AR,sizeof(float)*(size_t)BUSN);
    for(long i=0;i<BUSN;i++){
        float s=0;
        if(i-d1>=0) s+=AR[i-d1]; else s+=AR[0];
        if(i-d2>=0) s+=AR[i-d2]*0.8f;
        AR[i]=(float)(AR[i]+wet*s*0.5);
        /* feedback via re-reading dry + accumulated — cheap approximation */
        s=0;
        if(i-d3>=0) s+=DRY_R[i-d3]+AR[i-d3]*0.6f;
        AR[i]=(float)(AR[i]+wet*s*0.4);
        s=0;
        if(i-d2>=0) s+=DRY_L[i-d2]; else s+=DRY_L[0];
        if(i-d3>=0) s+=DRY_L[i-d3]*0.8f;
        AL[i]=(float)(AL[i]+wet*s*0.5);
        s=0;
        if(i-d1>=0) s+=DRY_L[i-d1]+AL[i-d1]*0.6f;
        AL[i]=(float)(AL[i]+wet*s*0.4);
    }
    free(DRY_L); free(DRY_R);
}

/* ================================================================== SCORE ==
   The film's story also lives here: every hit, note and silence is placed
   at the same clock the picture obeys. E minor. Silence is an instrument.
   ========================================================================== */
static void score(void){
    /* --- ambience: wind & rain, drowned out during the silence act --- */
    ins_wind(0, 44.5, 0.055);
    ins_wind(48.2, T_END-48.2, 0.045);
    ins_rain(0, 44.5, 0.030);
    ins_rain(48.2, T_END-48.2, 0.024);
    /* --- act I: dread ------------------------------------------------ */
    ins_subdrone(0.0, 14.5, 0.085);
    ins_heartbeat(2.6, 0.30); ins_heartbeat(4.4, 0.24); ins_heartbeat(5.9, 0.20);
    { int em[4]={28,40,43,47}; ins_pad(6.2, 8.2, em, 4, 0.10); }
    ins_taiko(8.0, 0.30); ins_taiko(12.0, 0.34);
    /* --- act II: the lamplighter (bridge motif, felt piano) ----------- */
    { int em9[4]={28,40,47,54}; ins_pad(14.0, 8.4, em9, 4, 0.075); }
    ins_piano(15.2,64,0.16,1.6); ins_piano(16.1,67,0.14,1.4);
    ins_piano(17.0,71,0.15,1.6); ins_piano(18.4,76,0.17,2.0);
    ins_piano(19.6,74,0.13,1.5); ins_piano(20.8,71,0.12,1.8);
    /* --- act III: lantern montage (cuts land on the taiko) ------------ */
    ins_taiko(22.0, 0.55); ins_taiko(24.83, 0.55); ins_taiko(27.66, 0.55);
    ins_piano(22.0,64,0.20,1.4); ins_piano(24.83,67,0.20,1.4); ins_piano(27.66,71,0.20,1.6);
    { int C[4]={36,48,52,55};  ins_pad(22.0, 3.0, C, 4, 0.10); }
    { int G[4]={31,43,47,50};  ins_pad(24.83,3.0, G, 4, 0.10); }
    { int D[4]={38,50,54,57};  ins_pad(27.66,3.0, D, 4, 0.10); }
    ins_shimmer(22.0, 8.5, 0.045);
    /* --- act IV: the spirit ------------------------------------------- */
    ins_braam(30.15, 3.6, 41.2, 0.20);
    ins_taiko(31.2, 0.5); ins_taiko(32.4, 0.5); ins_taiko(33.6, 0.5);
    { int eCb6[4]={28,40,48,52}; ins_pad(30.2, 7.8, eCb6, 4, 0.09); } /* E with b6: hunger */
    ins_subdrone(30.0, 8.5, 0.11);
    for(int i=0;i<6;i++) ins_heartbeat(34.4+i*0.62, 0.22+0.03*i);
    ins_riser(34.8, 3.2, 0.16);
    /* --- act V: rise (drums accelerate into the cut) ------------------- */
    ins_riser(40.0, 4.0, 0.22);
    ins_braam(41.6, 2.4, 41.2, 0.14);
    { double t=38.0; double iv=0.5;
      while(t<43.95){ ins_kick(t, 0.42);
        if(((int)(t*10))%5==0) { osc_noise(TMP,(long)(0.18*SR));
            svf_bp(TMP,(long)(0.18*SR),900,700,1.0); env_ad(TMP,(long)(0.18*SR),0.002,0.05);
            bus_add((long)(t*SR),TMP,(long)(0.18*SR),0.10,0); }
        t+=iv; if(t>40.0) iv=0.4; if(t>42.0) iv=0.3; } }
    /* --- act VI: silence (the price) ----------------------------------- */
    for(int i=0;i<19;i++) ins_tick(44.55+i*0.062, 0.05);   /* type-on */
    ins_heartbeat(45.6, 0.26); ins_heartbeat(46.9, 0.22);
    /* --- act VII: the release (music box over felt piano) -------------- */
    { int em9b[5]={28,40,43,47,54}; ins_pad(48.8, 7.4, em9b, 5, 0.085); }
    ins_musicbox(49.2,76,0.10); ins_musicbox(50.2,79,0.095);
    ins_musicbox(51.2,83,0.10); ins_musicbox(52.4,88,0.11);
    ins_musicbox(53.6,86,0.10); ins_musicbox(54.8,83,0.09);
    ins_piano(49.2,52,0.12,2.2); ins_piano(51.2,55,0.11,2.0); ins_piano(53.6,59,0.11,2.2);
    /* --- act VIII: title ------------------------------------------------ */
    ins_braam(56.15, 4.2, 41.2, 0.24); ins_taiko(56.2, 0.75);
    { int emT[5]={28,40,43,47,54}; ins_pad(56.2, 7.6, emT, 5, 0.115); }
    ins_kick(58.9, 0.5); ins_kick(60.4, 0.38);
    ins_shimmer(56.0, 8.0, 0.05);
    /* --- act IX: credits (rain and three notes) ------------------------- */
    ins_musicbox(64.9,76,0.085); ins_musicbox(66.9,83,0.08); ins_musicbox(68.9,88,0.075);
    /* --- act X: last light ----------------------------------------------- */
    ins_subdrone(66.0, 8.0, 0.055);
    ins_musicbox(70.6,76,0.075); ins_musicbox(71.6,83,0.07); ins_musicbox(72.6,88,0.065);
    ins_heartbeat(73.30, 0.4);
}

/* ================================================================== SCENES ==
   Ten acts of arithmetic. Letterbox interior: y in [36, 252).
   ========================================================================== */

/* the girl: a silhouette made of a few honest lines */
static void draw_girl(int x,int base_y,double sway,double t,double rim,int run){
    int hd_r=5;
    int hy=base_y-46;
    double sw=sway;
    int INKR=4,INKG=5,INKB=10;
    /* legs (stride if running) */
    if(run){
        int ph=((int)(t*10))&1;
        draw_line(x-2,base_y-18,x-6-ph*3,base_y,INKR,INKG,INKB,1);
        draw_line(x+2,base_y-18,x+6+ph*3,base_y,INKR,INKG,INKB,1);
    }else{
        draw_line(x-2,base_y-18,x-4,base_y,INKR,INKG,INKB,1);
        draw_line(x+2,base_y-18,x+4,base_y,INKR,INKG,INKB,1);
    }
    /* coat: slight A-line */
    int cxl[4]={x-7,x+7,x+5,x-5}, cyl[4]={base_y-40,base_y-40,base_y-16,base_y-16};
    cxl[0]=(int)(x-7+sw); cxl[1]=(int)(x+7+sw);
    fill_poly(cxl,cyl,4,INKR,INKG,INKB,1);
    /* head */
    fill_circle((int)(x+sw),hy,hd_r,INKR,INKG,INKB,1);
    /* hair strands blowing */
    for(int s=0;s<3;s++){
        double ph=t*2.2+s*1.9;
        int hx=(int)(x+sw-5), hyy=hy-2+s;
        for(int k=0;k<7;k++){
            int sx2=hx-k, sy2=hyy+(int)(2.2*k*0.4+1.6*sin(ph+k*0.7));
            px_a(sx2,sy2,4,5,10,1); px_a(sx2,sy2+1,4,5,10,0.8);
        }
    }
    /* rim light: cool edge on the right */
    if(rim>0){
        for(int a2=-60;a2<=60;a2+=6){
            double an=a2*PI/180.0;
            px_a((int)(x+sw+hd_r*cos(an)),(int)(hy+hd_r*sin(an)),140,175,225,rim*0.5);
        }
        draw_line((int)(x+sw+6),base_y-39,(int)(x+sw+5),base_y-17,150,180,230,rim*0.4);
    }
}
static void draw_lantern_pole(int hx,int hy,int px2,int py2,double t){
    draw_line(hx,hy,px2,py2,6,7,12,1);
    double sw2=1.8*sin(t*1.1);
    int lx=px2+(int)sw2, ly=py2+8;
    draw_line(px2,py2,lx,ly,6,7,12,1);
    return; /* caller draws the lantern glow so it sits over the rain */
}

/* ---- S0: studio card ----------------------------------------------------- */
static void s0(double lt,double t){
    fb_clear(4,5,9);
    rain_draw(1.0/24,0.05+0.06*ramp(lt,3.0,5.0),0,0,0);
    double a=pulsd(lt,0.8,4.6);
    /* thin frame */
    for(int x=40;x<W-40;x++){ px_a(x,92,120,130,150,a*0.35); px_a(x,H-92,120,130,150,a*0.35); }
    for(int y=92;y<H-92;y++){ px_a(40,y,120,130,150,a*0.35); px_a(W-40,y,120,130,150,a*0.35); }
    draw_text_c("A MACHINE-DREAMED FILM",H/2-8,1,3,215,222,238,a,-1);
    draw_text_c("NO  CAMERAS  WERE  HARMED",H/2+14,1,2,90,98,120,a*0.8,-1);
    (void)t;
}
/* ---- S1: the city and the dead moon -------------------------------------- */
static void s1(double lt,double t){
    sky_gradient(6,8,20, 24,28,56);
    stars(90,t,LB_BOT-90,0.5);
    double mox=348-((W/2.0))*0, moy=118;
    glow_add(mox,moy,120,120,140,190,0.35);
    dead_moon(mox,moy,74,0.0,1.0);
    cloud_band(96,44,6.0,lt*0.7,0.42);
    cloud_band(130,30,9.0,lt*1.1+40,0.5);
    skyline(3,LB_BOT-30,64,13,17,36,1.0,lt*7,0,lt);
    skyline(7,LB_BOT-10,84,8,11,24,1.0,lt*16,0.30,lt);
    skyline(11,LB_BOT+6,96,4,6,15,1.0,lt*28,0.5,lt);
    /* rising lanterns of the city */
    for(int i=0;i<9;i++){
        double ph=(lt*0.16+hashf(i)*1.0);
        if(ph>1) continue;
        double x=40+hashf(i+50)*430, y=LB_BOT-ph*(LB_BOT-70);
        lantern(x,y,0.8,t+i,0.75);
    }
    rain_draw(1.0/24,0.30,0,0,0);
    double a=pulsd(lt,10.4,13.6);
    draw_text_c("IN THE YEAR WITHOUT DAWN",224,1,2,205,214,232,a,-1);
}
/* ---- S2: the bridge ------------------------------------------------------- */
static void s2(double lt,double t){
    double push=1.0+0.055*ease_io(lt/8.0);      /* slow push-in */
    sky_gradient(7,9,22, 30,30,60);
    stars(70,t,LB_BOT-80,0.45);
    /* moon, big, left */
    double mcx=140, mcy=124, mr=78;
    glow_add(mcx,mcy,mr*1.9,110,130,180,0.4);
    dead_moon(mcx,mcy,mr,0.0,1.0);
    cloud_band(100,40,4.0,lt*0.5+9,0.45);
    /* far city */
    skyline(17,LB_BOT-34,58,11,15,32,1.0,lt*4,0.12,lt);
    /* water shimmer under moon */
    for(int y=200;y<LB_BOT;y++) for(int x=90;x<200;x++){
        double n=vnoise2(x*0.3,y*0.5+t*2.2);
        if(n>0.72) px_a(x,y,140,160,200,0.10*(n-0.72)*8);
    }
    /* bridge */
    for(int x=0;x<W;x++){
        double d=(x-256)/256.0;
        int y=(int)(238+10*d*d);
        for(int yy=y;yy<LB_BOT;yy++) px_a(x,yy,4,5,10,1);
    }
    for(int x=0;x<W;x+=3){
        double d=(x-256)/256.0;
        int y=(int)(238+10*d*d);
        px_a(x,y-1,7,9,16,1);
    }
    for(int x=8;x<W;x+=26){                      /* railing */
        double d=(x-256)/256.0;
        int y=(int)(238+10*d*d);
        draw_line(x,y-13,x,y,5,6,12,1);
    }
    /* the lamplighter */
    int gx=(int)(212*push+(256-212)*(1-push)*0 - 0), gy=(int)(238+10*((212-256)/256.0)*((212-256)/256.0));
    gx=(int)((gx-256)*push+256); gy=(int)((gy-256)*0)+gy;
    double sway=1.5*sin(t*0.9);
    draw_girl((int)(gx),gy,sway,t,0.55,0);
    /* lantern on her pole — drawn after rain for glow-over look later; here: pole */
    draw_lantern_pole((int)(gx+8),(int)(gy-38),(int)(gx+21),(int)(gy-52),t);
    /* mist */
    for(int x=0;x<W;x++){
        double n=vnoise1(x*0.02+t*0.35);
        for(int k=0;k<3;k++) px_a(x,LB_BOT-8-k-(int)(n*6),60,70,95,0.10);
    }
    rain_draw(1.0/24,0.32,(gx+21),(gy-44),0.55);
    /* her lantern glows through the rain */
    lantern(gx+21+1.8*sin(t*1.1),gy-44,1.0,t,1.0);
    double a=pulsd(lt,17.8,21.6);
    draw_text_c("THEY SAY THE MOON DIED SCREAMING",224,1,2,205,214,232,a,-1);
}
/* ---- S3: lantern montage (3 cuts) ------------------------------------------ */
static void s3(double lt,double t){
    if(lt<2.833){                                  /* 3a: the window */
        fb_clear(8,9,16);
        fill_rect(0,0,W,LB_BOT,7,8,14,1);
        int wx=150,wy=70,ww=210,wh=130;
        fill_rect(wx,wy,wx+ww,wy+wh,3,4,8,1);
        glow_add(wx+ww/2,wy+wh/2,120,255,170,80,0.16+0.1*ramp(lt,1.2,2.4));
        fill_rect(wx,wy,wx+ww,wy+wh,0,0,0,0.0);
        for(int i=0;i<10;i++) fill_rect(wx+8,wy+8+i*13,wx+ww-8,wy+12+i*13,10,11,20,1);
        fill_rect(wx+ww/2-3,wy,wx+ww/2+3,wy+wh,4,5,10,1);
        draw_girl(120,240,0.6*sin(t*0.8),t,0.35,0);
        lantern(305,196,0.9,t,0.4+0.6*ramp(lt,1.4,2.5));
        rain_draw(1.0/24,0.20,wx+ww/2,wy+wh/2,0.3);
    }else if(lt<5.666){                            /* 3b: the torii, release */
        double l2=lt-2.833;
        sky_gradient(14,12,30, 40,32,52);
        glow_add(256,140,170,255,140,60,0.12);
        /* torii gate */
        int ty=110;
        fill_rect(120,ty,136,LB_BOT,4,4,10,1);
        fill_rect(376,ty,392,LB_BOT,4,4,10,1);
        fill_rect(96,ty-6,416,ty+10,4,4,10,1);
        fill_rect(108,ty+34,404,ty+44,4,4,10,1);
        draw_girl(180,236,0.4*sin(t),t,0.3,0);
        double ry=196-l2*44, rx=180+14+l2*6;
        lantern(rx,ry,0.9,t,1.0);
        rain_draw(1.0/24,0.22,0,0,0);
        for(int i=0;i<24;i++){                       /* light motes */
            double ph=frac1(l2*0.5+hashf(i));
            px_add((int)(hashf(i+9)*W),(int)(LB_BOT-ph*180),255,200,120,0.25*(1-ph));
        }
    }else{                                          /* 3c: the city answers */
        double l3=lt-5.666;
        sky_gradient(6,8,20, 22,26,52);
        dead_moon(340,110,60,0.0,1.0);
        cloud_band(92,30,7.0,t*0.8+30,0.4);
        skyline(23,LB_BOT-24,66,11,15,32,1.0,l3*10,0.1,t);
        skyline(29,LB_BOT,80,5,7,16,1.0,l3*20,0.3,t);
        for(int i=0;i<46;i++){
            double ph=frac1(l3*0.55+hashf(i)*0.9);
            double x=20+hashf(i+3)*472, y=LB_BOT-ph*(LB_BOT-64);
            lantern(x,y,0.55+0.4*hashf(i+7),t+i*0.3,0.8*(1-ph*0.4));
        }
        rain_draw(1.0/24,0.2,0,0,0);
    }
    double a=pulsd(lt,23.6,29.6);
    draw_text_c("EVERY LANTERN REMEMBERS A NAME",224,1,2,210,216,232,a,-1);
}
/* ---- S4: the spirit --------------------------------------------------------- */
static void s4(double lt,double t){
    if(lt<1.05){                                   /* eyes in the dark */
        fb_clear(2,3,7);
        double a=ramp(lt,0.15,0.7);
        int ex=W/2-26, ey=138;
        for(int e=0;e<2;e++){
            int x0=ex+e*52;
            for(int i=0;i<18;i++){
                double u=i/17.0;
                int yy=ey+(int)(6*u);
                int len=(int)(13*(1-fabs(u-0.5)*1.3));
                if(len<1) continue;
                draw_line(x0-len,yy,x0+len,yy,185,232,255,a*0.9);
            }
            glow_add(x0,ey+3,17,120,190,255,0.5*a);
        }
        return;
    }
    double l2=lt-1.05;
    if(l2<0.0) l2=0;
    /* the wolf crosses the dead moon */
    sky_gradient(4,5,12, 14,16,34);
    double mox=256+40, moy=128;
    glow_add(mox,moy,150,100,120,170,0.5);
    dead_moon(mox,moy,88,0.0,1.0);
    /* wolf head silhouette slides across — enters almost at once */
    double wx=-300+l2*205;
    int xs[12], ys[12];
    double s=1.0;
    double pts[12][2]={
        {0,0},{70,-52},{120,-40},{168,-72},{196,-30},   /* ears/mane */
        {238,-2},{260,26},{224,38},{170,44},{110,40},{40,30},{0,26}};
    for(int i=0;i<12;i++){ xs[i]=(int)(wx+pts[i][0]*s); ys[i]=(int)(128+pts[i][1]*s); }
    fill_poly(xs,ys,12,3,4,9,1);
    /* smoke off its back */
    for(int i=0;i<26;i++){
        double ph=frac1(t*0.4+hashf(i));
        double px3=wx+40+hashf(i+2)*200, py3=128-40-ph*90;
        px_a((int)px3,(int)py3,20,22,34,0.30*(1-ph));
    }
    /* eyes track the camera */
    double ea=ramp(l2,0.05,0.4)*(1-ramp(l2,6.4,6.9));
    double exoff=10*sin(t*0.7);
    for(int e=0;e<2;e++){
        double x0=wx+176+e*30, y0=128-14;
        glow_add(x0+exoff,y0,15,150,220,255,0.75*ea);
        draw_line((int)(x0+exoff-7),(int)(y0+3),(int)(x0+exoff+7),(int)(y0+1),210,240,255,ea);
    }
    if(lt>=2.15&&lt<4.85){                          /* quick cuts */
        double q=lt-2.15;
        if(q<0.9){                                   /* flame closeup */
            fb_clear(3,3,7);
            double fl=0.8+0.25*vnoise1(t*13);
            int fx=256,fy=190;
            glow_add(fx,fy-30,120,255,150,50,0.5*fl);
            int fxp[8]={fx-16,fx-6,fx-3,fy-64- (int)(14*fl),fx+5,fy-50,fx+14,fy-20};
            (void)fxp;
            int pxl[6]={fx-15,fy+10, fx-7,fy-40, fx-2,fy-58-(int)(12*fl)};
            int pyl[6]={fy+10,fy-10,fy-58-(int)(12*fl),fy-58-(int)(12*fl),fy-30,fy-4};
            (void)pxl;(void)pyl;
            int xsf[5]={fx-15,fx-5,fx-1,fx+6,fx+14};
            int ysf[5]={fy+12,fy-34,fy-56-(int)(16*fl),fy-30,fy+12};
            fill_poly(xsf,ysf,5,255,170,60,1);
            int xsc[3]={fx-6,fx,fx+6}; int ysc[3]={fy+8,fy-30,fy+8};
            fill_poly(xsc,ysc,3,255,240,200,1);
            glow_add(fx,fy-24,40,255,200,110,0.8);
        }else if(q<1.8){                             /* her face, lantern-lit from below */
            fb_clear(7,8,14);
            int hx=256,hy=150;
            /* warm underlight first, so her silhouette cuts against it */
            glow_add(hx-30,hy+86,110,255,150,60,0.30);
            for(int y=-58;y<58;y++){
                double w2=44*sqrt(dmax(0,1-(y/58.0)*(y/58.0)));
                for(int x=0;x<(int)w2;x++){ px_a(hx-x,hy+y,3,3,8,1); if(x<10) px_a(hx+x,hy+y,3,3,8,1); }
            }
            for(int a2=-90;a2<=90;a2+=3){
                double an=a2*PI/180.0;
                px_a((int)(hx+44*cos(an)),(int)(hy+58*sin(an)*0.98),160,190,235,0.5);  /* cool rim, right */
            }
            for(int a2=100;a2<=200;a2+=4){           /* warm rim, lower-left */
                double an=a2*PI/180.0;
                px_a((int)(hx+46*cos(an)),(int)(hy+58*sin(an)*0.98),255,170,90,0.4);
            }
            px_a(hx+14,hy-6,220,235,255,0.9); px_a(hx+15,hy-6,220,235,255,0.9); /* eye glint */
            glow_add(hx+14,hy-6,12,180,210,255,0.5);
        }else{                                       /* the run */
            fb_clear(4,4,9);
            skyline(31,LB_BOT-20,40,7,9,18,1.0,t*90,0,t);
            double shk=3.0*vnoise1(t*30);
            draw_girl((int)(200+shk),236,0,t,0.4,1);
            for(int i=0;i<30;i++){
                int y2=60+ (int)(hashf(i)*160);
                draw_line((int)(hashf(i+4)*W),y2,(int)(hashf(i+4)*W)+50,y2,90,100,130,0.2);
            }
        }
    }
    double a=pulsd(lt,5.9,8.0);
    draw_text_c("IT HUNGERS FOR WHAT WE KEEP",224,1,2,210,216,232,a,-1);
}
/* ---- S5: riser montage ------------------------------------------------------- */
static void s5(double lt,double t){
    double cuts[8]={0.55,0.5,0.45,0.4,0.36,0.32,0.28,0.25};
    double acc=0; int idx=0; double lt2=lt;
    while(lt2>=cuts[idx%8]){ lt2-=cuts[idx%8]; acc+=cuts[idx%8]; idx++; if(idx>60) break; }
    int shot=idx%5;
    double p=lt2/dmin(cuts[idx%8],0.6);
    switch(shot){
    case 0:{ /* lantern grid igniting */
        fb_clear(5,6,13);
        skyline(37,LB_BOT,60,8,10,22,1.0,acc*40,0.2,t);
        int rows=5, cols=12;
        int lit=(int)(p*rows*cols*(acc*2+1))% (rows*cols);
        for(int r2=0;r2<rows;r2++) for(int c2=0;c2<cols;c2++){
            if(r2*cols+c2<=lit) lantern(40+c2*38,120+r2*22,0.5,t+c2,0.85);
        }
    } break;
    case 1:{ /* the moon pulses */
        sky_gradient(4,5,12,16,18,38);
        double pr=64+10*sin(acc*9.0);
        glow_add(256,132,pr*2.0,140,160,210,0.5);
        dead_moon(256,132,pr,0.0,1.0);
    } break;
    case 2:{ /* the run, wider */
        fb_clear(4,4,9);
        skyline(41,LB_BOT-16,50,7,9,18,1.0,acc*120,0,t);
        draw_girl(180+ (int)(120*acc*3)%300,234,0,t,0.45,1);
        for(int i=0;i<40;i++){
            int y2=50+(int)(hashf(i)*180);
            draw_line((int)(frac1(hashf(i+8)+acc*0.4)*W),y2,((int)(frac1(hashf(i+8)+acc*0.4)*W))+40,y2,90,100,135,0.16);
        }
    } break;
    case 3:{ /* the eyes narrow */
        fb_clear(2,3,7);
        for(int e=0;e<2;e++){
            int x0=186+e*140, y0=140;
            glow_add(x0,y0,26,140,210,255,0.8);
            draw_line(x0-20,y0+4,x0+20,y0-2,215,240,255,0.9);
        }
    } break;
    default:{ /* everything rises */
        sky_gradient(4,5,12,18,20,42);
        for(int i=0;i<70;i++){
            double ph=frac1(acc*1.4+hashf(i)*0.9);
            double x=hashf(i+1)*W, y=LB_BOT-ph*(LB_BOT-60);
            px_add((int)x,(int)y,255,200,130,0.5*(1-ph));
            if(i%9==0) lantern(x,y,0.4,t+i,0.6*(1-ph));
        }
    } break;
    }
    /* shake on the pulse */
    double sh=2.2*vnoise1(t*26);
    if(sh>0.1){
        static uint8_t shbuf[W*H*3];
        memcpy(shbuf,fb,sizeof(fb));
        int dx=(int)(sh*2-2), dy=(int)(sh-1);
        for(int y=0;y<H;y++) for(int x=0;x<W;x++){
            int sx=x+dx, sy=y+dy;
            if(sx<0)sx=0; if(sx>=W)sx=W-1; if(sy<0)sy=0; if(sy>=H)sy=H-1;
            uint8_t*q=&shbuf[(sy*W+sx)*3];
            fb[(y*W+x)*3]=q[0]; fb[(y*W+x)*3+1]=q[1]; fb[(y*W+x)*3+2]=q[2];
        }
    }
}
/* ---- S6: silence, the price ---------------------------------------------------- */
static void s6(double lt,double t){
    fb_clear(1,1,3);
    double a1=ramp(lt,0.3,0.7);
    int n1=(int)((lt-0.55)/0.062); if(n1<0) n1=0;
    draw_text_c("TO RELIGHT THE MOON",126,2,4,222,228,240,a1,-1);
    if(n1>0) draw_text_c("TO RELIGHT THE MOON",126,2,4,222,228,240,1,n1);
    double a2=pulsd(lt,2.4,4.3);
    draw_text_c("SHE MUST LET GO OF HERS",152,2,4,222,228,240,a2,-1);
    (void)t;
}
/* ---- S7: the release ------------------------------------------------------------ */
static void s7(double lt,double t){
    sky_gradient(5,7,16, 20,22,44);
    stars(60,t,120,0.4);
    /* the moon, right, huge, beginning to stir */
    double mox=392, moy=142, mr=86;
    double stir=ramp(lt,3.8,7.4);
    glow_add(mox,moy,mr*(1.8+0.5*stir),110+80*stir,130+70*stir,180+50*stir,0.35+0.3*stir);
    dead_moon(mox,moy,mr,0.0,1.0);
    cloud_band(104,34,5.0,lt*0.6+50,0.4);
    /* hill she stands on */
    for(int x=0;x<W;x++){
        int top=(int)(224+8*sin(x*0.012+1.2)+4*vnoise1(x*0.05+9));
        for(int y=top;y<LB_BOT;y++) px_a(x,y,3,4,8,1);
    }
    int gx=176, gy=228;
    draw_girl(gx,gy,0.4*sin(t*0.8),t,0.5,0);
    /* the released lantern rises, carrying everything */
    double ph=ease_io(dmin(lt/7.2,1.0));
    double lx=gx+18+ph*190+16*sin(t*0.8+ph*3);
    double ly=gy-56-ph*(gy-56-64);
    lantern(lx,ly,1.05,t,1.0);
    /* drifting siblings */
    for(int i=0;i<10;i++){
        double p2=frac1(lt*0.09+hashf(i));
        lantern(30+hashf(i+4)*452,LB_BOT-p2*(LB_BOT-80),0.6,t+i,0.6);
    }
    rain_draw(1.0/24,0.10,0,0,0);
}
/* ---- S8: title ------------------------------------------------------------------- */
static void s8(double lt,double t){
    fb_clear(2,2,6);
    double a=ramp(lt,0.4,2.2);
    glow_add(256,138,150,90,105,150,0.4*a);
    dead_moon(256,132,84,0.0,0.16*a);
    draw_text_c("HOLLOW MOON",118,4,6,235,240,250,a,-1);
    glow_add(256,130,110,120,140,200,0.25*a);      /* bloom assist on the type */
    double a2=ramp(lt,3.6,4.6)*(1-ramp(lt,7.2,7.9));
    draw_text_c("SOME LIGHT IS WORTH EVERYTHING",176,1,3,205,214,232,a2,-1);
    (void)t;
}
/* ---- S9: credits ------------------------------------------------------------------ */
static void s9(double lt,double t){
    fb_clear(3,3,7);
    rain_draw(1.0/24,0.22,0,0,0);
    struct { const char*l1,*l2; } cards[3]={
        {"WRITTEN, DIRECTED & SCORED","BY ONE C PROGRAM"},
        {"EVERY PIXEL. EVERY SAMPLE.","FROM NOTHING ELSE."},
        {"NO IMAGES. NO CLIPS.","NO MERCY."}};
    int ci=(int)(lt/2.0); if(ci>2) ci=2;
    double a=pulsd(lt,ci*2.0+0.25,ci*2.0+1.85);
    draw_text_c(cards[ci].l1,128,1,3,205,212,230,a,-1);
    draw_text_c(cards[ci].l2,142,1,3,205,212,230,a,-1);
    (void)t;
}
/* ---- S10: last light ----------------------------------------------------------------- */
static void s10(double lt,double t){
    fb_clear(1,1,3);
    /* a sliver of waking moon, bottom right */
    glow_add(512,300,120,110,130,180,0.4);
    dead_moon(516,304,92,0.0,0.55);
    /* the last lantern we see */
    double ly=210-lt*13;
    lantern(320,ly,0.8,t,0.9);
    double a=pulsd(lt,0.9,3.4);
    draw_text_c("THIS WINTER",224,1,3,205,214,232,a,-1);
}

/* ================================================================== MAIN ==
   Pre-render the score, then run the projector: one frame of arithmetic,
   one JPEG squeezed out of it, one sliver of PCM, 1776 times.
   ========================================================================== */
static uint8_t *PCM;                    /* interleaved s16le stereo */

static void audio_to_pcm(void){
    double peak=1e-9;
    for(long i=0;i<BUSN;i++){
        double a=fabs(AL[i]); if(a>peak) peak=a;
        double b=fabs(AR[i]); if(b>peak) peak=b;
    }
    double g=0.90/peak;
    fprintf(stderr,"  audio peak %.3f -> gain %.3f\n",peak,g);
    for(long i=0;i<BUSN;i++){
        double l=tanh(1.2*AL[i]*g)*0.95;
        double r=tanh(1.2*AR[i]*g)*0.95;
        long j=i*4;
        int16_t sl=(int16_t)lrint(l*32767.0);
        int16_t sr=(int16_t)lrint(r*32767.0);
        PCM[j]  =(uint8_t)(sl&0xFF); PCM[j+1]=(uint8_t)((sl>>8)&0xFF);
        PCM[j+2]=(uint8_t)(sr&0xFF); PCM[j+3]=(uint8_t)((sr>>8)&0xFF);
    }
}

int main(int argc,char**argv){
    const char*out = argc>1? argv[1] : "hollow_moon.avi";
    fprintf(stderr,"HOLLOW MOON — rendering a film from nothing.\n");
    font_init();
    vignette_init();
    rain_init();
    jpeg_init();

    /* ---- audio ---- */
    BUSN=NAUDIO;
    AL=calloc((size_t)BUSN,sizeof(float));
    AR=calloc((size_t)BUSN,sizeof(float));
    PCM=malloc((size_t)NAUDIO*4);
    if(!AL||!AR||!PCM){ fprintf(stderr,"out of memory\n"); return 1; }
    rs=0xC0FFEE11u;                     /* deterministic sound */
    score();
    apply_hall(0.16);
    audio_to_pcm();

    /* ---- video ---- */
    avi_begin(out);
    rs=0xDEADBEEFu;                     /* deterministic picture */
    for(int f=0; f<NFRAMES; f++){
        double t=(double)f/FPS;
        if(f%120==0) fprintf(stderr,"  frame %d/%d  (%.1fs)\n",f,NFRAMES,t);
        if(t<T_S1)       s0(t-T_S0,t);
        else if(t<T_S2)  s1(t-T_S1,t);
        else if(t<T_S3)  s2(t-T_S2,t);
        else if(t<T_S4)  s3(t-T_S3,t);
        else if(t<T_S5)  s4(t-T_S4,t);
        else if(t<T_S6)  s5(t-T_S5,t);
        else if(t<T_S7)  s6(t-T_S6,t);
        else if(t<T_S8)  s7(t-T_S7,t);
        else if(t<T_S9)  s8(t-T_S8,t);
        else if(t<T_S10) s9(t-T_S9,t);
        else             s10(t-T_S10,t);

        if(t>43.85&&t<44.25) white_flash(t,43.9,0.22,0.85);
        bloom();
        vignette();
        grain(f,0);
        apply_fade(fade_mul(t));
        letterbox();

        /* encode + interleave */
        static uint8_t jpgbuf[512*1024];
        FILE*mem=tmpfile();
        if(!mem){ fprintf(stderr,"tmpfile failed\n"); return 1; }
        encode_jpeg_frame(mem);
        long n=ftell(mem);
        fseek(mem,0,SEEK_SET);
        if(n>(long)sizeof(jpgbuf)){ fprintf(stderr,"frame too big\n"); return 1; }
        if(fread(jpgbuf,1,(size_t)n,mem)!=(size_t)n){ fprintf(stderr,"readback failed\n"); return 1; }
        fclose(mem);
        avi_video_frame(jpgbuf,n);
        avi_audio_chunk(PCM+(long)f*SPF*4);
    }
    avi_finish();

    FILE*probe=fopen(out,"rb");
    fseek(probe,0,SEEK_END); long sz=ftell(probe); fclose(probe);
    fprintf(stderr,"done: %s (%.1f MB) — %d frames, %.1f s, every byte from arithmetic.\n",
            out,sz/1048576.0,NFRAMES,T_END);
    free(AL); free(AR); free(PCM);
    return 0;
}
