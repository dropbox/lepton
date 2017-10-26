#include "numeric.hh"
#include "branch.hh"

Branch Branch::update_lookup[256][256][2];
int do_set_update_lookup() {
    for (int i = 0;i < 256; ++i) {
        for (int j= 0;j< 256;++j) {
            for (int obs = 0; obs < 2; ++obs) {
                Branch cur = Branch::set_particular_value(i,j);
                cur.record_obs_and_update(obs ? true : false);
                Branch::update_lookup[i][j][obs] = cur;
            }
        }
    }
    return 1;
}
int set_update_lookup = do_set_update_lookup();
