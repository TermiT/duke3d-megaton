//
//  mactools.m
//  duke3d
//
//  Created by termit on 1/22/13.
//  Copyright (c) 2013 General Arcade. All rights reserved.
//

#import "mactools.h"
#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#define MAX_PATH 2048

const char* Sys_GetResourceDir(void) {
    static char data[MAX_PATH] = { 0 };
    NSString * path = [[NSBundle mainBundle].resourcePath stringByAppendingPathComponent:@"gameroot"];
    if (!data[0]) {
        strcpy(data, path.UTF8String);
    }
    return data;
}

void Sys_ShowAlert(const char * text) {
    dispatch_async(dispatch_get_main_queue(), ^{
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:[NSString stringWithUTF8String:text]];
        [alert setInformativeText:@"Duke Nukem"];
        [alert addButtonWithTitle:@"OK"];
        [alert runModal];
    });
}

void Sys_StoreData(const char *filepath, const char *data) {
    FILE *fp = fopen(filepath, "ab");
    if (fp != NULL)
    {
        fputs(data, fp);
        fclose(fp);
    }
}

NSString * Sys_GetOSVersion() {
    NSProcessInfo *pInfo = [NSProcessInfo processInfo];
    NSString *version = [pInfo operatingSystemVersionString];
    return version;
}

bool Sys_IsSnowLeopard() {
    return (bool)([Sys_GetOSVersion() rangeOfString:@"10.6"].location != NSNotFound);
}