struct huffCodes {
    unsigned short cval[ 256 ];
    unsigned short clen[ 256 ];
    unsigned short max_eobrun;
};

struct huffTree {
    unsigned short l[ 256 ];
    unsigned short r[ 256 ];
};

struct MergeJpegProgress {
    //unsigned int   len ; // length of current marker segment
    unsigned int   hpos; // current position in header
    unsigned int   ipos; // current position in imagedata
    unsigned int   rpos; // current restart marker position
    unsigned int   cpos; // in scan corrected rst marker position
    unsigned int   scan; // number of current scan
    unsigned char  type; // type of current marker segment
    unsigned int num_rst_markers_this_scan;
    bool within_scan;
    MergeJpegProgress *parent;
    MergeJpegProgress() {
        //len  = 0; // length of current marker segment
        hpos = 0; // current position in header
        ipos = 0; // current position in imagedata
        rpos = 0; // current restart marker position
        cpos = 0; // in scan corrected rst marker position
        num_rst_markers_this_scan = 0;
        scan = 1; // number of current scan
        type = 0x00; // type of current marker segment
        within_scan = false;
        parent = NULL;
    }
    MergeJpegProgress(MergeJpegProgress*par) {
        memcpy(this, par, sizeof(MergeJpegProgress));
        parent = par;
    }
    ~MergeJpegProgress() {
        if (parent != NULL) {
            MergeJpegProgress *origParent = parent->parent;
            memcpy(parent, this, sizeof(MergeJpegProgress));
            parent->parent = origParent;
        }
    }
private:
        MergeJpegProgress(const MergeJpegProgress&other); // disallow copy construction
        MergeJpegProgress& operator=(const MergeJpegProgress&other); // disallow gets
};
class bounded_iostream;
bool recode_baseline_jpeg(bounded_iostream* str_out,
                          int max_file_size);
