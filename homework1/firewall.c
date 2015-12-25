//
//  firewall.c
//  homework1
//
//  Created by 傑瑞 on 2015/12/24.
//  Copyright © 2015年 傑瑞. All rights reserved.
//

#include "firewall.h"

typedef struct {
    in_addr_t network_ip;
    int bit_shift;
} Permit;

int b_permit_size = 0;
Permit b_permits[50];
int c_permit_size = 0;
Permit c_permits[50];

void init_firewall(char *conf) {
    FILE *file = fopen(conf, "r");
    char buffer[100];
    int line = 0;
    if (file == NULL) {
        fprintf(stderr, "Firewall conf file: %s doesn't exist", conf);
        exit(2);
    }
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        line++;
        
        if (buffer[0] == '\n' || (buffer[0] == '\r' && buffer[0] == '\n'))
            continue;
        
        char method;
        in_addr_t ip_addr;
        int ip[4];
        int bits;
        
        int result = sscanf(buffer, "permit %c %d.%d.%d.%d/%d", &method, &ip[0], &ip[1], &ip[2], &ip[3], &bits);
        if (result != 6 || bits > 32 || bits < 0) {
            fprintf(stderr, "Firewall conf syntax error at line %d\n", line);
            exit(2);
        }
        
        ip_addr = ip[0] | ip[1] << 8 | ip[2] << 16 | ip[3] << 24;

        bits = 32 - bits;
        Permit permit;
        permit.bit_shift = bits;
        if (bits == 32)
            permit.network_ip = 0;
        else
            permit.network_ip = (ip_addr << bits) >> bits;
        
        if (method == 'b') {
            b_permits[b_permit_size++] = permit;
        } else if (method == 'c') {
            c_permits[c_permit_size++] = permit;
        } else {
            fprintf(stderr, "Unknown method '%c' at line %d\n", method, line);
            exit(2);
        }
    }
    fclose(file);
}

int deny_bind_ip(in_addr_t ip) {
    for (int i = 0; i < b_permit_size; i++) {
        int bit = b_permits[i].bit_shift;
        if (bit == 32)
            return 0;
        if (!(((ip << bit) >> bit) ^ b_permits[i].network_ip))
            return 0;
    }
    return 1;
}


int deny_connect_ip(in_addr_t ip) {
    for (int i = 0; i < c_permit_size; i++) {
        int bit = c_permits[i].bit_shift;
        if (bit == 32)
            return 0;
        if (!(((ip << bit) >> bit) ^ c_permits[i].network_ip))
            return 0;
    }
    return 1;
}