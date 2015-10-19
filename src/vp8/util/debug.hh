#ifndef _DEBUG_HH_
#define _DEBUG_HH_
extern bool g_threaded;
namespace LeptonDebug {
extern int raw_decoded_fp_Y;
extern int raw_decoded_fp_Cb;
extern int raw_decoded_fp_Cr;
extern unsigned char *raw_YCbCr[3];
int getDebugWidth(int color);
int getDebugHeight(int color);
void dumpDebugData();
void setupDebugData(int lumaWidth, int lumaHeight,
                   int chromaWidth, int chromaHeight);
}
#endif
