#ifndef DEBUG_HH_
#define DEBUG_HH_
extern bool g_threaded;
namespace LeptonDebug {
extern int raw_decoded_fp_Y;
extern int raw_decoded_fp_Cb;
extern int raw_decoded_fp_Cr;
extern int med_err;
extern int amd_err;
extern int avg_err;
extern int ori_err;
extern int loc_err;
extern unsigned char *raw_YCbCr[4];
int getDebugWidth(int color);
int getDebugHeight(int color);
void dumpDebugData();
void setupDebugData(int lumaWidth, int lumaHeight,
                   int chromaWidth, int chromaHeight);
}
#endif
