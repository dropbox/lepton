#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <errno.h>
#include "billing.hh"
#define BILLING_MAP_INIT(X) 0,

Sirikata::Array1d<Sirikata::Array1d<std::atomic<uint32_t>,
                                    (uint32_t)Billing::NUM_BILLING_ELEMENTS>, 2> billing_map;
void write_string(int fd, const char *write_ptr) {
    while(write(fd, write_ptr, strlen(write_ptr)) < 0 && errno == EINTR) {
    }
}
void write_number(int fd, int64_t number) {
    char output[32];
    memset(output, 0, sizeof(output));
    char * write_ptr = output + 1;
    {
        write_ptr[0] = '0' + (number / 1000000000)%10;
        write_ptr[1] = '0' + (number / 100000000)%10;
        write_ptr[2] = '0' + (number / 10000000)%10;
        write_ptr[3] = '0' + (number / 1000000)%10;
        write_ptr[4] = '0' + (number / 100000)%10;
        write_ptr[5] = '0' + (number / 10000)%10;
        write_ptr[6] = '0' + (number / 1000)%10;
        write_ptr[7] = '0' + (number / 100)%10;
        write_ptr[8] = '0' + (number / 10)%10;
        write_ptr[9] = '0' + (number / 1)%10;
        while(write_ptr[0] == '0') {
            ++write_ptr;
        }
    }
    if (number < 0) {
        --write_ptr;
        write_ptr[0] = '-';
    } else if (number == 0) {
        --write_ptr;
        write_ptr[0] = '0';        
    }
    write_string(fd, write_ptr);
}

void write_pct(int fd, double ratio) {
    write_number(fd, (int64_t)(ratio * 100));
    while(write(fd, ".", 1) < 0 && errno == EINTR) {
    }
    write_number(fd, (int64_t)((int64_t)(ratio * 100000) % 1000));
}

template<class T> void print_item(int fd, const char * name, const T &uncompressed, const T &compressed) {
    write_string(fd, name);
    write_string(fd, ": ");
    write_number(fd, uncompressed / 8);
    write_string(fd, ".");
    write_number(fd, uncompressed % 8);
    write_string(fd, " vs ");
    write_number(fd, compressed/8);
    write_string(fd, ".");
    write_number(fd, compressed % 8);
    write_string(fd, " = ");
    double x = compressed;
    if (uncompressed) {
        x /= uncompressed;
    } else {
        x = 0;
    }
    write_pct(fd, x);
    write_string(fd, "%\n");

}

void fixup_bill() {
    size_t edge_cost = billing_map[0][(int)Billing::BITMAP_EDGE].load();
    edge_cost += billing_map[0][(int)Billing::EXP1_EDGE].load();
    edge_cost += billing_map[0][(int)Billing::EXP2_EDGE].load();
    edge_cost += billing_map[0][(int)Billing::EXP3_EDGE].load();
    edge_cost += billing_map[0][(int)Billing::EXPN_EDGE].load();
    edge_cost += billing_map[0][(int)Billing::SIGN_EDGE].load();
    edge_cost += billing_map[0][(int)Billing::RES_EDGE].load();

    size_t cost_7x7 = billing_map[0][(int)Billing::BITMAP_7x7].load();
    cost_7x7 += billing_map[0][(int)Billing::EXP1_7x7].load();
    cost_7x7 += billing_map[0][(int)Billing::EXP2_7x7].load();
    cost_7x7 += billing_map[0][(int)Billing::EXP3_7x7].load();
    cost_7x7 += billing_map[0][(int)Billing::EXPN_7x7].load();
    cost_7x7 += billing_map[0][(int)Billing::SIGN_7x7].load();
    cost_7x7 += billing_map[0][(int)Billing::RES_7x7].load();
    // we only track overall EOB cost... we divide this among edge vs 7x7 by
    // using the ratio of other bits used by edge vs 7x7
    (void)cost_7x7;
    (void)edge_cost;
    /*
    size_t non_nonzero_cost = cost_7x7 + edge_cost;
    size_t num_nonzero_cost = billing_map[0][(int)Billing::NZ_7x7].load()
        + billing_map[0][(int)Billing::NZ_EDGE].load();
    billing_map[0][(int)Billing::NZ_7x7] -= billing_map[0][(int)Billing::NZ_7x7].load();
    billing_map[0][(int)Billing::NZ_EDGE] -= billing_map[0][(int)Billing::NZ_EDGE].load();
    billing_map[0][(int)Billing::NZ_EDGE] += num_nonzero_cost * edge_cost / non_nonzero_cost;
    billing_map[0][(int)Billing::NZ_7x7] += num_nonzero_cost * cost_7x7 / non_nonzero_cost;
    */
    // we also tally some of the bitmap cost to EOB cost, since the "not eob" idea gets
    // partially paid for in the bitmap huffman code cost
    /*
    uint32_t bitmap = billing_map[0][(int)Billing::BITMAP_7x7];
    billing_map[0][(int)Billing::BITMAP_7x7] -= bitmap/2;
    billing_map[0][(int)Billing::NZ_7x7] += bitmap/2;

    bitmap = billing_map[0][(int)Billing::BITMAP_EDGE];
    billing_map[0][(int)Billing::BITMAP_EDGE] -= bitmap/2;
    billing_map[0][(int)Billing::NZ_EDGE] += bitmap/2;
    */
    // not all signs are created equal in jpeg spec
    // this balances positive and negative by using the cost of unpredicted signs in
    // lepton-encoded jpegs to get the 'right' cost in normal jpeg
    double sign_ratio = billing_map[1][(int)Billing::SIGN_7x7]
        / (double)billing_map[0][(int)Billing::SIGN_7x7];
    int delta_7x7 = billing_map[1][(int)Billing::SIGN_7x7] - billing_map[0][(int)Billing::SIGN_7x7];
    billing_map[0][(int)Billing::SIGN_7x7] += delta_7x7;
    billing_map[0][(int)Billing::EXP1_7x7] -= delta_7x7;
    int delta_edge = sign_ratio * billing_map[0][(int)Billing::SIGN_EDGE]
        - billing_map[0][(int)Billing::SIGN_EDGE];
    billing_map[0][(int)Billing::SIGN_EDGE] += delta_edge;
    billing_map[0][(int)Billing::EXP1_EDGE] -= delta_edge;
    
    int delta_dc = sign_ratio * billing_map[0][(int)Billing::SIGN_DC]
        - billing_map[0][(int)Billing::SIGN_DC];
    billing_map[0][(int)Billing::SIGN_DC] += delta_dc;
    billing_map[0][(int)Billing::EXP1_DC] -= delta_dc;

}

void print_bill(int fd) {
#if defined(ENABLE_BILLING) || !defined(NDEBUG)
    fixup_bill(); // we made some approximations in mapping the JPEG spec to the new billing items
    write_string(fd, "::::BILL::::\n");
    size_t totals[2] = {0, 0};
    size_t totals_edge[2] = {0, 0};
    size_t totals_other[2] = {0, 0};
    size_t totals7x7[2] = {0, 0};
    size_t totals_dc[2] = {0, 0};
    for (int i = 0; i < (int)Billing::NUM_BILLING_ELEMENTS; ++i) {
        if (billing_map[0][i] || billing_map[1][i]) {
            totals[0] += billing_map[0][i];
            totals[1] += billing_map[1][i];
            print_item(fd, BillingString((Billing)i), billing_map[0][i], billing_map[1][i]);
        }
    }
    for (int comp = 0; comp < 2; ++comp) {

        totals_other[comp] += billing_map[comp][(int)Billing::HEADER].load();
        totals_other[comp] += billing_map[comp][(int)Billing::DELIMITERS].load();


        totals_edge[comp] += billing_map[comp][(int)Billing::NZ_EDGE].load();
        totals_edge[comp] += billing_map[comp][(int)Billing::BITMAP_EDGE].load();
        totals_edge[comp] += billing_map[comp][(int)Billing::EXP1_EDGE].load();
        totals_edge[comp] += billing_map[comp][(int)Billing::EXP2_EDGE].load();
        totals_edge[comp] += billing_map[comp][(int)Billing::EXP3_EDGE].load();
        totals_edge[comp] += billing_map[comp][(int)Billing::EXPN_EDGE].load();
        totals_edge[comp] += billing_map[comp][(int)Billing::SIGN_EDGE].load();
        totals_edge[comp] += billing_map[comp][(int)Billing::RES_EDGE].load();

        totals7x7[comp] += billing_map[comp][(int)Billing::NZ_7x7].load();
        totals7x7[comp] += billing_map[comp][(int)Billing::BITMAP_7x7].load();
        totals7x7[comp] += billing_map[comp][(int)Billing::EXP1_7x7].load();
        totals7x7[comp] += billing_map[comp][(int)Billing::EXP2_7x7].load();
        totals7x7[comp] += billing_map[comp][(int)Billing::EXP3_7x7].load();
        totals7x7[comp] += billing_map[comp][(int)Billing::EXPN_7x7].load();
        totals7x7[comp] += billing_map[comp][(int)Billing::SIGN_7x7].load();
        totals7x7[comp] += billing_map[comp][(int)Billing::RES_7x7].load();


        totals_dc[comp] += billing_map[comp][(int)Billing::EXP0_DC].load();
        totals_dc[comp] += billing_map[comp][(int)Billing::EXP1_DC].load();
        totals_dc[comp] += billing_map[comp][(int)Billing::EXP2_DC].load();
        totals_dc[comp] += billing_map[comp][(int)Billing::EXP3_DC].load();
        totals_dc[comp] += billing_map[comp][(int)Billing::EXPN_DC].load();
        totals_dc[comp] += billing_map[comp][(int)Billing::SIGN_DC].load();
        totals_dc[comp] += billing_map[comp][(int)Billing::RES_DC].load();

    }
    print_item(fd, "Overall 7x7", totals7x7[0], totals7x7[1]);
    print_item(fd, "Overall Edge", totals_edge[0], totals_edge[1]);
    print_item(fd, "Overall DC", totals_dc[0], totals_dc[1]);
    print_item(fd, "Overall Misc", totals_other[0], totals_other[1]);
    print_item(fd, "Total", totals[0], totals[1]);
    write_string(fd, "::::::::::::\n");
#endif
}
