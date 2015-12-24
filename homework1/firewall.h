//
//  firewall.h
//  homework1
//
//  Created by 傑瑞 on 2015/12/24.
//  Copyright © 2015年 傑瑞. All rights reserved.
//

#ifndef firewall_h
#define firewall_h

#include <stdio.h>
#include <stdlib.h>
void init_firewall(char *conf);
int deny_connect_ip(in_addr_t ip);
int deny_bind_ip(in_addr_t ip);
#endif /* firewall_h */
