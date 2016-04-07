#include <unistd.h>
#include "billing.hh"
#define BILLING_MAP_INIT(X) 0,

std::atomic<uint32_t> billing_map[2][(uint32_t)Billing::NUM_BILLING_ELEMENTS] = {{}, {}};
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
void print_bill(int fd) {
    write_string(fd, "::::BILL::::\n");
    for (int i = 0; i < (int)Billing::NUM_BILLING_ELEMENTS; ++i) {
        if (billing_map[0][i] || billing_map[1][i]) {
            write_string(fd, BillingString((Billing)i));
            write_string(fd, ": ");
            write_number(fd, billing_map[0][i]);
            write_string(fd, " vs ");
            write_number(fd, billing_map[1][i]);
            write_string(fd, " = ");
            double x = billing_map[1][i];
            if (billing_map[0][i]) {
                x /= billing_map[0][i];
            } else {
                x = 0;
            }
            write_pct(fd, x);
            write_string(fd, "%\n");
        }
    }
    write_string(fd, "::::::::::::\n");
}
